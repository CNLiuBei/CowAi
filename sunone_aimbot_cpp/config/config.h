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
    std::string capture_target;
    std::string capture_window_title;
    std::string gstreamer_pipeline;
    int detection_resolution;
    int capture_fps;
    int monitor_idx;
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
    int crosshair_offset_x;  // Positive = crosshair right of center, Negative = left
    int crosshair_offset_y;  // Positive = crosshair above center, Negative = below
    
    // Aim offset (additional offset applied during aiming calculation)
    int aim_offset_x;
    int aim_offset_y;

    // Mouse
    int fovX;
    int fovY;
    float minSpeedMultiplier;
    float maxSpeedMultiplier;
    float speedMultiplierX;  // X axis speed multiplier (1.0 = normal)
    float speedMultiplierY;  // Y axis speed multiplier (1.0 = normal)

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
    int auto_shoot_fov;  // 自动开枪独立视野范围（像素）

    // AI
    std::string backend;
    std::string ai_model;
    float confidence_threshold;
    float nms_threshold;
    int max_detections;
#ifdef USE_CUDA
    bool export_enable_fp8;
    bool export_enable_fp16;
#else
    int dml_device_id;
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
    bool exit_to_minimize;  // 退出键改为最小化配置面板
    
    // Weapon slot buttons
    std::vector<std::string> button_weapon_primary;    // Primary weapon key
    std::vector<std::string> button_weapon_secondary;  // Secondary weapon key
    bool weapon_primary_lock_enabled;   // Enable target lock for primary weapon
    bool weapon_secondary_lock_enabled; // Enable target lock for secondary weapon

    // Per-weapon aim config
    struct WeaponAimConfig
    {
        int fovX = 27;
        int fovY = 27;
        float minSpeedMultiplier = 0.3f;
        float maxSpeedMultiplier = 0.4f;
        float speedMultiplierX = 1.0f;
        float speedMultiplierY = 1.0f;
        float predictionInterval = 0.01f;
        bool auto_shoot = true;
        float bScope_multiplier = 1.0f;
    };
    bool weapon_separate_config_enabled = false;
    WeaponAimConfig weapon_primary_aim;
    WeaponAimConfig weapon_secondary_aim;
    void applyWeaponSlotConfig(int slot);
    void saveWeaponSlotConfig(int slot);

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
    
    // Auto Pickup (一键拾取)
    bool auto_pickup_enabled;
    std::vector<std::string> button_auto_pickup;
    int pickup_screen_type;      // 1=1920x1080, 2=2560x1080, 3=2560x1440
    int pickup_items_per_loop;   // 每次拾取物品数量
    int pickup_delay_ms;         // 拾取间隔延迟
    
    // Weapon Recognition (武器识别)
    bool weapon_recognition_enabled;
    std::string weapon_template_path;
    float weapon_recognition_threshold;
    int weapon_scan_interval_ms;

    // TGAuth 授权配置 (硬编码，不暴露给用户)
    bool auth_enabled = true;  // 启用卡密验证

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
