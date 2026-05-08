#pragma once

#include <opencv2/core.hpp>
#include <cmath>
#include <limits>

namespace LeadPrediction {

/**
 * @brief 速度过滤器 - 过滤和平滑速度数据
 * 
 * 功能：
 * - 置信度检查（拒绝 confidence < 0.6）
 * - NaN/Inf 检查
 * - 速度突变检测和拒绝
 * - 速度上限裁剪
 * - 指数移动平均（EMA）平滑
 * 
 * 设计要求：
 * - Requirements: 7.1, 7.2, 7.3, 7.4, 7.5
 * - Property 17-20: 速度过滤和平滑属性
 */
class VelocityFilter {
public:
    /**
     * @brief 构造函数
     * @param minConfidence 最小置信度阈值（默认 0.6）
     * @param maxVelocity 最大速度阈值（默认 500.0 px/s）
     * @param spikeThreshold 突变检测阈值（默认 3.0 倍标准差）
     * @param emaAlpha EMA 平滑系数（默认 0.7）
     */
    explicit VelocityFilter(
        double minConfidence = 0.6,
        double maxVelocity = 500.0,
        double spikeThreshold = 3.0,
        double emaAlpha = 0.7
    );
    
    /**
     * @brief 过滤和平滑速度
     * @param velocity 原始速度
     * @param confidence 置信度 [0, 1]
     * @param previousVelocity 上一帧速度（用于突变检测）
     * @param stdDev 速度标准差（用于突变检测）
     * @return 过滤和平滑后的速度
     */
    cv::Point2d Filter(
        const cv::Point2d& velocity,
        double confidence,
        const cv::Point2d& previousVelocity,
        double stdDev
    );
    
    /**
     * @brief 重置过滤器状态
     */
    void Reset();
    
    /**
     * @brief 获取当前平滑后的速度
     */
    cv::Point2d GetSmoothedVelocity() const { return m_smoothedVelocity; }
    
    /**
     * @brief 检查速度是否有效
     */
    bool IsValid(const cv::Point2d& velocity) const;

private:
    /**
     * @brief 检查置信度
     * @return true 如果置信度足够高
     */
    bool CheckConfidence(double confidence) const;
    
    /**
     * @brief 检查 NaN/Inf
     * @return true 如果速度有效
     */
    bool CheckNaNInf(const cv::Point2d& velocity) const;
    
    /**
     * @brief 检测速度突变
     * @return true 如果检测到突变
     */
    bool DetectSpike(
        const cv::Point2d& velocity,
        const cv::Point2d& previousVelocity,
        double stdDev
    ) const;
    
    /**
     * @brief 裁剪速度到最大值
     */
    cv::Point2d ClipVelocity(const cv::Point2d& velocity) const;
    
    /**
     * @brief 应用 EMA 平滑
     */
    cv::Point2d ApplyEMA(const cv::Point2d& velocity);

private:
    // 配置参数
    double m_minConfidence;    ///< 最小置信度阈值
    double m_maxVelocity;      ///< 最大速度阈值
    double m_spikeThreshold;   ///< 突变检测阈值（倍数）
    double m_emaAlpha;         ///< EMA 平滑系数
    
    // 状态
    cv::Point2d m_smoothedVelocity;  ///< 当前平滑后的速度
    bool m_initialized;              ///< 是否已初始化
};

} // namespace LeadPrediction

