#include "VelocityBuffer.h"
#include <algorithm>
#include <numeric>

namespace LeadPrediction {

VelocityBuffer::VelocityBuffer(int bufferSize)
    : m_capacity(bufferSize)
    , m_head(0)
    , m_count(0)
    , m_cacheValid(false)
    , m_cachedAvgVelocity(0.0, 0.0)
    , m_cachedAvgMagnitude(0.0)
    , m_cachedStdDev(0.0)
{
    m_buffer.resize(m_capacity);
}

void VelocityBuffer::Push(double vx, double vy, std::chrono::steady_clock::time_point timestamp) {
    // 写入新样本到循环缓冲区
    m_buffer[m_head] = VelocitySample(vx, vy, timestamp);
    
    // 更新头指针（循环）
    m_head = (m_head + 1) % m_capacity;
    
    // 更新计数（最多到容量）
    if (m_count < m_capacity) {
        m_count++;
    }
    
    // 标记缓存失效
    m_cacheValid = false;
}

cv::Point2d VelocityBuffer::GetAverageVelocity() const {
    if (m_count == 0) {
        return cv::Point2d(0.0, 0.0);
    }
    
    // 使用缓存
    if (m_cacheValid) {
        return m_cachedAvgVelocity;
    }
    
    double sum_vx = 0.0;
    double sum_vy = 0.0;
    
    for (int i = 0; i < m_count; ++i) {
        sum_vx += m_buffer[i].vx;
        sum_vy += m_buffer[i].vy;
    }
    
    m_cachedAvgVelocity = cv::Point2d(sum_vx / m_count, sum_vy / m_count);
    return m_cachedAvgVelocity;
}

double VelocityBuffer::GetAverageMagnitude() const {
    if (m_count == 0) {
        return 0.0;
    }
    
    // 使用缓存
    if (m_cacheValid) {
        return m_cachedAvgMagnitude;
    }
    
    double sum = 0.0;
    for (int i = 0; i < m_count; ++i) {
        sum += CalculateMagnitude(m_buffer[i]);
    }
    
    m_cachedAvgMagnitude = sum / m_count;
    return m_cachedAvgMagnitude;
}

double VelocityBuffer::GetStandardDeviation() const {
    if (m_count < 2) {
        return 0.0;
    }
    
    // 使用缓存
    if (m_cacheValid) {
        return m_cachedStdDev;
    }
    
    // 计算平均速度大小
    double mean = GetAverageMagnitude();
    
    // 计算方差
    double variance = 0.0;
    for (int i = 0; i < m_count; ++i) {
        double magnitude = CalculateMagnitude(m_buffer[i]);
        double diff = magnitude - mean;
        variance += diff * diff;
    }
    variance /= m_count;
    
    // 返回标准差
    m_cachedStdDev = std::sqrt(variance);
    
    // 标记缓存有效（所有统计值都已计算）
    m_cacheValid = true;
    
    return m_cachedStdDev;
}

bool VelocityBuffer::DetectSpike(double threshold) const {
    if (m_count < 2) {
        return false;
    }
    
    // 获取最新样本的索引
    int latestIdx = (m_head - 1 + m_capacity) % m_capacity;
    int previousIdx = (m_head - 2 + m_capacity) % m_capacity;
    
    // 计算速度变化
    double latestMag = CalculateMagnitude(m_buffer[latestIdx]);
    double previousMag = CalculateMagnitude(m_buffer[previousIdx]);
    double change = std::abs(latestMag - previousMag);
    
    return change > threshold;
}

void VelocityBuffer::Clear() {
    m_head = 0;
    m_count = 0;
    m_cacheValid = false;
    // 不需要清空 vector 内容，只需重置指针
}

void VelocityBuffer::SetCapacity(int capacity) {
    if (capacity <= 0) {
        capacity = 1;
    }
    
    if (capacity != m_capacity) {
        m_capacity = capacity;
        m_buffer.resize(m_capacity);
        m_head = 0;
        m_count = 0;  // 重置计数，因为旧数据可能不再有效
        m_cacheValid = false;
    }
}

} // namespace LeadPrediction
