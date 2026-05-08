#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <winsock2.h>
#include <Windows.h>

#include <shellapi.h>
#include <atomic>

#include "imgui/imgui.h"
#include <imgui_internal.h>

#include "sunone_aimbot_cpp.h"
#include "include/other_tools.h"
#include "kmbox_net/picture.h"
#include "keyboard/keycodes.h"
#include "custom_widgets.h"

using namespace CustomWidgets;

std::string ghub_version = get_ghub_version();

int prev_fovX = config.fovX;
int prev_fovY = config.fovY;
float prev_minSpeedMultiplier = config.minSpeedMultiplier;
float prev_maxSpeedMultiplier = config.maxSpeedMultiplier;
float prev_predictionInterval = config.predictionInterval;
float prev_snapRadius = config.snapRadius;
float prev_nearRadius = config.nearRadius;
float prev_speedCurveExponent = config.speedCurveExponent;
float prev_snapBoostFactor = config.snapBoostFactor;

bool  prev_wind_mouse_enabled = config.wind_mouse_enabled;
float prev_wind_G = config.wind_G;
float prev_wind_W = config.wind_W;
float prev_wind_M = config.wind_M;
float prev_wind_D = config.wind_D;

bool prev_auto_shoot = config.auto_shoot;
float prev_bScope_multiplier = config.bScope_multiplier;

static void draw_target_correction_demo()
{
    if (ImGui::CollapsingHeader(u8"可视化演示"))
    {
        ImVec2 canvas_sz(220, 220);
        ImGui::InvisibleButton("##tc_canvas", canvas_sz);

        ImVec2 p0 = ImGui::GetItemRectMin();
        ImVec2 p1 = ImGui::GetItemRectMax();
        ImVec2 center{ (p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f };

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p0, p1, IM_COL32(25, 25, 25, 255));

        const float scale = 4.0f;
        float near_px = config.nearRadius * scale;
        float snap_px = config.snapRadius * scale;
        near_px = ImClamp(near_px, 10.0f, canvas_sz.x * 0.45f);
        snap_px = ImClamp(snap_px, 6.0f, near_px - 4.0f);

        dl->AddCircle(center, near_px, IM_COL32(80, 120, 255, 180), 64, 2.0f);
        dl->AddCircle(center, snap_px, IM_COL32(255, 100, 100, 180), 64, 2.0f);

        static float  dist_px = 0.0f;
        static float  vel_px = 0.0f;
        static double last_t = ImGui::GetTime();
        double now = ImGui::GetTime();
        double dt = now - last_t;
        last_t = now;
        dt = ImClamp(dt, 0.0, 0.1);

        if (dist_px <= 0.0f || dist_px > near_px)
            dist_px = near_px;

        double dist_units = dist_px / scale;
        double speed_mult;
        if (dist_units < config.snapRadius)
            speed_mult = config.minSpeedMultiplier * config.snapBoostFactor;
        else if (dist_units < config.nearRadius)
        {
            double t = dist_units / config.nearRadius;
            double crv = 1.0 - pow(1.0 - t, config.speedCurveExponent);
            speed_mult = config.minSpeedMultiplier +
                (config.maxSpeedMultiplier - config.minSpeedMultiplier) * crv;
        }
        else
        {
            double norm = ImClamp(dist_units / config.nearRadius, 0.0, 1.0);
            speed_mult = config.minSpeedMultiplier +
                (config.maxSpeedMultiplier - config.minSpeedMultiplier) * norm;
        }

        float max_multiplier = ImMax(0.1f, config.maxSpeedMultiplier);
        float demo_duration_s = ImClamp(2.2f / max_multiplier, 0.6f, 3.0f);
        float base_px_s = near_px / demo_duration_s;
        vel_px = base_px_s * static_cast<float>(speed_mult);
        dist_px -= vel_px * static_cast<float>(dt);
        if (dist_px <= 0.0f) dist_px = near_px;

        ImVec2 dot{ center.x - dist_px, center.y };
        dl->AddCircleFilled(dot, 4.0f, IM_COL32(255, 255, 80, 255));

        ImGui::Dummy(ImVec2(0, 4));
        ImGui::TextColored(ImVec4(0.31f, 0.48f, 1.0f, 1.0f), u8"近距半径");
        ImGui::SameLine(130);
        ImGui::TextColored(ImVec4(1.0f, 0.39f, 0.39f, 1.0f), u8"吸附半径");
    }
}

