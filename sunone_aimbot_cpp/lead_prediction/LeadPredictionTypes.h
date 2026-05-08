#ifndef LEAD_PREDICTION_TYPES_H
#define LEAD_PREDICTION_TYPES_H

#include <chrono>
#include <opencv2/opencv.hpp>

namespace LeadPrediction {

/**
 * @brief 运动状态枚举
 */
enum class MotionState {
    STILL,    // 静止
    MOVING    // 移动
};

/**
 * @brief 速度样本
 */
struct VelocitySample {
    double vx;  // X轴速度 (px/s)
    double vy;  // Y轴速度 (px/s)
    std::chrono::steady_clock::time_point timestamp;  // 时间戳
    
    VelocitySample() : vx(0.0), vy(0.0), timestamp(std::chrono::steady_clock::now()) {}
    VelocitySample(double x, double y, std::chrono::steady_clock::time_point t)
        : vx(x), vy(y), timestamp(t) {}
};

/**
 * @brief 延迟组成部分（用于调试）
 */
struct DelayComponents {
    double base;        // 基础延迟 (ms)
    double velocity;    // 速度补偿 (ms)
    double stability;   // 稳定度补偿 (ms)
    double fps;         // FPS补偿 (ms)
    double total;       // 总延迟 (ms)
    
    DelayComponents() : base(0.0), velocity(0.0), stability(0.0), fps(0.0), total(0.0) {}
};

/**
 * @brief 系统输入
 */
struct SystemInput {
    cv::Point2d kalmanPosition;     // 卡尔曼预测位置
    cv::Point2d velocity;           // 速度 (px/s)
    double confidence;              // 跟踪置信度 [0, 1]
    bool fireTriggerActive;         // 射击触发器状态
    double currentFPS;              // 当前帧率
    std::chrono::steady_clock::time_point timestamp;  // 时间戳
    
    SystemInput()
        : kalmanPosition(0.0, 0.0)
        , velocity(0.0, 0.0)
        , confidence(0.0)
        , fireTriggerActive(false)
        , currentFPS(60.0)
        , timestamp(std::chrono::steady_clock::now())
    {}
};

/**
 * @brief 系统输出
 */
struct SystemOutput {
    cv::Point2d aimPosition;        // 最终瞄准位置
    cv::Point2d leadAmount;         // 应用的提前量 (像素)
    MotionState motionState;        // 运动状态
    double fireDelay;               // 使用的射击延迟 (秒)
    bool leadApplied;               // 是否应用了提前量
    
    SystemOutput()
        : aimPosition(0.0, 0.0)
        , leadAmount(0.0, 0.0)
        , motionState(MotionState::STILL)
        , fireDelay(0.0)
        , leadApplied(false)
    {}
};

/**
 * @brief 调试信息
 */
struct DebugInfo {
    double processingTime;          // 处理时间 (ms)
    MotionState motionState;        // 运动状态
    double velocityMagnitude;       // 速度大小 (px/s)
    double velocityStdDev;          // 速度标准差
    cv::Point2d rawVelocity;        // 原始输入速度 (px/s) - 调试用
    bool suddenStopActive;          // 急停标志
    double stateDuration;           // 状态持续时间 (秒)
    int transitionCounter;          // 状态转换计数
    double avgVelocityMagnitude;    // 平均速度大小 (px/s)
    MotionState state;              // 当前状态
    int framesInState;              // 当前状态持续帧数
    double fireDelay;               // 射击延迟 (秒)
    cv::Point2d rawLead;            // 原始提前量
    cv::Point2d smoothedLead;       // 平滑后提前量
    int stateTransitionsPerMinute;  // 每分钟状态转换次数
    
    DebugInfo()
        : processingTime(0.0)
        , motionState(MotionState::STILL)
        , velocityMagnitude(0.0)
        , velocityStdDev(0.0)
        , rawVelocity(0.0, 0.0)
        , suddenStopActive(false)
        , stateDuration(0.0)
        , transitionCounter(0)
        , avgVelocityMagnitude(0.0)
        , state(MotionState::STILL)
        , framesInState(0)
        , fireDelay(0.0)
        , rawLead(0.0, 0.0)
        , smoothedLead(0.0, 0.0)
        , stateTransitionsPerMinute(0)
    {}
};

} // namespace LeadPrediction

#endif // LEAD_PREDICTION_TYPES_H
