#include "LeadCalculator.h"
#include <algorithm>
#include <cmath>

namespace LeadPrediction {

LeadCalculator::LeadCalculator(const Config& config)
    : m_config(config)
{
}

cv::Point2d LeadCalculator::Calculate(
    double vx,
    double vy,
    double deltaT,
    MotionState motionState
) {
    // 静止状态：零提前量
    if (motionState == MotionState::STILL) {
        return cv::Point2d(0.0, 0.0);
    }
    
    // 计算原始提前量
    double rawLeadX = vx * deltaT;
    double rawLeadY = vy * deltaT;
    
    // 横向：完整保留
    double leadX = rawLeadX * m_config.lateralFactor;  // × 1.0
    
    // 纵向：差异化处理
    double verticalFactor = GetVerticalFactor(vy);
    double leadY = rawLeadY * verticalFactor;
    
    // 应用限制
    return ClipLead(leadX, leadY);
}

double LeadCalculator::GetVerticalFactor(double vy) {
    // 基础因子
    double factor = (vy > 0) ? m_config.verticalFactorUp    // 0.4 (向上)
                             : m_config.verticalFactorDown;  // 0.5 (向下)
    
    // 高速纵向额外减少
    if (std::abs(vy) > m_config.highVerticalThreshold) {
        factor *= (1.0 - m_config.highVerticalReduction);  // × 0.8
    }
    
    return factor;
}

cv::Point2d LeadCalculator::ClipLead(double leadX, double leadY) {
    // 单轴裁剪
    leadX = std::clamp(leadX, -m_config.maxLeadX, m_config.maxLeadX);
    leadY = std::clamp(leadY, -m_config.maxLeadY, m_config.maxLeadY);
    
    // 径向裁剪
    double magnitude = std::sqrt(leadX * leadX + leadY * leadY);
    if (magnitude > m_config.maxLeadRadial) {
        double scale = m_config.maxLeadRadial / magnitude;
        leadX *= scale;
        leadY *= scale;
    }
    
    return cv::Point2d(leadX, leadY);
}

} // namespace LeadPrediction
