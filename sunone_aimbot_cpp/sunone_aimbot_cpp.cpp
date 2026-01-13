#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <winsock2.h>
#include <Windows.h>
#include <shellapi.h>

#include <iostream>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <array>
#include <random>
#include <filesystem>
#include <cmath>
#include <locale>

#include "capture.h"
#include "mouse.h"
#include "sunone_aimbot_cpp.h"
#include "keyboard_listener.h"
#include "overlay.h"
#include "Game_overlay.h"
#include "ghub.h"
#include "other_tools.h"
#include "virtual_camera.h"
#include "mem/gpu_resource_manager.h"
#include "mem/cpu_affinity_manager.h"
#include "ByteTracker.h"
#include "dataset/dataset_collector.h"
#include "dataset/webdav_uploader.h"
#include "auth/tg_auth.h"

#include <wincodec.h>
#include <wrl/client.h>
#include <comdef.h>

#pragma comment(lib, "windowscodecs.lib")

using Microsoft::WRL::ComPtr;

std::condition_variable frameCV;
std::atomic<bool> shouldExit(false);
std::atomic<bool> aiming(false);
std::atomic<bool> detectionPaused(false);
std::atomic<bool> norecoilActive(false);
std::mutex configMutex;

#ifdef USE_CUDA
TrtDetector trt_detector;
#endif

DirectMLDetector* dml_detector = nullptr;
MouseThread* globalMouseThread = nullptr;
Config config;

// ByteTracker instance
ByteTracker* byteTracker = nullptr;
std::mutex trackerMutex;

// DatasetCollector instance
DatasetCollector* datasetCollector = nullptr;

Game_overlay* gameOverlayPtr = nullptr;
std::thread gameOverlayThread;
std::atomic<bool> gameOverlayShouldExit(false);

GhubMouse* gHub = nullptr;
SerialConnection* arduinoSerial = nullptr;
Kmbox_b_Connection* kmboxSerial = nullptr;
KmboxNetConnection* kmboxNetSerial = nullptr;
MakcuConnection* makcuSerial = nullptr;

std::atomic<bool> detection_resolution_changed(false);
std::atomic<bool> capture_method_changed(false);
std::atomic<bool> capture_cursor_changed(false);
std::atomic<bool> capture_borders_changed(false);
std::atomic<bool> capture_fps_changed(false);
std::atomic<bool> capture_window_changed(false);
std::atomic<bool> detector_model_changed(false);
std::atomic<bool> show_window_changed(false);
std::atomic<bool> input_method_changed(false);

std::atomic<bool> zooming(false);
std::atomic<bool> shooting(false);

static std::string g_lastIconPath;
static int g_iconImageId = 0;
static std::mutex g_iconMutex;

std::string g_iconLastError;

static std::string hr_to_string(HRESULT hr)
{
    _com_error err(hr);
    const wchar_t* ws = err.ErrorMessage();
    int len = WideCharToMultiByte(CP_UTF8, 0, ws, -1, nullptr, 0, nullptr, nullptr);
    std::string s(len > 0 ? len - 1 : 0, '\0');
    if (len > 0) WideCharToMultiByte(CP_UTF8, 0, ws, -1, s.data(), len, nullptr, nullptr);
    return s;
}

static bool IsValidImageFile(const std::wstring& wpath, UINT& outW, UINT& outH, std::string& outErr)
{
    outW = outH = 0;
    outErr.clear();

    static std::once_flag coinit_flag;
    static HRESULT coinit_hr = S_OK;
    std::call_once(coinit_flag, [] {
        coinit_hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        });

    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) { outErr = "WIC factory error: " + hr_to_string(hr); return false; }

    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(wpath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) { outErr = "DecoderFromFilename failed: " + hr_to_string(hr); return false; }

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) { outErr = "GetFrame(0) failed: " + hr_to_string(hr); return false; }

    UINT w = 0, h = 0;
    hr = frame->GetSize(&w, &h);
    if (FAILED(hr)) { outErr = "GetSize failed: " + hr_to_string(hr); return false; }

    const UINT MAX_DIM = 16384;
    if (w == 0 || h == 0 || w > MAX_DIM || h > MAX_DIM)
    {
        outErr = "Invalid image size: " + std::to_string(w) + "x" + std::to_string(h);
        return false;
    }

    ComPtr<IWICFormatConverter> conv;
    hr = factory->CreateFormatConverter(&conv);
    if (FAILED(hr)) { outErr = "CreateFormatConverter failed: " + hr_to_string(hr); return false; }

    hr = conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) { outErr = "Converter Initialize failed: " + hr_to_string(hr); return false; }

    const UINT probe_rows = (std::min)(h, 8u);
    std::vector<uint8_t> probe;
    probe.resize((size_t)w * probe_rows * 4);
    WICRect rect{ 0, 0, (INT)w, (INT)probe_rows };
    hr = conv->CopyPixels(&rect, (UINT)(w * 4), (UINT)probe.size(), probe.data());
    if (FAILED(hr)) { outErr = "CopyPixels failed: " + hr_to_string(hr); return false; }

    outW = w; outH = h;
    return true;
}

inline void SetRandomConsoleTitle()
{
    static constexpr std::array<const wchar_t*, 10> kTitles = {
        L"Microsoft Edge",
        L"Google Chrome",
        L"Notepad",
        L"Windows Terminal",
        L"PowerShell",
        L"Visual Studio Code",
        L"Task Manager",
        L"File Explorer",
        L"Calculator",
        L"Command Prompt",
    };

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dist(0, kTitles.size() - 1);

    ::SetConsoleTitleW(kTitles[dist(gen)]);
}

void createInputDevices()
{
    if (arduinoSerial)
    {
        delete arduinoSerial;
        arduinoSerial = nullptr;
    }

    if (gHub)
    {
        gHub->mouse_close();
        delete gHub;
        gHub = nullptr;
    }

    if (kmboxSerial)
    {
        delete kmboxSerial;
        kmboxSerial = nullptr;
    }

    if (kmboxNetSerial)
    {
        delete kmboxNetSerial;
        kmboxNetSerial = nullptr;
    }

    if (makcuSerial)
    {
        delete makcuSerial;
        makcuSerial = nullptr;
    }

    if (config.input_method == "ARDUINO")
    {
        std::cout << "[鼠标] 使用 Arduino 输入方式" << std::endl;
        arduinoSerial = new SerialConnection(config.arduino_port, config.arduino_baudrate);
    }
    else if (config.input_method == "GHUB")
    {
        std::cout << "[鼠标] 使用 Ghub 输入方式" << std::endl;
        gHub = new GhubMouse();
        if (!gHub->mouse_xy(0, 0))
        {
            std::cerr << "[Ghub] 打开鼠标失败" << std::endl;
            delete gHub;
            gHub = nullptr;
        }
    }
    else if (config.input_method == "KMBOX_B")
    {
        std::cout << "[鼠标] 使用 KMBOX_B 输入方式" << std::endl;
        kmboxSerial = new Kmbox_b_Connection(config.kmbox_b_port, config.kmbox_b_baudrate);
        if (!kmboxSerial->isOpen())
        {
            std::cerr << "[Kmbox] 连接 Kmbox 串口失败" << std::endl;
            delete kmboxSerial;
            kmboxSerial = nullptr;
        }
    }
    else if (config.input_method == "KMBOX_NET")
    {
        std::cout << "[鼠标] 使用 KMBOX_NET 输入方式" << std::endl;
        kmboxNetSerial = new KmboxNetConnection(config.kmbox_net_ip, config.kmbox_net_port, config.kmbox_net_uuid);
        if (!kmboxNetSerial->isOpen())
        {
            std::cerr << "[KmboxNet] 连接失败" << std::endl;
            delete kmboxNetSerial; kmboxNetSerial = nullptr;
        }
    }
    else if (config.input_method == "MAKCU")
    {
        std::cout << "[鼠标] 使用 MAKCU 输入方式" << std::endl;
        makcuSerial = new MakcuConnection(config.makcu_port, config.makcu_baudrate);
        if (!makcuSerial->isOpen())
        {
            std::cerr << "[Makcu] 连接失败" << std::endl;
            delete makcuSerial;
            makcuSerial = nullptr;
        }
    }
    else
    {
        std::cout << "[鼠标] 使用默认 Win32 输入方式" << std::endl;
    }
}

