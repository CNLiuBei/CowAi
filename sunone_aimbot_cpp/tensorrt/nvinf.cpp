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
std::string gTrtBuildError = "";
std::mutex gTrtBuildErrorMutex;

// 辅助函数：设置错误信息
static void setTrtBuildError(const std::string& error)
{
    std::lock_guard<std::mutex> lock(gTrtBuildErrorMutex);
    gTrtBuildError = error;
}

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
    std::ifstream file(engineFile, std::ios::binary);
    if (!file.good())
    {
        std::cerr << "[TensorRT] \xE6\x97\xA0\xE6\xB3\x95\xE6\x89\x93\xE5\xBC\x80\xE5\xBC\x95\xE6\x93\x8E\xE6\x96\x87\xE4\xBB\xB6: " << engineFile << std::endl;
        return nullptr;
    }

    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<char> engineData(size);
    file.read(engineData.data(), size);
    file.close();

    nvinfer1::ICudaEngine* engine = runtime->deserializeCudaEngine(engineData.data(), size);
    if (!engine)
    {
        std::cerr << "[TensorRT] \xE5\xBC\x95\xE6\x93\x8E\xE5\x8A\xA0\xE8\xBD\xBD\xE5\xA4\xB1\xE8\xB4\xA5" << std::endl;
        return nullptr;
    }

    return engine;
}

