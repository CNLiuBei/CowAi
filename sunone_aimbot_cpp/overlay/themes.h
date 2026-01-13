#pragma once

// 注意: 使用前需要先 include imgui.h

namespace Themes
{
    // 辅助函数：RGBA 转 ImVec4
    inline ImVec4 RGBA(int r, int g, int b, int a = 255)
    {
        return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    }

    // ============================================
    // 主题 1: CyberTeal - 赛博青绿主题 (推荐)
    // 现代感、科技感、清爽
    // ============================================
    inline void ApplyTheme_CyberTeal()
    {
        ImGuiStyle& style = ImGui::GetStyle();

        // 圆角设置
        style.WindowRounding = 12.0f;
        style.ChildRounding = 8.0f;
        style.PopupRounding = 8.0f;
        style.FrameRounding = 6.0f;
        style.TabRounding = 6.0f;
        style.ScrollbarRounding = 8.0f;
        style.GrabRounding = 6.0f;

        // 边框
        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.PopupBorderSize = 1.0f;
        style.TabBorderSize = 0.0f;

        // 间距
        style.WindowPadding = ImVec2(16, 14);
        style.FramePadding = ImVec2(12, 8);
        style.ItemSpacing = ImVec2(10, 8);
        style.ItemInnerSpacing = ImVec2(8, 6);
        style.ScrollbarSize = 12.0f;

        ImVec4* c = style.Colors;

        // 背景色 - 深蓝灰
        const ImVec4 bg_dark = RGBA(15, 23, 42);      // slate-900
        const ImVec4 bg_mid = RGBA(30, 41, 59);       // slate-800
        const ImVec4 bg_light = RGBA(51, 65, 85);     // slate-700

        // 强调色 - 青绿色
        const ImVec4 accent = RGBA(20, 184, 166);     // teal-500
        const ImVec4 accent_hover = RGBA(45, 212, 191); // teal-400
        const ImVec4 accent_dim = RGBA(17, 94, 89);   // teal-800

        // 文字
        const ImVec4 text = RGBA(241, 245, 249);      // slate-100
        const ImVec4 text_dim = RGBA(148, 163, 184);  // slate-400

        // 边框
        const ImVec4 border = RGBA(71, 85, 105);      // slate-600
        const ImVec4 border_light = RGBA(100, 116, 139); // slate-500

        // 窗口
        c[ImGuiCol_WindowBg] = RGBA(15, 23, 42, 250);
        c[ImGuiCol_ChildBg] = RGBA(0, 0, 0, 0);
        c[ImGuiCol_PopupBg] = RGBA(30, 41, 59, 250);

        // 文字
        c[ImGuiCol_Text] = text;
        c[ImGuiCol_TextDisabled] = text_dim;

        // 边框
        c[ImGuiCol_Border] = border;
        c[ImGuiCol_BorderShadow] = RGBA(0, 0, 0, 0);

        // 输入框/滑块背景
        c[ImGuiCol_FrameBg] = RGBA(30, 41, 59, 200);
        c[ImGuiCol_FrameBgHovered] = RGBA(51, 65, 85, 200);
        c[ImGuiCol_FrameBgActive] = RGBA(71, 85, 105, 200);

        // 标题栏
        c[ImGuiCol_TitleBg] = bg_dark;
        c[ImGuiCol_TitleBgActive] = RGBA(20, 30, 50);
        c[ImGuiCol_TitleBgCollapsed] = bg_dark;

        // 滚动条
        c[ImGuiCol_ScrollbarBg] = RGBA(15, 23, 42, 100);
        c[ImGuiCol_ScrollbarGrab] = RGBA(71, 85, 105, 180);
        c[ImGuiCol_ScrollbarGrabHovered] = RGBA(100, 116, 139, 200);
        c[ImGuiCol_ScrollbarGrabActive] = accent;

        // 复选框/滑块
        c[ImGuiCol_CheckMark] = accent;
        c[ImGuiCol_SliderGrab] = accent;
        c[ImGuiCol_SliderGrabActive] = accent_hover;

        // 按钮
        c[ImGuiCol_Button] = RGBA(51, 65, 85, 200);
        c[ImGuiCol_ButtonHovered] = accent_dim;
        c[ImGuiCol_ButtonActive] = accent;

        // 头部/选项
        c[ImGuiCol_Header] = RGBA(51, 65, 85, 180);
        c[ImGuiCol_HeaderHovered] = accent_dim;
        c[ImGuiCol_HeaderActive] = accent;

        // 分隔线
        c[ImGuiCol_Separator] = border;
        c[ImGuiCol_SeparatorHovered] = border_light;
        c[ImGuiCol_SeparatorActive] = accent;

        // 标签页
        c[ImGuiCol_Tab] = RGBA(30, 41, 59, 200);
        c[ImGuiCol_TabHovered] = accent_dim;
        c[ImGuiCol_TabActive] = accent;
        c[ImGuiCol_TabUnfocused] = RGBA(30, 41, 59, 150);
        c[ImGuiCol_TabUnfocusedActive] = RGBA(17, 94, 89, 200);

        // 调整大小手柄
        c[ImGuiCol_ResizeGrip] = RGBA(0, 0, 0, 0);
        c[ImGuiCol_ResizeGripHovered] = accent_dim;
        c[ImGuiCol_ResizeGripActive] = accent;

        // 图表
        c[ImGuiCol_PlotLines] = accent;
        c[ImGuiCol_PlotLinesHovered] = accent_hover;
        c[ImGuiCol_PlotHistogram] = accent;
        c[ImGuiCol_PlotHistogramHovered] = accent_hover;

        // 表格
        c[ImGuiCol_TableHeaderBg] = bg_mid;
        c[ImGuiCol_TableBorderStrong] = border;
        c[ImGuiCol_TableBorderLight] = RGBA(51, 65, 85, 100);
        c[ImGuiCol_TableRowBg] = RGBA(0, 0, 0, 0);
        c[ImGuiCol_TableRowBgAlt] = RGBA(255, 255, 255, 8);

        // 导航
        c[ImGuiCol_NavHighlight] = accent;
        c[ImGuiCol_NavWindowingHighlight] = RGBA(255, 255, 255, 40);
        c[ImGuiCol_NavWindowingDimBg] = RGBA(0, 0, 0, 120);
    }

