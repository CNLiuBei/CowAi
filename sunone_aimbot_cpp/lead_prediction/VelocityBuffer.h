#ifndef VELOCITY_BUFFER_H
#define VELOCITY_BUFFER_H

#include "LeadPredictionTypes.h"
#include <vector>
#include <cmath>

namespace LeadPrediction {

/**
 * @brief 速度缓冲区 - 维护固定大小的速度历史记录
 * 
 * 使用循环缓冲区实现，提供高效的统计计算
 */
class VelocityBuffer {
public:
    /**
     * @brief 构造函数
     * @param bufferSize 缓冲区大小（帧数）
     */
    explicit VelocityBuffer(int bufferSize = 8);
    
    /**
     * @brief 添加新的速度样本
     * @param vx X轴速度 (px/s)
     * @param vy Y轴速度 (px/s)
     * @param timestamp 时间戳
     */
    void Push(double vx, double vy, std::chrono::steady_clock::time_point timestamp);
    
    /**
     * @brief 计算平均速度
     * @return (avg_vx, avg_vy)
     */
    cv::Point2d GetAverageVelocity() const;
    
    /**
     * @brief 计算速度大小的平均值
     * @return 平均速度大小 (px/s)
     */
    double GetAverageMagnitude() const;
    
    /**
     * @brief 计算速度标准差（稳定度指标）
     * @return 速度大小的标准差
     */
    double GetStandardDeviation() const;
    
    /**
     * @brief 检测速度突变
     * @param threshold 突变阈值 (px/s)
     * @return true 如果检测到突变
     */
    bool DetectSpike(double threshold = 300.0) const;
    
    /**
     * @brief 清空缓冲区
     */
    void Clear();
    
    /**
     * @brief 获取当前样本数
     * @return 样本数
     */
    int Size() const { return m_count; }
    
    /**
     * @brief 获取缓冲区容量
     * @return 容量
     */
    int Capacity() const { return m_capacity; }
    
    /**
     * @brief 检查缓冲区是否已满
     * @return true 如果已满
     */
    bool IsFull() const { return m_count >= m_capacity; }
    
    /**
     * @brief 设置缓冲区容量
     * @param capacity 新容量
     */
    void SetCapacity(int capacity);
    
private:
    std::vector<VelocitySample> m_buffer;  // 循环缓冲区
    int m_capacity;                        // 缓冲区容量
    int m_head;                            // 写入位置
    int m_count;                           // 有效样本数
    
    // 性能优化：缓存统计值
    mutable bool m_cacheValid;             // 缓存是否有效
    mutable cv::Point2d m_cachedAvgVelocity;  // 缓存的平均速度
    mutable double m_cachedAvgMagnitude;   // 缓存的平均速度大小
    mutable double m_cachedStdDev;         // 缓存的标准差
    
    /**
     * @brief 计算速度大小
     * @param sample 速度样本
     * @return 速度大小
     */
    inline double CalculateMagnitude(const VelocitySample& sample) const {
        return std::sqrt(sample.vx * sample.vx + sample.vy * sample.vy);
    }
};

} // namespace LeadPrediction

#endif // VELOCITY_BUFFER_H