void assignInputDevices()
{
    if (globalMouseThread)
    {
        globalMouseThread->setSerialConnection(arduinoSerial);
        globalMouseThread->setGHubMouse(gHub);
        globalMouseThread->setKmboxConnection(kmboxSerial);
        globalMouseThread->setKmboxNetConnection(kmboxNetSerial);
        globalMouseThread->setMakcuConnection(makcuSerial);
    }
}

void handleEasyNoRecoil(MouseThread& mouseThread)
{
    // 压枪触发条件：easynorecoil 开启 + norecoilActive（右键+左键 或 单独压枪键）
    if (config.easynorecoil && norecoilActive.load())
    {
        int recoil_compensation = static_cast<int>(config.easynorecoilstrength);
        
        // Use queueMove to ensure recoil compensation is tracked in accumulated movement
        // This prevents velocity calculation errors in moveMousePivot
        std::lock_guard<std::mutex> lock(mouseThread.input_method_mutex);
        
        if (arduinoSerial)
        {
            arduinoSerial->move(0, recoil_compensation);
        }
        else if (gHub)
        {
            gHub->mouse_xy(0, recoil_compensation);
        }
        else if (kmboxSerial)
        {
            kmboxSerial->move(0, recoil_compensation);
        }
        else if (kmboxNetSerial)
        {
            kmboxNetSerial->move(0, recoil_compensation);
        }
        else if (makcuSerial)
        {
            makcuSerial->move(0, recoil_compensation);
        }
        else
        {
            INPUT input = { 0 };
            input.type = INPUT_MOUSE;
            input.mi.dx = 0;
            input.mi.dy = recoil_compensation;
            input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_VIRTUALDESK;
            SendInput(1, &input, sizeof(INPUT));
        }
        
        // Track recoil movement in accumulated mouse movement
        // so velocity calculation in moveMousePivot is correct
        mouseThread.addAccumulatedMovement(0, recoil_compensation);
    }
}

