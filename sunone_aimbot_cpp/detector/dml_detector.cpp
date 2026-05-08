#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <winsock2.h>
#include <Windows.h>
#include <dml_provider_factory.h>
#include <wrl/client.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <dxgi.h>
#include <cstring>

#include "dml_detector.h"
#include "sunone_aimbot_cpp.h"
#include "postProcess.h"
#include "capture.h"
#include "../capture/d3d11_staging_manager.h"
#include "include/other_tools.h"

extern std::atomic<bool> detector_model_changed;
extern std::atomic<bool> detection_resolution_changed;
extern std::atomic<bool> detectionPaused;

std::chrono::duration<double, std::milli> lastPreprocessTimeDML{};
std::chrono::duration<double, std::milli> lastCopyTimeDML{};
std::chrono::duration<double, std::milli> lastPostprocessTimeDML{};
std::chrono::duration<double, std::milli> lastNmsTimeDML{};

std::string GetDMLDeviceName(int deviceId)
{
    Microsoft::WRL::ComPtr<IDXGIFactory1> dxgiFactory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory))))
        return "Unknown";

    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    if (FAILED(dxgiFactory->EnumAdapters1(deviceId, &adapter)))
        return "Invalid device ID";

    DXGI_ADAPTER_DESC1 desc;
    if (FAILED(adapter->GetDesc1(&desc)))
        return "Failed to get description";

    std::wstring wname(desc.Description);
    return std::string(wname.begin(), wname.end());
}

DirectMLDetector::DirectMLDetector(const std::string& model_path)
    :
    env(ORT_LOGGING_LEVEL_WARNING, "DML_Detector"),
    memory_info(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault))
{
    session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    session_options.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
    session_options.SetIntraOpNumThreads(1);
    session_options.SetInterOpNumThreads(1);

    Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_DML(session_options, config.dml_device_id));

    if (config.verbose)
        std::cout << "[DirectML] Using adapter: " << GetDMLDeviceName(config.dml_device_id) << std::endl;

    initializeModel(model_path);
}

DirectMLDetector::~DirectMLDetector()
{
    shouldExit = true;
    inferenceCV.notify_all();
    stagingManager.reset();
}

bool DirectMLDetector::initializeStagingManager(ID3D11Device* device, ID3D11DeviceContext* context, int width, int height)
{
    if (!device || !context)
    {
        std::cerr << "[DirectMLDetector] Invalid D3D11 device or context" << std::endl;
        return false;
    }
    stagingManager = std::make_unique<D3D11StagingManager>();
    return stagingManager->Initialize(device, context, width, height);
}

void DirectMLDetector::processFrameFromGpu(ID3D11Texture2D* texture)
{
    if (detectionPaused)
    {
        std::lock_guard<std::mutex> lock(detectionBuffer.mutex);
        detectionBuffer.boxes.clear();
        detectionBuffer.classes.clear();
        detectionBuffer.version++;
        detectionBuffer.cv.notify_all();
        return;
    }

    if (!stagingManager || !stagingManager->IsInitialized() || !texture)
    {
        return;
    }

    // Copy texture to Mat using staging manager
    cv::Mat frame;
    if (!stagingManager->CopyToMat(texture, frame))
    {
        std::cerr << "[DirectMLDetector] Failed to copy texture to Mat" << std::endl;
        return;
    }

    // Convert BGRA to BGR
    cv::Mat bgrFrame;
    cv::cvtColor(frame, bgrFrame, cv::COLOR_BGRA2BGR);

    // Process the frame
    std::unique_lock<std::mutex> lock(inferenceMutex);
    currentFrame = std::move(bgrFrame);
    frameReady = true;
    inferenceCV.notify_one();
}

void DirectMLDetector::initializeModel(const std::string& model_path)
{
    std::wstring model_path_wide = utf8_to_path(model_path).wstring();
    session = Ort::Session(env, model_path_wide.c_str(), session_options);

    input_name = session.GetInputNameAllocated(0, allocator).get();
    output_name = session.GetOutputNameAllocated(0, allocator).get();

    Ort::TypeInfo input_type_info = session.GetInputTypeInfo(0);
    auto input_tensor_info = input_type_info.GetTensorTypeAndShapeInfo();
    input_shape = input_tensor_info.GetShape();

    bool isStatic = true;
    for (auto d : input_shape) if (d <= 0) isStatic = false;

    if (isStatic != config.fixed_input_size)
    {
        config.fixed_input_size = isStatic;
        config.saveConfig();
        detector_model_changed.store(true);
        std::cout << "[DML] Automatically set fixed_input_size = " << (isStatic ? "true" : "false") << std::endl;
    }
}

