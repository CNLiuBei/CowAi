#include "ReIDFeatureExtractor.h"
#include <onnxruntime_cxx_api.h>
#include <dml_provider_factory.h>
#include <algorithm>
#include <numeric>
#include <cmath>

// Ensure std::min and std::max are available
#undef min
#undef max

ReIDFeatureExtractor::ReIDFeatureExtractor(const Config& config)
    : m_config(config), m_initialized(false)
{
    m_initialized = initializeSession();
}

ReIDFeatureExtractor::~ReIDFeatureExtractor()
{
    // Cleanup handled by unique_ptr
}

bool ReIDFeatureExtractor::initializeSession()
{
    try
    {
        // Create ONNX Runtime environment
        m_ortEnv = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "ReIDFeatureExtractor");
        
        // Create session options
        m_sessionOptions = std::make_unique<Ort::SessionOptions>();
        m_sessionOptions->SetIntraOpNumThreads(1);
        m_sessionOptions->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        
        // Enable GPU acceleration if requested
        if (m_config.useGPU)
        {
            try
            {
#ifdef USE_CUDA
                // Try CUDA first for CUDA builds
                OrtCUDAProviderOptions cuda_options;
                cuda_options.device_id = 0;
                m_sessionOptions->AppendExecutionProvider_CUDA(cuda_options);
#else
                // Use DirectML for DML builds
                Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_DML(*m_sessionOptions, 0));
#endif
            }
            catch (const std::exception& e)
            {
                // Fallback to CPU if GPU fails
                m_config.useGPU = false;
            }
        }
        
        // Load model
        std::wstring modelPathW(m_config.modelPath.begin(), m_config.modelPath.end());
        m_ortSession = std::make_unique<Ort::Session>(*m_ortEnv, modelPathW.c_str(), *m_sessionOptions);
        
        // Get input/output names
        Ort::AllocatorWithDefaultOptions allocator;
        
        // Input name
        auto inputName = m_ortSession->GetInputNameAllocated(0, allocator);
        m_inputNames.push_back(inputName.get());
        
        // Output name
        auto outputName = m_ortSession->GetOutputNameAllocated(0, allocator);
        m_outputNames.push_back(outputName.get());
        
        // Create memory info
        m_memoryInfo = std::make_unique<Ort::MemoryInfo>(
            Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)
        );
        
        return true;
    }
    catch (const std::exception& e)
    {
        return false;
    }
}

cv::Mat ReIDFeatureExtractor::preprocessImage(const cv::Mat& crop)
{
    // Resize to model input size
    cv::Mat resized;
    cv::resize(crop, resized, cv::Size(m_config.inputWidth, m_config.inputHeight));
    
    // Convert BGR to RGB
    cv::Mat rgb;
    cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);
    
    // Convert to float and normalize to [0, 1]
    cv::Mat floatImg;
    rgb.convertTo(floatImg, CV_32F, 1.0 / 255.0);
    
    // Normalize with ImageNet mean and std
    // Mean: [0.485, 0.456, 0.406], Std: [0.229, 0.224, 0.225]
    std::vector<cv::Mat> channels(3);
    cv::split(floatImg, channels);
    
    channels[0] = (channels[0] - 0.485f) / 0.229f;  // R
    channels[1] = (channels[1] - 0.456f) / 0.224f;  // G
    channels[2] = (channels[2] - 0.406f) / 0.225f;  // B
    
    cv::merge(channels, floatImg);
    
    return floatImg;
}

std::vector<float> ReIDFeatureExtractor::runInference(const std::vector<float>& input)
{
    try
    {
        // Create input tensor
        std::vector<int64_t> inputShape = {1, 3, m_config.inputHeight, m_config.inputWidth};
        
        auto inputTensor = Ort::Value::CreateTensor<float>(
            *m_memoryInfo,
            const_cast<float*>(input.data()),
            input.size(),
            inputShape.data(),
            inputShape.size()
        );
        
        // Run inference
        auto outputTensors = m_ortSession->Run(
            Ort::RunOptions{nullptr},
            m_inputNames.data(),
            &inputTensor,
            1,
            m_outputNames.data(),
            1
        );
        
        // Extract output
        float* outputData = outputTensors[0].GetTensorMutableData<float>();
        auto outputShape = outputTensors[0].GetTensorTypeAndShapeInfo().GetShape();
        
        size_t outputSize = 1;
        for (auto dim : outputShape)
            outputSize *= dim;
        
        std::vector<float> feature(outputData, outputData + outputSize);
        
        // Normalize feature
        normalizeFeature(feature);
        
        return feature;
    }
    catch (const std::exception& e)
    {
        return std::vector<float>();
    }
}

std::vector<std::vector<float>> ReIDFeatureExtractor::runBatchInference(
    const std::vector<std::vector<float>>& inputs)
{
    if (inputs.empty())
        return {};
    
    try
    {
        size_t batchSize = inputs.size();
        size_t inputSize = inputs[0].size();
        
        // Flatten batch
        std::vector<float> batchInput;
        batchInput.reserve(batchSize * inputSize);
        for (const auto& input : inputs)
        {
            batchInput.insert(batchInput.end(), input.begin(), input.end());
        }
        
        // Create input tensor
        std::vector<int64_t> inputShape = {
            static_cast<int64_t>(batchSize),
            3,
            m_config.inputHeight,
            m_config.inputWidth
        };
        
        auto inputTensor = Ort::Value::CreateTensor<float>(
            *m_memoryInfo,
            batchInput.data(),
            batchInput.size(),
            inputShape.data(),
            inputShape.size()
        );
        
        // Run inference
        auto outputTensors = m_ortSession->Run(
            Ort::RunOptions{nullptr},
            m_inputNames.data(),
            &inputTensor,
            1,
            m_outputNames.data(),
            1
        );
        
        // Extract outputs
        float* outputData = outputTensors[0].GetTensorMutableData<float>();
        auto outputShape = outputTensors[0].GetTensorTypeAndShapeInfo().GetShape();
        
        size_t featureDim = outputShape.back();
        
        std::vector<std::vector<float>> features;
        features.reserve(batchSize);
        
        for (size_t i = 0; i < batchSize; ++i)
        {
            std::vector<float> feature(
                outputData + i * featureDim,
                outputData + (i + 1) * featureDim
            );
            normalizeFeature(feature);
            features.push_back(feature);
        }
        
        return features;
    }
    catch (const std::exception& e)
    {
        return std::vector<std::vector<float>>(inputs.size());
    }
}

