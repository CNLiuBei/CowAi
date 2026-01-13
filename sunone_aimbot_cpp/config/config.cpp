#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <windows.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <filesystem>
#include <unordered_map>

#include "config.h"
#include "modules/SimpleIni.h"

std::vector<std::string> Config::splitString(const std::string& str, char delimiter)
{
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, delimiter))
    {
        while (!item.empty() && (item.front() == ' ' || item.front() == '\t'))
            item.erase(item.begin());
        while (!item.empty() && (item.back() == ' ' || item.back() == '\t'))
            item.pop_back();

        tokens.push_back(item);
    }
    return tokens;
}

std::string Config::joinStrings(const std::vector<std::string>& vec, const std::string& delimiter)
{
    std::ostringstream oss;
    for (size_t i = 0; i < vec.size(); ++i)
    {
        if (i != 0) oss << delimiter;
        oss << vec[i];
    }
    return oss.str();
}

bool Config::loadConfig(const std::string& filename)
{
    if (!std::filesystem::exists(filename))
    {
        std::cerr << "[Config] Config file does not exist, creating default config: " << filename << std::endl;

        // Capture
        capture_method = "duplication_api";
        gpu_zero_copy = true;
        capture_target = "monitor";
        capture_window_title = "";
        detection_resolution = 320;
        capture_fps = 91;
        monitor_idx = 0;
        circle_mask = false;
        capture_borders = true;
        capture_cursor = true;
        virtual_camera_name = "None";
        virtual_camera_width = 1920;
        virtual_camera_heigth = 1080;

        // Target
        disable_headshot = false;
        body_y_offset = 0.35f;
        head_y_offset = 0.35f;
        auto_aim = false;
        crosshair_offset_y = 0;  // Default: crosshair at screen center

        // Mouse
        fovX = 80;
        fovY = 65;
        minSpeedMultiplier = 0.20f;
        maxSpeedMultiplier = 0.10f;

        predictionInterval = 0.01f;
        prediction_futurePositions = 20;
        draw_futurePositions = false;

        snapRadius = 1.5f;
        nearRadius = 25.0f;
        speedCurveExponent = 3.0f;
        snapBoostFactor = 1.15f;

        easynorecoil = true;
        easynorecoilstrength = 3.8f;
        norecoil_mode = 0;
        button_norecoil = splitString("None");
        input_method = "KMBOX_NET";

        // Wind mouse
        wind_mouse_enabled = false;
        wind_G = 18.0f;
        wind_W = 15.0f;
        wind_M = 10.0f;
        wind_D = 8.0f;

        // Arduino
        arduino_baudrate = 115200;
        arduino_port = "COM0";
        arduino_16_bit_mouse = false;
        arduino_enable_keys = false;

        // kmbox_B
        kmbox_b_baudrate = 115200;
        kmbox_b_port = "COM0";

        // kmbox_net
        kmbox_net_ip = "192.168.2.188";
        kmbox_net_port = "1536";
        kmbox_net_uuid = "66407019";
        kmbox_net_move_mode = "normal";
        kmbox_net_move_time_ms = 5;

        // makcu
        makcu_baudrate = 115200;
        makcu_port = "COM0";

        // Mouse shooting
        auto_shoot = true;
        bScope_multiplier = 1.0f;
        auto_shoot_interval = 52.0f;

        // AI
#ifdef USE_CUDA
        backend = "TRT";
#else
        backend = "DML";
#endif
        dml_device_id = 0;

#ifdef USE_CUDA
        ai_model = "PUBG8.engine";
#else
        ai_model = "PUBG8.onnx";
#endif

        confidence_threshold = 0.51f;
        nms_threshold = 0.50f;
        max_detections = 1;
#ifdef USE_CUDA
        export_enable_fp8 = false;
        export_enable_fp16 = true;
#endif
        fixed_input_size = true;

        // ByteTrack
        enable_bytetrack = true;
        bytetrack_track_thresh = 0.0f;
        bytetrack_high_thresh = 1.0f;
        bytetrack_match_thresh = 1.0f;
        bytetrack_max_time_lost = 30;

        // Target Lock
        enable_target_lock = true;
        target_lock_threshold = 2.0f;
        target_lock_hold_frames = 15;

        // CUDA
#ifdef USE_CUDA
        use_cuda_graph = false;
        use_pinned_memory = false;
        gpuMemoryReserveMB = 0;
        enableGpuExclusiveMode = true;
#endif

        // System
        cpuCoreReserveCount = 4;
        systemMemoryReserveMB = 0;

        // Buttons
        button_targeting = splitString("RightMouseButton");
        button_shoot = splitString("J,Shift+RightMouseButton");
        button_zoom = splitString("None");
        button_exit = splitString("F2");
        button_pause = splitString("F3");
        button_reload_config = splitString("F4");
        button_open_overlay = splitString("Home");
        enable_arrows_settings = false;

        // Overlay
        overlay_opacity = 225;
        overlay_ui_scale = 1.0f;
        overlay_theme = 0;

        // Game overlay
        game_overlay_enabled = false;
        game_overlay_max_fps = 0;
        game_overlay_draw_boxes = true;
        game_overlay_draw_future = true;
        game_overlay_draw_frame = true;
        game_overlay_box_a = 255;
        game_overlay_box_r = 0;
        game_overlay_box_g = 255;
        game_overlay_box_b = 0;
        game_overlay_frame_a = 180;
        game_overlay_frame_r = 255;
        game_overlay_frame_g = 255;
        game_overlay_frame_b = 255;
        game_overlay_box_thickness = 2.0f;
        game_overlay_frame_thickness = 1.5f;
        game_overlay_future_point_radius = 5.0f;
        game_overlay_future_alpha_falloff = 1.0f;

        game_overlay_icon_enabled = false;
        game_overlay_icon_path = "icon.png";
        game_overlay_icon_width = 64;
        game_overlay_icon_height = 64;
        game_overlay_icon_offset_x = 0.0f;
        game_overlay_icon_offset_y = 0.0f;
        game_overlay_icon_anchor = "center";

        // Classes
        class_player = 1;
        class_head = 0;

        // Debug
        show_window = true;
        show_fps = false;
        screenshot_button = splitString("None");
        screenshot_delay = 500;
        verbose = false;

        // Dataset Collector
        dataset_enabled = true;
        dataset_output_dir = "dataset";
        dataset_head_confidence = 0.45f;
        dataset_body_confidence = 0.50f;
        dataset_head_min_size = 4;
        dataset_body_min_height = 8;
        dataset_same_person_iou = 0.2f;
        dataset_laplacian_threshold = 100.0;
        dataset_overexposure_threshold = 0.70f;
        dataset_underexposure_threshold = 0.70f;
        dataset_hash_buffer_size = 1000;
        dataset_image_similarity = 0.75f;
        dataset_target_iou = 0.60f;
        dataset_target_displacement = 20;
        dataset_cooldown_ms = 200;
        dataset_package_size = 200;
        dataset_upload_enabled = false;
        dataset_upload_endpoint = "";
        
        // S3 默认配置
        dataset_s3_endpoint = "";
        dataset_s3_access_key = "";
        dataset_s3_secret_key = "";
        dataset_s3_bucket = "";
        dataset_s3_region = "us-east-1";
        dataset_s3_use_ssl = true;
        
        // WebDAV 默认配置
        dataset_webdav_endpoint = "https://openlist.trueliu.com/dav";
        dataset_webdav_username = "cowaim";
        dataset_webdav_password = "cowaim";
        dataset_webdav_remote_path = "/aim";
        dataset_webdav_use_ssl = true;
        dataset_upload_enabled = true;

        // Game profiles
        game_profiles.clear();
        GameProfile uni;
        uni.name = "UNIFIED";
        uni.sens = 3.5;
        uni.yaw = 0.022;
        uni.pitch = uni.yaw;
        uni.fovScaled = false;
        uni.baseFOV = 0.0;
        game_profiles[uni.name] = uni;
        active_game = uni.name;

        saveConfig(filename);
        return true;
    }

    CSimpleIniA ini;
    ini.SetUnicode();
    SI_Error rc = ini.LoadFile(filename.c_str());
    if (rc < 0)
    {
        std::cerr << "[Config] Error parsing INI file: " << filename << std::endl;
        return false;
    }

    auto get_string = [&](const char* key, const std::string& defval)
    {
        const char* val = ini.GetValue("", key, defval.c_str());
        return val ? std::string(val) : defval;
    };

    auto get_bool = [&](const char* key, bool defval)
        {
            return ini.GetBoolValue("", key, defval);
        };

    auto get_long = [&](const char* key, long defval)
        {
            return (int)ini.GetLongValue("", key, defval);
        };

    auto get_double = [&](const char* key, double defval)
        {
            return ini.GetDoubleValue("", key, defval);
        };

    game_profiles.clear();

    CSimpleIniA::TNamesDepend keys;
    ini.GetAllKeys("Games", keys);

    for (const auto& k : keys)
    {
        std::string name = k.pItem;
        std::string val = ini.GetValue("Games", k.pItem, "");
        auto parts = splitString(val, ',');

        try
        {
            if (parts.size() < 2)
                throw std::runtime_error("not enough values");

            GameProfile gp;
            gp.name = name;
            gp.sens = std::stod(parts[0]);
            gp.yaw = std::stod(parts[1]);
            gp.pitch = parts.size() > 2 ? std::stod(parts[2]) : gp.yaw;
            gp.fovScaled = parts.size() > 3 && (parts[3] == "true" || parts[3] == "1");
            gp.baseFOV = parts.size() > 4 ? std::stod(parts[4]) : 0.0;

            game_profiles[name] = gp;
        }
        catch (const std::exception& e)
        {
            std::cerr << "[Config] Failed to parse profile: " << name
                << " = " << val << " (" << e.what() << ")" << std::endl;
        }
    }

    if (!game_profiles.count("UNIFIED"))
    {
        GameProfile uni;
        uni.name = "UNIFIED";
        uni.sens = 3.5;
        uni.yaw = 0.022;
        uni.pitch = uni.yaw;
        uni.fovScaled = false;
        uni.baseFOV = 0.0;
        game_profiles[uni.name] = uni;
    }

    active_game = get_string("active_game", active_game);
    if (!game_profiles.count(active_game) && !game_profiles.empty())
        active_game = game_profiles.begin()->first;

    // Capture
    capture_method = get_string("capture_method", "duplication_api");
    gpu_zero_copy = get_bool("gpu_zero_copy", true);
    capture_target = get_string("capture_target", "monitor");
    capture_window_title = get_string("capture_window_title", "");
    gstreamer_pipeline = get_string("gstreamer_pipeline", "");
    detection_resolution = get_long("detection_resolution", 320);
    if (detection_resolution != 160 && detection_resolution != 320 && detection_resolution != 640)
        detection_resolution = 320;

    capture_fps = get_long("capture_fps", 60);
    monitor_idx = get_long("monitor_idx", 0);
    circle_mask = get_bool("circle_mask", true);
    capture_borders = get_bool("capture_borders", true);
    capture_cursor = get_bool("capture_cursor", true);
    virtual_camera_name = get_string("virtual_camera_name", "None");
    virtual_camera_width = get_long("virtual_camera_width", 1920);
    virtual_camera_heigth = get_long("virtual_camera_heigth", 1080);

    // Target
    disable_headshot = get_bool("disable_headshot", false);
    body_y_offset = (float)get_double("body_y_offset", 0.15);
    head_y_offset = (float)get_double("head_y_offset", 0.05);
    auto_aim = get_bool("auto_aim", false);
    crosshair_offset_y = get_long("crosshair_offset_y", 0);

    // Mouse
    fovX = get_long("fovX", 106);
    fovY = get_long("fovY", 74);
    minSpeedMultiplier = (float)get_double("minSpeedMultiplier", 0.1);
    maxSpeedMultiplier = (float)get_double("maxSpeedMultiplier", 0.1);

    predictionInterval = (float)get_double("predictionInterval", 0.01);
    prediction_futurePositions = get_long("prediction_futurePositions", 20);
    draw_futurePositions = get_bool("draw_futurePositions", true);
    
    snapRadius = (float)get_double("snapRadius", 1.5);
    nearRadius = (float)get_double("nearRadius", 25.0);
    speedCurveExponent = (float)get_double("speedCurveExponent", 3.0);
    snapBoostFactor = (float)get_double("snapBoostFactor", 1.15);

    easynorecoil = get_bool("easynorecoil", false);
    easynorecoilstrength = (float)get_double("easynorecoilstrength", 0.0);
    norecoil_mode = get_long("norecoil_mode", 0);
    button_norecoil = splitString(get_string("button_norecoil", "LeftMouseButton"));
    input_method = get_string("input_method", "WIN32");

    // Wind mouse
    wind_mouse_enabled = get_bool("wind_mouse_enabled", false);
    wind_G = (float)get_double("wind_G", 18.0f);
    wind_W = (float)get_double("wind_W", 15.0f);
    wind_M = (float)get_double("wind_M", 10.0f);
    wind_D = (float)get_double("wind_D", 8.0f);

    // Arduino
    arduino_baudrate = get_long("arduino_baudrate", 115200);
    arduino_port = get_string("arduino_port", "COM0");
    arduino_16_bit_mouse = get_bool("arduino_16_bit_mouse", false);
    arduino_enable_keys = get_bool("arduino_enable_keys", false);

    // kmbox_B
    kmbox_b_baudrate = get_long("kmbox_baudrate", 115200);
    kmbox_b_port = get_string("kmbox_port", "COM0");

    // kmbox_net
    kmbox_net_ip = get_string("kmbox_net_ip", "10.42.42.42");
    kmbox_net_port = get_string("kmbox_net_port", "1984");
    kmbox_net_uuid = get_string("kmbox_net_uuid", "DEADC0DE");

    // makcu
    makcu_baudrate = get_long("makcu_baudrate", 115200);
    makcu_port = get_string("makcu_port", "COM0");

    // Mouse shooting
    auto_shoot = get_bool("auto_shoot", false);
    bScope_multiplier = (float)get_double("bScope_multiplier", 1.2);
    auto_shoot_interval = (float)get_double("auto_shoot_interval", 100.0);

    // AI
#ifdef USE_CUDA
    backend = get_string("backend", "TRT");
#else
    backend = get_string("backend", "DML");
#endif

    dml_device_id = get_long("dml_device_id", 0);

#ifdef USE_CUDA
    ai_model = get_string("ai_model", "sunxds_0.5.6.engine");
#else
    ai_model = get_string("ai_model", "sunxds_0.5.6.onnx");
#endif
    confidence_threshold = (float)get_double("confidence_threshold", 0.15);
    nms_threshold = (float)get_double("nms_threshold", 0.50);
    max_detections = get_long("max_detections", 20);
#ifdef USE_CUDA
    export_enable_fp8 = get_bool("export_enable_fp8", true);
    export_enable_fp16 = get_bool("export_enable_fp16", true);
#endif
    fixed_input_size = get_bool("fixed_input_size", false);

    // ByteTrack
    enable_bytetrack = get_bool("enable_bytetrack", true);
    bytetrack_track_thresh = (float)get_double("bytetrack_track_thresh", 0.5);
    bytetrack_high_thresh = (float)get_double("bytetrack_high_thresh", 0.6);
    bytetrack_match_thresh = (float)get_double("bytetrack_match_thresh", 0.8);
    bytetrack_max_time_lost = get_long("bytetrack_max_time_lost", 30);

    // Target Lock
    enable_target_lock = get_bool("enable_target_lock", true);
    target_lock_threshold = (float)get_double("target_lock_threshold", 1.5);
    target_lock_hold_frames = get_long("target_lock_hold_frames", 15);

    // CUDA
#ifdef USE_CUDA
    use_cuda_graph = get_bool("use_cuda_graph", false);
    use_pinned_memory = get_bool("use_pinned_memory", true);
    gpuMemoryReserveMB = get_long("gpuMemoryReserveMB", 0);
    enableGpuExclusiveMode = get_bool("enableGpuExclusiveMode", true);
#endif

    // System
    cpuCoreReserveCount = get_long("cpuCoreReserveCount", 4);
    systemMemoryReserveMB = get_long("systemMemoryReserveMB", 0);

    // Buttons
    button_targeting = splitString(get_string("button_targeting", "RightMouseButton"));
    button_shoot = splitString(get_string("button_shoot", "LeftMouseButton"));
    button_zoom = splitString(get_string("button_zoom", "RightMouseButton"));
    button_exit = splitString(get_string("button_exit", "F2"));
    button_pause = splitString(get_string("button_pause", "F3"));
    button_reload_config = splitString(get_string("button_reload_config", "F4"));
    button_open_overlay = splitString(get_string("button_open_overlay", "Home"));
    enable_arrows_settings = get_bool("enable_arrows_settings", false);

    // Overlay
    overlay_opacity = get_long("overlay_opacity", 225);
    overlay_ui_scale = (float)get_double("overlay_ui_scale", 1.0);
    overlay_theme = get_long("overlay_theme", 0);

    game_overlay_enabled = get_bool("game_overlay_enabled", false);
    game_overlay_max_fps = get_long("game_overlay_max_fps", 0);
    game_overlay_draw_boxes = get_bool("game_overlay_draw_boxes", true);
    game_overlay_draw_future = get_bool("game_overlay_draw_future", true);
    game_overlay_draw_frame = get_bool("game_overlay_draw_frame", true);
    game_overlay_box_a = get_long("game_overlay_box_a", 255);
    game_overlay_box_r = get_long("game_overlay_box_r", 0);
    game_overlay_box_g = get_long("game_overlay_box_g", 255);
    game_overlay_box_b = get_long("game_overlay_box_b", 0);
    game_overlay_frame_a = get_long("game_overlay_frame_a", 180);
    game_overlay_frame_r = get_long("game_overlay_frame_r", 255);
    game_overlay_frame_g = get_long("game_overlay_frame_g", 255);
    game_overlay_frame_b = get_long("game_overlay_frame_b", 255);
    game_overlay_box_thickness = (float)get_double("game_overlay_box_thickness", 2.0);
    game_overlay_frame_thickness = (float)get_double("game_overlay_frame_thickness", 1.5);
    game_overlay_future_point_radius = (float)get_double("game_overlay_future_point_radius", 5.0);
    game_overlay_future_alpha_falloff = (float)get_double("game_overlay_future_alpha_falloff", 1.0);
    clampGameOverlayColor();

    game_overlay_icon_enabled = get_bool("game_overlay_icon_enabled", false);
    game_overlay_icon_path = get_string("game_overlay_icon_path", "icon.png");
    game_overlay_icon_width = get_long("game_overlay_icon_width", 64);
    game_overlay_icon_height = get_long("game_overlay_icon_height", 64);
    game_overlay_icon_offset_x = (float)get_double("game_overlay_icon_offset_x", 0.0f);
    game_overlay_icon_offset_y = (float)get_double("game_overlay_icon_offset_y", 0.0f);
    game_overlay_icon_anchor = get_string("game_overlay_icon_anchor", "center");

    // Classes
    class_player = get_long("class_player", 0);
    class_head = get_long("class_head", 1);

    // Debug window
    show_window = get_bool("show_window", true);
    screenshot_button = splitString(get_string("screenshot_button", "None"));
    screenshot_delay = get_long("screenshot_delay", 500);
    verbose = get_bool("verbose", false);

    // Dataset Collector
    dataset_enabled = get_bool("dataset_enabled", true);
    dataset_output_dir = get_string("dataset_output_dir", "dataset");
    dataset_head_confidence = (float)get_double("dataset_head_confidence", 0.45);
    dataset_body_confidence = (float)get_double("dataset_body_confidence", 0.50);
    dataset_head_min_size = get_long("dataset_head_min_size", 4);
    dataset_body_min_height = get_long("dataset_body_min_height", 8);
    dataset_same_person_iou = (float)get_double("dataset_same_person_iou", 0.2);
    dataset_laplacian_threshold = get_double("dataset_laplacian_threshold", 100.0);
    dataset_overexposure_threshold = (float)get_double("dataset_overexposure_threshold", 0.70);
    dataset_underexposure_threshold = (float)get_double("dataset_underexposure_threshold", 0.70);
    dataset_hash_buffer_size = get_long("dataset_hash_buffer_size", 500);
    dataset_image_similarity = (float)get_double("dataset_image_similarity", 0.90);
    dataset_target_iou = (float)get_double("dataset_target_iou", 0.85);
    dataset_target_displacement = get_long("dataset_target_displacement", 10);
    dataset_cooldown_ms = get_long("dataset_cooldown_ms", 200);
    dataset_package_size = get_long("dataset_package_size", 200);
    dataset_upload_enabled = get_bool("dataset_upload_enabled", true);
    dataset_upload_endpoint = get_string("dataset_upload_endpoint", "");
    
    // S3 配置
    dataset_s3_endpoint = get_string("dataset_s3_endpoint", "");
    dataset_s3_access_key = get_string("dataset_s3_access_key", "");
    dataset_s3_secret_key = get_string("dataset_s3_secret_key", "");
    dataset_s3_bucket = get_string("dataset_s3_bucket", "");
    dataset_s3_region = get_string("dataset_s3_region", "us-east-1");
    dataset_s3_use_ssl = get_bool("dataset_s3_use_ssl", true);
    
    // WebDAV 配置
    dataset_webdav_endpoint = get_string("dataset_webdav_endpoint", "https://openlist.trueliu.com/dav");
    dataset_webdav_username = get_string("dataset_webdav_username", "cowaim");
    dataset_webdav_password = get_string("dataset_webdav_password", "cowaim");
    dataset_webdav_remote_path = get_string("dataset_webdav_remote_path", "/aim");
    dataset_webdav_use_ssl = get_bool("dataset_webdav_use_ssl", true);

    return true;
}

