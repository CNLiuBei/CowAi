#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <winsock2.h>
#include <Windows.h>

#include "imgui/imgui.h"
#include "sunone_aimbot_cpp.h"
#include "overlay.h"
#include "keyboard/keycodes.h"

// 按键捕获状态
static int g_capture_type = -1;      // -1=无, 0=targeting, 1=shoot, 2=zoom, 3=exit, 4=pause, 5=reload_config, 6=overlay
static size_t g_capture_index = 0;
static bool g_waiting_for_release = false;

// 绘制单个按键绑定控件
bool DrawKeyBindButton(const char* label, std::string& key_name, int capture_type, size_t index)
{
    bool changed = false;
    bool is_capturing = (g_capture_type == capture_type && g_capture_index == index);
    
    ImGui::PushID(label);
    
    // 显示当前绑定的按键或等待提示
    std::string button_text;
    if (is_capturing) {
        button_text = u8"[按任意键...]";
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.4f, 0.1f, 1.0f));
    } else {
        button_text = key_name.empty() ? "None" : key_name;
    }
    
    // 按钮宽度
    float button_width = 180.0f;
    
    if (ImGui::Button(button_text.c_str(), ImVec2(button_width, 0))) {
        if (!is_capturing) {
            // 进入捕获模式
            g_capture_type = capture_type;
            g_capture_index = index;
            g_waiting_for_release = true;
        }
    }
    
    if (is_capturing) {
        ImGui::PopStyleColor();
        
        // 等待鼠标释放后再检测按键
        if (g_waiting_for_release) {
            if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) {
                g_waiting_for_release = false;
            }
        } else {
            // 检测组合键
            std::string combo = KeyCodes::detectPressedKeyCombo();
            if (!combo.empty()) {
                key_name = combo;
                changed = true;
                // 退出捕获模式
                g_capture_type = -1;
            }
            
            // ESC取消
            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
                g_capture_type = -1;
            }
        }
    }
    
    ImGui::PopID();
    return changed;
}

