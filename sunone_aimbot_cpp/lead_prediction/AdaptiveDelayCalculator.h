#ifndef ADAPTIVE_DELAY_CALCULATOR_H
#define ADAPTIVE_DELAY_CALCULATOR_H

#include "LeadPredictionTypes.h"

namespace LeadPrediction {

/**
 * @brief 自适应延迟计算器
 * 
 * 根据目标特征和系统状态计算最优射击延迟
 * Δt_fire = base + velocity_comp + stability_comp + fps_comp
 */
class AdaptiveDelayCalculator {
public:
    struct Config {
        double baseDelay = 50.0;              // 基础延迟 (ms)
        double velocityFactor = 0.001;        // 速度因子
        double stabilityFactor = 0.002;       // 稳定度因子
        double fpsCompensationRate = 20.0;    // FPS补偿率 (ms)
        double minDelay = 30.0;               // 最小延迟 (ms)
        double maxDelay = 150.0;              // 最大延迟 (ms)
        int referenceFPS = 144;               // 参考FPS
    };
    
    /**
     * @brief 构造函数
     * @param config 配置
     */
    explicit AdaptiveDelayCalculator(const Config& config = Config());
    
    /**
     * @brief 计算自适应射击延迟
     * @param velocityMagnitude 速度大小 (px/s)
     * @param velocityStdDev 速度标准差（不稳定度）
     * @param currentFPS 当前帧率
     * @return Δt_fire (秒)
     */
    double Calculate(
        double velocityMagnitude,
        double velocityStdDev,
        double currentFPS
    );
    
    /**
     * @brief 获取延迟组成部分（用于调试）
     * @return 延迟组成部分
     */
    DelayComponents GetLastComponents() const { return m_lastComponents; }
    
    /**
     * @brief 获取配置
     * @return 配置引用
     */
    Config& GetConfig() { return m_config; }
    const Config& GetConfig() const { return m_config; }
    
private:
    Config m_config;
    DelayComponents m_lastComponents;
};

} // namespace LeadPrediction

#endif // ADAPTIVE_DELAY_CALCULATOR_H