std::vector<Detection> DirectMLDetector::detect(const cv::Mat& input_frame)
{
    std::vector<cv::Mat> batch = { input_frame };
    auto batchResult = detectBatch(batch);
    if (!batchResult.empty())
        return batchResult[0];
    else
        return {};
}

std::vector<std::vector<Detection>> DirectMLDetector::detectBatch(const std::vector<cv::Mat>& frames)
{
    std::vector<std::vector<Detection>> empty;
    if (frames.empty()) return empty;

    const int batch_size = static_cast<int>(frames.size());

    int model_h = (input_shape.size() > 2) ? static_cast<int>(input_shape[2]) : -1;
    int model_w = (input_shape.size() > 3) ? static_cast<int>(input_shape[3]) : -1;
    const bool useFixed = config.fixed_input_size && model_h > 0 && model_w > 0;

    const int target_h = useFixed ? model_h : config.detection_resolution;
    const int target_w = useFixed ? model_w : config.detection_resolution;

    auto t0 = std::chrono::steady_clock::now();
    std::vector<float> input_tensor_values(batch_size * 3 * target_h * target_w);

    for (int b = 0; b < batch_size; ++b)
    {
        cv::Mat resized;
        cv::resize(frames[b], resized, cv::Size(target_w, target_h));
        cv::cvtColor(resized, resized, cv::COLOR_BGR2RGB);
        resized.convertTo(resized, CV_32FC3, 1.0f / 255.0f);

        // HWC -> CHW using cv::split (much faster than triple nested loop)
        std::vector<cv::Mat> channels(3);
        cv::split(resized, channels);
        const size_t planeSize = static_cast<size_t>(target_h) * target_w;
        const size_t batchOffset = b * 3 * planeSize;
        for (int c = 0; c < 3; ++c)
        {
            std::memcpy(&input_tensor_values[batchOffset + c * planeSize],
                        channels[c].data, planeSize * sizeof(float));
        }
    }
    auto t1 = std::chrono::steady_clock::now();

    std::vector<int64_t> ort_input_shape{ batch_size, 3, target_h, target_w };
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        memory_info, input_tensor_values.data(), input_tensor_values.size(),
        ort_input_shape.data(), ort_input_shape.size());

    const char* input_names[] = { input_name.c_str() };
    const char* output_names[] = { output_name.c_str() };

    auto t2 = std::chrono::steady_clock::now();
    auto output_tensors = session.Run(Ort::RunOptions{ nullptr },
        input_names, &input_tensor, 1,
        output_names, 1);
    auto t3 = std::chrono::steady_clock::now();

    float* outData = output_tensors.front().GetTensorMutableData<float>();
    Ort::TensorTypeAndShapeInfo outInfo = output_tensors.front().GetTensorTypeAndShapeInfo();
    std::vector<int64_t> outShape = outInfo.GetShape(); // [B, dim1, dim2]

    int dim1 = static_cast<int>(outShape[1]);
    int dim2 = static_cast<int>(outShape[2]);

    // Detect output format:
    // YOLOv8/v10 transposed: [B, attributes, N] where attributes < N (e.g. [1, 6, 2100])
    // YOLOv5 row format:     [B, N, attributes] where N > attributes (e.g. [1, 2100, 7])
    bool isTransposed = (dim1 < dim2);

    int rows, cols, num_classes;
    if (isTransposed)
    {
        // YOLOv8/v10: rows=attributes, cols=num_detections
        rows = dim1;
        cols = dim2;
        num_classes = rows - 4;
    }
    else
    {
        // YOLOv5: rows=num_detections, cols=attributes
        rows = dim1;
        cols = dim2;
        num_classes = cols - 5; // cx,cy,w,h,obj_conf + class_scores
    }

    std::vector<std::vector<Detection>> batchDetections(batch_size);
    float conf_thr = config.confidence_threshold;
    float nms_thr = config.nms_threshold;

    auto t4 = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> nmsTimeTmp{ 0 };

    for (int b = 0; b < batch_size; ++b)
    {
        const float* ptr = outData + b * dim1 * dim2;
        std::vector<Detection> detections;

        if (isTransposed)
        {
            // YOLOv8/v10 format: pass as {attributes, N}
            std::vector<int64_t> shp = { rows, cols };
            detections = postProcessYoloDML(ptr, shp, num_classes, conf_thr, nms_thr, &nmsTimeTmp);
        }
        else
        {
            // YOLOv5 format: pass as {N, attributes}
            std::vector<int64_t> shp = { rows, cols };
            detections = postProcessYoloDML(ptr, shp, num_classes, conf_thr, nms_thr, &nmsTimeTmp);
        }

        if (useFixed && (target_w != config.detection_resolution))
        {
            float scale = static_cast<float>(config.detection_resolution) / target_w;
            for (auto& d : detections)
            {
                d.box.x = static_cast<int>(d.box.x * scale);
                d.box.y = static_cast<int>(d.box.y * scale);
                d.box.width = static_cast<int>(d.box.width * scale);
                d.box.height = static_cast<int>(d.box.height * scale);
            }
        }

        batchDetections[b] = std::move(detections);
    }
    auto t5 = std::chrono::steady_clock::now();

    lastPreprocessTimeDML = t1 - t0;
    lastInferenceTimeDML = t3 - t2;
    lastCopyTimeDML = t4 - t3;
    lastPostprocessTimeDML = t5 - t4;
    lastNmsTimeDML = nmsTimeTmp;

    return batchDetections;
}