void draw_buttons()
{
    ImGui::Text(u8"瞄准按键");
    ImGui::SameLine();
    ImGui::TextDisabled(u8"(点击按钮后按任意键绑定)");

    for (size_t i = 0; i < config.button_targeting.size(); )
    {
        std::string& current_key_name = config.button_targeting[i];
        std::string label = u8"瞄准按键 " + std::to_string(i);
        
        ImGui::Text("%s:", label.c_str());
        ImGui::SameLine();
        
        if (DrawKeyBindButton((label + "##btn").c_str(), current_key_name, 0, i)) {
            config.saveConfig();
        }

        ImGui::SameLine();
        std::string remove_button_label = u8"移除##button_targeting" + std::to_string(i);
        if (ImGui::Button(remove_button_label.c_str()))
        {
            if (config.button_targeting.size() <= 1)
            {
                config.button_targeting[0] = std::string("None");
                config.saveConfig();
                continue;
            }
            else
            {
                config.button_targeting.erase(config.button_targeting.begin() + i);
                config.saveConfig();
                continue;
            }
        }

        ++i;
    }

    if (ImGui::Button(u8"添加按键##targeting"))
    {
        config.button_targeting.push_back("None");
        config.saveConfig();
    }

    ImGui::Separator();

    ImGui::Text(u8"射击按键");

    for (size_t i = 0; i < config.button_shoot.size(); )
    {
        std::string& current_key_name = config.button_shoot[i];
        std::string label = u8"射击按键 " + std::to_string(i);
        
        ImGui::Text("%s:", label.c_str());
        ImGui::SameLine();
        
        if (DrawKeyBindButton((label + "##btn").c_str(), current_key_name, 1, i)) {
            config.saveConfig();
        }

        ImGui::SameLine();
        std::string remove_button_label = u8"移除##button_shoot" + std::to_string(i);
        if (ImGui::Button(remove_button_label.c_str()))
        {
            if (config.button_shoot.size() <= 1)
            {
                config.button_shoot[0] = std::string("None");
                config.saveConfig();
                continue;
            }
            else
            {
                config.button_shoot.erase(config.button_shoot.begin() + i);
                config.saveConfig();
                continue;
            }
        }

        ++i;
    }

    if (ImGui::Button(u8"添加按键##shoot"))
    {
        config.button_shoot.push_back("None");
        config.saveConfig();
    }

    ImGui::Separator();

    ImGui::Text(u8"瞄准镜按键");

    for (size_t i = 0; i < config.button_zoom.size(); )
    {
        std::string& current_key_name = config.button_zoom[i];
        std::string label = u8"瞄准镜按键 " + std::to_string(i);
        
        ImGui::Text("%s:", label.c_str());
        ImGui::SameLine();
        
        if (DrawKeyBindButton((label + "##btn").c_str(), current_key_name, 2, i)) {
            config.saveConfig();
        }

        ImGui::SameLine();
        std::string remove_button_label = u8"移除##button_zoom" + std::to_string(i);
        if (ImGui::Button(remove_button_label.c_str()))
        {
            if (config.button_zoom.size() <= 1)
            {
                config.button_zoom[0] = std::string("None");
                config.saveConfig();
                continue;
            }
            else
            {
                config.button_zoom.erase(config.button_zoom.begin() + i);
                config.saveConfig();
                continue;
            }
        }

        ++i;
    }

    if (ImGui::Button(u8"添加按键##zoom"))
    {
        config.button_zoom.push_back("None");
        config.saveConfig();
    }

    ImGui::Separator();

    ImGui::Text(u8"退出按键");

    for (size_t i = 0; i < config.button_exit.size(); )
    {
        std::string& current_key_name = config.button_exit[i];
        std::string label = u8"退出按键 " + std::to_string(i);
        
        ImGui::Text("%s:", label.c_str());
        ImGui::SameLine();
        
        if (DrawKeyBindButton((label + "##btn").c_str(), current_key_name, 3, i)) {
            config.saveConfig();
        }

        ImGui::SameLine();
        std::string remove_button_label = u8"移除##button_exit" + std::to_string(i);
        if (ImGui::Button(remove_button_label.c_str()))
        {
            if (config.button_exit.size() <= 1)
            {
                config.button_exit[0] = std::string("None");
                config.saveConfig();
                continue;
            }
            else
            {
                config.button_exit.erase(config.button_exit.begin() + i);
                config.saveConfig();
                continue;
            }
        }

        ++i;
    }

    if (ImGui::Button(u8"添加按键##exit"))
    {
        config.button_exit.push_back("None");
        config.saveConfig();
    }

    ImGui::Separator();

    ImGui::Text(u8"暂停按键");

    for (size_t i = 0; i < config.button_pause.size(); )
    {
        std::string& current_key_name = config.button_pause[i];
        std::string label = u8"暂停按键 " + std::to_string(i);
        
        ImGui::Text("%s:", label.c_str());
        ImGui::SameLine();
        
        if (DrawKeyBindButton((label + "##btn").c_str(), current_key_name, 4, i)) {
            config.saveConfig();
        }

        ImGui::SameLine();
        std::string remove_button_label = u8"移除##button_pause" + std::to_string(i);
        if (ImGui::Button(remove_button_label.c_str()))
        {
            if (config.button_pause.size() <= 1)
            {
                config.button_pause[0] = std::string("None");
                config.saveConfig();
                continue;
            }
            else
            {
                config.button_pause.erase(config.button_pause.begin() + i);
                config.saveConfig();
                continue;
            }
        }
        ++i;
    }

    if (ImGui::Button(u8"添加按键##pause"))
    {
        config.button_pause.push_back("None");
        config.saveConfig();
    }

    ImGui::Separator();

    ImGui::Text(u8"重载配置按键");

    for (size_t i = 0; i < config.button_reload_config.size(); )
    {
        std::string& current_key_name = config.button_reload_config[i];
        std::string label = u8"重载配置按键 " + std::to_string(i);
        
        ImGui::Text("%s:", label.c_str());
        ImGui::SameLine();
        
        if (DrawKeyBindButton((label + "##btn").c_str(), current_key_name, 5, i)) {
            config.saveConfig();
        }

        ImGui::SameLine();
        std::string remove_button_label = u8"移除##button_reload_config" + std::to_string(i);
        if (ImGui::Button(remove_button_label.c_str()))
        {
            if (config.button_reload_config.size() <= 1)
            {
                config.button_reload_config[0] = std::string("None");
                config.saveConfig();
                continue;
            }
            else
            {
                config.button_reload_config.erase(config.button_reload_config.begin() + i);
                config.saveConfig();
                continue;
            }
        }

        ++i;
    }

    if (ImGui::Button(u8"添加按键##reload_config"))
    {
        config.button_reload_config.push_back("None");
        config.saveConfig();
    }

    ImGui::Separator();

    ImGui::Text(u8"覆盖层按键");

    for (size_t i = 0; i < config.button_open_overlay.size(); )
    {
        std::string& current_key_name = config.button_open_overlay[i];
        std::string label = u8"覆盖层按键 " + std::to_string(i);
        
        ImGui::Text("%s:", label.c_str());
        ImGui::SameLine();
        
        if (DrawKeyBindButton((label + "##btn").c_str(), current_key_name, 6, i)) {
            config.saveConfig();
        }

        ImGui::SameLine();
        std::string remove_button_label = u8"移除##button_open_overlay" + std::to_string(i);
        if (ImGui::Button(remove_button_label.c_str()))
        {
            config.button_open_overlay.erase(config.button_open_overlay.begin() + i);
            config.saveConfig();
            continue;
        }

        ++i;
    }

    if (ImGui::Button(u8"添加按键##overlay"))
    {
        config.button_open_overlay.push_back("None");
        config.saveConfig();
    }

    ImGui::Separator();

    if (ImGui::Checkbox(u8"启用方向键选项", &config.enable_arrows_settings))
    {
        config.saveConfig();
    }
    
    ImGui::Separator();
    
    // 武器槽位设置
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), u8"[武器槽位设置]");
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), u8"按下对应按键切换武器槽位，可单独控制每个槽位的目标锁定");
    
    // 主武器按键
    ImGui::Text(u8"主武器按键");
    for (size_t i = 0; i < config.button_weapon_primary.size(); )
    {
        std::string& current_key_name = config.button_weapon_primary[i];
        std::string label = u8"主武器 " + std::to_string(i);
        
        ImGui::Text("%s:", label.c_str());
        ImGui::SameLine();
        
        if (DrawKeyBindButton((label + "##btn").c_str(), current_key_name, 7, i)) {
            config.saveConfig();
        }

        ImGui::SameLine();
        std::string remove_button_label = u8"移除##button_weapon_primary" + std::to_string(i);
        if (ImGui::Button(remove_button_label.c_str()))
        {
            if (config.button_weapon_primary.size() <= 1)
            {
                config.button_weapon_primary[0] = std::string("None");
                config.saveConfig();
                continue;
            }
            else
            {
                config.button_weapon_primary.erase(config.button_weapon_primary.begin() + i);
                config.saveConfig();
                continue;
            }
        }
        ++i;
    }
    if (ImGui::Button(u8"添加按键##weapon_primary"))
    {
        config.button_weapon_primary.push_back("None");
        config.saveConfig();
    }
    
    ImGui::SameLine();
    if (ImGui::Checkbox(u8"启用辅助##primary", &config.weapon_primary_lock_enabled))
    {
        config.saveConfig();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(u8"主武器槽位是否启用瞄准辅助功能");
    
    // 副武器按键
    ImGui::Text(u8"副武器按键");
    for (size_t i = 0; i < config.button_weapon_secondary.size(); )
    {
        std::string& current_key_name = config.button_weapon_secondary[i];
        std::string label = u8"副武器 " + std::to_string(i);
        
        ImGui::Text("%s:", label.c_str());
        ImGui::SameLine();
        
        if (DrawKeyBindButton((label + "##btn").c_str(), current_key_name, 8, i)) {
            config.saveConfig();
        }

        ImGui::SameLine();
        std::string remove_button_label = u8"移除##button_weapon_secondary" + std::to_string(i);
        if (ImGui::Button(remove_button_label.c_str()))
        {
            if (config.button_weapon_secondary.size() <= 1)
            {
                config.button_weapon_secondary[0] = std::string("None");
                config.saveConfig();
                continue;
            }
            else
            {
                config.button_weapon_secondary.erase(config.button_weapon_secondary.begin() + i);
                config.saveConfig();
                continue;
            }
        }
        ++i;
    }
    if (ImGui::Button(u8"添加按键##weapon_secondary"))
    {
        config.button_weapon_secondary.push_back("None");
        config.saveConfig();
    }
    
    ImGui::SameLine();
    if (ImGui::Checkbox(u8"启用辅助##secondary", &config.weapon_secondary_lock_enabled))
    {
        config.saveConfig();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(u8"副武器槽位是否启用瞄准辅助功能");
    
    // 显示当前武器槽位状态
    extern std::atomic<int> currentWeaponSlot;
    int slot = currentWeaponSlot.load();
    const char* slot_name = (slot == 1) ? u8"主武器" : (slot == 2) ? u8"副武器" : u8"未知";
    bool lock_enabled = (slot == 1) ? config.weapon_primary_lock_enabled : 
                        (slot == 2) ? config.weapon_secondary_lock_enabled : true;
    
    ImGui::TextColored(
        lock_enabled ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
        u8"当前槽位: %s | 辅助: %s", 
        slot_name, 
        lock_enabled ? u8"开启" : u8"关闭"
    );

    // 主副武器独立瞄准配置
    ImGui::Separator();
    if (ImGui::Checkbox(u8"主副武器独立配置", &config.weapon_separate_config_enabled))
    {
        if (config.weapon_separate_config_enabled)
        {
            // 首次启用时，用当前全局配置初始化两个槽位
            config.saveWeaponSlotConfig(1);
            config.saveWeaponSlotConfig(2);
        }
        config.saveConfig();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(u8"启用后主副武器使用独立的FOV、速度、预测等参数，自动瞄准开关不受影响");

    // [已隐藏] 武器独立参数编辑器 - 已在"鼠标"页面统一管理
    /*
    if (config.weapon_separate_config_enabled)
    {
        auto drawWeaponAimConfig = [&](const char* label, Config::WeaponAimConfig& wc, int wslot)
        {
            ImGui::PushID(wslot);
            bool isActive = (slot == wslot);
            if (isActive)
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), u8"▶ %s (当前)", label);
            else
                ImGui::Text(u8"  %s", label);

            bool changed = false;
            changed |= ImGui::SliderInt(u8"FOV X##w", &wc.fovX, 10, 120);
            changed |= ImGui::SliderInt(u8"FOV Y##w", &wc.fovY, 10, 120);
            changed |= ImGui::SliderFloat(u8"最小速度##w", &wc.minSpeedMultiplier, 0.1f, 5.0f, "%.1f");
            changed |= ImGui::SliderFloat(u8"最大速度##w", &wc.maxSpeedMultiplier, 0.1f, 5.0f, "%.1f");
            changed |= ImGui::SliderFloat(u8"X轴速度##w", &wc.speedMultiplierX, 0.1f, 3.0f, "%.2f");
            changed |= ImGui::SliderFloat(u8"Y轴速度##w", &wc.speedMultiplierY, 0.1f, 3.0f, "%.2f");
            changed |= ImGui::SliderFloat(u8"预测间隔##w", &wc.predictionInterval, 0.0f, 0.5f, "%.2f");
            changed |= ImGui::Checkbox(u8"自动射击##w", &wc.auto_shoot);
            if (wc.auto_shoot)
                changed |= ImGui::SliderFloat(u8"瞄准镜倍率##w", &wc.bScope_multiplier, 0.5f, 2.0f, "%.1f");

            if (changed)
            {
                config.saveConfig();
                if (isActive)
                {
                    config.applyWeaponSlotConfig(wslot);
                    if (globalMouseThread)
                    {
                        globalMouseThread->updateConfig(
                            config.detection_resolution,
                            config.fovX, config.fovY,
                            config.minSpeedMultiplier, config.maxSpeedMultiplier,
                            config.predictionInterval, config.auto_shoot, config.bScope_multiplier);
                    }
                }
            }
            ImGui::PopID();
        };

        drawWeaponAimConfig(u8"主武器", config.weapon_primary_aim, 1);
        ImGui::Separator();
        drawWeaponAimConfig(u8"副武器", config.weapon_secondary_aim, 2);
    }
    */
}
