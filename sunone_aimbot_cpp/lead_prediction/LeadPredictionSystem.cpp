#include "LeadPredictionSystem.h"
#include <algorithm>

namespace LeadPrediction {

LeadPredictionSystem::LeadPredictionSystem(const LeadPredictionConfig& config)
    : m_config(config)
    , m_velocityBuffer(config.velocity_buffer_size)
    , m_velocityFilter(config.min_tracking_confidence, config.max_realistic_velocity, config.velocity_spike_threshold, config.velocity_smooth_alpha)
    , m_stateMachine()
    , m_delayCalculator()
    , m_leadCalculator()
    , m_smoothTransition(config.alpha_still_to_moving, config.alpha_moving_to_still)
    , m_previousVelocity(0.0, 0.0)
    , m_suddenStopActive(false)
    , m_suddenStopCooldownFrames(0)
    , m_stableFramesCount(0)
{
    // 配置 MotionStateMachine
    auto& smConfig = m_stateMachine.GetConfig();
    smConfig.enterThreshold = config.motion_enter_threshold;
    smConfig.exitThreshold = config.motion_exit_threshold;
    smConfig.minFramesForTransition = config.min_frames_for_transition;
    smConfig.suddenStopVelocityDrop = config.sudden_stop_velocity_drop;
    smConfig.suddenStopDetectionFrames = config.sudden_stop_detection_frames;
    smConfig.suddenStopCooldownFrames = config.sudden_stop_cooldown_frames;
    
    // 配置 AdaptiveDelayCalculator
    auto& delayConfig = m_delayCalculator.GetConfig();
    delayConfig.baseDelay = config.base_fire_delay;
    delayConfig.velocityFactor = config.velocity_factor;
    delayConfig.stabilityFactor = config.stability_factor;
    delayConfig.fpsCompensationRate = config.fps_compensation_rate;
    delayConfig.minDelay = config.min_fire_delay;
    delayConfig.maxDelay = config.max_fire_delay;
    delayConfig.referenceFPS = config.reference_fps;
    
    // 配置 LeadCalculator
    auto& leadConfig = m_leadCalculator.GetConfig();
    leadConfig.lateralFactor = config.lateral_factor;
    leadConfig.verticalFactorUp = config.vertical_factor_up;
    leadConfig.verticalFactorDown = config.vertical_factor_down;
    leadConfig.highVerticalThreshold = config.high_vertical_threshold;
    leadConfig.highVerticalReduction = config.high_vertical_reduction;
    leadConfig.maxLeadX = config.max_lead_x;
    leadConfig.maxLeadY = config.max_lead_y;
    leadConfig.maxLeadRadial = config.max_lead_radial;
}

SystemOutput LeadPredictionSystem::Process(const SystemInput& input) {
    SystemOutput output;
    output.aimPosition = input.kalmanPosition;  // 默认：Kalman 位置
    output.leadAmount = cv::Point2d(0.0, 0.0);
    output.leadApplied = false;
    output.motionState = MotionState::STILL;
    
    // Step 1: 输入验证
    if (!m_velocityFilter.IsValid(input.velocity)) {
        return output;
    }
    
    // Step 2: 速度过滤和平滑
    cv::Point2d filteredVelocity = m_velocityFilter.Filter(
        input.velocity,
        input.confidence,
        m_previousVelocity,
        m_velocityBuffer.GetStandardDeviation()
    );
    
    // Step 3: 更新速度缓冲区
    m_velocityBuffer.Push(filteredVelocity.x, filteredVelocity.y, input.timestamp);
    
    // Step 4: 计算平均速度和稳定度（优化：一次性计算所有统计值）
    cv::Point2d avgVelocity = m_velocityBuffer.GetAverageVelocity();
    double avgMagnitude = m_velocityBuffer.GetAverageMagnitude();
    double stdDev = m_velocityBuffer.GetStandardDeviation();  // 触发缓存计算
    
    // Step 5: 更新运动状态机
    MotionState currentState = m_stateMachine.Update(avgMagnitude);
    output.motionState = currentState;
    
    // Step 6: 检测急停
    bool suddenStop = m_stateMachine.DetectSuddenStop(avgMagnitude);
    
    if (suddenStop && !m_suddenStopActive) {
        m_suddenStopActive = true;
        m_suddenStopCooldownFrames = 0;
        HandleSuddenStop();
    }
    
    // Step 7: 射击触发门控
    if (!input.fireTriggerActive) {
        // 未按下射击键：只跟随 Kalman 位置
        output.aimPosition = input.kalmanPosition;
        m_previousVelocity = filteredVelocity;
        
        // 更新调试信息（简化版，不计算提前量）
        UpdateDebugInfo(input, filteredVelocity, 0.0, cv::Point2d(0, 0), cv::Point2d(0, 0));
        return output;
    }
    
    // Step 8: 计算自适应延迟
    double deltaT = m_delayCalculator.Calculate(
        avgMagnitude,
        stdDev,
        input.currentFPS
    );
    
    // Step 9: 计算提前量
    cv::Point2d rawLead = m_leadCalculator.Calculate(
        avgVelocity.x,
        avgVelocity.y,
        deltaT,
        currentState
    );
    
    // Step 10: 急停处理 - 提前量减半
    if (m_suddenStopActive && m_suddenStopCooldownFrames < 5) {
        rawLead.x *= 0.5;
        rawLead.y *= 0.5;
        m_suddenStopCooldownFrames++;
    }
    
    // Step 11: 平滑过渡
    cv::Point2d smoothedLead = m_smoothTransition.Update(rawLead, currentState);
    
    // Step 12: 应用提前量
    output.aimPosition = input.kalmanPosition + smoothedLead;
    output.leadAmount = smoothedLead;
    output.leadApplied = true;
    
    // 更新状态
    m_previousVelocity = filteredVelocity;
    CheckSuddenStopCooldown();
    
    // 更新调试信息
    UpdateDebugInfo(input, filteredVelocity, deltaT, rawLead, smoothedLead);
    
    return output;
}

void LeadPredictionSystem::Reset() {
    m_velocityBuffer.SetCapacity(m_config.velocity_buffer_size);
    m_velocityBuffer.Clear();
    m_velocityFilter.Reset();
    m_stateMachine.Reset();
    m_smoothTransition.Reset();
    
    m_previousVelocity = cv::Point2d(0.0, 0.0);
    m_suddenStopActive = false;
    m_suddenStopCooldownFrames = 0;
    m_stableFramesCount = 0;
}

void LeadPredictionSystem::UpdateConfig(const LeadPredictionConfig& config) {
    m_config = config;
    
    // 重新创建子组件（使用新配置）
    m_velocityBuffer.SetCapacity(config.velocity_buffer_size);
    m_velocityFilter = VelocityFilter(
        config.min_tracking_confidence,
        config.max_realistic_velocity,
        config.velocity_spike_threshold,
        config.velocity_smooth_alpha
    );
    
    // 更新各组件配置
    auto& smConfig = m_stateMachine.GetConfig();
    smConfig.enterThreshold = config.motion_enter_threshold;
    smConfig.exitThreshold = config.motion_exit_threshold;
    smConfig.minFramesForTransition = config.min_frames_for_transition;
    smConfig.suddenStopVelocityDrop = config.sudden_stop_velocity_drop;
    smConfig.suddenStopDetectionFrames = config.sudden_stop_detection_frames;
    smConfig.suddenStopCooldownFrames = config.sudden_stop_cooldown_frames;
    
    auto& delayConfig = m_delayCalculator.GetConfig();
    delayConfig.baseDelay = config.base_fire_delay;
    delayConfig.velocityFactor = config.velocity_factor;
    delayConfig.stabilityFactor = config.stability_factor;
    delayConfig.fpsCompensationRate = config.fps_compensation_rate;
    delayConfig.minDelay = config.min_fire_delay;
    delayConfig.maxDelay = config.max_fire_delay;
    delayConfig.referenceFPS = config.reference_fps;
    
    auto& leadConfig = m_leadCalculator.GetConfig();
    leadConfig.lateralFactor = config.lateral_factor;
    leadConfig.verticalFactorUp = config.vertical_factor_up;
    leadConfig.verticalFactorDown = config.vertical_factor_down;
    leadConfig.highVerticalThreshold = config.high_vertical_threshold;
    leadConfig.highVerticalReduction = config.high_vertical_reduction;
    leadConfig.maxLeadX = config.max_lead_x;
    leadConfig.maxLeadY = config.max_lead_y;
    leadConfig.maxLeadRadial = config.max_lead_radial;
    
    m_smoothTransition = SmoothTransition(
        config.alpha_still_to_moving,
        config.alpha_moving_to_still
    );
}

void LeadPredictionSystem::HandleSuddenStop() {
    // 急停处理：
    // 1. 提前量立即减少 50%（在 Process 中处理）
    // 2. 状态转换加速（调整阈值）
    // 3. 临时调整退出阈值
    
    // 临时增加退出阈值 20%，持续 10 帧
    m_stateMachine.TemporarilyIncreaseExitThreshold(1.2, 10);
}

void LeadPredictionSystem::CheckSuddenStopCooldown() {
    if (!m_suddenStopActive) {
        return;
    }
    
    // 检查速度是否稳定
    double avgMagnitude = m_velocityBuffer.GetAverageMagnitude();
    double stdDev = m_velocityBuffer.GetStandardDeviation();
    
    if (avgMagnitude < m_config.motion_exit_threshold && stdDev < 10.0) {
        m_stableFramesCount++;
    } else {
        m_stableFramesCount = 0;
    }
    
    // 10 帧稳定后重置急停标志
    if (m_stableFramesCount >= 10) {
        m_suddenStopActive = false;
        m_suddenStopCooldownFrames = 0;
        m_stableFramesCount = 0;
    }
}

void LeadPredictionSystem::UpdateDebugInfo(
    const SystemInput& input,
    const cv::Point2d& filteredVelocity,
    double deltaT,
    const cv::Point2d& rawLead,
    const cv::Point2d& smoothedLead
) {
    m_debugInfo.motionState = m_stateMachine.GetState();
    m_debugInfo.state = m_stateMachine.GetState();
    m_debugInfo.velocityMagnitude = m_velocityBuffer.GetAverageMagnitude();
    m_debugInfo.avgVelocityMagnitude = m_velocityBuffer.GetAverageMagnitude();
    m_debugInfo.velocityStdDev = m_velocityBuffer.GetStandardDeviation();
    m_debugInfo.rawVelocity = input.velocity;  // 保存原始输入速度
    m_debugInfo.fireDelay = deltaT;
    m_debugInfo.rawLead = rawLead;
    m_debugInfo.smoothedLead = smoothedLead;
    m_debugInfo.suddenStopActive = m_suddenStopActive;
    m_debugInfo.framesInState = m_stateMachine.GetFramesInState();
    m_debugInfo.stateDuration = m_stateMachine.GetFramesInState() / input.currentFPS;
    m_debugInfo.transitionCounter = 0;  // TODO: 实现状态转换计数
    m_debugInfo.stateTransitionsPerMinute = 0;  // TODO: 实现每分钟转换次数
    m_debugInfo.processingTime = 0.0;  // 不计算处理时间，节省CPU
}

} // namespace LeadPrediction

