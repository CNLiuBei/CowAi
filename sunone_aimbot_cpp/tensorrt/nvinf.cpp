#ifdef USE_CUDA
#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <winsock2.h>
#include <Windows.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <NvOnnxParser.h>
#include <cuda_runtime.h>

#include "nvinf.h"
#include "sunone_aimbot_cpp.h"
#include "trt_monitor.h"

Logger gLogger;

void Logger::log(nvinfer1::ILogger::Severity severity, const char* msg) noexcept
{
    if (severity <= nvinfer1::ILogger::Severity::kWARNING)
    {
        std::string devMsg = msg;

        std::string magicTag = "Serialization assertion plan->header.magicTag == rt::kPLAN_MAGIC_TAG failed.";
        std::string old_deserialization = "Using old deserialization call on a weight-separated plan file.";
        if (devMsg.find(magicTag) != std::string::npos || devMsg.find(old_deserialization) != std::string::npos)
        {
            std::cout << "[TensorRT] 错误: 此引擎模型不适合执行。请删除此引擎模型并在设置中设置此模型的ONNX版本，程序将自动导出模型。" << std::endl;
        }
        else
        {
            std::cout << "[TensorRT] " << severityLevelName(severity) << ": " << msg << std::endl;
        }
    }
}

const char* Logger::severityLevelName(nvinfer1::ILogger::Severity severity)
{
    switch (severity)
    {
        case nvinfer1::ILogger::Severity::kINTERNAL_ERROR: return "INTERNAL_ERROR";
        case nvinfer1::ILogger::Severity::kERROR:          return "ERROR";
        case nvinfer1::ILogger::Severity::kWARNING:        return "WARNING";
        case nvinfer1::ILogger::Severity::kINFO:           return "INFO";
        case nvinfer1::ILogger::Severity::kVERBOSE:        return "VERBOSE";
        default:                                           return "UNKNOWN";
    }
}

nvinfer1::IBuilder* createInferBuilder()
{
    return nvinfer1::createInferBuilder(gLogger);
}

nvinfer1::INetworkDefinition* createNetwork(nvinfer1::IBuilder* builder)
{
    const auto explicitBatch = 1U << static_cast<uint32_t>(nvinfer1::NetworkDefinitionCreationFlag::kEXPLICIT_BATCH);
    return builder->createNetworkV2(explicitBatch);
}

nvinfer1::IBuilderConfig* createBuilderConfig(nvinfer1::IBuilder* builder)
{
    return builder->createBuilderConfig();
}

nvinfer1::ICudaEngine* loadEngineFromFile(const std::string& engineFile, nvinfer1::IRuntime* runtime)
{
    std::cout << "[TensorRT] \xE6\x89\x93\xE5\xBC\x80\xE5\xBC\x95\xE6\x93\x8E\xE6\x96\x87\xE4\xBB\xB6: " << engineFile << std::endl;
    std::cout.flush();
    
    std::ifstream file(engineFile, std::ios::binary);
    if (!file.good())
    {
        std::cerr << "[TensorRT] \xE6\x97\xA0\xE6\xB3\x95\xE6\x89\x93\xE5\xBC\x80\xE5\xBC\x95\xE6\x93\x8E\xE6\x96\x87\xE4\xBB\xB6: " << engineFile << std::endl;
        return nullptr;
    }

    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::cout << "[TensorRT] \xE5\xBC\x95\xE6\x93\x8E\xE6\x96\x87\xE4\xBB\xB6\xE5\xA4\xA7\xE5\xB0\x8F: " << size << " \xE5\xAD\x97\xE8\x8A\x82, \xE6\xAD\xA3\xE5\x9C\xA8\xE8\xAF\xBB\xE5\x8F\x96..." << std::endl;
    std::cout.flush();
    
    std::vector<char> engineData(size);
    file.read(engineData.data(), size);
    file.close();

    std::cout << "[TensorRT] \xE6\xAD\xA3\xE5\x9C\xA8\xE5\x8F\x8D\xE5\xBA\x8F\xE5\x88\x97\xE5\x8C\x96\xE5\xBC\x95\xE6\x93\x8E (\xE8\xBF\x99\xE5\x8F\xAF\xE8\x83\xBD\xE9\x9C\x80\xE8\xA6\x81\xE4\xB8\x80\xE6\xAE\xB5\xE6\x97\xB6\xE9\x97\xB4)..." << std::endl;
    std::cout.flush();
    
    nvinfer1::ICudaEngine* engine = runtime->deserializeCudaEngine(engineData.data(), size);
    if (!engine)
    {
        std::cerr << "[TensorRT] \xE5\xBC\x95\xE6\x93\x8E\xE5\x8F\x8D\xE5\xBA\x8F\xE5\x88\x97\xE5\x8C\x96\xE5\xA4\xB1\xE8\xB4\xA5: " << engineFile << std::endl;
        std::cerr << "[TensorRT] \xE8\xBF\x99\xE9\x80\x9A\xE5\xB8\xB8\xE6\x84\x8F\xE5\x91\xB3\xE7\x9D\x80\xE5\xBC\x95\xE6\x93\x8E\xE6\x98\xAF\xE7\x94\xA8\xE4\xB8\x8D\xE5\x90\x8C\xE7\x9A\x84 TensorRT \xE7\x89\x88\xE6\x9C\xAC\xE6\x9E\x84\xE5\xBB\xBA\xE7\x9A\x84" << std::endl;
        return nullptr;
    }

    std::cout << "[TensorRT] \xE5\xBC\x95\xE6\x93\x8E\xE5\x8A\xA0\xE8\xBD\xBD\xE6\x88\x90\xE5\x8A\x9F" << std::endl;
    std::cout.flush();
    
    return engine;
}

