#pragma once

#include "LeadPredictionTypes.h"
#include "LeadPredictionConfig.h"
#include "VelocityBuffer.h"
#include "VelocityFilter.h"
#include "MotionStateMachine.h"
#include "AdaptiveDelayCalculator.h"
#include "LeadCalculator.h"
#include "SmoothTransition.h"
#include <opencv2/core.hpp>
#include <chrono>

namespace LeadPrediction {

/**
 * @brief 射击提前量预测系统 - 主集成类
 * 
 * 功能：
 * - 集成所有子组件
 * - 实现完整的 12 步处理流程
 * - 处理射击触发门控
 * - 处理急停场景
 * - 提供调试信息
 * 
 * 设计要求：
 * - Requirements: 所有需求
 * - Property 13-15, 22-25: 系统级属性
 * - 性能: < 0.5ms 处理时间
 */
class LeadPredictionSystem {
public:
    /**
     * @brief 构造函数
     * @param config 配置参数
     */
    explicit LeadPredictionSystem(const LeadPredictionConfig& config);
    
    /**
     * @brief 处理一帧数据
     * @param input 系统输入
     * @return 系统输出（最终瞄准位置）
     */
    SystemOutput Process(const SystemInput& input);
    
    /**
     * @brief 重置系统状态
     */
    void Reset();
    
    /**
     * @brief 获取调试信息
     */
    DebugInfo GetDebugInfo() const { return m_debugInfo; }
    
    /**
     * @brief 更新配置
     */
    void UpdateConfig(const LeadPredictionConfig& config);
    
    /**
     * @brief 获取当前配置
     */
    const LeadPredictionConfig& GetConfig() const { return m_config; }

private:
    /**
     * @brief 处理急停场景
     */
    void HandleSuddenStop();
    
    /**
     * @brief 更新调试信息
     */
    void UpdateDebugInfo(
        const SystemInput& input,
        const cv::Point2d& filteredVelocity,
        double deltaT,
        const cv::Point2d& rawLead,
        const cv::Point2d& smoothedLead
    );
    
    /**
     * @brief 检查是否需要重置急停状态
     */
    void CheckSuddenStopCooldown();

private:
    // 配置
    LeadPredictionConfig m_config;
    
    // 子组件
    VelocityBuffer m_velocityBuffer;
    VelocityFilter m_velocityFilter;
    MotionStateMachine m_stateMachine;
    AdaptiveDelayCalculator m_delayCalculator;
    LeadCalculator m_leadCalculator;
    SmoothTransition m_smoothTransition;
    
    // 状态
    cv::Point2d m_previousVelocity;      ///< 上一帧速度
    bool m_suddenStopActive;             ///< 急停标志
    int m_suddenStopCooldownFrames;      ///< 急停冷却帧数
    int m_stableFramesCount;             ///< 稳定帧计数
    
    // 调试信息
    DebugInfo m_debugInfo;
    
    // 性能监控
    std::chrono::steady_clock::time_point m_lastProcessTime;
};

} // namespace LeadPrediction

