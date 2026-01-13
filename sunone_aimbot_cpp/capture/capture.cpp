#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <iostream>
#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>
#include <timeapi.h>
#include <condition_variable>

#include "capture.h"
#ifdef USE_CUDA
#include "trt_detector.h"
#include "cuda_d3d11_interop.h"
#endif
#include "d3d11_staging_manager.h"
#include "sunone_aimbot_cpp.h"
#include "keycodes.h"
#include "keyboard_listener.h"
#include "other_tools.h"
#include "duplication_api_capture.h"
#include "winrt_capture.h"
#include "virtual_camera.h"
#include "gstreamer_capture.h"
#include "capture_utils.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "windowsapp.lib")

cv::Mat latestFrame;
std::mutex frameMutex;
std::mutex capturerMutex;

int screenWidth = 0;
int screenHeight = 0;

std::atomic<int> captureFrameCount(0);
std::atomic<int> captureFps(0);
std::chrono::time_point<std::chrono::high_resolution_clock> captureFpsStartTime;

// Zero-copy capture status (for UI display)
std::atomic<bool> zeroCopyActiveStatus(false);

std::deque<cv::Mat> frameQueue;

std::vector<cv::Mat> getBatchFromQueue(int batch_size)
{
    std::vector<cv::Mat> batch;
    std::lock_guard<std::mutex> lk(frameMutex);
    int n = std::min((int)frameQueue.size(), batch_size);

    for (int i = 0; i < n; ++i)
        batch.push_back(frameQueue[frameQueue.size() - n + i]);

    while (batch.size() < batch_size && !batch.empty())
        batch.push_back(batch.back().clone());
    return batch;
}