    // ============================================
    // 主题 2: NeonPurple - 霓虹紫主题
    // 游戏感、炫酷、电竞风
    // ============================================
    inline void ApplyTheme_NeonPurple()
    {
        ImGuiStyle& style = ImGui::GetStyle();

        style.WindowRounding = 10.0f;
        style.ChildRounding = 8.0f;
        style.PopupRounding = 8.0f;
        style.FrameRounding = 6.0f;
        style.TabRounding = 6.0f;
        style.ScrollbarRounding = 8.0f;
        style.GrabRounding = 6.0f;

        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.PopupBorderSize = 1.0f;

        style.WindowPadding = ImVec2(14, 12);
        style.FramePadding = ImVec2(10, 8);
        style.ItemSpacing = ImVec2(10, 8);
        style.ItemInnerSpacing = ImVec2(8, 6);
        style.ScrollbarSize = 12.0f;

        ImVec4* c = style.Colors;

        // 背景 - 深紫黑
        const ImVec4 bg_dark = RGBA(13, 10, 22);
        const ImVec4 bg_mid = RGBA(22, 18, 38);
        const ImVec4 bg_light = RGBA(35, 28, 58);

        // 强调色 - 霓虹紫/粉
        const ImVec4 accent = RGBA(168, 85, 247);     // purple-500
        const ImVec4 accent_hover = RGBA(192, 132, 252); // purple-400
        const ImVec4 accent_pink = RGBA(236, 72, 153);   // pink-500

        // 文字
        const ImVec4 text = RGBA(250, 250, 250);
        const ImVec4 text_dim = RGBA(163, 163, 163);

        // 边框
        const ImVec4 border = RGBA(88, 28, 135, 180);  // purple-900

        c[ImGuiCol_WindowBg] = RGBA(13, 10, 22, 250);
        c[ImGuiCol_ChildBg] = RGBA(0, 0, 0, 0);
        c[ImGuiCol_PopupBg] = RGBA(22, 18, 38, 250);

        c[ImGuiCol_Text] = text;
        c[ImGuiCol_TextDisabled] = text_dim;

        c[ImGuiCol_Border] = border;
        c[ImGuiCol_BorderShadow] = RGBA(0, 0, 0, 0);

        c[ImGuiCol_FrameBg] = RGBA(35, 28, 58, 200);
        c[ImGuiCol_FrameBgHovered] = RGBA(55, 45, 85, 200);
        c[ImGuiCol_FrameBgActive] = RGBA(75, 60, 110, 200);

        c[ImGuiCol_TitleBg] = bg_dark;
        c[ImGuiCol_TitleBgActive] = RGBA(18, 14, 30);
        c[ImGuiCol_TitleBgCollapsed] = bg_dark;

        c[ImGuiCol_ScrollbarBg] = RGBA(13, 10, 22, 100);
        c[ImGuiCol_ScrollbarGrab] = RGBA(88, 28, 135, 180);
        c[ImGuiCol_ScrollbarGrabHovered] = RGBA(126, 34, 206, 200);
        c[ImGuiCol_ScrollbarGrabActive] = accent;

        c[ImGuiCol_CheckMark] = accent_pink;
        c[ImGuiCol_SliderGrab] = accent;
        c[ImGuiCol_SliderGrabActive] = accent_hover;

        c[ImGuiCol_Button] = RGBA(55, 45, 85, 200);
        c[ImGuiCol_ButtonHovered] = RGBA(88, 28, 135, 200);
        c[ImGuiCol_ButtonActive] = accent;

        c[ImGuiCol_Header] = RGBA(55, 45, 85, 180);
        c[ImGuiCol_HeaderHovered] = RGBA(88, 28, 135, 200);
        c[ImGuiCol_HeaderActive] = accent;

        c[ImGuiCol_Separator] = border;
        c[ImGuiCol_SeparatorHovered] = RGBA(126, 34, 206, 200);
        c[ImGuiCol_SeparatorActive] = accent;

        c[ImGuiCol_Tab] = RGBA(35, 28, 58, 200);
        c[ImGuiCol_TabHovered] = RGBA(88, 28, 135, 200);
        c[ImGuiCol_TabActive] = accent;
        c[ImGuiCol_TabUnfocused] = RGBA(22, 18, 38, 150);
        c[ImGuiCol_TabUnfocusedActive] = RGBA(88, 28, 135, 150);

        c[ImGuiCol_ResizeGrip] = RGBA(0, 0, 0, 0);
        c[ImGuiCol_ResizeGripHovered] = RGBA(88, 28, 135, 200);
        c[ImGuiCol_ResizeGripActive] = accent;

        c[ImGuiCol_PlotLines] = accent_pink;
        c[ImGuiCol_PlotLinesHovered] = accent_hover;
        c[ImGuiCol_PlotHistogram] = accent;
        c[ImGuiCol_PlotHistogramHovered] = accent_hover;

        c[ImGuiCol_TableHeaderBg] = bg_mid;
        c[ImGuiCol_TableBorderStrong] = border;
        c[ImGuiCol_TableBorderLight] = RGBA(55, 45, 85, 100);
        c[ImGuiCol_TableRowBg] = RGBA(0, 0, 0, 0);
        c[ImGuiCol_TableRowBgAlt] = RGBA(255, 255, 255, 6);

        c[ImGuiCol_NavHighlight] = accent;
        c[ImGuiCol_NavWindowingHighlight] = RGBA(255, 255, 255, 40);
        c[ImGuiCol_NavWindowingDimBg] = RGBA(0, 0, 0, 120);
    }

