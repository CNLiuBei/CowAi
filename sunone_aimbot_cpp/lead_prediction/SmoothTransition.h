#pragma once

#include "LeadPredictionTypes.h"
#include <opencv2/core.hpp>

namespace LeadPrediction {

/**
 * @brief 平滑过渡器 - 在状态切换时平滑提前量变化
 * 
 * 功能：
 * - STILL → MOVING: 缓慢引入提前量（alpha = 0.1）
 * - MOVING → STILL: 快速归零提前量（alpha = 0.3）
 * - 使用线性插值（lerp）实现平滑过渡
 * 
 * 设计要求：
 * - Requirements: 4.1, 4.2, 4.3, 4.4, 4.5, 4.6
 * - Property 16: 平滑提前量单调逼近目标值
 */
class SmoothTransition {
public:
    /**
     * @brief 构造函数
     * @param alphaStillToMoving STILL→MOVING 的插值系数（默认 0.1）
     * @param alphaMovingToStill MOVING→STILL 的插值系数（默认 0.3）
     */
    explicit SmoothTransition(
        double alphaStillToMoving = 0.1,
        double alphaMovingToStill = 0.3
    );
    
    /**
     * @brief 更新平滑提前量
     * @param targetLead 目标提前量（来自 LeadCalculator）
     * @param currentState 当前运动状态
     * @return 平滑后的提前量
     */
    cv::Point2d Update(const cv::Point2d& targetLead, MotionState currentState);
    
    /**
     * @brief 重置平滑器状态
     */
    void Reset();
    
    /**
     * @brief 获取当前平滑后的提前量
     */
    cv::Point2d GetSmoothedLead() const { return m_smoothedLead; }
    
    /**
     * @brief 获取上一次的状态
     */
    MotionState GetPreviousState() const { return m_previousState; }

private:
    /**
     * @brief 线性插值
     * @param current 当前值
     * @param target 目标值
     * @param alpha 插值系数 [0, 1]
     * @return 插值结果
     */
    static double Lerp(double current, double target, double alpha);
    
    /**
     * @brief 检测状态切换
     * @param currentState 当前状态
     * @return true 如果状态发生切换
     */
    bool DetectStateChange(MotionState currentState) const;
    
    /**
     * @brief 选择合适的 alpha 值
     * @param currentState 当前状态
     * @return 插值系数
     */
    double SelectAlpha(MotionState currentState) const;

private:
    // 配置参数
    double m_alphaStillToMoving;  ///< STILL→MOVING 插值系数
    double m_alphaMovingToStill;  ///< MOVING→STILL 插值系数
    
    // 状态
    cv::Point2d m_smoothedLead;   ///< 当前平滑后的提前量
    MotionState m_previousState;  ///< 上一帧的状态
    bool m_initialized;           ///< 是否已初始化
};

} // namespace LeadPrediction