void mouseThreadFunction(MouseThread& mouseThread)
{
    int lastVersion = -1;

    while (!shouldExit)
    {
        std::vector<cv::Rect> boxes;
        std::vector<int> classes;
        std::vector<float> scores;

        {
            std::unique_lock<std::mutex> lock(detectionBuffer.mutex);
            detectionBuffer.cv.wait(lock, [&] {
                return detectionBuffer.version > lastVersion || shouldExit;
                }
            );

            if (shouldExit) break;
            boxes = detectionBuffer.boxes;
            classes = detectionBuffer.classes;
            scores = detectionBuffer.scores;
            lastVersion = detectionBuffer.version;
        }

        // Dataset collection: feed frame and detections to collector
        if (datasetCollector && datasetCollector->IsEnabled() && aiming.load())
        {
            // Update hotkey state
            datasetCollector->OnHotkeyPressed(true);
            
            // Only clone image if rate limit allows (expensive operation)
            if (datasetCollector->CanSaveNow() && !boxes.empty())
            {
                // Build detections first (cheap operation)
                std::vector<Detection> detections;
                detections.reserve(boxes.size());
                for (size_t i = 0; i < boxes.size(); i++)
                {
                    Detection det;
                    det.box = boxes[i];
                    det.classId = classes[i];
                    det.confidence = (i < scores.size()) ? scores[i] : 0.0f;
                    detections.push_back(det);
                }
                
                // Get frame only when needed (clone is expensive)
                cv::Mat currentFrame;
                {
                    std::lock_guard<std::mutex> lock(detectionBuffer.mutex);
                    if (!detectionBuffer.sourceFrame.empty())
                    {
                        currentFrame = detectionBuffer.sourceFrame.clone();
                        detectionBuffer.sourceFrame.release();  // Free memory after clone
                    }
                }
                
                if (!currentFrame.empty())
                {
                    datasetCollector->OnNewFrame(currentFrame, detections);
                }
            }
            else
            {
                // Not saving this frame, but still need to release sourceFrame to prevent memory buildup
                std::lock_guard<std::mutex> lock(detectionBuffer.mutex);
                detectionBuffer.sourceFrame.release();
            }
        }
        else if (datasetCollector)
        {
            datasetCollector->OnHotkeyPressed(false);
            // Release sourceFrame when not collecting
            std::lock_guard<std::mutex> lock(detectionBuffer.mutex);
            detectionBuffer.sourceFrame.release();
        }
        else
        {
            // No dataset collector, still release sourceFrame if it exists
            std::lock_guard<std::mutex> lock(detectionBuffer.mutex);
            if (!detectionBuffer.sourceFrame.empty())
                detectionBuffer.sourceFrame.release();
        }

        if (input_method_changed.load())
        {
            createInputDevices();
            assignInputDevices();
            input_method_changed.store(false);
        }

        if (detection_resolution_changed.load())
        {
            {
                std::lock_guard<std::mutex> cfgLock(configMutex);
                mouseThread.updateConfig(
                    config.detection_resolution,
                    config.fovX,
                    config.fovY,
                    config.minSpeedMultiplier,
                    config.maxSpeedMultiplier,
                    config.predictionInterval,
                    config.auto_shoot,
                    config.bScope_multiplier
                );
            }
            
            // Reset ByteTracker on resolution change
            if (byteTracker)
            {
                std::lock_guard<std::mutex> lock(trackerMutex);
                byteTracker->reset();
            }
            
            detection_resolution_changed.store(false);
        }

        // Apply ByteTrack if enabled
        std::vector<cv::Rect> trackedBoxes;
        std::vector<int> trackedClasses;
        std::vector<int> trackedIds;
        std::vector<std::pair<float, float>> trackedVelocities;
        
        if (config.enable_bytetrack && byteTracker && !boxes.empty())
        {
            std::vector<TrackerDetection> detections;
            for (size_t i = 0; i < boxes.size(); i++)
            {
                TrackerDetection det;
                det.rect = boxes[i];
                det.classId = classes[i];
                det.score = (i < scores.size()) ? scores[i] : config.confidence_threshold;
                detections.push_back(det);
            }
            
            std::vector<TrackedObject> tracked;
            {
                std::lock_guard<std::mutex> lock(trackerMutex);
                tracked = byteTracker->update(detections);
            }
            
            if (!tracked.empty())
            {
                for (const auto& obj : tracked)
                {
                    // Use predicted position for smoother tracking
                    trackedBoxes.push_back(obj.predictedRect);
                    trackedClasses.push_back(obj.classId);
                    trackedIds.push_back(obj.trackId);
                    trackedVelocities.push_back({obj.velocityX, obj.velocityY});
                }
            }
            else
            {
                // ByteTrack returned no results, fallback to raw detections
                trackedBoxes = boxes;
                trackedClasses = classes;
                // No trackIds available - target lock won't work without ByteTrack
                trackedIds.clear();
                trackedVelocities.clear();
            }
        }
        else
        {
            trackedBoxes = boxes;
            trackedClasses = classes;
            // No trackIds available - target lock won't work without ByteTrack
            trackedIds.clear();
            trackedVelocities.clear();
        }

        // Target lock state
        static int lockedTrackId = -1;
        static int lockedBodyTrackId = -1;  // Remember associated body when locked to head
        static int lockLostFrames = 0;
        static bool wasAiming = false;
        
        // Last known target position for brief disappearance handling
        static double lastPivotX = 0.0;
        static double lastPivotY = 0.0;
        static double lastVelX = 0.0;
        static double lastVelY = 0.0;
        static bool hasLastPosition = false;
        
        bool isAiming = aiming.load() || (config.auto_shoot && shooting.load());
        bool aimJustPressed = isAiming && !wasAiming;
        
        // Reset lock when aim key is pressed (re-acquire target)
        if (aimJustPressed && config.enable_target_lock)
        {
            lockedTrackId = -1;
            lockedBodyTrackId = -1;
            lockLostFrames = 0;
        }
        
        int currentLockedId = (config.enable_target_lock && config.enable_bytetrack && !trackedIds.empty()) ? lockedTrackId : -1;
        
        AimbotTarget* target = sortTargets(
            trackedBoxes,
            trackedClasses,
            trackedIds,
            config.detection_resolution,
            config.detection_resolution,
            config.disable_headshot,
            currentLockedId,
            config.target_lock_threshold,
            lockedBodyTrackId,
            trackedVelocities.empty() ? nullptr : &trackedVelocities
        );

        if (target)
        {
            // Save last known position
            lastPivotX = target->pivotX;
            lastPivotY = target->pivotY;
            lastVelX = target->velocityX;
            lastVelY = target->velocityY;
            hasLastPosition = true;
            
            // Update locked target
            if (config.enable_target_lock && config.enable_bytetrack && isAiming)
            {
                if (lockedTrackId < 0)
                {
                    // No locked target, lock to current
                    lockedTrackId = target->trackId;
                    lockedBodyTrackId = target->associatedBodyTrackId;
                    lockLostFrames = 0;
                }
                else if (target->trackId == lockedTrackId)
                {
                    // Still tracking locked target
                    lockLostFrames = 0;
                    // Update body association if we have a better one now
                    if (target->associatedBodyTrackId >= 0 && lockedBodyTrackId < 0)
                    {
                        lockedBodyTrackId = target->associatedBodyTrackId;
                    }
                }
                else if (target->trackId >= 0)
                {
                    // sortTargets returned a different target (intentional switch, e.g., body->head)
                    // Update lock immediately without waiting
                    lockedTrackId = target->trackId;
                    // Remember associated body if switching to head
                    if (target->associatedBodyTrackId >= 0)
                    {
                        lockedBodyTrackId = target->associatedBodyTrackId;
                    }
                    lockLostFrames = 0;
                }
                else
                {
                    // Target has no valid trackId, count as lost
                    lockLostFrames++;
                    if (lockLostFrames > config.target_lock_hold_frames)
                    {
                        lockedTrackId = -1;
                        lockedBodyTrackId = -1;
                        lockLostFrames = 0;
                    }
                }
            }
            
            mouseThread.setLastTargetTime(std::chrono::steady_clock::now());
            mouseThread.setTargetDetected(true);

            auto futurePositions = mouseThread.predictFuturePositions(
                target->pivotX,
                target->pivotY,
                config.prediction_futurePositions
            );
            mouseThread.storeFuturePositions(futurePositions);
        }
        else
        {
            // No target detected this frame
            if (config.enable_target_lock && isAiming)
            {
                lockLostFrames++;
                if (lockLostFrames > config.target_lock_hold_frames)
                {
                    // Target lost for too long, reset lock
                    lockedTrackId = -1;
                    lockedBodyTrackId = -1;
                    lockLostFrames = 0;
                    hasLastPosition = false;
                    // Only clear positions when truly lost
                    mouseThread.clearFuturePositions();
                    mouseThread.setTargetDetected(false);
                }
                // else: Keep last position during brief disappearance (don't clear)
            }
            else
            {
                // Not aiming or lock disabled, clear immediately
                hasLastPosition = false;
                mouseThread.clearFuturePositions();
                mouseThread.setTargetDetected(false);
            }
        }
        
        // Reset lock when not aiming
        if (!isAiming && config.enable_target_lock)
        {
            lockedTrackId = -1;
            lockedBodyTrackId = -1;
            lockLostFrames = 0;
            hasLastPosition = false;
        }
        
        wasAiming = isAiming;

        // Auto shoot logic
        static std::chrono::steady_clock::time_point auto_shoot_last_fire;
        static bool auto_shoot_active = false;
        static int on_target_frames = 0;  // Count consecutive frames on target
        const int MIN_ON_TARGET_FRAMES = 2;  // Require 2 frames on target before firing

        if (isAiming)
        {
            if (target)
            {
                // Pass shooting state for crosshair offset compensation
                bool currentlyShooting = shooting.load();
                mouseThread.moveMousePivot(target->pivotX, target->pivotY, target->velocityX, target->velocityY, currentlyShooting);

                if (config.auto_shoot && shooting)
                {
                    // Use pivot point for auto-shoot detection
                    // For head targets, use larger detection area (head box is too small)
                    // For body targets, use the body box size
                    double detectW = target->w;
                    double detectH = target->h;
                    
                    // If target is head (small box), expand detection area
                    if (target->classId == config.class_head)
                    {
                        detectW = std::max(detectW * 2.0, 40.0);  // At least 40px wide
                        detectH = std::max(detectH * 2.0, 40.0);  // At least 40px tall
                    }
                    
                    // bScope_multiplier: 1.0 = target size, 1.5 = 1.5x size (easier)
                    bool on_target = mouseThread.check_pivot_in_scope(
                        target->pivotX, target->pivotY, detectW, detectH, config.bScope_multiplier);

                    if (on_target)
                    {
                        on_target_frames++;
                        
                        // Only fire after being on target for MIN_ON_TARGET_FRAMES (stability check)
                        if (on_target_frames >= MIN_ON_TARGET_FRAMES)
                        {
                            auto now = std::chrono::steady_clock::now();
                            if (!auto_shoot_active)
                            {
                                // First shot
                                mouseThread.pressMouseDirect();
                                auto_shoot_last_fire = now;
                                auto_shoot_active = true;
                            }
                            else
                            {
                                // Check if need to re-click (continuous fire)
                                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - auto_shoot_last_fire).count();
                                if (elapsed >= config.auto_shoot_interval)
                                {
                                    mouseThread.releaseMouse();
                                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                                    mouseThread.pressMouseDirect();
                                    auto_shoot_last_fire = now;
                                }
                            }
                        }
                    }
                    else
                    {
                        // Crosshair left target
                        on_target_frames = 0;
                        if (auto_shoot_active)
                        {
                            mouseThread.releaseMouse();
                            auto_shoot_active = false;
                        }
                    }
                }
                else
                {
                    // Shooting key released or auto_shoot disabled
                    on_target_frames = 0;
                    if (auto_shoot_active)
                    {
                        mouseThread.releaseMouse();
                        auto_shoot_active = false;
                    }
                }
            }
            else
            {
                // Target not detected this frame - stop moving
                on_target_frames = 0;
                
                if (config.auto_shoot && auto_shoot_active)
                {
                    mouseThread.releaseMouse();
                    auto_shoot_active = false;
                }
            }
        }
        else
        {
            on_target_frames = 0;
            if (config.auto_shoot && auto_shoot_active)
            {
                mouseThread.releaseMouse();
                auto_shoot_active = false;
            }
        }

        handleEasyNoRecoil(mouseThread);

        mouseThread.checkAndResetPredictions();

        delete target;
    }
}

