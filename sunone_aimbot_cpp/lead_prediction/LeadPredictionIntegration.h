#pragma once

#include "LeadPredictionSystem.h"
#include "../mouse/AimbotTarget.h"
#include <chrono>
#include <memory>

namespace LeadPrediction {

/**
 * @brief 集成辅助类 - 简化 LeadPredictionSystem 在主循环中的使用
 * 
 * 功能：
 * - 管理 LeadPredictionSystem 实例
 * - 处理配置加载和更新
 * - 提供简单的接口用于应用提前量
 */
class LeadPredictionIntegration {
public:
    /**
     * @brief 构造函数
     * @param configFile 配置文件路径
     */
    explicit LeadPredictionIntegration(const std::string& configFile = "config.ini");
    
    /**
     * @brief 应用提前量到目标
     * @param target 目标对象（会修改 pivotX, pivotY, leadX, leadY, leadApplied）
     * @param fireTriggerActive 射击触发是否激活
     * @param currentFPS 当前 FPS
     * @param trackingConfidence 跟踪置信度 [0, 1]
     * @return true 如果成功应用提前量
     */
    bool ApplyLeadPrediction(
        AimbotTarget* target,
        bool fireTriggerActive,
        double currentFPS,
        double trackingConfidence = 0.9
    );
    
    /**
     * @brief 重新加载配置
     */
    void ReloadConfig();
    
    /**
     * @brief 启用/禁用提前量预测
     */
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    
    /**
     * @brief 检查是否启用
     */
    bool IsEnabled() const { return m_enabled; }
    
    /**
     * @brief 获取调试信息
     */
    DebugInfo GetDebugInfo() const;
    
    /**
     * @brief 重置系统状态
     */
    void Reset();

private:
    std::unique_ptr<LeadPredictionSystem> m_system;
    LeadPredictionConfig m_config;
    std::string m_configFile;
    bool m_enabled;
    bool m_initialized;
};

} // namespace LeadPrediction

