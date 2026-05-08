#include "SmoothTransition.h"

namespace LeadPrediction {

SmoothTransition::SmoothTransition(
    double alphaStillToMoving,
    double alphaMovingToStill
)
    : m_alphaStillToMoving(alphaStillToMoving)
    , m_alphaMovingToStill(alphaMovingToStill)
    , m_smoothedLead(0.0, 0.0)
    , m_previousState(MotionState::STILL)
    , m_initialized(false)
{
}

cv::Point2d SmoothTransition::Update(
    const cv::Point2d& targetLead,
    MotionState currentState
) {
    // 首次调用：直接使用目标值
    if (!m_initialized) {
        m_smoothedLead = targetLead;
        m_previousState = currentState;
        m_initialized = true;
        return m_smoothedLead;
    }
    
    // 选择合适的 alpha
    double alpha = SelectAlpha(currentState);
    
    // 线性插值
    m_smoothedLead.x = Lerp(m_smoothedLead.x, targetLead.x, alpha);
    m_smoothedLead.y = Lerp(m_smoothedLead.y, targetLead.y, alpha);
    
    // 更新状态
    m_previousState = currentState;
    
    return m_smoothedLead;
}

void SmoothTransition::Reset() {
    m_smoothedLead = cv::Point2d(0.0, 0.0);
    m_previousState = MotionState::STILL;
    m_initialized = false;
}

double SmoothTransition::Lerp(double current, double target, double alpha) {
    return current + alpha * (target - current);
}

bool SmoothTransition::DetectStateChange(MotionState currentState) const {
    return currentState != m_previousState;
}

double SmoothTransition::SelectAlpha(MotionState currentState) const {
    // 检测状态切换
    bool stateChanged = DetectStateChange(currentState);
    
    if (!stateChanged) {
        // 状态未变化：使用当前状态的 alpha
        if (currentState == MotionState::MOVING) {
            return m_alphaStillToMoving;  // 0.1（缓慢引入）
        } else {
            return m_alphaMovingToStill;  // 0.3（快速归零）
        }
    }
    
    // 状态切换：根据切换方向选择
    if (m_previousState == MotionState::STILL && currentState == MotionState::MOVING) {
        // STILL → MOVING: 缓慢引入
        return m_alphaStillToMoving;  // 0.1
    } else if (m_previousState == MotionState::MOVING && currentState == MotionState::STILL) {
        // MOVING → STILL: 快速归零
        return m_alphaMovingToStill;  // 0.3
    }
    
    // 默认（不应该到达这里）
    return 0.1;
}

} // namespace LeadPrediction