static void gameOverlayRenderLoop()
{
    const int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);

    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    HMONITOR hPrimary = MonitorFromPoint(POINT{ 0,0 }, MONITOR_DEFAULTTOPRIMARY);
    GetMonitorInfo(hPrimary, &mi);
    RECT pr = mi.rcMonitor;
    const int pw = pr.right - pr.left;
    const int ph = pr.bottom - pr.top;

    const int offX = pr.left - vx;
    const int offY = pr.top - vy;

    while (!gameOverlayShouldExit.load())
    {
        if (!config.game_overlay_enabled)
        {
            if (gameOverlayPtr)
            {
                gameOverlayPtr->Stop();
                delete gameOverlayPtr;
                gameOverlayPtr = nullptr;
                std::lock_guard<std::mutex> lk(g_iconMutex);
                g_lastIconPath.clear();
                g_iconImageId = 0;
                g_iconLastError.clear();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            continue;
        }

        if (!gameOverlayPtr)
        {
            gameOverlayPtr = new Game_overlay();
            gameOverlayPtr->SetWindowBounds(0, 0, pw, ph);
            gameOverlayPtr->SetMaxFPS(config.game_overlay_max_fps > 0 ? (unsigned)config.game_overlay_max_fps : 0);
            gameOverlayPtr->Start();
        }
        else if (!gameOverlayPtr->IsRunning())
        {
            gameOverlayPtr->SetWindowBounds(0, 0, pw, ph);
            gameOverlayPtr->SetMaxFPS(config.game_overlay_max_fps > 0 ? (unsigned)config.game_overlay_max_fps : 0);
            gameOverlayPtr->Start();
        }

        if (!gameOverlayPtr || !gameOverlayPtr->IsRunning())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            continue;
        }

        gameOverlayPtr->SetMaxFPS(config.game_overlay_max_fps > 0 ? (unsigned)config.game_overlay_max_fps : 0);

        const int detRes = config.detection_resolution;
        if (detRes <= 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        int regionW = detRes;
        int regionH = detRes;

        if (regionW > pw) regionW = pw;
        if (regionH > ph) regionH = ph;

        const int baseX = (pw - regionW) / 2;
        const int baseY = (ph - regionH) / 2;

        const float scaleX = 1.0f;
        const float scaleY = 1.0f;

        std::vector<cv::Rect> boxesCopy;
        std::vector<int> classesCopy;
        {
            std::lock_guard<std::mutex> lk(detectionBuffer.mutex);
            boxesCopy = detectionBuffer.boxes;
            classesCopy = detectionBuffer.classes;
        }

        decltype(globalMouseThread->getFuturePositions()) futurePts;
        if (config.game_overlay_draw_future && globalMouseThread)
            futurePts = globalMouseThread->getFuturePositions();

        if (config.game_overlay_icon_enabled)
        {
            std::lock_guard<std::mutex> lk(g_iconMutex);
            if (config.game_overlay_icon_path != g_lastIconPath)
            {
                if (g_iconImageId != 0)
                {
                    gameOverlayPtr->UnloadImage(g_iconImageId);
                    g_iconImageId = 0;
                }
                g_lastIconPath = config.game_overlay_icon_path;
                std::error_code fsErr;
                std::filesystem::path p;
                try
                {
                    p = std::filesystem::u8path(g_lastIconPath);
                }
                catch (const std::exception&)
                {
                    p = std::filesystem::path(g_lastIconPath);
                }
                const bool hasFile = !g_lastIconPath.empty() && p.has_filename() && std::filesystem::is_regular_file(p, fsErr);
                if (fsErr)
                {
                    g_iconImageId = 0;
                    g_iconLastError = "[游戏覆盖层] 读取图标路径失败: " + g_lastIconPath + " (" + fsErr.message() + ")";
                    std::cerr << g_iconLastError << std::endl;
                }
                else if (hasFile)
                {
                    const std::wstring wpath = p.wstring();
                    g_iconLastError.clear();

                    UINT iw = 0, ih = 0;
                    std::string verr;
                    if (!IsValidImageFile(wpath, iw, ih, verr))
                    {
                        g_iconImageId = 0;
                        g_iconLastError = "[游戏覆盖层] 无效图片 '" + g_lastIconPath + "': " + verr;
                        std::cerr << g_iconLastError << std::endl;
                    }
                    else
                    {
                        try
                        {
                            int id = gameOverlayPtr->LoadImageFromFile(wpath);
                            if (id != 0)
                            {
                                g_iconImageId = id;
                                std::cout << "[游戏覆盖层] 已加载图标 (" << iw << "x" << ih << "): " << g_lastIconPath << std::endl;
                            }
                            else
                            {
                                g_iconImageId = 0;
                                g_iconLastError = "[游戏覆盖层] 加载图标失败 (返回值为0): " + g_lastIconPath;
                                std::cerr << g_iconLastError << std::endl;
                            }
                        }
                        catch (const std::exception& e)
                        {
                            g_iconImageId = 0;
                            g_iconLastError = std::string("[游戏覆盖层] 加载图标时发生异常: ") + e.what();
                            std::cerr << g_iconLastError << std::endl;
                        }
                        catch (...)
                        {
                            g_iconImageId = 0;
                            g_iconLastError = "[游戏覆盖层] 加载图标时发生未知异常";
                            std::cerr << g_iconLastError << std::endl;
                        }
                    }
                }
                else
                {
                    g_iconImageId = 0;
                    g_iconLastError = "[游戏覆盖层] 图标文件未找到: " + g_lastIconPath;
                    std::cerr << g_iconLastError << std::endl;
                }
            }
        }

        gameOverlayPtr->BeginFrame();

        // CAPTURE FRAME
        if (config.game_overlay_draw_frame)
        {
            int A = config.game_overlay_frame_a;
            int R = config.game_overlay_frame_r;
            int G = config.game_overlay_frame_g;
            int B = config.game_overlay_frame_b;
            auto clamp255 = [](int& v) { if (v < 0) v = 0; else if (v > 255) v = 255; };
            clamp255(A); clamp255(R); clamp255(G); clamp255(B);
            const uint32_t col =
                (uint32_t(A) << 24) |
                (uint32_t(R) << 16) |
                (uint32_t(G) << 8) |
                uint32_t(B);

            float thickness = config.game_overlay_frame_thickness;
            if (thickness <= 0.f) thickness = 1.f;

            if (config.circle_mask)
            {
                float cx = baseX + regionW * 0.5f;
                float cy = baseY + regionH * 0.5f;
                float radius = std::min(regionW, regionH) * 0.5f;
                gameOverlayPtr->AddCircle({ cx, cy, radius }, col, thickness);
            }
            else
            {
                gameOverlayPtr->AddRect(
                    { static_cast<float>(baseX),
                      static_cast<float>(baseY),
                      static_cast<float>(regionW),
                      static_cast<float>(regionH) },
                    col, thickness);
            }
        }

        // BOXES
        if (config.game_overlay_draw_boxes && !boxesCopy.empty())
        {
            int A = config.game_overlay_box_a;
            int R = config.game_overlay_box_r;
            int G = config.game_overlay_box_g;
            int B = config.game_overlay_box_b;
            auto clamp255 = [](int& v) { if (v < 0) v = 0; else if (v > 255) v = 255; };
            clamp255(A); clamp255(R); clamp255(G); clamp255(B);
            const uint32_t col =
                (uint32_t(A) << 24) |
                (uint32_t(R) << 16) |
                (uint32_t(G) << 8) |
                uint32_t(B);

            float thickness = config.game_overlay_box_thickness;
            if (thickness <= 0.f) thickness = 1.f;

            for (const auto& b : boxesCopy)
            {
                if (b.width <= 0 || b.height <= 0) continue;

                int bx = std::max(0, std::min(b.x, detRes));
                int by = std::max(0, std::min(b.y, detRes));
                int bw = std::max(0, std::min(b.width, detRes - bx));
                int bh = std::max(0, std::min(b.height, detRes - by));
                if (bw == 0 || bh == 0) continue;

                float x = baseX + bx * scaleX;
                float y = baseY + by * scaleY;
                float w = bw * scaleX;
                float h = bh * scaleY;

                if (x + w < baseX || y + h < baseY ||
                    x > baseX + regionW || y > baseY + regionH)
                    continue;

                gameOverlayPtr->AddRect({ x, y, w, h }, col, thickness);
            }
        }

        // FUTURE POINTS
        if (config.game_overlay_draw_future && !futurePts.empty())
        {
            const int total = static_cast<int>(futurePts.size());
            const int baseA = std::max(5, std::min(255, config.game_overlay_box_a));

            for (int i = 0; i < total; ++i)
            {
                float alphaFactor =
                    std::exp(-config.game_overlay_future_alpha_falloff *
                        (static_cast<float>(i) / static_cast<float>(total)));

                int a = static_cast<int>(baseA * alphaFactor);
                if (a < 12) a = 12;

                const uint32_t col =
                    (uint32_t(a) << 24) |
                    (uint32_t(255 - (i * 255 / total)) << 16) |
                    (uint32_t(50) << 8) |
                    (uint32_t(i * 255 / total));

                float px = static_cast<float>(baseX) + static_cast<float>(futurePts[i].first) * scaleX;
                float py = static_cast<float>(baseY) + static_cast<float>(futurePts[i].second) * scaleY;

                if (px < baseX - 40 || py < baseY - 40 ||
                    px > baseX + regionW + 40 || py > baseY + regionH + 40)
                    continue;

                gameOverlayPtr->FillCircle({ px, py, config.game_overlay_future_point_radius }, col);
            }
        }

        // ICONS
        if (config.game_overlay_icon_enabled && g_iconImageId != 0 && !boxesCopy.empty())
        {
            const int iconW = config.game_overlay_icon_width;
            const int iconH = config.game_overlay_icon_height;
            const float offXIcon = config.game_overlay_icon_offset_x;
            const float offYIcon = config.game_overlay_icon_offset_y;
            std::string anchor = config.game_overlay_icon_anchor;
            int i = 0;
            for (const auto& b : boxesCopy)
            {
                // temporary: only draw for players
                if (classesCopy[i] != 0)
                {
                    continue;
                }

                if (b.width <= 0 || b.height <= 0) continue;
                int bx = std::max(0, std::min(b.x, detRes));
                int by = std::max(0, std::min(b.y, detRes));
                int bw = std::max(0, std::min(b.width, detRes - bx));
                int bh = std::max(0, std::min(b.height, detRes - by));
                if (bw == 0 || bh == 0) continue;

                float boxX = baseX + bx * scaleX;
                float boxY = baseY + by * scaleY;
                float boxW = bw * scaleX;
                float boxH = bh * scaleY;

                float drawX = boxX;
                float drawY = boxY;

                if (anchor == "center")
                {
                    drawX = boxX + boxW / 2.0f - iconW / 2.0f;
                    drawY = boxY + boxH / 2.0f - iconH / 2.0f;
                }
                else if (anchor == "top" || anchor == "head")
                {
                    drawX = boxX + boxW / 2.0f - iconW / 2.0f;
                    drawY = boxY - iconH;
                }
                else if (anchor == "bottom")
                {
                    drawX = boxX + boxW / 2.0f - iconW / 2.0f;
                    drawY = boxY + boxH;
                }
                else
                {
                    drawX = boxX + boxW / 2.0f - iconW / 2.0f;
                    drawY = boxY + boxH / 2.0f - iconH / 2.0f;
                }

                drawX += offXIcon;
                drawY += offYIcon;

                gameOverlayPtr->DrawImage(g_iconImageId, drawX, drawY, (float)iconW, (float)iconH, 1.0f);
                i++;
            }
        }

        gameOverlayPtr->EndFrame();

        unsigned fpsCap = (unsigned)config.game_overlay_max_fps;
        if (boxesCopy.empty() && futurePts.empty())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
            continue;
        }
        if (fpsCap > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(1000 / fpsCap));
        else
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (gameOverlayPtr)
    {
        gameOverlayPtr->Stop();
        delete gameOverlayPtr;
        gameOverlayPtr = nullptr;
    }
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    setlocale(LC_ALL, ".UTF-8");
    SetRandomConsoleTitle();
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_FATAL);

    if (!config.loadConfig())
    {
        std::cerr << "[配置] 加载配置文件失败！" << std::endl;
        std::cin.get();
        return -1;
    }

    // ========== TGAuth 授权检查 ==========
    if (config.auth_enabled)
    {
        std::cout << "[授权] 正在初始化授权模块..." << std::endl;
        
        TGAuth::AuthConfig authConfig;
        // 硬编码配置，不暴露给用户
        authConfig.worker_api_url = "https://cowai.trueliu.com";
        authConfig.bot_username = "cowyanzhengbot";
        authConfig.token_storage_path = "auth_token.dat";
        authConfig.poll_interval_ms = 2000;
        authConfig.session_timeout_sec = 300;
        
        auto& authManager = TGAuth::AuthManager::getInstance();
        if (!authManager.init(authConfig))
        {
            std::cerr << "[授权] 初始化授权模块失败！" << std::endl;
            std::cin.get();
            return -1;
        }
        
        // 检查本地 Token
        std::cout << "[\xE6\x8E\x88\xE6\x9D\x83] \xE6\xAD\xA3\xE5\x9C\xA8\xE9\xAA\x8C\xE8\xAF\x81\xE6\x8E\x88\xE6\x9D\x83\xE7\x8A\xB6\xE6\x80\x81..." << std::endl;  // [授权] 正在验证授权状态...
        TGAuth::AuthResult result = authManager.checkAuthorization();
        
        if (result.status == TGAuth::AuthStatus::AUTHORIZED)
        {
            std::cout << "[\xE6\x8E\x88\xE6\x9D\x83] \xE5\xB7\xB2\xE6\x8E\x88\xE6\x9D\x83" << std::endl;  // [授权] 已授权
        }
        else if (result.status == TGAuth::AuthStatus::BANNED)
        {
            std::cerr << "[\xE6\x8E\x88\xE6\x9D\x83] \xE6\x82\xA8\xE7\x9A\x84\xE8\xB4\xA6\xE5\x8F\xB7\xE5\xB7\xB2\xE8\xA2\xAB\xE5\xB0\x81\xE7\xA6\x81\xEF\xBC\x81" << std::endl;  // [授权] 您的账号已被封禁！
            std::cin.get();
            return -1;
        }
        else if (result.status == TGAuth::AuthStatus::KEY_REVOKED)
        {
            std::cerr << "[\xE6\x8E\x88\xE6\x9D\x83] \xE5\x8D\xA1\xE5\xAF\x86\xE5\xB7\xB2\xE8\xA2\xAB\xE6\x92\xA4\xE9\x94\x80\xEF\xBC\x8C\xE8\xAF\xB7\xE9\x87\x8D\xE6\x96\xB0\xE8\xBE\x93\xE5\x85\xA5\xE5\x8D\xA1\xE5\xAF\x86" << std::endl;  // [授权] 卡密已被撤销，请重新输入卡密
        }
        else if (result.status == TGAuth::AuthStatus::KEY_EXPIRED)
        {
            std::cerr << "[\xE6\x8E\x88\xE6\x9D\x83] \xE5\x8D\xA1\xE5\xAF\x86\xE5\xB7\xB2\xE8\xBF\x87\xE6\x9C\x9F\xEF\xBC\x8C\xE8\xAF\xB7\xE9\x87\x8D\xE6\x96\xB0\xE8\xBE\x93\xE5\x85\xA5\xE5\x8D\xA1\xE5\xAF\x86" << std::endl;  // [授权] 卡密已过期，请重新输入卡密
        }
        else if (result.status == TGAuth::AuthStatus::DEVICE_LIMIT_EXCEEDED)
        {
            std::cerr << "[\xE6\x8E\x88\xE6\x9D\x83] \xE8\xAE\xBE\xE5\xA4\x87\xE6\x95\xB0\xE9\x87\x8F\xE8\xB6\x85\xE5\x87\xBA\xE9\x99\x90\xE5\x88\xB6\xEF\xBC\x81" << std::endl;  // [授权] 设备数量超出限制！
            std::cin.get();
            return -1;
        }
        
        // 需要登录或重新登录
        if (result.status != TGAuth::AuthStatus::AUTHORIZED)
        {
            // 卡密激活
            bool authSuccess = false;
            
            while (!authSuccess)
            {
                std::cout << "\n========================================" << std::endl;
                std::cout << "           \xE6\x8E\x88\xE6\x9D\x83\xE7\x99\xBB\xE5\xBD\x95" << std::endl;  // 授权登录
                std::cout << "========================================" << std::endl;
                std::cout << "\n1. \xE8\xBE\x93\xE5\x85\xA5\xE5\x8D\xA1\xE5\xAF\x86\xE6\xBF\x80\xE6\xB4\xBB" << std::endl;  // 1. 输入卡密激活
                std::cout << "2. \xE8\xB4\xAD\xE4\xB9\xB0\xE5\x8D\xA1\xE5\xAF\x86" << std::endl;  // 2. 购买卡密
                std::cout << "0. \xE9\x80\x80\xE5\x87\xBA\xE7\xA8\x8B\xE5\xBA\x8F" << std::endl;  // 0. 退出程序
                std::cout << "\n\xE8\xAF\xB7\xE9\x80\x89\xE6\x8B\xA9: ";  // 请选择:
                
                std::string input;
                std::getline(std::cin, input);
                
                // 去除首尾空格
                input.erase(0, input.find_first_not_of(" \t\r\n"));
                input.erase(input.find_last_not_of(" \t\r\n") + 1);
                
                if (input == "0")
                {
                    return 0;
                }
                else if (input == "2")
                {
                    // 购买卡密选项
                    std::cout << "\n[\xE6\x8E\x88\xE6\x9D\x83] \xE6\xAD\xA3\xE5\x9C\xA8\xE6\x89\x93\xE5\xBC\x80\xE8\xB4\xAD\xE4\xB9\xB0\xE9\xA1\xB5\xE9\x9D\xA2..." << std::endl;  // [授权] 正在打开购买页面...
                    
                    // 使用 ShellExecute 打开浏览器
                    HINSTANCE result = ShellExecuteA(NULL, "open", "https://cowai.trueliu.com/buy", NULL, NULL, SW_SHOWNORMAL);
                    
                    if ((INT_PTR)result > 32)
                    {
                        std::cout << "[\xE6\x8E\x88\xE6\x9D\x83] \xE5\xB7\xB2\xE5\x9C\xA8\xE6\xB5\x8F\xE8\xA7\x88\xE5\x99\xA8\xE4\xB8\xAD\xE6\x89\x93\xE5\xBC\x80\xE8\xB4\xAD\xE4\xB9\xB0\xE9\xA1\xB5\xE9\x9D\xA2" << std::endl;  // [授权] 已在浏览器中打开购买页面
                        std::cout << "[\xE6\x8E\x88\xE6\x9D\x83] \xE8\xB4\xAD\xE4\xB9\xB0\xE5\xAE\x8C\xE6\x88\x90\xE5\x90\x8E\xE8\xAF\xB7\xE9\x80\x89\xE6\x8B\xA9\xE9\x80\x89\xE9\xA1\xB9 1 \xE8\xBE\x93\xE5\x85\xA5\xE5\x8D\xA1\xE5\xAF\x86\xE6\xBF\x80\xE6\xB4\xBB\n" << std::endl;  // [授权] 购买完成后请选择选项 1 输入卡密激活
                    }
                    else
                    {
                        std::cerr << "[\xE6\x8E\x88\xE6\x9D\x83] \xE6\x89\x93\xE5\xBC\x80\xE6\xB5\x8F\xE8\xA7\x88\xE5\x99\xA8\xE5\xA4\xB1\xE8\xB4\xA5\xEF\xBC\x8C\xE8\xAF\xB7\xE6\x89\x8B\xE5\x8A\xA8\xE8\xAE\xBF\xE9\x97\xAE: https://cowai.trueliu.com/buy" << std::endl;  // [授权] 打开浏览器失败，请手动访问: https://cowai.trueliu.com/buy
                    }
                    continue;
                }
                else if (input == "1")
                {
                    // 输入卡密
                    std::cout << "\n\xE8\xAF\xB7\xE8\xBE\x93\xE5\x85\xA5\xE5\x8D\xA1\xE5\xAF\x86: ";  // 请输入卡密:
                    
                    std::string keyCode;
                    std::getline(std::cin, keyCode);
                    
                    // 去除首尾空格
                    keyCode.erase(0, keyCode.find_first_not_of(" \t\r\n"));
                    keyCode.erase(keyCode.find_last_not_of(" \t\r\n") + 1);
                    
                    if (keyCode.empty())
                    {
                        std::cerr << "[\xE6\x8E\x88\xE6\x9D\x83] \xE5\x8D\xA1\xE5\xAF\x86\xE4\xB8\x8D\xE8\x83\xBD\xE4\xB8\xBA\xE7\xA9\xBA\xEF\xBC\x81" << std::endl;  // [授权] 卡密不能为空！
                        continue;
                    }
                    
                    std::cout << "[\xE6\x8E\x88\xE6\x9D\x83] \xE6\xAD\xA3\xE5\x9C\xA8\xE9\xAA\x8C\xE8\xAF\x81\xE5\x8D\xA1\xE5\xAF\x86..." << std::endl;  // [授权] 正在验证卡密...
                    
                    TGAuth::AuthResult keyResult = authManager.activateKey(keyCode);
                    
                    if (keyResult.status == TGAuth::AuthStatus::AUTHORIZED)
                    {
                        std::cout << "[\xE6\x8E\x88\xE6\x9D\x83] \xE5\x8D\xA1\xE5\xAF\x86\xE6\xBF\x80\xE6\xB4\xBB\xE6\x88\x90\xE5\x8A\x9F\xEF\xBC\x81" << std::endl;  // [授权] 卡密激活成功！
                        if (keyResult.key_expires_at > 0)
                        {
                            time_t expTime = keyResult.key_expires_at;
                            std::cout << "[\xE6\x8E\x88\xE6\x9D\x83] \xE5\x88\xB0\xE6\x9C\x9F\xE6\x97\xB6\xE9\x97\xB4: " << std::ctime(&expTime);  // [授权] 到期时间:
                        }
                        else
                        {
                            std::cout << "[\xE6\x8E\x88\xE6\x9D\x83] \xE6\xB0\xB8\xE4\xB9\x85\xE6\x9C\x89\xE6\x95\x88" << std::endl;  // [授权] 永久有效
                        }
                        authSuccess = true;
                    }
                    else
                    {
                        std::cerr << "[\xE6\x8E\x88\xE6\x9D\x83] \xE5\x8D\xA1\xE5\xAF\x86\xE6\xBF\x80\xE6\xB4\xBB\xE5\xA4\xB1\xE8\xB4\xA5: " << keyResult.error_message << std::endl;  // [授权] 卡密激活失败:
                    }
                }
                else
                {
                    std::cerr << "[\xE6\x8E\x88\xE6\x9D\x83] \xE6\x97\xA0\xE6\x95\x88\xE7\x9A\x84\xE9\x80\x89\xE9\xA1\xB9\xEF\xBC\x8C\xE8\xAF\xB7\xE8\xBE\x93\xE5\x85\xA5 0, 1 \xE6\x88\x96 2" << std::endl;  // [授权] 无效的选项，请输入 0, 1 或 2
                }
            }  // end while (!authSuccess)
        }
        
        std::cout << "\n[\xE6\x8E\x88\xE6\x9D\x83] \xE9\xAA\x8C\xE8\xAF\x81\xE9\x80\x9A\xE8\xBF\x87\xEF\xBC\x8C\xE5\x90\xAF\xE5\x8A\xA8\xE7\xA8\x8B\xE5\xBA\x8F...\n" << std::endl;  // [授权] 验证通过，启动程序...
        
        // 检查 Token 是否即将过期
        if (authManager.isTokenExpiringSoon(24))
        {
            std::cout << "[\xE6\x8E\x88\xE6\x9D\x83] \xE6\x8F\x90\xE7\xA4\xBA: \xE6\x82\xA8\xE7\x9A\x84\xE6\x8E\x88\xE6\x9D\x83\xE5\xB0\x86\xE5\x9C\xA8 24 \xE5\xB0\x8F\xE6\x97\xB6\xE5\x86\x85\xE8\xBF\x87\xE6\x9C\x9F" << std::endl;  // [授权] 提示: 您的授权将在 24 小时内过期
        }
    }
    // ========== TGAuth 授权检查结束 ==========

    CPUAffinityManager cpuManager;

    if (config.cpuCoreReserveCount > 0)
    {
        if (!cpuManager.reserveCPUCores(config.cpuCoreReserveCount)) return -1;
    }

    if (config.systemMemoryReserveMB > 0)
    {
        if (!cpuManager.reserveSystemMemory(config.systemMemoryReserveMB)) return -1;
    }

    try
    {
#ifdef USE_CUDA
        int cuda_runtime_version = 0;
        cudaError_t runtime_status = cudaRuntimeGetVersion(&cuda_runtime_version);

        if (runtime_status != cudaSuccess)
        {
            std::cerr << "[主程序] CUDA 运行时检查失败: " << cudaGetErrorString(runtime_status) << std::endl;
            std::cin.get();
            return -1;
        }

        if (config.verbose)
            std::cout << "[CUDA] 版本: " << cuda_runtime_version << std::endl;

        const int required_cuda_version = 13010;
        if (cuda_runtime_version < required_cuda_version)
        {
            int required_major = required_cuda_version / 1000;
            int required_minor = (required_cuda_version % 1000) / 10;
            int runtime_major = cuda_runtime_version / 1000;
            int runtime_minor = (cuda_runtime_version % 1000) / 10;
            std::cerr << "[主程序] 需要 CUDA " << required_major << "." << required_minor
                << "，检测到 " << runtime_major << "." << runtime_minor << std::endl;
            std::cin.get();
            return -1;
        }

        GPUResourceManager gpuManager;
        if (config.gpuMemoryReserveMB > 0)
        {
            if (!gpuManager.reserveGPUMemory(config.gpuMemoryReserveMB)) return -1;
        }
        
        if (config.enableGpuExclusiveMode)
        {
            if (!gpuManager.setGPUExclusiveMode()) return -1;
        }

        int cuda_devices = 0;
        if (cudaGetDeviceCount(&cuda_devices) != cudaSuccess || cuda_devices == 0)
        {
            std::cerr << "[主程序] 需要 CUDA 但未找到设备" << std::endl;
            std::cin.get();
            return -1;
        }
#endif
        if (!CreateDirectory(L"screenshots", NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        {
            std::cout << "[主程序] 创建截图文件夹失败" << std::endl;
            std::cin.get();
            return -1;
        }

        if (!CreateDirectory(L"models", NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        {
            std::cout << "[主程序] 创建模型文件夹失败" << std::endl;
            std::cin.get();
            return -1;
        }

        if (config.capture_method == "virtual_camera")
        {
            auto cams = VirtualCameraCapture::GetAvailableVirtualCameras();
            if (!cams.empty())
            {
                if (config.virtual_camera_name == "None" ||
                    std::find(cams.begin(), cams.end(), config.virtual_camera_name) == cams.end())
                {
                    config.virtual_camera_name = cams[0];
                    config.saveConfig("config.ini");
                    std::cout << "[主程序] 设置 virtual_camera_name = " << config.virtual_camera_name << std::endl;
                }
                std::cout << "[主程序] 已加载虚拟摄像头: " << cams.size() << " 个" << std::endl;
            }
            else
            {
                std::cerr << "[主程序] 未找到虚拟摄像头" << std::endl;
            }
        }

        std::string modelPath = "models/" + config.ai_model;

        if (!std::filesystem::exists(modelPath))
        {
            std::cerr << "[主程序] 指定的模型不存在: " << modelPath << std::endl;

            std::vector<std::string> modelFiles = getModelFiles();

            if (!modelFiles.empty())
            {
                config.ai_model = modelFiles[0];
                config.saveConfig();
                std::cout << "[主程序] 已加载第一个可用模型: " << config.ai_model << std::endl;
            }
            else
            {
                std::cerr << "[主程序] 'models' 目录中未找到模型文件" << std::endl;
                std::cin.get();
                return -1;
            }
        }

        createInputDevices();

        // Initialize ByteTracker
        byteTracker = new ByteTracker(
            config.bytetrack_track_thresh,
            config.bytetrack_high_thresh,
            config.bytetrack_match_thresh,
            config.bytetrack_max_time_lost
        );
        std::cout << "[ByteTrack] Tracker initialized" << std::endl;

        // Initialize DatasetCollector
        if (config.dataset_enabled)
        {
            DatasetConfig datasetConfig;
            datasetConfig.enabled = config.dataset_enabled;
            datasetConfig.outputDirectory = config.dataset_output_dir;
            datasetConfig.classHead = config.class_head;      // 从主配置读取类别 ID
            datasetConfig.classBody = config.class_player;    // class_player 对应 body
            datasetConfig.headConfidenceThreshold = config.dataset_head_confidence;
            datasetConfig.bodyConfidenceThreshold = config.dataset_body_confidence;
            datasetConfig.headMinSize = config.dataset_head_min_size;
            datasetConfig.bodyMinHeight = config.dataset_body_min_height;
            datasetConfig.samePersonIouThreshold = config.dataset_same_person_iou;
            datasetConfig.laplacianThreshold = config.dataset_laplacian_threshold;
            datasetConfig.overexposureThreshold = config.dataset_overexposure_threshold;
            datasetConfig.underexposureThreshold = config.dataset_underexposure_threshold;
            datasetConfig.hashBufferSize = config.dataset_hash_buffer_size;
            datasetConfig.imageSimilarityThreshold = config.dataset_image_similarity;
            datasetConfig.targetIouThreshold = config.dataset_target_iou;
            datasetConfig.targetDisplacementThreshold = config.dataset_target_displacement;
            datasetConfig.cooldownMs = config.dataset_cooldown_ms;
            datasetConfig.packageSize = config.dataset_package_size;
            datasetConfig.uploadEnabled = config.dataset_upload_enabled;
            datasetConfig.uploadEndpoint = config.dataset_upload_endpoint;
            
            // std::cout << "[数据集] 初始化配置: classHead=" << datasetConfig.classHead 
            //           << ", classBody=" << datasetConfig.classBody << std::endl;
            
            datasetCollector = new DatasetCollector(datasetConfig);
            
            // 配置 WebDAV 上传
            WebDAVConfig webdavConfig;
            webdavConfig.endpoint = config.dataset_webdav_endpoint;
            webdavConfig.username = config.dataset_webdav_username;
            webdavConfig.password = config.dataset_webdav_password;
            webdavConfig.remotePath = config.dataset_webdav_remote_path;
            webdavConfig.useSSL = config.dataset_webdav_use_ssl;
            datasetCollector->ConfigureWebDAV(webdavConfig);
            
            std::cout << "[数据集] 采集器已初始化" << std::endl;
        }

        MouseThread mouseThread(
            config.detection_resolution,
            config.fovX,
            config.fovY,
            config.minSpeedMultiplier,
            config.maxSpeedMultiplier,
            config.predictionInterval,
            config.auto_shoot,
            config.bScope_multiplier,
            arduinoSerial,
            gHub,
            kmboxSerial,
            kmboxNetSerial
        );

        globalMouseThread = &mouseThread;
        assignInputDevices();

        std::cout << "\n[\xE6\xA8\xA1\xE5\x9E\x8B] \xE6\xA3\x80\xE6\x9F\xA5\xE6\xA8\xA1\xE5\x9E\x8B\xE9\x85\x8D\xE7\xBD\xAE..." << std::endl;
        std::cout.flush();
        
        std::vector<std::string> availableModels = getAvailableModels();

        if (!config.ai_model.empty())
        {
            std::cout << "[\xE6\xA8\xA1\xE5\x9E\x8B] \xE9\x85\x8D\xE7\xBD\xAE\xE6\x8C\x87\xE5\xAE\x9A\xE6\xA8\xA1\xE5\x9E\x8B: " << config.ai_model << std::endl;
            std::string modelPath = "models/" + config.ai_model;
            if (!std::filesystem::exists(modelPath))
            {
                std::cerr << "[\xE6\xA8\xA1\xE5\x9E\x8B] \xE9\x94\x99\xE8\xAF\xAF: \xE6\x8C\x87\xE5\xAE\x9A\xE7\x9A\x84\xE6\xA8\xA1\xE5\x9E\x8B\xE4\xB8\x8D\xE5\xAD\x98\xE5\x9C\xA8: " << modelPath << std::endl;

                if (!availableModels.empty())
                {
                    config.ai_model = availableModels[0];
                    config.saveConfig("config.ini");
                    std::cout << "[\xE6\xA8\xA1\xE5\x9E\x8B] \xE5\xB7\xB2\xE8\x87\xAA\xE5\x8A\xA8\xE9\x80\x89\xE6\x8B\xA9\xE7\xAC\xAC\xE4\xB8\x80\xE4\xB8\xAA\xE5\x8F\xAF\xE7\x94\xA8\xE6\xA8\xA1\xE5\x9E\x8B: " << config.ai_model << std::endl;
                }
                else
                {
                    std::cerr << "[\xE6\xA8\xA1\xE5\x9E\x8B] \xE8\x87\xB4\xE5\x91\xBD\xE9\x94\x99\xE8\xAF\xAF: 'models' \xE7\x9B\xAE\xE5\xBD\x95\xE4\xB8\xAD\xE6\x9C\xAA\xE6\x89\xBE\xE5\x88\xB0\xE6\xA8\xA1\xE5\x9E\x8B\xE6\x96\x87\xE4\xBB\xB6!" << std::endl;
                    std::cerr << "[\xE6\xA8\xA1\xE5\x9E\x8B] \xE8\xAF\xB7\xE5\x9C\xA8 'models' \xE6\x96\x87\xE4\xBB\xB6\xE5\xA4\xB9\xE4\xB8\xAD\xE6\x94\xBE\xE5\x85\xA5 .engine \xE6\x88\x96 .onnx \xE6\xA8\xA1\xE5\x9E\x8B\xE6\x96\x87\xE4\xBB\xB6" << std::endl;
                    std::cin.get();
                    return -1;
                }
            }
            else
            {
                // Model exists, check file size
                uintmax_t fileSize = std::filesystem::file_size(modelPath);
                std::cout << "[\xE6\xA8\xA1\xE5\x9E\x8B] \xE6\x89\xBE\xE5\x88\xB0\xE6\xA8\xA1\xE5\x9E\x8B\xE6\x96\x87\xE4\xBB\xB6: " << modelPath << " (" << (fileSize / (1024 * 1024)) << " MB)" << std::endl;
                if (fileSize == 0)
                {
                    std::cerr << "[\xE6\xA8\xA1\xE5\x9E\x8B] \xE9\x94\x99\xE8\xAF\xAF: \xE6\xA8\xA1\xE5\x9E\x8B\xE6\x96\x87\xE4\xBB\xB6\xE4\xB8\xBA\xE7\xA9\xBA!" << std::endl;
                    std::cin.get();
                    return -1;
                }
            }
        }
        else
        {
            std::cout << "[\xE6\xA8\xA1\xE5\x9E\x8B] \xE9\x85\x8D\xE7\xBD\xAE\xE4\xB8\xAD\xE6\x9C\xAA\xE6\x8C\x87\xE5\xAE\x9A\xE6\xA8\xA1\xE5\x9E\x8B\xEF\xBC\x8C\xE8\x87\xAA\xE5\x8A\xA8\xE6\xA3\x80\xE6\xB5\x8B..." << std::endl;
            if (!availableModels.empty())
            {
                config.ai_model = availableModels[0];
                config.saveConfig();
                std::cout << "[\xE6\xA8\xA1\xE5\x9E\x8B] \xE5\xB7\xB2\xE8\x87\xAA\xE5\x8A\xA8\xE9\x80\x89\xE6\x8B\xA9\xE6\xA8\xA1\xE5\x9E\x8B: " << config.ai_model << std::endl;
            }
            else
            {
                std::cerr << "[\xE6\xA8\xA1\xE5\x9E\x8B] \xE8\x87\xB4\xE5\x91\xBD\xE9\x94\x99\xE8\xAF\xAF: 'models' \xE7\x9B\xAE\xE5\xBD\x95\xE4\xB8\xAD\xE6\x9C\xAA\xE6\x89\xBE\xE5\x88\xB0\xE6\xA8\xA1\xE5\x9E\x8B\xE6\x96\x87\xE4\xBB\xB6!" << std::endl;
                std::cerr << "[\xE6\xA8\xA1\xE5\x9E\x8B] \xE8\xAF\xB7\xE5\x9C\xA8 'models' \xE6\x96\x87\xE4\xBB\xB6\xE5\xA4\xB9\xE4\xB8\xAD\xE6\x94\xBE\xE5\x85\xA5 .engine \xE6\x88\x96 .onnx \xE6\xA8\xA1\xE5\x9E\x8B\xE6\x96\x87\xE4\xBB\xB6" << std::endl;
                std::cin.get();
                return -1;
            }
        }
        
        std::cout << "[\xE6\xA8\xA1\xE5\x9E\x8B] \xE4\xBD\xBF\xE7\x94\xA8\xE6\xA8\xA1\xE5\x9E\x8B: " << config.ai_model << std::endl;
        std::cout.flush();

        std::thread dml_detThread;

        std::cout << "\n[\xE6\xA3\x80\xE6\xB5\x8B\xE5\x99\xA8] \xE6\xAD\xA3\xE5\x9C\xA8\xE5\x88\x9D\xE5\xA7\x8B\xE5\x8C\x96\xE6\xA3\x80\xE6\xB5\x8B\xE5\x99\xA8 (\xE5\x90\x8E\xE7\xAB\xAF: " << config.backend << ")..." << std::endl;
        std::cout.flush();
        
        if (config.backend == "DML")
        {
            std::cout << "[\xE6\xA3\x80\xE6\xB5\x8B\xE5\x99\xA8] \xE5\x88\x9B\xE5\xBB\xBA DirectML \xE6\xA3\x80\xE6\xB5\x8B\xE5\x99\xA8..." << std::endl;
            dml_detector = new DirectMLDetector("models/" + config.ai_model);
            std::cout << "[\xE6\xA3\x80\xE6\xB5\x8B\xE5\x99\xA8] DirectML \xE6\xA3\x80\xE6\xB5\x8B\xE5\x99\xA8\xE5\xB7\xB2\xE5\x88\x9D\xE5\xA7\x8B\xE5\x8C\x96" << std::endl;
            dml_detThread = std::thread(&DirectMLDetector::dmlInferenceThread, dml_detector);
        }
#ifdef USE_CUDA
        else
        {
            std::cout << "[\xE6\xA3\x80\xE6\xB5\x8B\xE5\x99\xA8] \xE5\x88\x9B\xE5\xBB\xBA TensorRT \xE6\xA3\x80\xE6\xB5\x8B\xE5\x99\xA8..." << std::endl;
            std::cout.flush();
            trt_detector.initialize("models/" + config.ai_model);
            std::cout << "[\xE6\xA3\x80\xE6\xB5\x8B\xE5\x99\xA8] TensorRT \xE6\xA3\x80\xE6\xB5\x8B\xE5\x99\xA8\xE5\xB7\xB2\xE5\x88\x9D\xE5\xA7\x8B\xE5\x8C\x96" << std::endl;
        }
#endif

        detection_resolution_changed.store(true);

        welcome_message();

        std::thread keyThread(keyboardListener);
        std::thread capThread(captureThread, config.detection_resolution, config.detection_resolution);

#ifdef USE_CUDA
        std::thread trt_detThread(&TrtDetector::inferenceThread, &trt_detector);
#endif
        std::thread mouseMovThread(mouseThreadFunction, std::ref(mouseThread));
        std::thread overlayThread(OverlayThread);

        if (config.game_overlay_enabled)
        {
            gameOverlayPtr = new Game_overlay();
            int pw = GetSystemMetrics(SM_CXSCREEN);
            int ph = GetSystemMetrics(SM_CYSCREEN);
            gameOverlayPtr->SetWindowBounds(0, 0, pw, ph);
            gameOverlayPtr->SetMaxFPS(config.game_overlay_max_fps > 0 ? (unsigned)config.game_overlay_max_fps : 0);
            gameOverlayPtr->Start();
            gameOverlayShouldExit.store(false);
            gameOverlayThread = std::thread(gameOverlayRenderLoop);
        }

        keyThread.join();
        capThread.join();
        if (dml_detThread.joinable())
        {
            dml_detector->shouldExit = true;
            dml_detector->inferenceCV.notify_all();
            dml_detThread.join();
        }

#ifdef USE_CUDA
        trt_detThread.join();
#endif
        mouseMovThread.join();
        overlayThread.join();

        if (arduinoSerial)
        {
            delete arduinoSerial;
        }

        if (gHub)
        {
            gHub->mouse_close();
            delete gHub;
        }

        if (dml_detector)
        {
            delete dml_detector;
            dml_detector = nullptr;
        }

        if (byteTracker)
        {
            delete byteTracker;
            byteTracker = nullptr;
        }

        if (datasetCollector)
        {
            delete datasetCollector;
            datasetCollector = nullptr;
        }

        gameOverlayShouldExit.store(true);
        if (gameOverlayThread.joinable()) gameOverlayThread.join();
        if (gameOverlayPtr)
        {
            gameOverlayPtr->Stop();
            delete gameOverlayPtr;
            gameOverlayPtr = nullptr;
        }

        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[主程序] 主线程发生错误: " << e.what() << std::endl;
        std::cout << "按回车键退出...";
        std::cin.get();
        return -1;
    }
}