void captureThread(int CAPTURE_WIDTH, int CAPTURE_HEIGHT)
{
    try
    {
        if (config.verbose)
            std::cout << "[Capture] OpenCV version: " << CV_VERSION << std::endl;

        IScreenCapture* capturer = nullptr;
        
        // Zero-copy state variables
        bool zeroCopyInitialized = false;
        bool zeroCopyActive = false;
        
        if (config.capture_method == "duplication_api")
        {
            capturer = new DuplicationAPIScreenCapture(CAPTURE_WIDTH, CAPTURE_HEIGHT);
            if (config.verbose)
                std::cout << "[Capture] Using Duplication API" << std::endl;
            
            // Initialize zero-copy if enabled
            if (config.gpu_zero_copy && capturer->SupportsGpuCapture())
            {
                auto* ddaCapturer = dynamic_cast<DuplicationAPIScreenCapture*>(capturer);
                if (ddaCapturer)
                {
                    ID3D11Device* d3dDevice = ddaCapturer->GetD3D11Device();
                    ID3D11DeviceContext* d3dContext = ddaCapturer->GetD3D11Context();
                    
                    if (d3dDevice && d3dContext)
                    {
#ifdef USE_CUDA
                        if (config.backend == "TRT")
                        {
                            if (trt_detector.initializeCudaInterop(d3dDevice))
                            {
                                zeroCopyInitialized = true;
                                if (config.verbose)
                                    std::cout << "[Capture] CUDA-D3D11 interop initialized for TRT zero-copy" << std::endl;
                            }
                            else
                            {
                                std::cerr << "[Capture] Failed to initialize CUDA interop, falling back to CPU path" << std::endl;
                            }
                        }
#endif
                        if (config.backend == "DML" && dml_detector)
                        {
                            if (dml_detector->initializeStagingManager(d3dDevice, d3dContext, CAPTURE_WIDTH, CAPTURE_HEIGHT))
                            {
                                zeroCopyInitialized = true;
                                if (config.verbose)
                                    std::cout << "[Capture] D3D11 staging manager initialized for DML zero-copy" << std::endl;
                            }
                            else
                            {
                                std::cerr << "[Capture] Failed to initialize staging manager, falling back to CPU path" << std::endl;
                            }
                        }
                    }
                }
            }
        }
        else if (config.capture_method == "winrt")
        {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            capturer = new WinRTScreenCapture(CAPTURE_WIDTH, CAPTURE_HEIGHT);
            if (config.verbose)
                std::cout << "[Capture] Using WinRT" << std::endl;
        }
        else if (config.capture_method == "virtual_camera")
        {
            {
                std::lock_guard<std::mutex> lock(capturerMutex);
                capturer = new VirtualCameraCapture(config.virtual_camera_width, config.virtual_camera_heigth);
            }
            if (config.verbose)
                std::cout << "[Capture] Using Virtual Camera" << std::endl;
        }
        else if (config.capture_method == "gstreamer")
        {
            capturer = new GStreamerCapture(config.gstreamer_pipeline, CAPTURE_WIDTH, CAPTURE_HEIGHT);
            if (config.verbose)
                std::cout << "[Capture] Using GStreamer (network)" << std::endl;
        }
        else
        {
            config.capture_method = "duplication_api";
            config.saveConfig();
            capturer = new DuplicationAPIScreenCapture(CAPTURE_WIDTH, CAPTURE_HEIGHT);
            std::cout << "[Capture] Unknown capture_method. Set to duplication_api by default." << std::endl;
        }

        bool frameLimitingEnabled = false;
        std::optional<std::chrono::duration<double, std::milli>> frame_duration;
        if (config.capture_fps > 0.0)
        {
            timeBeginPeriod(1);
            frame_duration = std::chrono::duration<double, std::milli>(1000.0 / config.capture_fps);
            frameLimitingEnabled = true;
        }

        captureFpsStartTime = std::chrono::high_resolution_clock::now();

        auto start_time = std::chrono::high_resolution_clock::now();
        auto lastSaveTime = std::chrono::steady_clock::now();

        while (!shouldExit)
        {
            if (capture_fps_changed.load())
            {
                if (config.capture_fps > 0.0)
                {
                    if (!frameLimitingEnabled)
                    {
                        timeBeginPeriod(1);
                        frameLimitingEnabled = true;
                    }
                    frame_duration = std::chrono::duration<double, std::milli>(1000.0 / config.capture_fps);
                }
                else
                {
                    if (frameLimitingEnabled)
                    {
                        timeEndPeriod(1);
                        frameLimitingEnabled = false;
                    }
                    frame_duration.reset();
                }
                capture_fps_changed.store(false);
            }

            if (detection_resolution_changed.load() ||
                capture_method_changed.load() ||
                capture_cursor_changed.load() ||
                capture_borders_changed.load())
            {
                // Reset zero-copy state on re-initialization
                zeroCopyInitialized = false;
                zeroCopyActive = false;
                
                delete capturer;
                capturer = nullptr;

                int newWidth = config.detection_resolution;
                int newHeight = config.detection_resolution;

                if (config.capture_method == "duplication_api")
                {
                    capturer = new DuplicationAPIScreenCapture(newWidth, newHeight);
                    if (config.verbose)
                        std::cout << "[Capture] Re-init with Duplication API." << std::endl;
                    
                    // Re-initialize zero-copy if enabled
                    if (config.gpu_zero_copy && capturer->SupportsGpuCapture())
                    {
                        auto* ddaCapturer = dynamic_cast<DuplicationAPIScreenCapture*>(capturer);
                        if (ddaCapturer)
                        {
                            ID3D11Device* d3dDevice = ddaCapturer->GetD3D11Device();
                            ID3D11DeviceContext* d3dContext = ddaCapturer->GetD3D11Context();
                            
                            if (d3dDevice && d3dContext)
                            {
#ifdef USE_CUDA
                                if (config.backend == "TRT")
                                {
                                    if (trt_detector.initializeCudaInterop(d3dDevice))
                                    {
                                        zeroCopyInitialized = true;
                                        if (config.verbose)
                                            std::cout << "[Capture] CUDA-D3D11 interop re-initialized" << std::endl;
                                    }
                                }
#endif
                                if (config.backend == "DML" && dml_detector)
                                {
                                    if (dml_detector->initializeStagingManager(d3dDevice, d3dContext, newWidth, newHeight))
                                    {
                                        zeroCopyInitialized = true;
                                        if (config.verbose)
                                            std::cout << "[Capture] D3D11 staging manager re-initialized" << std::endl;
                                    }
                                }
                            }
                        }
                    }
                }
                else if (config.capture_method == "winrt")
                {
                    capturer = new WinRTScreenCapture(newWidth, newHeight);
                    if (config.verbose)
                        std::cout << "[Capture] Re-init with WinRT." << std::endl;
                }
                else if (config.capture_method == "virtual_camera")
                {
                    {
                        std::lock_guard<std::mutex> lock(capturerMutex);
                        capturer = new VirtualCameraCapture(config.virtual_camera_width, config.virtual_camera_heigth);
                    }
                    if (config.verbose)
                        std::cout << "[Capture] Re-init with Virtual Camera." << std::endl;
                }
                else if (config.capture_method == "gstreamer")
                {
                    capturer = new GStreamerCapture(config.gstreamer_pipeline, newWidth, newHeight);
                    if (config.verbose)
                        std::cout << "[Capture] Re-init with GStreamer (network)." << std::endl;
                }
                else
                {
                    config.capture_method = "duplication_api";
                    config.saveConfig();
                    capturer = new DuplicationAPIScreenCapture(newWidth, newHeight);
                    std::cout << "[Capture] Unknown capture_method. Set to duplication_api." << std::endl;
                }

                detection_resolution_changed.store(false);
                capture_method_changed.store(false);
                capture_cursor_changed.store(false);
                capture_borders_changed.store(false);
            }

            // Determine if zero-copy path should be used
            bool useZeroCopy = config.gpu_zero_copy && 
                               zeroCopyInitialized && 
                               config.capture_method == "duplication_api" &&
                               capturer->SupportsGpuCapture();
            
            cv::Mat screenshotCpu;
            bool frameProcessedViaZeroCopy = false;
            
            // Performance timing variables
            std::chrono::steady_clock::time_point t_capture_start, t_capture_end;
            std::chrono::steady_clock::time_point t_interop_start, t_interop_end;
            double captureTimeMs = 0.0;
            double interopTimeMs = 0.0;
            
            // Try zero-copy path first if enabled
            if (useZeroCopy)
            {
                t_capture_start = std::chrono::steady_clock::now();
                
                GpuFrame gpuFrame;
                {
                    std::lock_guard<std::mutex> lock(capturerMutex);
                    gpuFrame = capturer->GetNextFrameGpu();
                }
                
                t_capture_end = std::chrono::steady_clock::now();
                captureTimeMs = std::chrono::duration<double, std::milli>(t_capture_end - t_capture_start).count();
                
                if (gpuFrame.valid && gpuFrame.texture)
                {
                    t_interop_start = std::chrono::steady_clock::now();
                    
#ifdef USE_CUDA
                    if (config.backend == "TRT" && trt_detector.cudaInterop && trt_detector.cudaInterop->IsInitialized())
                    {
                        cv::cuda::GpuMat gpuMat;
                        if (trt_detector.cudaInterop->MapTextureToGpuMat(gpuFrame.texture, gpuMat))
                        {
                            t_interop_end = std::chrono::steady_clock::now();
                            interopTimeMs = std::chrono::duration<double, std::milli>(t_interop_end - t_interop_start).count();
                            
                            // Process frame via GPU path
                            trt_detector.processFrameGpu(gpuMat);
                            trt_detector.cudaInterop->Unmap();
                            frameProcessedViaZeroCopy = true;
                            zeroCopyActive = true;
                            
                            // Still need CPU frame for latestFrame/frameQueue (for overlay display)
                            // Convert GpuMat to Mat
                            gpuMat.download(screenshotCpu);
                            
                            if (config.verbose)
                            {
                                std::cout << "[Capture] Zero-copy TRT: capture=" << captureTimeMs 
                                          << "ms, interop=" << interopTimeMs << "ms" << std::endl;
                            }
                        }
                        else
                        {
                            if (config.verbose)
                                std::cerr << "[Capture] CUDA interop map failed, falling back to CPU path" << std::endl;
                        }
                    }
#endif
                    if (config.backend == "DML" && dml_detector && !frameProcessedViaZeroCopy)
                    {
                        // Process frame via DML GPU path
                        dml_detector->processFrameFromGpu(gpuFrame.texture);
                        
                        t_interop_end = std::chrono::steady_clock::now();
                        interopTimeMs = std::chrono::duration<double, std::milli>(t_interop_end - t_interop_start).count();
                        
                        frameProcessedViaZeroCopy = true;
                        zeroCopyActive = true;
                        
                        // Get CPU frame for latestFrame/frameQueue
                        auto* ddaCapturer = dynamic_cast<DuplicationAPIScreenCapture*>(capturer);
                        if (ddaCapturer)
                        {
                            // Use staging manager to get CPU copy
                            D3D11StagingManager tempStaging;
                            if (tempStaging.Initialize(ddaCapturer->GetD3D11Device(), 
                                                       ddaCapturer->GetD3D11Context(),
                                                       gpuFrame.width, gpuFrame.height))
                            {
                                tempStaging.CopyToMat(gpuFrame.texture, screenshotCpu);
                            }
                        }
                        
                        if (config.verbose)
                        {
                            std::cout << "[Capture] Zero-copy DML: capture=" << captureTimeMs 
                                      << "ms, staging=" << interopTimeMs << "ms" << std::endl;
                        }
                    }
                    
                    // Release GPU frame
                    {
                        std::lock_guard<std::mutex> lock(capturerMutex);
                        capturer->ReleaseGpuFrame();
                    }
                }
                else
                {
                    zeroCopyActive = false;
                }
            }
            
            // Update global zero-copy status for UI display
            zeroCopyActiveStatus.store(zeroCopyActive);
            
            // Fallback to CPU path if zero-copy not used or failed
            if (!frameProcessedViaZeroCopy)
            {
                {
                    std::lock_guard<std::mutex> lock(capturerMutex);
                    screenshotCpu = capturer->GetNextFrameCpu();
                }
                zeroCopyActive = false;
                zeroCopyActiveStatus.store(false);
            }

            if (screenshotCpu.empty())
                continue;

            if (config.capture_method == "virtual_camera")
            {
                int x = (screenshotCpu.cols - CAPTURE_WIDTH) / 2;
                int y = (screenshotCpu.rows - CAPTURE_HEIGHT) / 2;
                x = std::max(x, 0);
                y = std::max(y, 0);
                screenshotCpu = screenshotCpu(cv::Rect(x, y, CAPTURE_WIDTH, CAPTURE_HEIGHT)).clone();
            }

            if (config.circle_mask)
                screenshotCpu = apply_circle_mask(screenshotCpu);

            {
                std::lock_guard<std::mutex> lock(frameMutex);
                latestFrame = screenshotCpu.clone();
                if (frameQueue.size() >= 1)
                    frameQueue.pop_front();
                frameQueue.push_back(latestFrame);
            }
            frameCV.notify_one();

            // Process frame via CPU path if not already processed via zero-copy
            if (!frameProcessedViaZeroCopy)
            {
                if (config.backend == "DML" && dml_detector)
                {
                    dml_detector->processFrame(screenshotCpu);
                }
#ifdef USE_CUDA
                else if (config.backend == "TRT")
                {
                    trt_detector.processFrame(screenshotCpu);
                }
#endif
            }

            if (!config.screenshot_button.empty() && config.screenshot_button[0] != "None")
            {
                bool buttonPressed = isAnyKeyPressed(config.screenshot_button);
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSaveTime).count();
                if (buttonPressed && elapsed >= config.screenshot_delay)
                {
                    cv::Mat saveMat;
                    {
                        std::lock_guard<std::mutex> lock(frameMutex);
                        saveMat = latestFrame.clone();
                    }

                    auto epoch_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()
                    ).count();
                    std::string filename = std::to_string(epoch_time) + ".jpg";
                    cv::imwrite("screenshots/" + filename, saveMat);

                    lastSaveTime = now;
                }
            }

            captureFrameCount++;
            auto currentTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsedTime = currentTime - captureFpsStartTime;
            if (elapsedTime.count() >= 1.0)
            {
                captureFps = static_cast<int>(captureFrameCount / elapsedTime.count());
                captureFrameCount = 0;
                captureFpsStartTime = currentTime;
            }

            if (frame_duration.has_value())
            {
                auto end_time = std::chrono::high_resolution_clock::now();
                auto work_duration = end_time - start_time;
                auto sleep_duration = frame_duration.value() - work_duration;

                if (sleep_duration > std::chrono::duration<double, std::milli>(0))
                    std::this_thread::sleep_for(sleep_duration);
                start_time = std::chrono::high_resolution_clock::now();
            }
        }

        if (frameLimitingEnabled)
            timeEndPeriod(1);

        if (capturer)
        {
            std::lock_guard<std::mutex> lock(capturerMutex);
            delete capturer;
            capturer = nullptr;
        }

        if (config.capture_method == "winrt")
            winrt::uninit_apartment();
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Capture] Unhandled exception: " << e.what() << std::endl;
    }
}
