#ifndef MOTION_STATE_MACHINE_H
#define MOTION_STATE_MACHINE_H

#include "LeadPredictionTypes.h"
#include "LeadPredictionConfig.h"

namespace LeadPrediction {

/**
 * @brief 运动状态机 - 使用迟滞阈值判定静止/移动状态
 * 
 * 防止状态频繁抖动，提供稳定的状态判定
 */
class MotionStateMachine {
public:
    struct Config {
        double enterThreshold = 30.0;      // 进入 MOVING 的速度阈值 (px/s)
        double exitThreshold = 15.0;       // 退出 MOVING 的速度阈值 (px/s)
        int minFramesForTransition = 3;    // 状态切换所需最小帧数
        
        // 急停检测
        double suddenStopVelocityDrop = 0.8;    // 速度下降80%
        int suddenStopDetectionFrames = 2;       // 连续检测帧数
        int suddenStopCooldownFrames = 10;       // 冷却时间
    };
    
    /**
     * @brief 构造函数
     * @param config 配置
     */
    explicit MotionStateMachine(const Config& config = Config());
    
    /**
     * @brief 更新状态机
     * @param avgVelocityMagnitude 平均速度大小 (px/s)
     * @return 当前状态
     */
    MotionState Update(double avgVelocityMagnitude);
    
    /**
     * @brief 获取当前状态
     * @return 当前状态
     */
    MotionState GetState() const { return m_currentState; }
    
    /**
     * @brief 获取状态持续帧数
     * @return 帧数
     */
    int GetFramesInState() const { return m_framesInState; }
    
    /**
     * @brief 检测急停
     * @param currentVelocity 当前速度大小 (px/s)
     * @return true 如果检测到急停
     */
    bool DetectSuddenStop(double currentVelocity);
    
    /**
     * @brief 重置状态机
     */
    void Reset();
    
    /**
     * @brief 获取配置
     * @return 配置引用
     */
    Config& GetConfig() { return m_config; }
    const Config& GetConfig() const { return m_config; }
    
    /**
     * @brief 临时提高退出阈值（用于急停）
     * @param multiplier 倍数
     * @param frames 持续帧数
     */
    void TemporarilyIncreaseExitThreshold(double multiplier, int frames);
    
private:
    Config m_config;
    MotionState m_currentState;
    int m_framesInState;
    int m_transitionCounter;  // 过渡计数器
    
    // 急停检测
    double m_lastVelocity;
    std::vector<double> m_recentVelocities;  // 最近的速度记录
    int m_suddenStopCooldown;
    
    // 临时阈值调整
    double m_tempExitThresholdMultiplier;
    int m_tempThresholdFrames;
};

} // namespace LeadPrediction

#endif // MOTION_STATE_MACHINE_H