nvinfer1::ICudaEngine* buildEngineFromOnnx(const std::string& onnxFile, nvinfer1::ILogger& logger)
{
    nvinfer1::IBuilder* builder = nvinfer1::createInferBuilder(logger);
    const auto explicitBatch = 1U << static_cast<uint32_t>(nvinfer1::NetworkDefinitionCreationFlag::kEXPLICIT_BATCH);
    nvinfer1::INetworkDefinition* network = builder->createNetworkV2(explicitBatch);

    nvinfer1::IBuilderConfig* cfg = builder->createBuilderConfig();

    nvonnxparser::IParser* parser = nvonnxparser::createParser(*network, logger);

    ImGuiProgressMonitor progressMonitor;
    cfg->setProgressMonitor(&progressMonitor);
    gIsTrtExporting = true;

    if (!parser->parseFromFile(onnxFile.c_str(), static_cast<int>(nvinfer1::ILogger::Severity::kWARNING)))
    {
        std::cerr << "[TensorRT] 错误: 解析ONNX文件失败: " << onnxFile << std::endl;
        delete parser;
        delete network;
        delete builder;
        delete cfg;
        return nullptr;
    }

    nvinfer1::ITensor* inputTensor = network->getInput(0);
    const char* inName = inputTensor->getName();
    nvinfer1::Dims inDims = inputTensor->getDimensions();
    int H = (inDims.nbDims >= 4) ? inDims.d[2] : -1;
    int W = (inDims.nbDims >= 4) ? inDims.d[3] : -1;

    bool fixedByModel = (H > 0 && W > 0);
    bool fixedByConfig = config.fixed_input_size;
    bool makeStatic = fixedByModel || fixedByConfig;

    if (fixedByConfig && (H <= 0 || W <= 0))
        H = W = config.detection_resolution;

    nvinfer1::IOptimizationProfile* profile = builder->createOptimizationProfile();
    if (makeStatic)
    {
        nvinfer1::Dims4 d{ 1, 3, H, W };
        profile->setDimensions(inName, nvinfer1::OptProfileSelector::kMIN, d);
        profile->setDimensions(inName, nvinfer1::OptProfileSelector::kOPT, d);
        profile->setDimensions(inName, nvinfer1::OptProfileSelector::kMAX, d);
        if (config.verbose)
            std::cout << "[TensorRT] 静态配置 " << H << "x" << W << std::endl;
    }
    else
    {
        profile->setDimensions(inName, nvinfer1::OptProfileSelector::kMIN, nvinfer1::Dims4{ 1, 3, 160, 160 });
        profile->setDimensions(inName, nvinfer1::OptProfileSelector::kOPT, nvinfer1::Dims4{ 1, 3, 320, 320 });
        profile->setDimensions(inName, nvinfer1::OptProfileSelector::kMAX, nvinfer1::Dims4{ 1, 3, 640, 640 });
        if (config.verbose)
            std::cout << "[TensorRT] 动态配置 160/320/640" << std::endl;
    }

    cfg->addOptimizationProfile(profile);


    if (config.export_enable_fp16)
    {
        if (config.verbose)
            std::cout << "[TensorRT] 设置 FP16" << std::endl;
        cfg->setFlag(nvinfer1::BuilderFlag::kFP16);
    }
    if (config.export_enable_fp8)
    {
        if (config.verbose)
            std::cout << "[TensorRT] 设置 FP8" << std::endl;
        cfg->setFlag(nvinfer1::BuilderFlag::kFP8);
    }

    cudaStream_t stream;
    cudaStreamCreate(&stream);

    std::cout << "[TensorRT] 正在构建引擎 (这可能需要几分钟)..." << std::endl;

    auto plan = builder->buildSerializedNetwork(*network, *cfg);
    if (!plan)
    {
        std::cerr << "[TensorRT] 错误: 无法构建引擎" << std::endl;
        delete parser;
        delete network;
        delete builder;
        delete cfg;
        return nullptr;
    }

    nvinfer1::IRuntime* runtime = nvinfer1::createInferRuntime(logger);
    nvinfer1::ICudaEngine* engine = runtime->deserializeCudaEngine(plan->data(), plan->size());

    cudaStreamSynchronize(stream);
    cudaStreamDestroy(stream);

    if (!engine)
    {
        std::cerr << "[TensorRT] 错误: 无法创建引擎" << std::endl;
        delete plan;
        delete runtime;
        delete parser;
        delete network;
        delete builder;
        delete cfg;
        return nullptr;
    }

    nvinfer1::IHostMemory* serializedModel = engine->serialize();
    std::string engineFile = onnxFile.substr(0, onnxFile.find_last_of('.')) + ".engine";
    std::ofstream p(engineFile, std::ios::binary);
    if (!p)
    {
        std::cerr << "[TensorRT] 错误: 无法打开文件进行写入: " << engineFile << std::endl;
        delete serializedModel;
        delete engine;
        delete parser;
        delete network;
        delete builder;
        delete cfg;
        return nullptr;
    }
    p.write(static_cast<const char*>(plan->data()), plan->size());
    p.close();

    delete plan;
    delete runtime;
    delete parser;
    delete network;
    delete cfg;
    delete builder;

    std::cout << "[TensorRT] 引擎已构建并保存到文件: " << engineFile << std::endl;
    return engine;
}
#endif