#include "MotionStateMachine.h"
#include <algorithm>
#include <cmath>

namespace LeadPrediction {

MotionStateMachine::MotionStateMachine(const Config& config)
    : m_config(config)
    , m_currentState(MotionState::STILL)
    , m_framesInState(0)
    , m_transitionCounter(0)
    , m_lastVelocity(0.0)
    , m_suddenStopCooldown(0)
    , m_tempExitThresholdMultiplier(1.0)
    , m_tempThresholdFrames(0)
{
    m_recentVelocities.reserve(m_config.suddenStopDetectionFrames);
}

MotionState MotionStateMachine::Update(double avgVelocityMagnitude) {
    MotionState nextState = m_currentState;
    
    // 计算有效的退出阈值（可能被临时调整）
    double effectiveExitThreshold = m_config.exitThreshold * m_tempExitThresholdMultiplier;
    
    // 状态转换逻辑（带迟滞）
    if (m_currentState == MotionState::STILL) {
        // STILL → MOVING
        if (avgVelocityMagnitude > m_config.enterThreshold) {
            m_transitionCounter++;
            if (m_transitionCounter >= m_config.minFramesForTransition) {
                nextState = MotionState::MOVING;
                m_transitionCounter = 0;
            }
        } else {
            m_transitionCounter = 0;
        }
    } else {  // MOVING
        // MOVING → STILL
        if (avgVelocityMagnitude < effectiveExitThreshold) {
            m_transitionCounter++;
            if (m_transitionCounter >= m_config.minFramesForTransition) {
                nextState = MotionState::STILL;
                m_transitionCounter = 0;
            }
        } else {
            m_transitionCounter = 0;
        }
    }
    
    // 状态切换
    if (nextState != m_currentState) {
        m_currentState = nextState;
        m_framesInState = 0;
    } else {
        m_framesInState++;
    }
    
    // 更新临时阈值
    if (m_tempThresholdFrames > 0) {
        m_tempThresholdFrames--;
        if (m_tempThresholdFrames == 0) {
            m_tempExitThresholdMultiplier = 1.0;
        }
    }
    
    // 更新急停冷却
    if (m_suddenStopCooldown > 0) {
        m_suddenStopCooldown--;
    }
    
    return m_currentState;
}

bool MotionStateMachine::DetectSuddenStop(double currentVelocity) {
    // 如果在冷却期，不检测
    if (m_suddenStopCooldown > 0) {
        m_lastVelocity = currentVelocity;
        return false;
    }
    
    // 添加到最近速度记录
    m_recentVelocities.push_back(currentVelocity);
    if (m_recentVelocities.size() > static_cast<size_t>(m_config.suddenStopDetectionFrames)) {
        m_recentVelocities.erase(m_recentVelocities.begin());
    }
    
    // 需要足够的历史数据
    if (m_recentVelocities.size() < static_cast<size_t>(m_config.suddenStopDetectionFrames)) {
        m_lastVelocity = currentVelocity;
        return false;
    }
    
    // 检测连续的速度下降
    bool suddenStop = true;
    for (size_t i = 1; i < m_recentVelocities.size(); ++i) {
        double prevVel = m_recentVelocities[i - 1];
        double currVel = m_recentVelocities[i];
        
        // 避免除以零
        if (prevVel < 1.0) {
            suddenStop = false;
            break;
        }
        
        // 计算速度下降率
        double dropRate = (prevVel - currVel) / prevVel;
        
        if (dropRate < m_config.suddenStopVelocityDrop) {
            suddenStop = false;
            break;
        }
    }
    
    if (suddenStop) {
        // 启动冷却
        m_suddenStopCooldown = m_config.suddenStopCooldownFrames;
        m_recentVelocities.clear();
    }
    
    m_lastVelocity = currentVelocity;
    return suddenStop;
}

void MotionStateMachine::Reset() {
    m_currentState = MotionState::STILL;
    m_framesInState = 0;
    m_transitionCounter = 0;
    m_lastVelocity = 0.0;
    m_recentVelocities.clear();
    m_suddenStopCooldown = 0;
    m_tempExitThresholdMultiplier = 1.0;
    m_tempThresholdFrames = 0;
}

void MotionStateMachine::TemporarilyIncreaseExitThreshold(double multiplier, int frames) {
    m_tempExitThresholdMultiplier = multiplier;
    m_tempThresholdFrames = frames;
}

} // namespace LeadPrediction