cv::Rect ReIDFeatureExtractor::clipBBox(const cv::Rect& bbox, const cv::Size& frameSize)
{
    int x = std::max(0, bbox.x);
    int y = std::max(0, bbox.y);
    int w = std::min(bbox.width, frameSize.width - x);
    int h = std::min(bbox.height, frameSize.height - y);
    
    return cv::Rect(x, y, w, h);
}

std::vector<float> ReIDFeatureExtractor::extractFeature(
    const cv::Mat& frame,
    const cv::Rect& bbox)
{
    if (!m_initialized || frame.empty())
        return {};
    
    // Clip bbox to frame bounds
    cv::Rect clippedBBox = clipBBox(bbox, frame.size());
    
    // Check minimum size
    if (clippedBBox.width < m_config.minDetectionSize ||
        clippedBBox.height < m_config.minDetectionSize)
        return {};
    
    // Crop region
    cv::Mat crop = frame(clippedBBox);
    
    // Preprocess
    cv::Mat preprocessed = preprocessImage(crop);
    
    // Convert to CHW format
    std::vector<float> input;
    input.reserve(3 * m_config.inputHeight * m_config.inputWidth);
    
    for (int c = 0; c < 3; ++c)
    {
        for (int h = 0; h < m_config.inputHeight; ++h)
        {
            for (int w = 0; w < m_config.inputWidth; ++w)
            {
                input.push_back(preprocessed.at<cv::Vec3f>(h, w)[c]);
            }
        }
    }
    
    // Run inference
    return runInference(input);
}

std::vector<std::vector<float>> ReIDFeatureExtractor::extractFeatures(
    const cv::Mat& frame,
    const std::vector<cv::Rect>& bboxes)
{
    if (!m_initialized || frame.empty() || bboxes.empty())
        return std::vector<std::vector<float>>(bboxes.size());
    
    // Limit batch size
    size_t numBatches = (bboxes.size() + m_config.maxBatchSize - 1) / m_config.maxBatchSize;
    
    std::vector<std::vector<float>> allFeatures;
    allFeatures.reserve(bboxes.size());
    
    for (size_t batchIdx = 0; batchIdx < numBatches; ++batchIdx)
    {
        size_t startIdx = batchIdx * m_config.maxBatchSize;
        size_t endIdx = std::min(startIdx + m_config.maxBatchSize, bboxes.size());
        
        // Prepare batch inputs
        std::vector<std::vector<float>> batchInputs;
        std::vector<size_t> validIndices;
        
        for (size_t i = startIdx; i < endIdx; ++i)
        {
            cv::Rect clippedBBox = clipBBox(bboxes[i], frame.size());
            
            if (clippedBBox.width >= m_config.minDetectionSize &&
                clippedBBox.height >= m_config.minDetectionSize)
            {
                cv::Mat crop = frame(clippedBBox);
                cv::Mat preprocessed = preprocessImage(crop);
                
                // Convert to CHW format
                std::vector<float> input;
                input.reserve(3 * m_config.inputHeight * m_config.inputWidth);
                
                for (int c = 0; c < 3; ++c)
                {
                    for (int h = 0; h < m_config.inputHeight; ++h)
                    {
                        for (int w = 0; w < m_config.inputWidth; ++w)
                        {
                            input.push_back(preprocessed.at<cv::Vec3f>(h, w)[c]);
                        }
                    }
                }
                
                batchInputs.push_back(input);
                validIndices.push_back(i);
            }
        }
        
        // Run batch inference
        auto batchFeatures = runBatchInference(batchInputs);
        
        // Map results back to original indices
        size_t validIdx = 0;
        for (size_t i = startIdx; i < endIdx; ++i)
        {
            if (validIdx < validIndices.size() && validIndices[validIdx] == i)
            {
                allFeatures.push_back(batchFeatures[validIdx]);
                ++validIdx;
            }
            else
            {
                allFeatures.push_back({});  // Empty for invalid detections
            }
        }
    }
    
    return allFeatures;
}

float ReIDFeatureExtractor::cosineSimilarity(
    const std::vector<float>& feat1,
    const std::vector<float>& feat2)
{
    if (feat1.empty() || feat2.empty() || feat1.size() != feat2.size())
        return 0.0f;
    
    // Dot product (assuming features are already normalized)
    float dotProduct = std::inner_product(
        feat1.begin(), feat1.end(),
        feat2.begin(),
        0.0f
    );
    
    // Clamp to [0, 1] range (should already be in [-1, 1] for normalized vectors)
    return std::max(0.0f, std::min(1.0f, (dotProduct + 1.0f) * 0.5f));
}

void ReIDFeatureExtractor::normalizeFeature(std::vector<float>& feature)
{
    if (feature.empty())
        return;
    
    // Calculate L2 norm
    float norm = 0.0f;
    for (float val : feature)
        norm += val * val;
    norm = std::sqrt(norm);
    
    // Normalize
    if (norm > 1e-6f)
    {
        for (float& val : feature)
            val /= norm;
    }
}