nvinfer1::ICudaEngine* buildEngineFromOnnx(const std::string& onnxFile, nvinfer1::ILogger& logger)
{
    setTrtBuildError("");  // 清除之前的错误
    
    // 确保CUDA设备已初始化
    cudaError_t cudaErr = cudaSetDevice(0);
    if (cudaErr != cudaSuccess)
    {
        std::string err = std::string(u8"CUDA设备错误: ") + cudaGetErrorString(cudaErr);
        setTrtBuildError(err);
        gIsTrtExporting = false;
        return nullptr;
    }

    // 检查ONNX文件是否存在
    std::ifstream checkFile(onnxFile);
    if (!checkFile.good())
    {
        setTrtBuildError(u8"ONNX文件不存在: " + onnxFile);
        gIsTrtExporting = false;
        return nullptr;
    }
    checkFile.close();

    nvinfer1::IBuilder* builder = nvinfer1::createInferBuilder(logger);
    if (!builder)
    {
        setTrtBuildError(u8"无法创建InferBuilder");
        gIsTrtExporting = false;
        return nullptr;
    }

    const auto explicitBatch = 1U << static_cast<uint32_t>(nvinfer1::NetworkDefinitionCreationFlag::kEXPLICIT_BATCH);
    nvinfer1::INetworkDefinition* network = builder->createNetworkV2(explicitBatch);
    if (!network)
    {
        setTrtBuildError(u8"无法创建网络定义");
        delete builder;
        gIsTrtExporting = false;
        return nullptr;
    }

    nvinfer1::IBuilderConfig* cfg = builder->createBuilderConfig();
    if (!cfg)
    {
        setTrtBuildError(u8"无法创建BuilderConfig");
        delete network;
        delete builder;
        gIsTrtExporting = false;
        return nullptr;
    }

    nvonnxparser::IParser* parser = nvonnxparser::createParser(*network, logger);
    if (!parser)
    {
        setTrtBuildError(u8"无法创建ONNX解析器");
        delete cfg;
        delete network;
        delete builder;
        gIsTrtExporting = false;
        return nullptr;
    }

    ImGuiProgressMonitor progressMonitor;
    cfg->setProgressMonitor(&progressMonitor);
    gIsTrtExporting = true;

    if (!parser->parseFromFile(onnxFile.c_str(), static_cast<int>(nvinfer1::ILogger::Severity::kWARNING)))
    {
        std::string errMsg = u8"解析ONNX文件失败";
        for (int i = 0; i < parser->getNbErrors(); ++i)
        {
            errMsg += " | " + std::string(parser->getError(i)->desc());
        }
        setTrtBuildError(errMsg);
        delete parser;
        delete network;
        delete builder;
        delete cfg;
        gIsTrtExporting = false;
        return nullptr;
    }

    nvinfer1::ITensor* inputTensor = network->getInput(0);
    if (!inputTensor)
    {
        setTrtBuildError(u8"无法获取输入张量");
        delete parser;
        delete network;
        delete builder;
        delete cfg;
        gIsTrtExporting = false;
        return nullptr;
    }
    
    const char* inName = inputTensor->getName();
    nvinfer1::Dims inDims = inputTensor->getDimensions();
    int H = (inDims.nbDims >= 4) ? inDims.d[2] : -1;
    int W = (inDims.nbDims >= 4) ? inDims.d[3] : -1;

    // 记录模型输入信息
    std::string inputInfo = u8"输入: " + std::string(inName) + " [";
    for (int i = 0; i < inDims.nbDims; i++)
    {
        inputInfo += std::to_string(inDims.d[i]);
        if (i < inDims.nbDims - 1) inputInfo += ",";
    }
    inputInfo += "]";

    bool fixedByModel = (H > 0 && W > 0);
    bool fixedByConfig = config.fixed_input_size;
    bool makeStatic = fixedByModel || fixedByConfig;

    if (fixedByConfig && (H <= 0 || W <= 0))
        H = W = config.detection_resolution;

    nvinfer1::IOptimizationProfile* profile = builder->createOptimizationProfile();
    if (!profile)
    {
        setTrtBuildError(u8"无法创建优化配置");
        delete parser;
        delete network;
        delete builder;
        delete cfg;
        gIsTrtExporting = false;
        return nullptr;
    }
    
    if (makeStatic)
    {
        nvinfer1::Dims4 d{ 1, 3, H, W };
        profile->setDimensions(inName, nvinfer1::OptProfileSelector::kMIN, d);
        profile->setDimensions(inName, nvinfer1::OptProfileSelector::kOPT, d);
        profile->setDimensions(inName, nvinfer1::OptProfileSelector::kMAX, d);
    }
    else
    {
        profile->setDimensions(inName, nvinfer1::OptProfileSelector::kMIN, nvinfer1::Dims4{ 1, 3, 160, 160 });
        profile->setDimensions(inName, nvinfer1::OptProfileSelector::kOPT, nvinfer1::Dims4{ 1, 3, 320, 320 });
        profile->setDimensions(inName, nvinfer1::OptProfileSelector::kMAX, nvinfer1::Dims4{ 1, 3, 640, 640 });
    }

    if (!profile->isValid())
    {
        setTrtBuildError(u8"优化配置无效 - " + inputInfo);
        delete parser;
        delete network;
        delete builder;
        delete cfg;
        gIsTrtExporting = false;
        return nullptr;
    }

    cfg->addOptimizationProfile(profile);

    // 注意: TensorRT-RTX 不支持 FP16/FP8，跳过这些设置
    // if (config.export_enable_fp16)
    // {
    //     cfg->setFlag(nvinfer1::BuilderFlag::kFP16);
    // }
    // if (config.export_enable_fp8)
    // {
    //     cfg->setFlag(nvinfer1::BuilderFlag::kFP8);
    // }

    cudaStream_t stream;
    cudaStreamCreate(&stream);

    // 构建引擎前显示配置信息
    std::string buildInfo = inputInfo;
    if (makeStatic)
    {
        buildInfo += u8" 静态:" + std::to_string(H) + "x" + std::to_string(W);
    }
    else
    {
        buildInfo += u8" 动态:160-640";
    }

    auto plan = builder->buildSerializedNetwork(*network, *cfg);
    if (!plan)
    {
        cudaStreamDestroy(stream);
        // TensorRT-RTX 不支持构建引擎
        setTrtBuildError(u8"TensorRT-RTX不支持构建引擎，请使用trtexec工具");
        delete parser;
        delete network;
        delete builder;
        delete cfg;
        gIsTrtExporting = false;
        return nullptr;
    }

    nvinfer1::IRuntime* runtime = nvinfer1::createInferRuntime(logger);
    nvinfer1::ICudaEngine* engine = runtime->deserializeCudaEngine(plan->data(), plan->size());

    cudaStreamSynchronize(stream);
    cudaStreamDestroy(stream);

    if (!engine)
    {
        setTrtBuildError(u8"无法创建引擎");
        delete plan;
        delete runtime;
        delete parser;
        delete network;
        delete builder;
        delete cfg;
        gIsTrtExporting = false;
        return nullptr;
    }

    nvinfer1::IHostMemory* serializedModel = engine->serialize();
    std::string engineFile = onnxFile.substr(0, onnxFile.find_last_of('.')) + ".engine";
    std::ofstream p(engineFile, std::ios::binary);
    if (!p)
    {
        setTrtBuildError(u8"无法写入文件: " + engineFile);
        delete serializedModel;
        delete engine;
        delete parser;
        delete network;
        delete builder;
        delete cfg;
        gIsTrtExporting = false;
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