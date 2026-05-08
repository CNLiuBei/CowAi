#include "LeadPredictionConfig.h"
#include "../modules/SimpleIni.h"
#include <iostream>
#include <filesystem>

namespace LeadPrediction {

bool LeadPredictionConfig::LoadFromFile(const std::string& filename) {
    CSimpleIniA ini;
    ini.SetUnicode();
    
    bool fileExists = std::filesystem::exists(filename);
    bool needsSave = false;
    
    if (fileExists) {
        SI_Error rc = ini.LoadFile(filename.c_str());
        if (rc < 0) {
            std::cerr << "[LeadPrediction] Error loading config file: " << filename << std::endl;
            *this = GetDefault();
            return false;
        }
    } else {
        std::cerr << "[LeadPrediction] Config file does not exist: " << filename << std::endl;
        *this = GetDefault();
        return false;
    }
    
    // 检查 [Lead_Prediction] 配置段是否存在
    CSimpleIniA::TNamesDepend sections;
    ini.GetAllSections(sections);
    bool sectionExists = false;
    for (const auto& section : sections) {
        if (std::string(section.pItem) == "Lead_Prediction") {
            sectionExists = true;
            break;
        }
    }
    
    // 如果配置段不存在，使用默认值并标记需要保存
    if (!sectionExists) {
        std::cout << "[LeadPrediction] Section [Lead_Prediction] not found, creating with default values..." << std::endl;
        *this = GetDefault();
        needsSave = true;
    } else {
        // 辅助函数
        auto get_bool = [&](const char* key, bool defval) {
            return ini.GetBoolValue("Lead_Prediction", key, defval);
        };
        
        auto get_long = [&](const char* key, long defval) {
            return (int)ini.GetLongValue("Lead_Prediction", key, defval);
        };
        
        auto get_double = [&](const char* key, double defval) {
            return ini.GetDoubleValue("Lead_Prediction", key, defval);
        };
        
        // 加载配置
        enabled = get_bool("enabled", true);  // 默认启用
        
        // 速度缓冲区
        velocity_buffer_size = get_long("velocity_buffer_size", 8);
        
        // 运动状态判定
        motion_enter_threshold = get_double("motion_enter_threshold", 30.0);
        motion_exit_threshold = get_double("motion_exit_threshold", 15.0);
        min_frames_for_transition = get_long("min_frames_for_transition", 3);
        
        // 自适应延迟
        base_fire_delay = get_double("base_fire_delay", 50.0);
        velocity_factor = get_double("velocity_factor", 0.001);
        stability_factor = get_double("stability_factor", 0.002);
        fps_compensation_rate = get_double("fps_compensation_rate", 20.0);
        min_fire_delay = get_double("min_fire_delay", 30.0);
        max_fire_delay = get_double("max_fire_delay", 150.0);
        reference_fps = get_long("reference_fps", 144);
        
        // 提前量计算
        lateral_factor = get_double("lateral_factor", 1.0);
        vertical_factor_up = get_double("vertical_factor_up", 0.4);
        vertical_factor_down = get_double("vertical_factor_down", 0.5);
        high_vertical_threshold = get_double("high_vertical_threshold", 150.0);
        high_vertical_reduction = get_double("high_vertical_reduction", 0.2);
        
        // 提前量限制
        max_lead_x = get_double("max_lead_x", 100.0);
        max_lead_y = get_double("max_lead_y", 50.0);
        max_lead_radial = get_double("max_lead_radial", 120.0);
        
        // 速度限制
        max_realistic_velocity = get_double("max_realistic_velocity", 500.0);
        velocity_spike_threshold = get_double("velocity_spike_threshold", 300.0);
        velocity_smooth_alpha = get_double("velocity_smooth_alpha", 0.7);
        min_tracking_confidence = get_double("min_tracking_confidence", 0.6);
        
        // 平滑过渡
        alpha_still_to_moving = get_double("alpha_still_to_moving", 0.1);
        alpha_moving_to_still = get_double("alpha_moving_to_still", 0.3);
        transition_frames_intro = get_long("transition_frames_intro", 10);
        transition_frames_removal = get_long("transition_frames_removal", 5);
        
        // 急停检测
        sudden_stop_velocity_drop = get_double("sudden_stop_velocity_drop", 0.8);
        sudden_stop_detection_frames = get_long("sudden_stop_detection_frames", 2);
        sudden_stop_lead_reduction = get_double("sudden_stop_lead_reduction", 0.5);
        sudden_stop_cooldown_frames = get_long("sudden_stop_cooldown_frames", 10);
        
        // 调试
        debug_visualization = get_bool("debug_visualization", true);
        debug_overlay = get_bool("debug_overlay", true);
        log_state_transitions = get_bool("log_state_transitions", false);
        
        // 验证配置
        if (!Validate()) {
            std::cerr << "[LeadPrediction] Invalid configuration, using defaults" << std::endl;
            *this = GetDefault();
            needsSave = true;
        }
    }
    
    // 如果需要保存，将默认配置写入文件
    if (needsSave) {
        if (SaveToFile(filename)) {
            std::cout << "[LeadPrediction] Default configuration saved to: " << filename << std::endl;
        } else {
            std::cerr << "[LeadPrediction] Failed to save default configuration" << std::endl;
        }
    }
    
    return true;
}

bool LeadPredictionConfig::SaveToFile(const std::string& filename) {
    CSimpleIniA ini;
    ini.SetUnicode();
    
    // 如果文件存在，先加载以保留其他配置
    if (std::filesystem::exists(filename)) {
        SI_Error rc = ini.LoadFile(filename.c_str());
        if (rc < 0) {
            std::cerr << "[LeadPrediction] Error loading existing config: " << filename << std::endl;
        }
    }
    
    // 保存配置
    ini.SetBoolValue("Lead_Prediction", "enabled", enabled);
    
    // 速度缓冲区
    ini.SetLongValue("Lead_Prediction", "velocity_buffer_size", velocity_buffer_size);
    
    // 运动状态判定
    ini.SetDoubleValue("Lead_Prediction", "motion_enter_threshold", motion_enter_threshold);
    ini.SetDoubleValue("Lead_Prediction", "motion_exit_threshold", motion_exit_threshold);
    ini.SetLongValue("Lead_Prediction", "min_frames_for_transition", min_frames_for_transition);
    
    // 自适应延迟
    ini.SetDoubleValue("Lead_Prediction", "base_fire_delay", base_fire_delay);
    ini.SetDoubleValue("Lead_Prediction", "velocity_factor", velocity_factor);
    ini.SetDoubleValue("Lead_Prediction", "stability_factor", stability_factor);
    ini.SetDoubleValue("Lead_Prediction", "fps_compensation_rate", fps_compensation_rate);
    ini.SetDoubleValue("Lead_Prediction", "min_fire_delay", min_fire_delay);
    ini.SetDoubleValue("Lead_Prediction", "max_fire_delay", max_fire_delay);
    ini.SetLongValue("Lead_Prediction", "reference_fps", reference_fps);
    
    // 提前量计算
    ini.SetDoubleValue("Lead_Prediction", "lateral_factor", lateral_factor);
    ini.SetDoubleValue("Lead_Prediction", "vertical_factor_up", vertical_factor_up);
    ini.SetDoubleValue("Lead_Prediction", "vertical_factor_down", vertical_factor_down);
    ini.SetDoubleValue("Lead_Prediction", "high_vertical_threshold", high_vertical_threshold);
    ini.SetDoubleValue("Lead_Prediction", "high_vertical_reduction", high_vertical_reduction);
    
    // 提前量限制
    ini.SetDoubleValue("Lead_Prediction", "max_lead_x", max_lead_x);
    ini.SetDoubleValue("Lead_Prediction", "max_lead_y", max_lead_y);
    ini.SetDoubleValue("Lead_Prediction", "max_lead_radial", max_lead_radial);
    
    // 速度限制
    ini.SetDoubleValue("Lead_Prediction", "max_realistic_velocity", max_realistic_velocity);
    ini.SetDoubleValue("Lead_Prediction", "velocity_spike_threshold", velocity_spike_threshold);
    ini.SetDoubleValue("Lead_Prediction", "velocity_smooth_alpha", velocity_smooth_alpha);
    ini.SetDoubleValue("Lead_Prediction", "min_tracking_confidence", min_tracking_confidence);
    
    // 平滑过渡
    ini.SetDoubleValue("Lead_Prediction", "alpha_still_to_moving", alpha_still_to_moving);
    ini.SetDoubleValue("Lead_Prediction", "alpha_moving_to_still", alpha_moving_to_still);
    ini.SetLongValue("Lead_Prediction", "transition_frames_intro", transition_frames_intro);
    ini.SetLongValue("Lead_Prediction", "transition_frames_removal", transition_frames_removal);
    
    // 急停检测
    ini.SetDoubleValue("Lead_Prediction", "sudden_stop_velocity_drop", sudden_stop_velocity_drop);
    ini.SetLongValue("Lead_Prediction", "sudden_stop_detection_frames", sudden_stop_detection_frames);
    ini.SetDoubleValue("Lead_Prediction", "sudden_stop_lead_reduction", sudden_stop_lead_reduction);
    ini.SetLongValue("Lead_Prediction", "sudden_stop_cooldown_frames", sudden_stop_cooldown_frames);
    
    // 调试
    ini.SetBoolValue("Lead_Prediction", "debug_visualization", debug_visualization);
    ini.SetBoolValue("Lead_Prediction", "debug_overlay", debug_overlay);
    ini.SetBoolValue("Lead_Prediction", "log_state_transitions", log_state_transitions);
    
    // 保存到文件
    SI_Error rc = ini.SaveFile(filename.c_str());
    if (rc < 0) {
        std::cerr << "[LeadPrediction] Error saving config to: " << filename << std::endl;
        return false;
    }
    
    return true;
}

bool LeadPredictionConfig::Validate() {
    bool valid = true;
    
    // 验证阈值关系
    if (motion_exit_threshold >= motion_enter_threshold) {
        std::cerr << "[LeadPrediction] Invalid: exit_threshold must be < enter_threshold" << std::endl;
        valid = false;
    }
    
    // 验证延迟范围
    if (min_fire_delay >= max_fire_delay) {
        std::cerr << "[LeadPrediction] Invalid: min_fire_delay must be < max_fire_delay" << std::endl;
        valid = false;
    }
    
    // 验证缓冲区大小
    if (velocity_buffer_size < 3 || velocity_buffer_size > 20) {
        std::cerr << "[LeadPrediction] Invalid: velocity_buffer_size must be in [3, 20]" << std::endl;
        valid = false;
    }
    
    // 验证因子范围
    if (lateral_factor < 0.0 || lateral_factor > 2.0) {
        std::cerr << "[LeadPrediction] Invalid: lateral_factor must be in [0.0, 2.0]" << std::endl;
        valid = false;
    }
    
    if (vertical_factor_up < 0.0 || vertical_factor_up > 1.0) {
        std::cerr << "[LeadPrediction] Invalid: vertical_factor_up must be in [0.0, 1.0]" << std::endl;
        valid = false;
    }
    
    if (vertical_factor_down < 0.0 || vertical_factor_down > 1.0) {
        std::cerr << "[LeadPrediction] Invalid: vertical_factor_down must be in [0.0, 1.0]" << std::endl;
        valid = false;
    }
    
    // 验证alpha范围
    if (alpha_still_to_moving <= 0.0 || alpha_still_to_moving > 1.0) {
        std::cerr << "[LeadPrediction] Invalid: alpha_still_to_moving must be in (0.0, 1.0]" << std::endl;
        valid = false;
    }
    
    if (alpha_moving_to_still <= 0.0 || alpha_moving_to_still > 1.0) {
        std::cerr << "[LeadPrediction] Invalid: alpha_moving_to_still must be in (0.0, 1.0]" << std::endl;
        valid = false;
    }
    
    return valid;
}

LeadPredictionConfig LeadPredictionConfig::GetDefault() {
    LeadPredictionConfig config;
    // 所有成员变量已经在声明时初始化为默认值
    return config;
}

} // namespace LeadPrediction