    // ============================================
    // 主题 3: OceanBlue - 海洋蓝主题
    // 专业、稳重、清晰
    // ============================================
    inline void ApplyTheme_OceanBlue()
    {
        ImGuiStyle& style = ImGui::GetStyle();

        style.WindowRounding = 8.0f;
        style.ChildRounding = 6.0f;
        style.PopupRounding = 6.0f;
        style.FrameRounding = 4.0f;
        style.TabRounding = 4.0f;
        style.ScrollbarRounding = 6.0f;
        style.GrabRounding = 4.0f;

        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.PopupBorderSize = 1.0f;

        style.WindowPadding = ImVec2(14, 12);
        style.FramePadding = ImVec2(10, 7);
        style.ItemSpacing = ImVec2(10, 7);
        style.ItemInnerSpacing = ImVec2(8, 5);
        style.ScrollbarSize = 12.0f;

        ImVec4* c = style.Colors;

        // 背景 - 深蓝
        const ImVec4 bg_dark = RGBA(17, 24, 39);      // gray-900
        const ImVec4 bg_mid = RGBA(31, 41, 55);       // gray-800
        const ImVec4 bg_light = RGBA(55, 65, 81);     // gray-700

        // 强调色 - 蓝色
        const ImVec4 accent = RGBA(59, 130, 246);     // blue-500
        const ImVec4 accent_hover = RGBA(96, 165, 250); // blue-400
        const ImVec4 accent_dim = RGBA(30, 64, 175);  // blue-800

        // 文字
        const ImVec4 text = RGBA(243, 244, 246);      // gray-100
        const ImVec4 text_dim = RGBA(156, 163, 175);  // gray-400

        // 边框
        const ImVec4 border = RGBA(75, 85, 99);       // gray-600

        c[ImGuiCol_WindowBg] = RGBA(17, 24, 39, 250);
        c[ImGuiCol_ChildBg] = RGBA(0, 0, 0, 0);
        c[ImGuiCol_PopupBg] = RGBA(31, 41, 55, 250);

        c[ImGuiCol_Text] = text;
        c[ImGuiCol_TextDisabled] = text_dim;

        c[ImGuiCol_Border] = border;
        c[ImGuiCol_BorderShadow] = RGBA(0, 0, 0, 0);

        c[ImGuiCol_FrameBg] = RGBA(31, 41, 55, 200);
        c[ImGuiCol_FrameBgHovered] = RGBA(55, 65, 81, 200);
        c[ImGuiCol_FrameBgActive] = RGBA(75, 85, 99, 200);

        c[ImGuiCol_TitleBg] = bg_dark;
        c[ImGuiCol_TitleBgActive] = RGBA(22, 30, 48);
        c[ImGuiCol_TitleBgCollapsed] = bg_dark;

        c[ImGuiCol_ScrollbarBg] = RGBA(17, 24, 39, 100);
        c[ImGuiCol_ScrollbarGrab] = RGBA(75, 85, 99, 180);
        c[ImGuiCol_ScrollbarGrabHovered] = RGBA(107, 114, 128, 200);
        c[ImGuiCol_ScrollbarGrabActive] = accent;

        c[ImGuiCol_CheckMark] = accent;
        c[ImGuiCol_SliderGrab] = accent;
        c[ImGuiCol_SliderGrabActive] = accent_hover;

        c[ImGuiCol_Button] = RGBA(55, 65, 81, 200);
        c[ImGuiCol_ButtonHovered] = accent_dim;
        c[ImGuiCol_ButtonActive] = accent;

        c[ImGuiCol_Header] = RGBA(55, 65, 81, 180);
        c[ImGuiCol_HeaderHovered] = accent_dim;
        c[ImGuiCol_HeaderActive] = accent;

        c[ImGuiCol_Separator] = border;
        c[ImGuiCol_SeparatorHovered] = RGBA(107, 114, 128, 200);
        c[ImGuiCol_SeparatorActive] = accent;

        c[ImGuiCol_Tab] = RGBA(31, 41, 55, 200);
        c[ImGuiCol_TabHovered] = accent_dim;
        c[ImGuiCol_TabActive] = accent;
        c[ImGuiCol_TabUnfocused] = RGBA(31, 41, 55, 150);
        c[ImGuiCol_TabUnfocusedActive] = RGBA(30, 64, 175, 150);

        c[ImGuiCol_ResizeGrip] = RGBA(0, 0, 0, 0);
        c[ImGuiCol_ResizeGripHovered] = accent_dim;
        c[ImGuiCol_ResizeGripActive] = accent;

        c[ImGuiCol_PlotLines] = accent;
        c[ImGuiCol_PlotLinesHovered] = accent_hover;
        c[ImGuiCol_PlotHistogram] = accent;
        c[ImGuiCol_PlotHistogramHovered] = accent_hover;

        c[ImGuiCol_TableHeaderBg] = bg_mid;
        c[ImGuiCol_TableBorderStrong] = border;
        c[ImGuiCol_TableBorderLight] = RGBA(55, 65, 81, 100);
        c[ImGuiCol_TableRowBg] = RGBA(0, 0, 0, 0);
        c[ImGuiCol_TableRowBgAlt] = RGBA(255, 255, 255, 6);

        c[ImGuiCol_NavHighlight] = accent;
        c[ImGuiCol_NavWindowingHighlight] = RGBA(255, 255, 255, 40);
        c[ImGuiCol_NavWindowingDimBg] = RGBA(0, 0, 0, 120);
    }

} // namespace Themes

// 根据索引应用主题
inline void ApplyThemeByIndex(int index)
{
    switch (index)
    {
    case 0:
        Themes::ApplyTheme_CyberTeal();
        break;
    case 1:
        Themes::ApplyTheme_NeonPurple();
        break;
    case 2:
        Themes::ApplyTheme_OceanBlue();
        break;
    default:
        Themes::ApplyTheme_CyberTeal();
        break;
    }
}
