#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <winsock2.h>
#include <Windows.h>

#include "imgui/imgui.h"
#include "sunone_aimbot_cpp.h"
#include "overlay.h"
#include "themes.h"

// 主题名称
static const char* theme_names[] = {
    u8"赛博青绿",
    u8"霓虹紫",
    u8"海洋蓝"
};

void draw_overlay()
{
    // ========== 主题选择 ==========
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), u8"[主题]");
    
    static int current_theme = -1;
    if (current_theme < 0 || current_theme > 2) 
        current_theme = (config.overlay_theme >= 0 && config.overlay_theme <= 2) ? config.overlay_theme : 0;
    
    if (ImGui::Combo(u8"界面主题", &current_theme, theme_names, IM_ARRAYSIZE(theme_names)))
    {
        config.overlay_theme = current_theme;
        ApplyThemeByIndex(current_theme);
        config.saveConfig("config.ini");
    }
    
    // 主题预览色块
    ImGui::SameLine();
    ImVec4 preview_color;
    switch (current_theme)
    {
    case 0: preview_color = ImVec4(0.08f, 0.72f, 0.65f, 1.0f); break;  // Teal
    case 1: preview_color = ImVec4(0.66f, 0.33f, 0.97f, 1.0f); break;  // Purple
    case 2: preview_color = ImVec4(0.23f, 0.51f, 0.96f, 1.0f); break;  // Blue
    default: preview_color = ImVec4(0.08f, 0.72f, 0.65f, 1.0f); break;
    }
    ImGui::ColorButton("##theme_preview", preview_color, ImGuiColorEditFlags_NoTooltip, ImVec2(20, 20));

    ImGui::Separator();

    // ========== 透明度设置 ==========
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), u8"[窗口]");
    
    int prev_opacity = config.overlay_opacity;
    if (ImGui::SliderInt(u8"窗口透明度", &config.overlay_opacity, 40, 255))
    {
        if (config.overlay_opacity < 20) config.overlay_opacity = 20;
        if (config.overlay_opacity > 255) config.overlay_opacity = 255;

        Overlay_SetOpacity(config.overlay_opacity);

        if (config.overlay_opacity != prev_opacity)
            config.saveConfig("config.ini");
    }

    // ========== 缩放设置 ==========
    static float ui_scale = config.overlay_ui_scale;

    if (ImGui::SliderFloat(u8"界面缩放", &ui_scale, 0.5f, 3.0f, "%.2f"))
    {
        ImGui::GetIO().FontGlobalScale = ui_scale;

        config.overlay_ui_scale = ui_scale;
        config.saveConfig("config.ini");

        extern const int BASE_OVERLAY_WIDTH;
        extern const int BASE_OVERLAY_HEIGHT;
        overlayWidth = static_cast<int>(BASE_OVERLAY_WIDTH * ui_scale);
        overlayHeight = static_cast<int>(BASE_OVERLAY_HEIGHT * ui_scale);

        SetWindowPos(g_hwnd, NULL, 0, 0, overlayWidth, overlayHeight, SWP_NOMOVE | SWP_NOZORDER);
    }
    
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), u8"提示: 更改缩放后可能需要重启以获得最佳效果");
}
