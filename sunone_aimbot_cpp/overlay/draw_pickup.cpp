// draw_pickup.cpp - Ghost hand pickup settings UI (1:1 replica)
#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <winsock2.h>
#include <Windows.h>

#include "imgui/imgui.h"
#include "sunone_aimbot_cpp.h"
#include "overlay.h"
#include "keyboard/keycodes.h"
#include "pickup/auto_pickup.h"

extern AutoPickup* globalAutoPickup;

static int g_pickup_capture_type = -1;
static size_t g_pickup_capture_index = 0;
static bool g_pickup_waiting_for_release = false;

static bool DrawPickupKeyBindButton(const char* label, std::string& key_name, int capture_type, size_t index)
{
    bool changed = false;
    bool is_capturing = (g_pickup_capture_type == capture_type && g_pickup_capture_index == index);
    
    ImGui::PushID(label);
    
    std::string button_text;
    if (is_capturing) {
        button_text = u8"[按任意键...]";
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.4f, 0.1f, 1.0f));
    } else {
        button_text = key_name.empty() ? "None" : key_name;
    }
    
    if (ImGui::Button(button_text.c_str(), ImVec2(180.0f, 0))) {
        if (!is_capturing) {
            g_pickup_capture_type = capture_type;
            g_pickup_capture_index = index;
            g_pickup_waiting_for_release = true;
        }
    }
    
    if (is_capturing) {
        ImGui::PopStyleColor();
        if (g_pickup_waiting_for_release) {
            if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) {
                g_pickup_waiting_for_release = false;
            }
        } else {
            std::string combo = KeyCodes::detectPressedKeyCombo();
            if (!combo.empty()) {
                key_name = combo;
                changed = true;
                g_pickup_capture_type = -1;
            }
            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
                g_pickup_capture_type = -1;
            }
        }
    }
    
    ImGui::PopID();
    return changed;
}

void draw_pickup_settings()
{
    ImGui::Text(u8"鬼手拾取设置");
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Checkbox(u8"启用鬼手拾取", &config.auto_pickup_enabled))
    {
        config.saveConfig();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text(u8"按下热键后自动打开Tab背包并拾取物品到身上");
        ImGui::Text(u8"1:1复刻原版鬼手拾取逻辑");
        ImGui::EndTooltip();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Hotkey settings
    ImGui::Text(u8"拾取热键");
    ImGui::SameLine();
    ImGui::TextDisabled(u8"(点击按钮后按任意键绑定)");

    for (size_t i = 0; i < config.button_auto_pickup.size(); )
    {
        std::string& current_key_name = config.button_auto_pickup[i];
        std::string label = u8"热键 " + std::to_string(i);
        
        ImGui::Text("%s:", label.c_str());
        ImGui::SameLine();
        
        if (DrawPickupKeyBindButton((label + "##btn").c_str(), current_key_name, 0, i)) {
            config.saveConfig();
        }

        ImGui::SameLine();
        std::string remove_label = u8"移除##pickup_key" + std::to_string(i);
        if (ImGui::Button(remove_label.c_str()))
        {
            if (config.button_auto_pickup.size() <= 1)
            {
                config.button_auto_pickup[0] = "None";
                config.saveConfig();
                continue;
            }
            config.button_auto_pickup.erase(config.button_auto_pickup.begin() + i);
            config.saveConfig();
            continue;
        }
        ++i;
    }

    if (ImGui::Button(u8"添加热键##pickup"))
    {
        config.button_auto_pickup.push_back("None");
        config.saveConfig();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Screen type selection (matching original: GuiShou_Ping)
    ImGui::Text(u8"屏幕类型 (GuiShou_Ping)");
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text(u8"1 = 1920x1080 (普通屏)");
        ImGui::Text(u8"2 = 2560x1080 (带鱼屏)");
        ImGui::Text(u8"3 = 2560x1440 (2K屏)");
        ImGui::EndTooltip();
    }
    
    const char* screen_types[] = { "1 - 1920x1080", "2 - 2560x1080", "3 - 2560x1440" };
    int screen_type_idx = config.pickup_screen_type - 1;
    if (screen_type_idx < 0 || screen_type_idx > 2) screen_type_idx = 0;
    
    if (ImGui::Combo("##screen_type", &screen_type_idx, screen_types, IM_ARRAYSIZE(screen_types)))
    {
        config.pickup_screen_type = screen_type_idx + 1;
        
        if (globalAutoPickup)
        {
            globalAutoPickup->configure(config.pickup_screen_type, config.pickup_items_per_loop, 
                                        0, 0, 0, 0, 0, config.pickup_delay_ms);
        }
        config.saveConfig();
    }

    ImGui::Spacing();

    // Items per loop (original: CiShu, default 13)
    ImGui::Text(u8"拾取次数 (CiShu)");
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text(u8"原版默认13次，建议设置为1-15次");
        ImGui::EndTooltip();
    }
    
    if (ImGui::SliderInt("##items_per_loop", &config.pickup_items_per_loop, 1, 20))
    {
        if (globalAutoPickup)
        {
            globalAutoPickup->configure(config.pickup_screen_type, config.pickup_items_per_loop, 
                                        0, 0, 0, 0, 0, config.pickup_delay_ms);
        }
        config.saveConfig();
    }

    ImGui::Spacing();

    // Delay - increased max to 500ms
    ImGui::Text(u8"拾取延迟 (ms)");
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text(u8"每个物品拾取之间的延迟");
        ImGui::Text(u8"原版无延迟，如果拾取不稳定可以增加");
        ImGui::EndTooltip();
    }
    
    if (ImGui::SliderInt("##pickup_delay", &config.pickup_delay_ms, 0, 500))
    {
        if (globalAutoPickup)
        {
            globalAutoPickup->configure(config.pickup_screen_type, config.pickup_items_per_loop, 
                                        0, 0, 0, 0, 0, config.pickup_delay_ms);
        }
        config.saveConfig();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Manual trigger button
    if (config.auto_pickup_enabled)
    {
        if (globalAutoPickup && globalAutoPickup->isActive())
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button(u8"停止拾取", ImVec2(200, 35)))
            {
                globalAutoPickup->stop();
            }
            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
            if (ImGui::Button(u8"立即拾取", ImVec2(200, 35)))
            {
                if (globalAutoPickup)
                {
                    globalAutoPickup->fastPickup();
                }
            }
            ImGui::PopStyleColor();
        }
    }
    else
    {
        ImGui::TextDisabled(u8"请先启用鬼手拾取功能");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Usage instructions
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), u8"使用说明:");
    ImGui::BulletText(u8"在游戏中靠近地上物品按下热键");
    ImGui::BulletText(u8"程序会自动打开Tab背包并拾取物品");
    ImGui::BulletText(u8"物品会从左侧拖动到右侧身上");
    ImGui::BulletText(u8"请确保屏幕类型设置正确");
}