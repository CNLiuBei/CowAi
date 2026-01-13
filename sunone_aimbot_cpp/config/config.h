#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <vector>
#include <unordered_map>
#include <utility>

class Config
{
public:
    // Capture
    std::string capture_method; // "duplication_api", "winrt", "virtual_camera", "gstreamer"
    bool gpu_zero_copy = true;  // GPU zero-copy capture optimization
    std::string capture_target;
    std::string capture_window_title;
    std::string gstreamer_pipeline;
    int detection_resolution;
    int capture_fps;
    int monitor_idx;
    bool circle_mask;
    bool capture_borders;
    bool capture_cursor;
    std::string virtual_camera_name;
    int virtual_camera_width;
    int virtual_camera_heigth;

    // Target
    bool disable_headshot;
    float body_y_offset;
    float head_y_offset;
    bool auto_aim;
    
    // Crosshair offset (actual crosshair position relative to screen center)
    int crosshair_offset_y;  // Positive = crosshair above center, Negative = below

    // Mouse
    int fovX;
    int fovY;
    float minSpeedMultiplier;
    float maxSpeedMultiplier;

    float predictionInterval;
    int prediction_futurePositions;
    bool draw_futurePositions;

    float snapRadius;
    float nearRadius;
    float speedCurveExponent;
    float snapBoostFactor;

    bool easynorecoil;
    float easynorecoilstrength;
    int norecoil_mode;
    std::vector<std::string> button_norecoil;
    std::string input_method;

    // Wind mouse
    bool wind_mouse_enabled;
    float wind_G;
    float wind_W;
    float wind_M;
    float wind_D;

    // Arduino
    int arduino_baudrate;
    std::string arduino_port;
    bool arduino_16_bit_mouse;
    bool arduino_enable_keys;

    // kmbox_b
    int kmbox_b_baudrate;
    std::string kmbox_b_port;

    // kmbox_net
    std::string kmbox_net_ip;
    std::string kmbox_net_port;
    std::string kmbox_net_uuid;
    std::string kmbox_net_move_mode;
    int kmbox_net_move_time_ms;

    // makcu
    int makcu_baudrate;
    std::string makcu_port;

    // Mouse shooting
    bool auto_shoot;
    float bScope_multiplier;
    float auto_shoot_interval;

    // AI
    std::string backend;
    int dml_device_id;
    std::string ai_model;
    float confidence_threshold;
    float nms_threshold;
    int max_detections;
#ifdef USE_CUDA
    bool export_enable_fp8;
    bool export_enable_fp16;
#endif
    bool fixed_input_size;

    // ByteTrack
    bool enable_bytetrack;
    float bytetrack_track_thresh;
    float bytetrack_high_thresh;
    float bytetrack_match_thresh;
    int bytetrack_max_time_lost;

    // Target Lock (Hysteresis)
    bool enable_target_lock;
    float target_lock_threshold;
    int target_lock_hold_frames;

    // CUDA
#ifdef USE_CUDA
    bool use_cuda_graph;
    bool use_pinned_memory;
    int gpuMemoryReserveMB;
    bool enableGpuExclusiveMode;
#endif

    // System
    int cpuCoreReserveCount;
    int systemMemoryReserveMB;

    // Buttons
    std::vector<std::string> button_targeting;
    std::vector<std::string> button_shoot;
    std::vector<std::string> button_zoom;
    std::vector<std::string> button_exit;
    std::vector<std::string> button_pause;
    std::vector<std::string> button_reload_config;
    std::vector<std::string> button_open_overlay;
    bool enable_arrows_settings;

    // Overlay
    int overlay_opacity;
    float overlay_ui_scale;
    int overlay_theme;  // 0=CyberTeal, 1=NeonPurple, 2=OceanBlue

    // Game Overlay
    bool game_overlay_enabled;
    int game_overlay_max_fps;
    bool game_overlay_draw_boxes;
    bool game_overlay_draw_future;
    bool game_overlay_draw_frame;
    int game_overlay_box_a;
    int game_overlay_box_r;
    int game_overlay_box_g;
    int game_overlay_box_b;
    int game_overlay_frame_a;
    int game_overlay_frame_r;
    int game_overlay_frame_g;
    int game_overlay_frame_b;
    float game_overlay_box_thickness;
    float game_overlay_frame_thickness;
    float game_overlay_future_point_radius;
    float game_overlay_future_alpha_falloff;

    bool game_overlay_icon_enabled;
    std::string game_overlay_icon_path;
    int game_overlay_icon_width;
    int game_overlay_icon_height;
    float game_overlay_icon_offset_x;
    float game_overlay_icon_offset_y;
    std::string game_overlay_icon_anchor;

    void clampGameOverlayColor()
    {
        auto clamp255 = [](int& v) { if (v < 0) v = 0; if (v > 255) v = 255; };
        clamp255(game_overlay_box_a);
        clamp255(game_overlay_box_r);
        clamp255(game_overlay_box_g);
        clamp255(game_overlay_box_b);
        clamp255(game_overlay_frame_a);
        clamp255(game_overlay_frame_r);
        clamp255(game_overlay_frame_g);
        clamp255(game_overlay_frame_b);
    }

    // Classes
    int class_player;
    int class_head;

    // Debug
    bool show_window;
    bool show_fps;
    std::vector<std::string> screenshot_button;
    int screenshot_delay;
    bool verbose;

    // Dataset Collector
    bool dataset_enabled;
    std::string dataset_output_dir;
    float dataset_head_confidence;
    float dataset_body_confidence;
    int dataset_head_min_size;
    int dataset_body_min_height;
    float dataset_same_person_iou;
    double dataset_laplacian_threshold;
    float dataset_overexposure_threshold;
    float dataset_underexposure_threshold;
    int dataset_hash_buffer_size;
    float dataset_image_similarity;
    float dataset_target_iou;
    int dataset_target_displacement;
    int dataset_cooldown_ms;
    int dataset_package_size;
    bool dataset_upload_enabled;
    std::string dataset_upload_endpoint;
    
    // S3 config (OpenList compatible)
    std::string dataset_s3_endpoint;
    std::string dataset_s3_access_key;
    std::string dataset_s3_secret_key;
    std::string dataset_s3_bucket;
    std::string dataset_s3_region;
    bool dataset_s3_use_ssl;
    
    // WebDAV config
    std::string dataset_webdav_endpoint;
    std::string dataset_webdav_username;
    std::string dataset_webdav_password;
    std::string dataset_webdav_remote_path;
    bool dataset_webdav_use_ssl;

    // TGAuth 授权配置 (硬编码，不暴露给用户)
    bool auth_enabled = true;

    struct GameProfile
    {
        std::string name;
        double sens;
        double yaw;
        double pitch;
        bool fovScaled;
        double baseFOV;
    };

    std::unordered_map<std::string, GameProfile> game_profiles;
    std::string active_game;

    const GameProfile& currentProfile() const;
    std::pair<double, double> degToCounts(double degX, double degY, double fovNow) const;
    std::pair<double, double> countsToScreenPixels(double countsX, double countsY, double fovNow, double screenW, double screenH) const;

    bool loadConfig(const std::string& filename = "config.ini");
    bool saveConfig(const std::string& filename = "config.ini");

    std::string joinStrings(const std::vector<std::string>& vec, const std::string& delimiter = ",");
private:
    std::vector<std::string> splitString(const std::string& str, char delimiter = ',');
};

#endif // CONFIG_H
