// draw_lead_prediction.cpp - Lead Prediction UI
#pragma execution_character_set("utf-8")
#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <winsock2.h>
#include <Windows.h>

#include "imgui/imgui.h"
#include "config.h"
#include "sunone_aimbot_cpp.h"
//#include "../lead_prediction/LeadPredictionIntegration.h"  // 已禁用

extern Config config;
//extern LeadPrediction::LeadPredictionIntegration* leadPrediction;  // 已禁用

void draw_lead_prediction_settings()
{
    ImGui::SeparatorText("\xE5\xB0\x84\xE5\x87\xBB\xE6\x8F\x90\xE5\x89\x8D\xE9\x87\x8F\xE9\xA2\x84\xE6\xB5\x8B");  // 射击提前量预测
    
    // 功能已禁用
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "\xE6\x8F\x90\xE5\x89\x8D\xE9\x87\x8F\xE7\xB3\xBB\xE7\xBB\x9F\xE5\xB7\xB2\xE7\xA6\x81\xE7\x94\xA8");  // 提前量系统已禁用
    return;
    
    /*
    if (!leadPrediction)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "\xE6\x8F\x90\xE5\x89\x8D\xE9\x87\x8F\xE7\xB3\xBB\xE7\xBB\x9F\xE6\x9C\xAA\xE5\x88\x9D\xE5\xA7\x8B\xE5\x8C\x96");  // 提前量系统未初始化
        return;
    }
    
    // 显示 Tracker 状态
    ImGui::SeparatorText("\xE8\xB7\x9F\xE8\xB8\xAA\xE5\x99\xA8\xE7\x8A\xB6\xE6\x80\x81");  // 跟踪器状态
    if (config.enable_bytetrack)
    {
        const char* trackerType = config.use_botsort ? "BoTSORT" : "ByteTracker";
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "\xE8\xB7\x9F\xE8\xB8\xAA\xE5\x99\xA8: %s (\xE5\xB7\xB2\xE5\x90\xAF\xE7\x94\xA8)", trackerType);  // 跟踪器: XXX (已启用)
    }
    else
    {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "\xE8\xB7\x9F\xE8\xB8\xAA\xE5\x99\xA8: \xE6\x9C\xAA\xE5\x90\xAF\xE7\x94\xA8");  // 跟踪器: 未启用
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "\xE2\x9A\xA0 \xE6\x8F\x90\xE5\x89\x8D\xE9\x87\x8F\xE9\x9C\x80\xE8\xA6\x81\xE5\x90\xAF\xE7\x94\xA8\xE8\xB7\x9F\xE8\xB8\xAA\xE5\x99\xA8\xE6\x89\x8D\xE8\x83\xBD\xE8\xAE\xA1\xE7\xAE\x97\xE9\x80\x9F\xE5\xBA\xA6");  // ⚠ 提前量需要启用跟踪器才能计算速度
    }
    
    // 启用/禁用开关
    ImGui::Separator();
    bool enabled = leadPrediction->IsEnabled();
    if (ImGui::Checkbox("\xE5\x90\xAF\xE7\x94\xA8\xE6\x8F\x90\xE5\x89\x8D\xE9\x87\x8F\xE9\xA2\x84\xE6\xB5\x8B", &enabled))  // 启用提前量预测
    {
        leadPrediction->SetEnabled(enabled);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("\xE6\xA0\xB9\xE6\x8D\xAE\xE7\x9B\xAE\xE6\xA0\x87\xE9\x80\x9F\xE5\xBA\xA6\xE8\x87\xAA\xE5\x8A\xA8\xE8\xAE\xA1\xE7\xAE\x97\xE5\xB0\x84\xE5\x87\xBB\xE6\x8F\x90\xE5\x89\x8D\xE9\x87\x8F");  // 根据目标速度自动计算射击提前量
        ImGui::Text("\xE9\x80\x82\xE7\x94\xA8\xE4\xBA\x8E\xE7\xA7\xBB\xE5\x8A\xA8\xE7\x9B\xAE\xE6\xA0\x87\xE7\x9A\x84\xE7\xB2\xBE\xE7\xA1\xAE\xE7\x9E\x84\xE5\x87\x86");  // 适用于移动目标的精确瞄准
        ImGui::EndTooltip();
    }
    
    if (!enabled)
    {
        ImGui::TextDisabled("\xEF\xBC\x88\xE7\xB3\xBB\xE7\xBB\x9F\xE5\xB7\xB2\xE7\xA6\x81\xE7\x94\xA8\xEF\xBC\x89");  // (系统已禁用)
        return;
    }
    
    // 获取调试信息
    auto debugInfo = leadPrediction->GetDebugInfo();
    
    // 状态显示
    ImGui::SeparatorText("\xE5\xBD\x93\xE5\x89\x8D\xE7\x8A\xB6\xE6\x80\x81");  // 当前状态
    
    const char* stateText = (debugInfo.motionState == LeadPrediction::MotionState::STILL) ? "\xE9\x9D\x99\xE6\xAD\xA2" : "\xE7\xA7\xBB\xE5\x8A\xA8";  // 静止 : 移动
    ImGui::Text("\xE8\xBF\x90\xE5\x8A\xA8\xE7\x8A\xB6\xE6\x80\x81: %s", stateText);  // 运动状态
    ImGui::Text("\xE9\x80\x9F\xE5\xBA\xA6\xE5\xA4\xA7\xE5\xB0\x8F: %.1f px/s", debugInfo.velocityMagnitude);  // 速度大小
    
    // 显示原始速度输入（调试用）
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "\xE5\x8E\x9F\xE5\xA7\x8B\xE9\x80\x9F\xE5\xBA\xA6: (%.2f, %.2f) px/s", debugInfo.rawVelocity.x, debugInfo.rawVelocity.y);  // 原始速度
    
    ImGui::Text("\xE9\x80\x9F\xE5\xBA\xA6\xE6\xA0\x87\xE5\x87\x86\xE5\xB7\xAE: %.1f", debugInfo.velocityStdDev);  // 速度标准差
    ImGui::Text("\xE5\xB0\x84\xE5\x87\xBB\xE5\xBB\xB6\xE8\xBF\x9F: %.1f ms", debugInfo.fireDelay * 1000.0);  // 射击延迟
    ImGui::Text("\xE6\x8F\x90\xE5\x89\x8D\xE9\x87\x8F: (%.1f, %.1f) px", debugInfo.smoothedLead.x, debugInfo.smoothedLead.y);  // 提前量
    ImGui::Text("\xE5\xA4\x84\xE7\x90\x86\xE6\x97\xB6\xE9\x97\xB4: %.3f ms", debugInfo.processingTime);  // 处理时间
    
    if (debugInfo.suddenStopActive)
    {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "[\xE6\x80\xA5\xE5\x81\x9C\xE6\xA3\x80\xE6\xB5\x8B]");  // [急停检测]
    }
    
    // 操作按钮
    ImGui::Separator();
    
    if (ImGui::Button("\xE9\x87\x8D\xE7\xBD\xAE\xE7\xB3\xBB\xE7\xBB\x9F"))  // 重置系统
    {
        leadPrediction->Reset();
    }
    ImGui::SameLine();
    
    if (ImGui::Button("\xE9\x87\x8D\xE6\x96\xB0\xE5\x8A\xA0\xE8\xBD\xBD\xE9\x85\x8D\xE7\xBD\xAE"))  // 重新加载配置
    {
        leadPrediction->ReloadConfig();
    }
    */
}
