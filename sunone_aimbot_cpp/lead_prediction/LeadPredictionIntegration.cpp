#include "LeadPredictionIntegration.h"
#include <iostream>

namespace LeadPrediction {

LeadPredictionIntegration::LeadPredictionIntegration(const std::string& configFile)
    : m_configFile(configFile)
    , m_enabled(false)
    , m_initialized(false)
{
    // 加载配置
    if (m_config.LoadFromFile(configFile)) {
        m_enabled = m_config.enabled;
        m_system = std::make_unique<LeadPredictionSystem>(m_config);
        m_initialized = true;
    } else {
        std::cerr << "LeadPrediction: Failed to load config from " << configFile << std::endl;
        // 使用默认配置
        m_system = std::make_unique<LeadPredictionSystem>(m_config);
        m_initialized = true;
    }
}

bool LeadPredictionIntegration::ApplyLeadPrediction(
    AimbotTarget* target,
    bool fireTriggerActive,
    double currentFPS,
    double trackingConfidence
) {
    if (!m_initialized || !m_enabled || !target) {
        return false;
    }
    
    // 准备输入
    SystemInput input;
    input.kalmanPosition = cv::Point2d(target->pivotX, target->pivotY);
    
    // 将 pixels/frame 转换为 pixels/second
    // velocityX/Y 是 pixels per frame，需要乘以 FPS
    input.velocity = cv::Point2d(
        target->velocityX * currentFPS,
        target->velocityY * currentFPS
    );
    
    input.confidence = trackingConfidence;
    input.fireTriggerActive = fireTriggerActive;
    input.currentFPS = currentFPS;
    input.timestamp = std::chrono::steady_clock::now();
    
    // 处理
    SystemOutput output = m_system->Process(input);
    
    // 应用结果
    target->pivotX = output.aimPosition.x;
    target->pivotY = output.aimPosition.y;
    target->leadX = output.leadAmount.x;
    target->leadY = output.leadAmount.y;
    target->leadApplied = output.leadApplied;
    
    return true;
}

void LeadPredictionIntegration::ReloadConfig() {
    if (m_config.LoadFromFile(m_configFile)) {
        m_enabled = m_config.enabled;
        if (m_system) {
            m_system->UpdateConfig(m_config);
        }
    }
}

DebugInfo LeadPredictionIntegration::GetDebugInfo() const {
    if (m_system) {
        return m_system->GetDebugInfo();
    }
    return DebugInfo();
}

void LeadPredictionIntegration::Reset() {
    if (m_system) {
        m_system->Reset();
    }
}

} // namespace LeadPrediction