bool Config::saveConfig(const std::string& filename)
{
    std::ofstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Error opening config for writing: " << filename << std::endl;
        return false;
    }

    file << "# An explanation of the options can be found at:\n";
    file << "# https://github.com/SunOner/sunone_aimbot_docs/blob/main/config/config_cpp.md\n\n";

    // Capture
    file << "# Capture\n"
        << "capture_method = " << capture_method << "\n"
        << "gpu_zero_copy = " << (gpu_zero_copy ? "true" : "false") << "\n"
        << "capture_target = " << capture_target << "\n"
        << "capture_window_title = " << capture_window_title << "\n"
        << "gstreamer_pipeline = " << gstreamer_pipeline << "\n"
        << "detection_resolution = " << detection_resolution << "\n"
        << "capture_fps = " << capture_fps << "\n"
        << "monitor_idx = " << monitor_idx << "\n"
        << "circle_mask = " << (circle_mask ? "true" : "false") << "\n"
        << "capture_borders = " << (capture_borders ? "true" : "false") << "\n"
        << "capture_cursor = " << (capture_cursor ? "true" : "false") << "\n"
        << "virtual_camera_name = " << virtual_camera_name << "\n"
        << "virtual_camera_width = " << virtual_camera_width << "\n"
        << "virtual_camera_heigth = " << virtual_camera_heigth << "\n\n";

    // Target
    file << "# Target\n"
        << "disable_headshot = " << (disable_headshot ? "true" : "false") << "\n"
        << std::fixed << std::setprecision(2)
        << "body_y_offset = " << body_y_offset << "\n"
        << "head_y_offset = " << head_y_offset << "\n"
        << "auto_aim = " << (auto_aim ? "true" : "false") << "\n"
        << "crosshair_offset_y = " << crosshair_offset_y << "\n\n";

    // Mouse
    file << "# Mouse move\n"
        << "fovX = " << fovX << "\n"
        << "fovY = " << fovY << "\n"
        << "minSpeedMultiplier = " << minSpeedMultiplier << "\n"
        << "maxSpeedMultiplier = " << maxSpeedMultiplier << "\n"

        << std::fixed << std::setprecision(2)
        << "predictionInterval = " << predictionInterval << "\n"
        << "prediction_futurePositions = " << prediction_futurePositions << "\n"
        << "draw_futurePositions = " << (draw_futurePositions ? "true" : "false") << "\n"

        << "snapRadius = " << snapRadius << "\n"
        << "nearRadius = " << nearRadius << "\n"
        << "speedCurveExponent = " << speedCurveExponent << "\n"
        << std::fixed << std::setprecision(2)
        << "snapBoostFactor = " << snapBoostFactor << "\n"

        << "easynorecoil = " << (easynorecoil ? "true" : "false") << "\n"
        << std::fixed << std::setprecision(1)
        << "easynorecoilstrength = " << easynorecoilstrength << "\n"
        << "norecoil_mode = " << norecoil_mode << "\n"
        << "button_norecoil = " << joinStrings(button_norecoil) << "\n"

        << "# WIN32, GHUB, ARDUINO, KMBOX_B, KMBOX_NET, MAKCU\n"
        << "input_method = " << input_method << "\n\n";

    // Wind mouse
    file << "# Wind mouse\n"
        << "wind_mouse_enabled = " << (wind_mouse_enabled ? "true" : "false") << "\n"
        << "wind_G = " << wind_G << "\n"
        << "wind_W = " << wind_W << "\n"
        << "wind_M = " << wind_M << "\n"
        << "wind_D = " << wind_D << "\n\n";

    // Arduino
    file << "# Arduino\n"
        << "arduino_baudrate = " << arduino_baudrate << "\n"
        << "arduino_port = " << arduino_port << "\n"
        << "arduino_16_bit_mouse = " << (arduino_16_bit_mouse ? "true" : "false") << "\n"
        << "arduino_enable_keys = " << (arduino_enable_keys ? "true" : "false") << "\n\n";

    // kmbox_B
    file << "# Kmbox_B\n"
        << "kmbox_baudrate = " << kmbox_b_baudrate << "\n"
        << "kmbox_port = " << kmbox_b_port << "\n\n";

    // kmbox_net
    file << "# Kmbox_net\n"
        << "kmbox_net_ip = " << kmbox_net_ip << "\n"
        << "kmbox_net_port = " << kmbox_net_port << "\n"
        << "kmbox_net_uuid = " << kmbox_net_uuid << "\n\n";

    // makcu
    file << "# Makcu\n"
        << "makcu_baudrate = " << makcu_baudrate << "\n"
		<< "makcu_port = " << makcu_port << "\n\n";

    // Mouse shooting
    file << "# Mouse shooting\n"
        << "auto_shoot = " << (auto_shoot ? "true" : "false") << "\n"
        << std::fixed << std::setprecision(1)
        << "bScope_multiplier = " << bScope_multiplier << "\n"
        << "auto_shoot_interval = " << auto_shoot_interval << "\n\n";

    // AI
    file << "# AI\n"
        << "backend = " << backend << "\n"
        << "dml_device_id = " << dml_device_id << "\n"
        << "ai_model = " << ai_model << "\n"
        << std::fixed << std::setprecision(2)
        << "confidence_threshold = " << confidence_threshold << "\n"
        << "nms_threshold = " << nms_threshold << "\n"
        << std::setprecision(0)
        << "max_detections = " << max_detections << "\n"