int DirectMLDetector::getNumberOfClasses()
{
    Ort::TypeInfo output_type_info = session.GetOutputTypeInfo(0);
    auto tensor_info = output_type_info.GetTensorTypeAndShapeInfo();
    std::vector<int64_t> output_shape = tensor_info.GetShape();

    if (output_shape.size() == 3)
    {
        int64_t dim1 = output_shape[1];
        int64_t dim2 = output_shape[2];
        if (dim1 < dim2)
        {
            // YOLOv8/v10 transposed: [B, attributes, N]
            return static_cast<int>(dim1) - 4;
        }
        else
        {
            // YOLOv5 row format: [B, N, attributes]
            return static_cast<int>(dim2) - 5;
        }
    }
    else
    {
        std::cerr << "[DirectMLDetector] Unexpected output tensor shape." << std::endl;
        return -1;
    }
}

void DirectMLDetector::processFrame(const cv::Mat& frame)
{
    if (detectionPaused)
    {
        std::lock_guard<std::mutex> lock(detectionBuffer.mutex);
        detectionBuffer.boxes.clear();
        detectionBuffer.classes.clear();
        detectionBuffer.version++;
        detectionBuffer.cv.notify_all();
        return;
    }
    std::unique_lock<std::mutex> lock(inferenceMutex);
    currentFrame = frame.clone();
    frameReady = true;
    inferenceCV.notify_one();
}

void DirectMLDetector::dmlInferenceThread()
{
    while (!shouldExit)
    {
        if (detector_model_changed.load())
        {
            initializeModel("models/" + config.ai_model);
            detection_resolution_changed.store(true);
            detector_model_changed.store(false);
            std::cout << "[DML] Detector reloaded: " << config.ai_model << std::endl;
        }

        if (detectionPaused)
        {
            std::lock_guard<std::mutex> lock(detectionBuffer.mutex);
            detectionBuffer.boxes.clear();
            detectionBuffer.classes.clear();
            detectionBuffer.version++;
            detectionBuffer.cv.notify_all();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        cv::Mat frame;
        bool hasNewFrame = false;
        {
            std::unique_lock<std::mutex> lock(inferenceMutex);
            if (!frameReady && !shouldExit)
                inferenceCV.wait(lock, [this] { return frameReady || shouldExit; });

            if (shouldExit) break;

            if (frameReady)
            {
                frame = std::move(currentFrame);
                frameReady = false;
                hasNewFrame = true;
            }
        }

        if (hasNewFrame && !frame.empty())
        {
            auto start = std::chrono::steady_clock::now();

            std::vector<cv::Mat> batchFrames = { frame };
            auto detectionsBatch = detectBatch(batchFrames);
            const std::vector<Detection>& detections = detectionsBatch.back();

            auto end = std::chrono::steady_clock::now();
            lastInferenceTimeDML = end - start;

            std::lock_guard<std::mutex> lock(detectionBuffer.mutex);
            detectionBuffer.boxes.clear();
            detectionBuffer.classes.clear();
            detectionBuffer.scores.clear();
            for (const auto& d : detections) {
                detectionBuffer.boxes.push_back(d.box);
                detectionBuffer.classes.push_back(d.classId);
                detectionBuffer.scores.push_back(d.confidence);
            }
            
            detectionBuffer.version++;
            detectionBuffer.cv.notify_all();
        }
    }
}
