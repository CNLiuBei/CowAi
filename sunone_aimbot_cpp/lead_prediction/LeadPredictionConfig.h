#ifndef LEAD_PREDICTION_CONFIG_H
#define LEAD_PREDICTION_CONFIG_H

#include <string>

namespace LeadPrediction {

/**
 * @brief 射击提前量预测系统配置
 */
struct LeadPredictionConfig {
    // 启用/禁用
    bool enabled = false;  // 默认禁用，避免CPU占用过高
    
    // 速度缓冲区
    int velocity_buffer_size = 8;
    
    // 运动状态判定
    double motion_enter_threshold = 10.0;      // px/s (降低阈值，更容易检测到移动)
    double motion_exit_threshold = 5.0;        // px/s
    int min_frames_for_transition = 2;         // 减少所需帧数，更快响应
    
    // 自适应延迟
    double base_fire_delay = 50.0;             // ms
    double velocity_factor = 0.001;
    double stability_factor = 0.002;
    double fps_compensation_rate = 20.0;       // ms
    double min_fire_delay = 30.0;              // ms
    double max_fire_delay = 150.0;             // ms
    int reference_fps = 144;
    
    // 提前量计算
    double lateral_factor = 1.0;
    double vertical_factor_up = 0.4;
    double vertical_factor_down = 0.5;
    double high_vertical_threshold = 150.0;    // px/s
    double high_vertical_reduction = 0.2;
    
    // 提前量限制
    double max_lead_x = 100.0;                 // px
    double max_lead_y = 50.0;                  // px
    double max_lead_radial = 120.0;            // px
    
    // 速度限制
    double max_realistic_velocity = 500.0;     // px/s
    double velocity_spike_threshold = 300.0;   // px/s
    double velocity_smooth_alpha = 0.7;
    double min_tracking_confidence = 0.6;
    
    // 平滑过渡
    double alpha_still_to_moving = 0.1;
    double alpha_moving_to_still = 0.3;
    int transition_frames_intro = 10;
    int transition_frames_removal = 5;
    
    // 急停检测
    double sudden_stop_velocity_drop = 0.8;    // 80%
    int sudden_stop_detection_frames = 2;
    double sudden_stop_lead_reduction = 0.5;   // 50%
    int sudden_stop_cooldown_frames = 10;
    
    // 调试
    bool debug_visualization = true;
    bool debug_overlay = true;
    bool log_state_transitions = false;
    
    /**
     * @brief 从配置文件加载
     * @param filename 配置文件路径
     * @return 是否成功加载
     */
    bool LoadFromFile(const std::string& filename = "config.ini");
    
    /**
     * @brief 保存到配置文件
     * @param filename 配置文件路径
     * @return 是否成功保存
     */
    bool SaveToFile(const std::string& filename = "config.ini");
    
    /**
     * @brief 验证配置有效性
     * @return 是否有效
     */
    bool Validate();
    
    /**
     * @brief 获取默认配置
     * @return 默认配置
     */
    static LeadPredictionConfig GetDefault();
};

} // namespace LeadPrediction

#endif // LEAD_PREDICTION_CONFIG_H