void draw_mouse()
{
    ImGui::SeparatorText(u8"视野范围");
    
    // 主副武器独立配置时，显示当前武器槽位并同步修改
    if (config.weapon_separate_config_enabled)
    {
        extern std::atomic<int> currentWeaponSlot;
        int slot = currentWeaponSlot.load();
        const char* slot_name = (slot == 1) ? u8"主武器" : (slot == 2) ? u8"副武器" : u8"未知";
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), u8"当前武器: %s (独立配置已启用)", slot_name);
    }
    
    SliderIntWithKeys(u8"视野 X", &config.fovX, 10, 120);
    SliderIntWithKeys(u8"视野 Y", &config.fovY, 10, 120);

    ImGui::SeparatorText(u8"准星偏移");
    
    static int prev_offset_x = config.crosshair_offset_x;
    static int prev_offset_y = config.crosshair_offset_y;
    
    if (ImGui::SliderInt(u8"水平偏移 (X)", &config.crosshair_offset_x, -200, 200))
    {
        if (config.crosshair_offset_x != prev_offset_x)
        {
            config.saveConfig("config.ini");
            prev_offset_x = config.crosshair_offset_x;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"重置##offset_x"))
    {
        config.crosshair_offset_x = 0;
        config.saveConfig("config.ini");
        prev_offset_x = 0;
    }
    
    if (ImGui::SliderInt(u8"垂直偏移 (Y)", &config.crosshair_offset_y, -200, 200))
    {
        if (config.crosshair_offset_y != prev_offset_y)
        {
            config.saveConfig("config.ini");
            prev_offset_y = config.crosshair_offset_y;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"重置##offset_y"))
    {
        config.crosshair_offset_y = 0;
        config.saveConfig("config.ini");
        prev_offset_y = 0;
    }
    
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), u8"提示: 正值向右/上偏移，负值向左/下偏移");
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), u8"当前偏移: X=%d, Y=%d", config.crosshair_offset_x, config.crosshair_offset_y);

    ImGui::SeparatorText(u8"瞄准偏移");
    
    static int prev_aim_offset_x = config.aim_offset_x;
    static int prev_aim_offset_y = config.aim_offset_y;
    
    if (ImGui::SliderInt(u8"瞄准水平偏移 (X)", &config.aim_offset_x, -100, 100))
    {
        if (config.aim_offset_x != prev_aim_offset_x)
        {
            config.saveConfig("config.ini");
            prev_aim_offset_x = config.aim_offset_x;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"重置##aim_offset_x"))
    {
        config.aim_offset_x = 0;
        config.saveConfig("config.ini");
        prev_aim_offset_x = 0;
    }
    
    if (ImGui::SliderInt(u8"瞄准垂直偏移 (Y)", &config.aim_offset_y, -100, 100))
    {
        if (config.aim_offset_y != prev_aim_offset_y)
        {
            config.saveConfig("config.ini");
            prev_aim_offset_y = config.aim_offset_y;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"重置##aim_offset_y"))
    {
        config.aim_offset_y = 0;
        config.saveConfig("config.ini");
        prev_aim_offset_y = 0;
    }
    
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), u8"瞄准偏移始终生效，准星偏移仅射击时生效");

    ImGui::SeparatorText(u8"速度倍率");
    SliderFloatWithKeys(u8"最小速度倍率", &config.minSpeedMultiplier, 0.1f, 5.0f, "%.1f", 0.1f);
    SliderFloatWithKeys(u8"最大速度倍率", &config.maxSpeedMultiplier, 0.1f, 5.0f, "%.1f", 0.1f);

    ImGui::SeparatorText(u8"预测");
    SliderFloatWithKeys(u8"预测间隔", &config.predictionInterval, 0.00f, 0.5f, "%.2f", 0.01f);
    if (config.predictionInterval == 0.00f)
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(255, 0, 0, 255), u8"-> 已禁用");
    }
    else
    {
        
        if (SliderIntWithKeys(u8"预测位置数", &config.prediction_futurePositions, 1, 40))
        {
            config.saveConfig();
            input_method_changed.store(true);
        }

        ImGui::SameLine();
        if (ImGui::Checkbox(u8"绘制##draw_future_positions_button", &config.draw_futurePositions))
        {
            config.saveConfig();
        }
    }

    ImGui::SeparatorText(u8"目标修正");
    SliderFloatWithKeys(u8"吸附半径", &config.snapRadius, 0.1f, 5.0f, "%.1f", 0.1f);
    SliderFloatWithKeys(u8"近距半径", &config.nearRadius, 1.0f, 40.0f, "%.1f", 0.5f);
    SliderFloatWithKeys(u8"速度曲线指数", &config.speedCurveExponent, 0.1f, 10.0f, "%.1f", 0.1f);
    SliderFloatWithKeys(u8"吸附加速因子", &config.snapBoostFactor, 0.01f, 4.00f, "%.2f", 0.01f);
    draw_target_correction_demo();

    // [已隐藏] 游戏配置和配置管理 - 普通用户不需要
    /*
    ImGui::SeparatorText(u8"游戏配置");
    std::vector<std::string> profile_names;
    for (const auto& kv : config.game_profiles)
        profile_names.push_back(kv.first);

    static int selected_index = 0;
    for (size_t i = 0; i < profile_names.size(); ++i)
    {
        if (profile_names[i] == config.active_game)
        {
            selected_index = static_cast<int>(i);
            break;
        }
    }

    std::vector<const char*> profile_items;
    for (const auto& name : profile_names)
        profile_items.push_back(name.c_str());

    if (ImGui::Combo(u8"当前游戏配置", &selected_index, profile_items.data(), static_cast<int>(profile_items.size())))
    {
        config.active_game = profile_names[selected_index];
        config.saveConfig();
        globalMouseThread->updateConfig(
            config.detection_resolution,
            config.fovX,
            config.fovY,
            config.minSpeedMultiplier,
            config.maxSpeedMultiplier,
            config.predictionInterval,
            config.auto_shoot,
            config.bScope_multiplier
        );
        input_method_changed.store(true);
    }

    const auto& gp = config.currentProfile();

    ImGui::Text(u8"当前配置：%s", gp.name.c_str());
    ImGui::Text(u8"灵敏度：%.4f", gp.sens);
    ImGui::Text(u8"水平：%.4f", gp.yaw);
    ImGui::Text(u8"垂直：%.4f", gp.pitch);
    ImGui::Text(u8"视野缩放：%s", gp.fovScaled ? u8"是" : u8"否");

    if (gp.name != "UNIFIED")
    {
        Config::GameProfile& modifiable = config.game_profiles[gp.name];
        bool changed = false;

        float sens_f = static_cast<float>(modifiable.sens);
        float yaw_f = static_cast<float>(modifiable.yaw);
        float pitch_f = static_cast<float>(modifiable.pitch);
        float baseFOV_f = static_cast<float>(modifiable.baseFOV);

        changed |= ImGui::SliderFloat(u8"灵敏度", &sens_f, 0.001f, 10.0f, "%.4f");
        changed |= ImGui::SliderFloat(u8"水平", &yaw_f, 0.001f, 0.1f, "%.4f");
        changed |= ImGui::SliderFloat(u8"垂直", &pitch_f, 0.001f, 0.1f, "%.4f");

        changed |= ImGui::Checkbox(u8"视野缩放", &modifiable.fovScaled);
        if (modifiable.fovScaled)
        {
            changed |= ImGui::SliderFloat(u8"基础视野", &baseFOV_f, 10.0f, 180.0f, "%.1f");
        }

        if (changed)
        {
            modifiable.sens = static_cast<double>(sens_f);
            modifiable.yaw = static_cast<double>(yaw_f);

            if (gp.pitch == 0.0 || !gp.fovScaled)
                modifiable.pitch = modifiable.yaw;
            else
                modifiable.pitch = static_cast<double>(pitch_f);

            modifiable.baseFOV = static_cast<double>(baseFOV_f);

            config.saveConfig();
            input_method_changed.store(true);
        }
    }

    ImGui::SeparatorText(u8"配置管理");

    static char new_profile_name[64] = "";
    ImGui::InputText(u8"新配置名称", new_profile_name, sizeof(new_profile_name));
    ImGui::SameLine();
    if (ImGui::Button(u8"添加配置"))
    {
        std::string name = std::string(new_profile_name);
        if (!name.empty() && config.game_profiles.count(name) == 0)
        {
            Config::GameProfile gp;
            gp.name = name;
            gp.sens = 1.0;
            gp.yaw = 0.022;
            gp.pitch = 0.022;
            gp.fovScaled = false;
            gp.baseFOV = 90.0;
            config.game_profiles[name] = gp;
            config.active_game = name;
            config.saveConfig();
            input_method_changed.store(true);
            new_profile_name[0] = '\0';
        }
    }

    if (gp.name != "UNIFIED")
    {
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(200, 50, 50, 255));
        if (ImGui::Button(u8"删除当前配置"))
        {
            config.game_profiles.erase(gp.name);
            if (!config.game_profiles.empty())
                config.active_game = config.game_profiles.begin()->first;
            else
                config.active_game = "UNIFIED";

            config.saveConfig();
            input_method_changed.store(true);
        }
        ImGui::PopStyleColor();
    }
    */

    ImGui::SeparatorText(u8"简易压枪");
    if (ImGui::Checkbox(u8"简易压枪", &config.easynorecoil))
    {
        config.saveConfig();
    }
    if (config.easynorecoil)
    {
        if (SliderFloatWithKeys(u8"压枪强度", &config.easynorecoilstrength, 0.1f, 500.0f, "%.1f", 0.5f))
        {
            config.saveConfig();
        }
        
        // 压枪模式选择
        const char* norecoil_modes[] = { u8"右键+左键", u8"自定义按键" };
        if (ImGui::Combo(u8"压枪模式", &config.norecoil_mode, norecoil_modes, IM_ARRAYSIZE(norecoil_modes)))
        {
            config.saveConfig();
        }
        
        // 自定义按键模式时显示按键设置
        if (config.norecoil_mode == 1)
        {
            ImGui::Text(u8"压枪按键");
            static int detecting_norecoil_key = -1;
            
            for (size_t i = 0; i < config.button_norecoil.size(); )
            {
                std::string& current_key_name = config.button_norecoil[i];
                std::string label = u8"压枪按键 " + std::to_string(i);
                
                if (detecting_norecoil_key == (int)i)
                {
                    ImGui::Button((u8"按下任意键...##norecoil" + std::to_string(i)).c_str());
                    std::string detected = KeyCodes::detectPressedKeyCombo();
                    if (!detected.empty())
                    {
                        current_key_name = detected;
                        detecting_norecoil_key = -1;
                        config.saveConfig();
                    }
                }
                else
                {
                    if (ImGui::Button((current_key_name + "##norecoil" + std::to_string(i)).c_str()))
                    {
                        detecting_norecoil_key = (int)i;
                    }
                }
                
                ImGui::SameLine();
                std::string remove_button_label = u8"移除##button_norecoil" + std::to_string(i);
                if (ImGui::Button(remove_button_label.c_str()))
                {
                    if (config.button_norecoil.size() <= 1)
                    {
                        config.button_norecoil[0] = std::string("None");
                        config.saveConfig();
                        continue;
                    }
                    else
                    {
                        config.button_norecoil.erase(config.button_norecoil.begin() + i);
                        config.saveConfig();
                        continue;
                    }
                }
                ++i;
            }
            if (ImGui::Button(u8"添加按键##norecoil"))
            {
                config.button_norecoil.push_back("None");
                config.saveConfig();
            }
        }
        
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), u8"上/下方向键：调整压枪强度 ±0.1");
        
        if (config.easynorecoilstrength >= 100.0f)
        {
            ImGui::TextColored(ImVec4(255, 255, 0, 255), u8"警告：过高的压枪强度可能会被检测。");
        }
    }

    ImGui::SeparatorText(u8"自动射击");

    ImGui::Checkbox(u8"自动射击", &config.auto_shoot);
    if (config.auto_shoot)
    {
        SliderFloatWithKeys(u8"瞄准镜倍率", &config.bScope_multiplier, 0.5f, 2.0f, "%.1f", 0.1f);
        if (SliderFloatWithKeys(u8"攻击间隔", &config.auto_shoot_interval, 10.0f, 500.0f, "%.0f ms", 10.0f))
        {
            config.saveConfig();
        }
        {
            int fov = config.auto_shoot_fov;
            ImGui::SetNextItemWidth(150);
            if (ImGui::SliderInt(u8"识别范围", &fov, 10, 200))
            {
                config.auto_shoot_fov = fov;
                config.saveConfig();
            }
        }
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), u8"准星对准目标时自动连续开枪");
    }

    ImGui::SeparatorText(u8"风力鼠标");

    if (ImGui::Checkbox(u8"启用风力鼠标", &config.wind_mouse_enabled))
    {
        config.saveConfig();
        input_method_changed.store(true);
    }

    if (config.wind_mouse_enabled)
    {
        if (SliderFloatWithKeys(u8"重力", &config.wind_G, 4.00f, 40.00f, "%.2f", 0.5f))
        {
            config.saveConfig();
        }

        if (SliderFloatWithKeys(u8"风力波动", &config.wind_W, 1.00f, 40.00f, "%.2f", 0.5f))
        {
            config.saveConfig();
        }

        if (SliderFloatWithKeys(u8"最大步长", &config.wind_M, 1.00f, 40.00f, "%.2f", 0.5f))
        {
            config.saveConfig();
        }

        if (SliderFloatWithKeys(u8"行为变化距离", &config.wind_D, 1.00f, 40.00f, "%.2f", 0.5f))
        {
            config.saveConfig();
        }

        if (ImGui::Button(u8"重置风力鼠标为默认设置"))
        {
            config.wind_G = 18.0f;
            config.wind_W = 15.0f;
            config.wind_M = 10.0f;
            config.wind_D = 8.0f;
            config.saveConfig();
        }
    }

    ImGui::SeparatorText(u8"输入方式");
    std::vector<std::string> input_methods = { "GHUB", "ARDUINO", "KMBOX_B", "KMBOX_NET", "MAKCU" };

    std::vector<const char*> method_items;
    method_items.reserve(input_methods.size());
    for (const auto& item : input_methods)
    {
        method_items.push_back(item.c_str());
    }

    std::string combo_label = u8"鼠标输入方式";
    int input_method_index = 0;
    for (size_t i = 0; i < input_methods.size(); ++i)
    {
        if (input_methods[i] == config.input_method)
        {
            input_method_index = static_cast<int>(i);
            break;
        }
    }

    if (ImGui::Combo(u8"鼠标输入方式", &input_method_index, method_items.data(), static_cast<int>(method_items.size())))
    {
        std::string new_input_method = input_methods[input_method_index];

        if (new_input_method != config.input_method)
        {
            config.input_method = new_input_method;
            config.saveConfig();
            input_method_changed.store(true);
        }
    }

    if (config.input_method == "ARDUINO")
    {
        if (arduinoSerial)
        {
            if (arduinoSerial->isOpen())
            {
                ImGui::TextColored(ImVec4(0, 255, 0, 255), u8"Arduino 已连接");
            }
            else
            {
                ImGui::TextColored(ImVec4(255, 0, 0, 255), u8"Arduino 未连接");
            }
        }

        std::vector<std::string> port_list;
        for (int i = 1; i <= 30; ++i)
        {
            port_list.push_back("COM" + std::to_string(i));
        }

        std::vector<const char*> port_items;
        port_items.reserve(port_list.size());
        for (const auto& port : port_list)
        {
            port_items.push_back(port.c_str());
        }

        int port_index = 0;
        for (size_t i = 0; i < port_list.size(); ++i)
        {
            if (port_list[i] == config.arduino_port)
            {
                port_index = static_cast<int>(i);
                break;
            }
        }

        if (ImGui::Combo(u8"Arduino 端口", &port_index, port_items.data(), static_cast<int>(port_items.size())))
        {
            config.arduino_port = port_list[port_index];
            config.saveConfig();
            input_method_changed.store(true);
        }

        std::vector<int> baud_rate_list = { 9600, 19200, 38400, 57600, 115200 };
        std::vector<std::string> baud_rate_str_list;
        for (const auto& rate : baud_rate_list)
        {
            baud_rate_str_list.push_back(std::to_string(rate));
        }

        std::vector<const char*> baud_rate_items;
        baud_rate_items.reserve(baud_rate_str_list.size());
        for (const auto& rate_str : baud_rate_str_list)
        {
            baud_rate_items.push_back(rate_str.c_str());
        }

        int baud_rate_index = 0;
        for (size_t i = 0; i < baud_rate_list.size(); ++i)
        {
            if (baud_rate_list[i] == config.arduino_baudrate)
            {
                baud_rate_index = static_cast<int>(i);
                break;
            }
        }

        if (ImGui::Combo(u8"Arduino 波特率", &baud_rate_index, baud_rate_items.data(), static_cast<int>(baud_rate_items.size())))
        {
            config.arduino_baudrate = baud_rate_list[baud_rate_index];
            config.saveConfig();
            input_method_changed.store(true);
        }

        if (ImGui::Checkbox(u8"Arduino 16位鼠标", &config.arduino_16_bit_mouse))
        {
            config.saveConfig();
            input_method_changed.store(true);
        }
        if (ImGui::Checkbox(u8"Arduino 启用按键", &config.arduino_enable_keys))
        {
            config.saveConfig();
            input_method_changed.store(true);
        }
    }
    else if (config.input_method == "GHUB")
    {
        if (ghub_version == "13.1.4")
        {
            std::string ghub_version_label = u8"已安装正确版本的 Ghub：" + ghub_version;
            ImGui::Text(ghub_version_label.c_str());
        }
        else
        {
            ImGui::Text(u8"Ghub 版本错误或路径未设置为默认。\n默认系统路径：C:\\Program Files\\LGHUB");
            if (ImGui::Button(u8"GHub 文档"))
            {
                ShellExecute(0, 0, L"https://github.com/SunOner/sunone_aimbot_docs/blob/main/tips/ghub.md", 0, 0, SW_SHOW);
            }
        }
        ImGui::TextColored(ImVec4(255, 0, 0, 255), u8"使用风险自负，此方法在某些游戏中会被检测。");
    }
    else if (config.input_method == "KMBOX_B")
    {
        std::vector<std::string> port_list;
        for (int i = 1; i <= 30; ++i)
        {
            port_list.push_back("COM" + std::to_string(i));
        }
        std::vector<const char*> port_items;
        port_items.reserve(port_list.size());
        for (auto& p : port_list) port_items.push_back(p.c_str());

        int port_index = 0;
        for (size_t i = 0; i < port_list.size(); ++i)
        {
            if (port_list[i] == config.kmbox_b_port)
            {
                port_index = (int)i;
                break;
            }
        }

        if (ImGui::Combo(u8"kmbox 端口", &port_index, port_items.data(), (int)port_items.size()))
        {
            config.kmbox_b_port = port_list[port_index];
            config.saveConfig();
            input_method_changed.store(true);
        }

        std::vector<int> baud_list = { 9600, 19200, 38400, 57600, 115200 };
        std::vector<std::string> baud_str_list;
        for (int b : baud_list) baud_str_list.push_back(std::to_string(b));
        std::vector<const char*> baud_items;
        baud_items.reserve(baud_str_list.size());
        for (auto& bs : baud_str_list) baud_items.push_back(bs.c_str());

        int baud_index = 0;
        for (size_t i = 0; i < baud_list.size(); ++i)
        {
            if (baud_list[i] == config.kmbox_b_baudrate)
            {
                baud_index = (int)i;
                break;
            }
        }

        if (ImGui::Combo(u8"kmbox 波特率", &baud_index, baud_items.data(), (int)baud_items.size()))
        {
            config.kmbox_b_baudrate = baud_list[baud_index];
            config.saveConfig();
            input_method_changed.store(true);
        }

        if (ImGui::Button(u8"运行 boot.py"))
        {
            kmboxSerial->start_boot();
        }

        if (ImGui::Button(u8"重启 KMBOX"))
        {
            kmboxSerial->reboot();
        }

        if (ImGui::Button(u8"发送停止"))
        {
            kmboxSerial->send_stop();
        }
    }
    else if (config.input_method == "KMBOX_NET")
    {
        static char ip[32], port[8], uuid[16];
        strncpy(ip, config.kmbox_net_ip.c_str(), sizeof(ip));
        strncpy(port, config.kmbox_net_port.c_str(), sizeof(port));
        strncpy(uuid, config.kmbox_net_uuid.c_str(), sizeof(uuid));

        ImGui::InputText("IP", ip, sizeof(ip));
        ImGui::InputText("Port", port, sizeof(port));
        ImGui::InputText("UUID", uuid, sizeof(uuid));

        if (ImGui::Button(u8"保存并重连"))
        {
            config.kmbox_net_ip = ip;
            config.kmbox_net_port = port;
            config.kmbox_net_uuid = uuid;
            config.saveConfig();
            input_method_changed.store(true);
        }

        if (kmboxNetSerial && kmboxNetSerial->isOpen())
        {
            ImGui::TextColored(ImVec4(0, 255, 0, 255), u8"kmboxNet 已连接");
        }
        else
        {
            ImGui::TextColored(ImVec4(255, 0, 0, 255), u8"kmboxNet 未连接");
        }
        
        if (ImGui::Button(u8"重启盒子"))
        {
            if (kmboxNetSerial)
            {
                kmboxNetSerial->reboot();
            }
        }

        if (ImGui::Button(u8"更换 Kmbox 图片"))
        {
            if (kmboxNetSerial)
            {
                kmboxNetSerial->lcdColor(0);
                kmboxNetSerial->lcdPicture(gImage_128x160);
            }
        }
    }
    else if (config.input_method == "MAKCU")
    {
        std::vector<std::string> port_list;
        for (int i = 1; i <= 30; ++i)
        {
            port_list.push_back("COM" + std::to_string(i));
        }

        std::vector<const char*> port_items;
        port_items.reserve(port_list.size());
        for (const auto& port : port_list)
        {
            port_items.push_back(port.c_str());
        }

        int port_index = 0;
        for (size_t i = 0; i < port_list.size(); ++i)
        {
            if (port_list[i] == config.makcu_port)
            {
                port_index = static_cast<int>(i);
                break;
            }
        }

        if (ImGui::Combo(u8"Makcu 端口", &port_index, port_items.data(), static_cast<int>(port_items.size())))
        {
            config.makcu_port = port_list[port_index];
            config.saveConfig();
            input_method_changed.store(true);
        }

        std::vector<int> baud_list = { 9600, 19200, 38400, 57600, 115200 };
        std::vector<std::string> baud_str_list;
        for (int b : baud_list) baud_str_list.push_back(std::to_string(b));

        std::vector<const char*> baud_items;
        baud_items.reserve(baud_list.size());
        for (const auto& baud : baud_str_list)
        {
            baud_items.push_back(baud.c_str());
        }

        int baud_index = 0;
        for (size_t i = 0; i < baud_list.size(); ++i)
        {
            if (baud_list[i] == config.makcu_baudrate)
            {
                baud_index = static_cast<int>(i);
                break;
            }
        }

        if (ImGui::Combo(u8"Makcu 波特率", &baud_index, baud_items.data(), static_cast<int>(baud_items.size())))
        {
            config.makcu_baudrate = baud_list[baud_index];
            config.saveConfig();
            input_method_changed.store(true);
        }

        if (makcuSerial && makcuSerial->isOpen())
        {
            ImGui::TextColored(ImVec4(0, 255, 0, 255), u8"Makcu 已连接");
        }
        else
        {
            ImGui::TextColored(ImVec4(255, 0, 0, 255), u8"Makcu 未连接");
        }
    }

    ImGui::Separator();
    ImGui::TextColored(ImVec4(255, 255, 255, 100), u8"请勿在覆盖层打开时测试射击和瞄准。");

    if (prev_fovX != config.fovX ||
        prev_fovY != config.fovY ||
        prev_minSpeedMultiplier != config.minSpeedMultiplier ||
        prev_maxSpeedMultiplier != config.maxSpeedMultiplier ||
        prev_predictionInterval != config.predictionInterval ||
        prev_snapRadius != config.snapRadius ||
        prev_nearRadius != config.nearRadius ||
        prev_speedCurveExponent != config.speedCurveExponent ||
        prev_snapBoostFactor != config.snapBoostFactor)
    {
        // 独立配置时，将修改同步回当前武器槽位
        if (config.weapon_separate_config_enabled)
        {
            extern std::atomic<int> currentWeaponSlot;
            int slot = currentWeaponSlot.load();
            config.saveWeaponSlotConfig(slot);
        }

        prev_fovX = config.fovX;
        prev_fovY = config.fovY;
        prev_minSpeedMultiplier = config.minSpeedMultiplier;
        prev_maxSpeedMultiplier = config.maxSpeedMultiplier;
        prev_predictionInterval = config.predictionInterval;
        prev_snapRadius = config.snapRadius;
        prev_nearRadius = config.nearRadius;
        prev_speedCurveExponent = config.speedCurveExponent;
        prev_snapBoostFactor = config.snapBoostFactor;

        globalMouseThread->updateConfig(
            config.detection_resolution,
            config.fovX,
            config.fovY,
            config.minSpeedMultiplier,
            config.maxSpeedMultiplier,
            config.predictionInterval,
            config.auto_shoot,
            config.bScope_multiplier);

        config.saveConfig();
    }

    if (prev_wind_mouse_enabled != config.wind_mouse_enabled ||
        prev_wind_G != config.wind_G ||
        prev_wind_W != config.wind_W ||
        prev_wind_M != config.wind_M ||
        prev_wind_D != config.wind_D)
    {
        prev_wind_mouse_enabled = config.wind_mouse_enabled;
        prev_wind_G = config.wind_G;
        prev_wind_W = config.wind_W;
        prev_wind_M = config.wind_M;
        prev_wind_D = config.wind_D;

        globalMouseThread->updateConfig(
            config.detection_resolution,
            config.fovX,
            config.fovY,
            config.minSpeedMultiplier,
            config.maxSpeedMultiplier,
            config.predictionInterval,
            config.auto_shoot,
            config.bScope_multiplier);

        config.saveConfig();
    }

    if (prev_auto_shoot != config.auto_shoot ||
        prev_bScope_multiplier != config.bScope_multiplier)
    {
        // 独立配置时，将修改同步回当前武器槽位
        if (config.weapon_separate_config_enabled)
        {
            extern std::atomic<int> currentWeaponSlot;
            int slot = currentWeaponSlot.load();
            config.saveWeaponSlotConfig(slot);
        }

        prev_auto_shoot = config.auto_shoot;
        prev_bScope_multiplier = config.bScope_multiplier;

        globalMouseThread->updateConfig(
            config.detection_resolution,
            config.fovX,
            config.fovY,
            config.minSpeedMultiplier,
            config.maxSpeedMultiplier,
            config.predictionInterval,
            config.auto_shoot,
            config.bScope_multiplier);

        config.saveConfig();
    }
}