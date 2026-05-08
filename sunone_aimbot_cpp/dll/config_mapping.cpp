#include "config_mapping.h"
#include "sunone_aimbot_cpp.h"
#include <sstream>

namespace ConfigMapping {
    
    static std::string boolToString(bool value) {
        return value ? "true" : "false";
    }
    
    static bool stringToBool(const std::string& value) {
        return (value == "true" || value == "1" || value == "yes");
    }
    
    static std::string joinVector(const std::vector<std::string>& vec) {
        std::string result;
        for (size_t i = 0; i < vec.size(); ++i) {
            if (i > 0) result += ",";
            result += vec[i];
        }
        return result;
    }
    
    static std::vector<std::string> splitString(const std::string& str) {
        std::vector<std::string> result;
        std::stringstream ss(str);
        std::string item;
        while (std::getline(ss, item, ',')) {
            result.push_back(item);
        }
        return result;
    }

    std::map<std::string, std::string> getConfigMap() {
        std::map<std::string, std::string> m;
        
        #define ADD_STR(key) m[#key] = config.key
        #define ADD_INT(key) m[#key] = std::to_string(config.key)
        #define ADD_FLOAT(key) m[#key] = std::to_string(config.key)
        #define ADD_BOOL(key) m[#key] = boolToString(config.key)
        #define ADD_VEC(key) m[#key] = joinVector(config.key)
        
        // Capture
        ADD_STR(capture_method);
        ADD_STR(capture_target);
        ADD_STR(capture_window_title);
        ADD_STR(gstreamer_pipeline);
        ADD_INT(detection_resolution);
        ADD_INT(capture_fps);
        ADD_INT(monitor_idx);
        ADD_BOOL(capture_borders);
        ADD_BOOL(capture_cursor);
        ADD_STR(virtual_camera_name);
        ADD_INT(virtual_camera_width);
        ADD_INT(virtual_camera_heigth);
        
        // Target
        ADD_BOOL(disable_headshot);
        ADD_FLOAT(body_y_offset);
        ADD_FLOAT(head_y_offset);
        ADD_BOOL(auto_aim);
        ADD_INT(crosshair_offset_x);
        ADD_INT(crosshair_offset_y);
        
        // Mouse
        ADD_INT(fovX);
        ADD_INT(fovY);
        ADD_FLOAT(minSpeedMultiplier);
        ADD_FLOAT(maxSpeedMultiplier);
        ADD_FLOAT(predictionInterval);
        ADD_INT(prediction_futurePositions);
        ADD_BOOL(draw_futurePositions);
        ADD_FLOAT(snapRadius);
        ADD_FLOAT(nearRadius);
        ADD_FLOAT(speedCurveExponent);
        ADD_FLOAT(snapBoostFactor);
        ADD_BOOL(easynorecoil);
        ADD_FLOAT(easynorecoilstrength);
        ADD_INT(norecoil_mode);
        ADD_VEC(button_norecoil);
        ADD_STR(input_method);
        
        // Wind mouse
        ADD_BOOL(wind_mouse_enabled);
        ADD_FLOAT(wind_G);
        ADD_FLOAT(wind_W);
        ADD_FLOAT(wind_M);
        ADD_FLOAT(wind_D);
        
        // Arduino
        ADD_INT(arduino_baudrate);
        ADD_STR(arduino_port);
        ADD_BOOL(arduino_16_bit_mouse);
        ADD_BOOL(arduino_enable_keys);
        
        // Kmbox_b
        ADD_INT(kmbox_b_baudrate);
        ADD_STR(kmbox_b_port);
        
        // Kmbox_net
        ADD_STR(kmbox_net_ip);
        ADD_STR(kmbox_net_port);
        ADD_STR(kmbox_net_uuid);
        ADD_STR(kmbox_net_move_mode);
        ADD_INT(kmbox_net_move_time_ms);
        
        // Makcu
        ADD_INT(makcu_baudrate);
        ADD_STR(makcu_port);
        
        // Mouse shooting
        ADD_BOOL(auto_shoot);
        ADD_FLOAT(bScope_multiplier);
        ADD_FLOAT(auto_shoot_interval);
        
        // AI
        ADD_STR(backend);
        ADD_STR(ai_model);
        ADD_FLOAT(confidence_threshold);
        ADD_FLOAT(nms_threshold);
        ADD_INT(max_detections);
        ADD_BOOL(fixed_input_size);
        
        // ByteTrack
        ADD_BOOL(enable_bytetrack);
        ADD_FLOAT(bytetrack_track_thresh);
        ADD_FLOAT(bytetrack_high_thresh);
        ADD_FLOAT(bytetrack_match_thresh);
        ADD_INT(bytetrack_max_time_lost);
        
        // Target Lock
        ADD_BOOL(enable_target_lock);
        ADD_FLOAT(target_lock_threshold);
        ADD_INT(target_lock_hold_frames);
        
        // System
        ADD_INT(cpuCoreReserveCount);
        ADD_INT(systemMemoryReserveMB);
        
        // Buttons
        ADD_VEC(button_targeting);
        ADD_VEC(button_shoot);
        ADD_VEC(button_zoom);
        ADD_VEC(button_exit);
        ADD_VEC(button_pause);
        ADD_VEC(button_reload_config);
        ADD_VEC(button_open_overlay);
        ADD_BOOL(enable_arrows_settings);
        
        // Overlay
        ADD_INT(overlay_opacity);
        ADD_FLOAT(overlay_ui_scale);
        ADD_INT(overlay_theme);
        
        // Game Overlay
        ADD_BOOL(game_overlay_enabled);
        ADD_INT(game_overlay_max_fps);
        ADD_BOOL(game_overlay_draw_boxes);
        ADD_BOOL(game_overlay_draw_future);
        ADD_BOOL(game_overlay_draw_frame);
        ADD_INT(game_overlay_box_a);
        ADD_INT(game_overlay_box_r);
        ADD_INT(game_overlay_box_g);
        ADD_INT(game_overlay_box_b);
        ADD_INT(game_overlay_frame_a);
        ADD_INT(game_overlay_frame_r);
        ADD_INT(game_overlay_frame_g);
        ADD_INT(game_overlay_frame_b);
        ADD_FLOAT(game_overlay_box_thickness);
        ADD_FLOAT(game_overlay_frame_thickness);
        ADD_FLOAT(game_overlay_future_point_radius);
        ADD_FLOAT(game_overlay_future_alpha_falloff);
        ADD_BOOL(game_overlay_icon_enabled);
        ADD_STR(game_overlay_icon_path);
        ADD_INT(game_overlay_icon_width);
        ADD_INT(game_overlay_icon_height);
        ADD_FLOAT(game_overlay_icon_offset_x);
        ADD_FLOAT(game_overlay_icon_offset_y);
        ADD_STR(game_overlay_icon_anchor);
        
        // Classes
        ADD_INT(class_player);
        ADD_INT(class_head);
        
        // Debug
        ADD_BOOL(show_window);
        ADD_BOOL(show_fps);
        ADD_VEC(screenshot_button);
        ADD_INT(screenshot_delay);
        ADD_BOOL(verbose);
        
        // Auth
        ADD_BOOL(auth_enabled);
        ADD_STR(active_game);
        
        #undef ADD_STR
        #undef ADD_INT
        #undef ADD_FLOAT
        #undef ADD_BOOL
        #undef ADD_VEC
        
        return m;
    }

