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
}