#ifdef USE_CUDA
        << "export_enable_fp8 = " << (export_enable_fp8 ? "true" : "false") << "\n"
        << "export_enable_fp16 = " << (export_enable_fp16 ? "true" : "false") << "\n"
#endif
        << "fixed_input_size = " << (fixed_input_size ? "true" : "false") << "\n"
        << "enable_bytetrack = " << (enable_bytetrack ? "true" : "false") << "\n"
        << "bytetrack_track_thresh = " << bytetrack_track_thresh << "\n"
        << "bytetrack_high_thresh = " << bytetrack_high_thresh << "\n"
        << "bytetrack_match_thresh = " << bytetrack_match_thresh << "\n"
        << "bytetrack_max_time_lost = " << bytetrack_max_time_lost << "\n"
        << "enable_target_lock = " << (enable_target_lock ? "true" : "false") << "\n"
        << "target_lock_threshold = " << target_lock_threshold << "\n"
        << "target_lock_hold_frames = " << target_lock_hold_frames << "\n";

    // CUDA
#ifdef USE_CUDA
    file << "\n# CUDA\n"
        << "use_cuda_graph = " << (use_cuda_graph ? "true" : "false") << "\n"
        << "use_pinned_memory = " << (use_pinned_memory ? "true" : "false") << "\n"
        << "gpuMemoryReserveMB = " << gpuMemoryReserveMB << "\n"
        << "enableGpuExclusiveMode = " << (enableGpuExclusiveMode ? "true" : "false") << "\n\n";
