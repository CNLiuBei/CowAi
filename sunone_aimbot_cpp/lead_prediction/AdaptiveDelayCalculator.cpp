#include "AdaptiveDelayCalculator.h"
#include <algorithm>

namespace LeadPrediction {

AdaptiveDelayCalculator::AdaptiveDelayCalculator(const Config& config)
    : m_config(config)
{
}

double AdaptiveDelayCalculator::Calculate(
    double velocityMagnitude,
    double velocityStdDev,
    double currentFPS
) {
    // 基础延迟
    double baseDelay = m_config.baseDelay;
    
    // 速度补偿（高速增加延迟）
    double velocityComp = velocityMagnitude * m_config.velocityFactor;
    
    // 稳定度补偿（不稳定减少延迟）
    double stabilityComp = -velocityStdDev * m_config.stabilityFactor;
    
    // FPS补偿（低FPS增加延迟）
    double fpsComp = 0.0;
    if (currentFPS < m_config.referenceFPS) {
        fpsComp = (m_config.referenceFPS - currentFPS) / m_config.referenceFPS 
                  * m_config.fpsCompensationRate;
    }
    
    // 总延迟
    double totalDelay = baseDelay + velocityComp + stabilityComp + fpsComp;
    
    // 裁剪到有效范围
    totalDelay = std::clamp(totalDelay, m_config.minDelay, m_config.maxDelay);
    
    // 保存组成部分（用于调试）
    m_lastComponents.base = baseDelay;
    m_lastComponents.velocity = velocityComp;
    m_lastComponents.stability = stabilityComp;
    m_lastComponents.fps = fpsComp;
    m_lastComponents.total = totalDelay;
    
    // 转换为秒
    return totalDelay / 1000.0;
}

} // namespace LeadPrediction