    void setConfigFromMap(const std::map<std::string, std::string>& configMap) {
        for (const auto& pair : configMap) {
            setConfigValue(pair.first, pair.second);
        }
    }

    std::string getConfigValue(const std::string& key) {
        auto configMap = getConfigMap();
        auto it = configMap.find(key);
        return (it != configMap.end()) ? it->second : "";
    }

    void setConfigValue(const std::string& key, const std::string& value) {
        try {
            #define SET_STR(k) if(key==#k){config.k=value;return;}
            #define SET_INT(k) if(key==#k){config.k=std::stoi(value);return;}
            #define SET_FLOAT(k) if(key==#k){config.k=std::stof(value);return;}
            #define SET_BOOL(k) if(key==#k){config.k=stringToBool(value);return;}
            #define SET_VEC(k) if(key==#k){config.k=splitString(value);return;}
            
            // Capture
            SET_STR(capture_method);SET_STR(capture_target);
            SET_STR(capture_window_title);SET_STR(gstreamer_pipeline);SET_INT(detection_resolution);
            SET_INT(capture_fps);SET_INT(monitor_idx);
            SET_BOOL(capture_borders);SET_BOOL(capture_cursor);SET_STR(virtual_camera_name);
            SET_INT(virtual_camera_width);SET_INT(virtual_camera_heigth);
            
            // Target
            SET_BOOL(disable_headshot);SET_FLOAT(body_y_offset);SET_FLOAT(head_y_offset);
            SET_BOOL(auto_aim);SET_INT(crosshair_offset_x);SET_INT(crosshair_offset_y);
            
            // Mouse
            SET_INT(fovX);SET_INT(fovY);SET_FLOAT(minSpeedMultiplier);SET_FLOAT(maxSpeedMultiplier);
            SET_FLOAT(predictionInterval);SET_INT(prediction_futurePositions);SET_BOOL(draw_futurePositions);
            SET_FLOAT(snapRadius);SET_FLOAT(nearRadius);SET_FLOAT(speedCurveExponent);SET_FLOAT(snapBoostFactor);
            SET_BOOL(easynorecoil);SET_FLOAT(easynorecoilstrength);SET_INT(norecoil_mode);
            SET_VEC(button_norecoil);SET_STR(input_method);
            
            // Wind mouse
            SET_BOOL(wind_mouse_enabled);SET_FLOAT(wind_G);SET_FLOAT(wind_W);
            SET_FLOAT(wind_M);SET_FLOAT(wind_D);
            
            // Arduino
            SET_INT(arduino_baudrate);SET_STR(arduino_port);
            SET_BOOL(arduino_16_bit_mouse);SET_BOOL(arduino_enable_keys);
            
            // Kmbox_b
            SET_INT(kmbox_b_baudrate);SET_STR(kmbox_b_port);
            
            // Kmbox_net
            SET_STR(kmbox_net_ip);SET_STR(kmbox_net_port);SET_STR(kmbox_net_uuid);
            SET_STR(kmbox_net_move_mode);SET_INT(kmbox_net_move_time_ms);
            
            // Makcu
            SET_INT(makcu_baudrate);SET_STR(makcu_port);
            
            // Mouse shooting
            SET_BOOL(auto_shoot);SET_FLOAT(bScope_multiplier);SET_FLOAT(auto_shoot_interval);
            
            // AI
            SET_STR(backend);SET_STR(ai_model);
            SET_FLOAT(confidence_threshold);SET_FLOAT(nms_threshold);SET_INT(max_detections);
            SET_BOOL(fixed_input_size);
            
            // ByteTrack
            SET_BOOL(enable_bytetrack);SET_FLOAT(bytetrack_track_thresh);SET_FLOAT(bytetrack_high_thresh);
            SET_FLOAT(bytetrack_match_thresh);SET_INT(bytetrack_max_time_lost);
            
            // Target Lock
            SET_BOOL(enable_target_lock);SET_FLOAT(target_lock_threshold);SET_INT(target_lock_hold_frames);
            
            // System
            SET_INT(cpuCoreReserveCount);SET_INT(systemMemoryReserveMB);
            
            // Buttons
            SET_VEC(button_targeting);SET_VEC(button_shoot);SET_VEC(button_zoom);
            SET_VEC(button_exit);SET_VEC(button_pause);SET_VEC(button_reload_config);
            SET_VEC(button_open_overlay);SET_BOOL(enable_arrows_settings);
            
            // Overlay
            SET_INT(overlay_opacity);SET_FLOAT(overlay_ui_scale);SET_INT(overlay_theme);
            
            // Game Overlay
            SET_BOOL(game_overlay_enabled);SET_INT(game_overlay_max_fps);
            SET_BOOL(game_overlay_draw_boxes);SET_BOOL(game_overlay_draw_future);SET_BOOL(game_overlay_draw_frame);
            SET_INT(game_overlay_box_a);SET_INT(game_overlay_box_r);SET_INT(game_overlay_box_g);SET_INT(game_overlay_box_b);
            SET_INT(game_overlay_frame_a);SET_INT(game_overlay_frame_r);SET_INT(game_overlay_frame_g);SET_INT(game_overlay_frame_b);
            SET_FLOAT(game_overlay_box_thickness);SET_FLOAT(game_overlay_frame_thickness);
            SET_FLOAT(game_overlay_future_point_radius);SET_FLOAT(game_overlay_future_alpha_falloff);
            SET_BOOL(game_overlay_icon_enabled);SET_STR(game_overlay_icon_path);
            SET_INT(game_overlay_icon_width);SET_INT(game_overlay_icon_height);
            SET_FLOAT(game_overlay_icon_offset_x);SET_FLOAT(game_overlay_icon_offset_y);
            SET_STR(game_overlay_icon_anchor);
            
            // Classes
            SET_INT(class_player);SET_INT(class_head);
            
            // Debug
            SET_BOOL(show_window);SET_BOOL(show_fps);SET_VEC(screenshot_button);
            SET_INT(screenshot_delay);SET_BOOL(verbose);
            
            // Auth
            SET_BOOL(auth_enabled);SET_STR(active_game);
            
            #undef SET_STR
            #undef SET_INT
            #undef SET_FLOAT
            #undef SET_BOOL
            #undef SET_VEC
        } catch (...) {
            // Ignore conversion errors
        }
    }
}