#endif

	// System
    file << "# System\n"
        << "cpuCoreReserveCount = " << cpuCoreReserveCount << "\n"
        << "systemMemoryReserveMB = " << systemMemoryReserveMB << "\n\n";

    // Buttons
    file << "# Buttons\n"
        << "button_targeting = " << joinStrings(button_targeting) << "\n"
        << "button_shoot = " << joinStrings(button_shoot) << "\n"
        << "button_zoom = " << joinStrings(button_zoom) << "\n"
        << "button_exit = " << joinStrings(button_exit) << "\n"
        << "button_pause = " << joinStrings(button_pause) << "\n"
        << "button_reload_config = " << joinStrings(button_reload_config) << "\n"
        << "button_open_overlay = " << joinStrings(button_open_overlay) << "\n"
        << "enable_arrows_settings = " << (enable_arrows_settings ? "true" : "false") << "\n\n";

    // Overlay
    file << "# Overlay\n"
        << "overlay_opacity = " << overlay_opacity << "\n"
        << std::fixed << std::setprecision(2)
        << "overlay_ui_scale = " << overlay_ui_scale << "\n"
        << "overlay_theme = " << overlay_theme << "\n\n";

    file << "# Game Overlay\n"
        << "game_overlay_enabled = " << (game_overlay_enabled ? "true" : "false") << "\n"
        << "game_overlay_max_fps = " << game_overlay_max_fps << "\n"
        << "game_overlay_draw_boxes = " << (game_overlay_draw_boxes ? "true" : "false") << "\n"
        << "game_overlay_draw_future = " << (game_overlay_draw_future ? "true" : "false") << "\n"
        << "game_overlay_draw_frame = " << (game_overlay_draw_frame ? "true" : "false") << "\n"
        << "game_overlay_box_a = " << game_overlay_box_a << "\n"
        << "game_overlay_box_r = " << game_overlay_box_r << "\n"
        << "game_overlay_box_g = " << game_overlay_box_g << "\n"
        << "game_overlay_box_b = " << game_overlay_box_b << "\n"
        << "game_overlay_frame_a = " << game_overlay_frame_a << "\n"
        << "game_overlay_frame_r = " << game_overlay_frame_r << "\n"
        << "game_overlay_frame_g = " << game_overlay_frame_g << "\n"
        << "game_overlay_frame_b = " << game_overlay_frame_b << "\n"
        << std::fixed << std::setprecision(2)
        << "game_overlay_box_thickness = " << game_overlay_box_thickness << "\n"
        << "game_overlay_frame_thickness = " << game_overlay_frame_thickness << "\n"
        << "game_overlay_future_point_radius = " << game_overlay_future_point_radius << "\n"
        << "game_overlay_future_alpha_falloff = " << game_overlay_future_alpha_falloff << "\n\n";

    file << "game_overlay_icon_enabled = " << (game_overlay_icon_enabled ? "true" : "false") << "\n"
        << "game_overlay_icon_path = " << game_overlay_icon_path << "\n"
        << "game_overlay_icon_width = " << game_overlay_icon_width << "\n"
        << "game_overlay_icon_height = " << game_overlay_icon_height << "\n"
        << std::fixed << std::setprecision(2)
        << "game_overlay_icon_offset_x = " << game_overlay_icon_offset_x << "\n"
        << std::fixed << std::setprecision(2)
        << "game_overlay_icon_offset_y = " << game_overlay_icon_offset_y << "\n"
        << "game_overlay_icon_anchor = " << game_overlay_icon_anchor << "\n\n";

    // Classes
    file << "# Custom Classes\n"
        << "class_player = " << class_player << "\n"
        << "class_head = " << class_head << "\n\n";

    // Debug
    file << "# Debug\n"
        << "show_window = " << (show_window ? "true" : "false") << "\n"
        << "show_fps = " << (show_fps ? "true" : "false") << "\n"
        << "screenshot_button = " << joinStrings(screenshot_button) << "\n"
        << "screenshot_delay = " << screenshot_delay << "\n"
        << "verbose = " << (verbose ? "true" : "false") << "\n\n";

    // Active game
    file << "# Active game profile\n";
    file << "active_game = " << active_game << "\n\n";
    file << "[Games]\n";
    for (auto& kv : game_profiles)
    {
        auto & gp = kv.second;
        file << gp.name << " = "
             << gp.sens << "," << gp.yaw;
        file << "," << gp.pitch;
        if (gp.fovScaled)
            file << ",true," << gp.baseFOV;
        file << "\n";
    }

    file.close();
    return true;
}

