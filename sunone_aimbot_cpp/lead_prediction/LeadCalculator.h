#ifndef LEAD_CALCULATOR_H
#define LEAD_CALCULATOR_H

#include "LeadPredictionTypes.h"
#include "MotionStateMachine.h"
#include <opencv2/opencv.hpp>

namespace LeadPrediction {

/**
 * @brief 提前量计算器
 * 
 * 根据速度和延迟计算射击提前量，处理横向/纵向差异
 */
class LeadCalculator {
public:
    struct Config {
        double lateralFactor = 1.0;              // 横向因子（完整）
        double verticalFactorUp = 0.4;           // 纵向因子（向上跳跃）
        double verticalFactorDown = 0.5;         // 纵向因子（向下坠落）
        double highVerticalThreshold = 150.0;    // 高速纵向阈值 (px/s)
        double highVerticalReduction = 0.2;      // 高速纵向额外减少
        double maxLeadX = 100.0;                 // X轴最大提前量 (px)
        double maxLeadY = 50.0;                  // Y轴最大提前量 (px)
        double maxLeadRadial = 120.0;            // 径向最大提前量 (px)
    };
    
    /**
     * @brief 构造函数
     * @param config 配置
     */
    explicit LeadCalculator(const Config& config = Config());
    
    /**
     * @brief 计算射击提前量
     * @param vx X轴速度 (px/s)
     * @param vy Y轴速度 (px/s)
     * @param deltaT 射击延迟 (秒)
     * @param motionState 运动状态
     * @return (lead_x, lead_y) 提前量 (像素)
     */
    cv::Point2d Calculate(
        double vx,
        double vy,
        double deltaT,
        MotionState motionState
    );
    
    /**
     * @brief 应用提前量限制
     * @param leadX X轴提前量
     * @param leadY Y轴提前量
     * @return 裁剪后的提前量
     */
    cv::Point2d ClipLead(double leadX, double leadY);
    
    /**
     * @brief 获取配置
     * @return 配置引用
     */
    Config& GetConfig() { return m_config; }
    const Config& GetConfig() const { return m_config; }
    
private:
    Config m_config;
    
    /**
     * @brief 计算纵向因子
     * @param vy Y轴速度
     * @return 纵向因子
     */
    double GetVerticalFactor(double vy);
};

} // namespace LeadPrediction

#endif // LEAD_CALCULATOR_H
