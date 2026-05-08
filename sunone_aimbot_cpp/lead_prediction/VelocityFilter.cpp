#include "VelocityFilter.h"
#include <algorithm>

namespace LeadPrediction {

VelocityFilter::VelocityFilter(
    double minConfidence,
    double maxVelocity,
    double spikeThreshold,
    double emaAlpha
)
    : m_minConfidence(minConfidence)
    , m_maxVelocity(maxVelocity)
    , m_spikeThreshold(spikeThreshold)
    , m_emaAlpha(emaAlpha)
    , m_smoothedVelocity(0.0, 0.0)
    , m_initialized(false)
{
}

cv::Point2d VelocityFilter::Filter(
    const cv::Point2d& velocity,
    double confidence,
    const cv::Point2d& previousVelocity,
    double stdDev
) {
    // 1. 置信度检查
    if (!CheckConfidence(confidence)) {
        // 置信度过低：使用上一帧速度
        return m_initialized ? m_smoothedVelocity : cv::Point2d(0.0, 0.0);
    }
    
    // 2. NaN/Inf 检查
    if (!CheckNaNInf(velocity)) {
        // 无效值：使用上一帧速度
        return m_initialized ? m_smoothedVelocity : cv::Point2d(0.0, 0.0);
    }
    
    // 3. 速度突变检测
    if (m_initialized && DetectSpike(velocity, previousVelocity, stdDev)) {
        // 检测到突变：使用上一帧速度
        return m_smoothedVelocity;
    }
    
    // 4. 速度上限裁剪
    cv::Point2d clippedVelocity = ClipVelocity(velocity);
    
    // 5. EMA 平滑
    cv::Point2d smoothedVelocity = ApplyEMA(clippedVelocity);
    
    return smoothedVelocity;
}

void VelocityFilter::Reset() {
    m_smoothedVelocity = cv::Point2d(0.0, 0.0);
    m_initialized = false;
}

bool VelocityFilter::IsValid(const cv::Point2d& velocity) const {
    return CheckNaNInf(velocity);
}

bool VelocityFilter::CheckConfidence(double confidence) const {
    return confidence >= m_minConfidence;
}

bool VelocityFilter::CheckNaNInf(const cv::Point2d& velocity) const {
    return std::isfinite(velocity.x) && std::isfinite(velocity.y);
}

bool VelocityFilter::DetectSpike(
    const cv::Point2d& velocity,
    const cv::Point2d& previousVelocity,
    double stdDev
) const {
    // 计算速度变化
    double dx = velocity.x - previousVelocity.x;
    double dy = velocity.y - previousVelocity.y;
    double change = std::sqrt(dx * dx + dy * dy);
    
    // 如果标准差太小，使用固定阈值
    double threshold = (stdDev > 1.0) ? (m_spikeThreshold * stdDev) : 50.0;
    
    return change > threshold;
}

cv::Point2d VelocityFilter::ClipVelocity(const cv::Point2d& velocity) const {
    // 单轴裁剪
    double clippedX = std::clamp(velocity.x, -m_maxVelocity, m_maxVelocity);
    double clippedY = std::clamp(velocity.y, -m_maxVelocity, m_maxVelocity);
    
    // 径向裁剪
    double magnitude = std::sqrt(clippedX * clippedX + clippedY * clippedY);
    if (magnitude > m_maxVelocity) {
        double scale = m_maxVelocity / magnitude;
        clippedX *= scale;
        clippedY *= scale;
    }
    
    return cv::Point2d(clippedX, clippedY);
}

cv::Point2d VelocityFilter::ApplyEMA(const cv::Point2d& velocity) {
    if (!m_initialized) {
        // 首次调用：直接使用当前值
        m_smoothedVelocity = velocity;
        m_initialized = true;
        return m_smoothedVelocity;
    }
    
    // EMA: smoothed = alpha * current + (1 - alpha) * previous
    m_smoothedVelocity.x = m_emaAlpha * velocity.x + (1.0 - m_emaAlpha) * m_smoothedVelocity.x;
    m_smoothedVelocity.y = m_emaAlpha * velocity.y + (1.0 - m_emaAlpha) * m_smoothedVelocity.y;
    
    return m_smoothedVelocity;
}

} // namespace LeadPrediction