const Config::GameProfile& Config::currentProfile() const
{
    auto it = game_profiles.find(active_game);
    if (it != game_profiles.end()) return it->second;
    throw std::runtime_error("Active game profile not found: " + active_game);
}

std::pair<double, double> Config::degToCounts(double degX, double degY, double fovNow) const
{
    const auto& gp = currentProfile();
    double scale = (gp.fovScaled && gp.baseFOV > 1.0) ? (fovNow / gp.baseFOV) : 1.0;

    if (gp.sens == 0.0 || gp.yaw == 0.0 || gp.pitch == 0.0)
        return { 0.0, 0.0 };

    double cx = degX / (gp.sens * gp.yaw * scale);
    double cy = degY / (gp.sens * gp.pitch * scale);
    return { cx, cy };
}

std::pair<double, double> Config::countsToScreenPixels(double countsX, double countsY, double fovNow, double screenW, double screenH) const
{
    // Reverse of degToCounts + pixel conversion
    // counts -> degrees -> screen pixels
    const auto& gp = currentProfile();
    double scale = (gp.fovScaled && gp.baseFOV > 1.0) ? (fovNow / gp.baseFOV) : 1.0;

    if (gp.sens == 0.0 || gp.yaw == 0.0 || gp.pitch == 0.0)
        return { 0.0, 0.0 };

    // counts to degrees (reverse of degToCounts)
    double degX = countsX * gp.sens * gp.yaw * scale;
    double degY = countsY * gp.sens * gp.pitch * scale;

    // degrees to screen pixels
    double degPerPxX = fovNow / screenW;
    double degPerPxY = fovNow / screenH;  // Assuming same FOV for Y, adjust if needed

    double pixelsX = degX / degPerPxX;
    double pixelsY = degY / degPerPxY;

    return { pixelsX, pixelsY };
}