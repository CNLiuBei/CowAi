#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <winsock2.h>
#include <Windows.h>

#include "imgui/imgui.h"

#include "sunone_aimbot_cpp.h"
#include "include/other_tools.h"
#include "overlay.h"
#ifdef USE_CUDA
#include "trt_monitor.h"
#include "nvinf.h"
#include <thread>
#include <chrono>

extern std::atomic<bool> detectionPaused;
#endif

float prev_confidence_threshold = config.confidence_threshold;
float prev_nms_threshold = config.nms_threshold;
int prev_max_detections = config.max_detections;

static bool wasExporting = false;
static int selectedOnnxIndex = 0;
static std::string trtGenStatus = "";
static bool trtGenInProgress = false;

void draw_ai()
{
#ifdef USE_CUDA
    if (gIsTrtExporting)
    {
        ImGui::OpenPopup("TensorRT Export Progress");
    }

    if (ImGui::BeginPopupModal("TensorRT 导出进度", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        std::lock_guard<std::mutex> lock(gProgressMutex);
        if (!gProgressPhases.empty())
        {
            for (auto& [name, phase] : gProgressPhases)
            {
                float percent = phase.max > 0 ? phase.current / float(phase.max) : 0.0f;
                ImGui::Text("%s: %d/%d", name.c_str(), phase.current, phase.max);
                ImGui::ProgressBar(percent, ImVec2(300, 0));
            }
        }
        else
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::Text("引擎导出中，请稍候...");
        ImGui::EndPopup();
    }
#endif
    std::vector<std::string> availableModels = getAvailableModels(false);
    if (availableModels.empty())
    {
        ImGui::Text("'models' 文件夹中没有可用模型。");
    }
    else
    {
        int currentModelIndex = 0;
        auto it = std::find(availableModels.begin(), availableModels.end(), config.ai_model);

        if (it != availableModels.end())
        {
            currentModelIndex = static_cast<int>(std::distance(availableModels.begin(), it));
        }

        std::vector<const char*> modelsItems;
        modelsItems.reserve(availableModels.size());

        for (const auto& modelName : availableModels)
        {
            modelsItems.push_back(modelName.c_str());
        }

        if (ImGui::Combo(u8"模型", &currentModelIndex, modelsItems.data(), static_cast<int>(modelsItems.size())))
        {
            if (config.ai_model != availableModels[currentModelIndex])
            {
                config.ai_model = availableModels[currentModelIndex];
                config.saveConfig();
                detector_model_changed.store(true);
            }
        }
    }

    ImGui::Separator();

#ifdef USE_CUDA
    // Backend is always TensorRT in CUDA build
    ImGui::Separator();
#endif

    ImGui::Separator();
    ImGui::SliderFloat(u8"置信度阈值", &config.confidence_threshold, 0.01f, 1.00f, "%.2f");
    ImGui::SliderFloat(u8"NMS 阈值", &config.nms_threshold, 0.00f, 1.00f, "%.2f");
    ImGui::SliderInt(u8"最大检测数", &config.max_detections, 1, 100);

    if (ImGui::Checkbox(u8"固定模型尺寸", &config.fixed_input_size))
    {
        capture_method_changed.store(true);
        config.saveConfig();
        detector_model_changed.store(true);
    }
        
    if (prev_confidence_threshold != config.confidence_threshold ||
        prev_nms_threshold != config.nms_threshold ||
        prev_max_detections != config.max_detections)
    {
        prev_nms_threshold = config.nms_threshold;
        prev_confidence_threshold = config.confidence_threshold;
        prev_max_detections = config.max_detections;
        config.saveConfig();
    }

#ifdef USE_CUDA
    ImGui::Separator();
    ImGui::Text(u8"TensorRT 引擎生成");
    
    // 获取可用的ONNX模型
    std::vector<std::string> onnxModels = getOnnxFiles();
    
    if (onnxModels.empty())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), u8"'models' 文件夹中没有ONNX模型");
    }
    else
    {
        // 确保索引有效
        if (selectedOnnxIndex >= static_cast<int>(onnxModels.size()))
            selectedOnnxIndex = 0;
        
        std::vector<const char*> onnxItems;
        onnxItems.reserve(onnxModels.size());
        for (const auto& model : onnxModels)
        {
            onnxItems.push_back(model.c_str());
        }
        
        ImGui::Combo(u8"ONNX模型", &selectedOnnxIndex, onnxItems.data(), static_cast<int>(onnxItems.size()));
        
        // 检查是否已有对应的engine文件
        std::string selectedOnnx = onnxModels[selectedOnnxIndex];
        std::string enginePath = "models/" + selectedOnnx.substr(0, selectedOnnx.find_last_of('.')) + ".engine";
        bool engineExists = fileExists(enginePath);
        
        if (engineExists)
        {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), u8"已存在对应的engine文件");
        }
        
        // 生成按钮
        bool canGenerate = !trtGenInProgress && !gIsTrtExporting;
        if (!canGenerate)
        {
            ImGui::BeginDisabled();
        }
        
        if (ImGui::Button(u8"生成TRT引擎"))
        {
            trtGenInProgress = true;
            trtGenStatus = u8"正在准备...";
            
            std::string onnxPath = "models/" + onnxModels[selectedOnnxIndex];
            
            // 暂停检测并触发检测器重新加载以释放资源
            detectionPaused.store(true);
            detector_model_changed.store(true);
            
            // 在后台线程中生成引擎
            std::thread([onnxPath]() {
                // 等待检测器释放资源
                std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                
                trtGenStatus = u8"正在生成(可能需要几分钟)...";
                
                nvinfer1::ICudaEngine* engine = buildEngineFromOnnx(onnxPath, gLogger);
                if (engine)
                {
                    trtGenStatus = u8"生成成功!";
                    delete engine;
                }
                else
                {
                    std::lock_guard<std::mutex> lock(gTrtBuildErrorMutex);
                    if (!gTrtBuildError.empty())
                    {
                        trtGenStatus = u8"生成失败: " + gTrtBuildError;
                    }
                    else
                    {
                        trtGenStatus = u8"生成失败!";
                    }
                }
                
                // 恢复检测
                detectionPaused.store(false);
                detector_model_changed.store(true);
                trtGenInProgress = false;
            }).detach();
        }
        
        if (!canGenerate)
        {
            ImGui::EndDisabled();
        }
        
        ImGui::SameLine();
        if (ImGui::Button(u8"重新生成(覆盖)") && canGenerate)
        {
            // 先删除现有的engine文件
            if (engineExists)
            {
                DeleteFileA(enginePath.c_str());
            }
            
            trtGenInProgress = true;
            trtGenStatus = u8"正在准备...";
            
            std::string onnxPath = "models/" + onnxModels[selectedOnnxIndex];
            
            // 暂停检测并触发检测器重新加载以释放资源
            detectionPaused.store(true);
            detector_model_changed.store(true);
            
            std::thread([onnxPath]() {
                // 等待检测器释放资源
                std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                
                trtGenStatus = u8"正在重新生成(可能需要几分钟)...";
                
                nvinfer1::ICudaEngine* engine = buildEngineFromOnnx(onnxPath, gLogger);
                if (engine)
                {
                    trtGenStatus = u8"重新生成成功!";
                    delete engine;
                }
                else
                {
                    std::lock_guard<std::mutex> lock(gTrtBuildErrorMutex);
                    if (!gTrtBuildError.empty())
                    {
                        trtGenStatus = u8"重新生成失败: " + gTrtBuildError;
                    }
                    else
                    {
                        trtGenStatus = u8"重新生成失败!";
                    }
                }
                
                // 恢复检测
                detectionPaused.store(false);
                detector_model_changed.store(true);
                trtGenInProgress = false;
            }).detach();
        }
        
        // 显示状态
        if (!trtGenStatus.empty())
        {
            if (trtGenStatus.find(u8"成功") != std::string::npos)
            {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s", trtGenStatus.c_str());
            }
            else if (trtGenStatus.find(u8"失败") != std::string::npos)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", trtGenStatus.c_str());
            }
            else
            {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s", trtGenStatus.c_str());
            }
        }
    }
#endif
}