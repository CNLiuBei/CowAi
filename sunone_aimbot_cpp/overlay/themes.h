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
    // 主题 0: GlassMorphism - 玻璃拟态主题 (默认)
    // 现代、通透、高级感
    // ============================================
    inline void ApplyTheme_GlassMorphism()
    {
        ImGuiStyle& style = ImGui::GetStyle();

        // 圆角设置 - 更大的圆角营造柔和感
        style.WindowRounding = 16.0f;
        style.ChildRounding = 12.0f;
        style.PopupRounding = 12.0f;
        style.FrameRounding = 8.0f;
        style.TabRounding = 8.0f;
        style.ScrollbarRounding = 10.0f;
        style.GrabRounding = 8.0f;

        // 边框
        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.PopupBorderSize = 1.0f;
        style.TabBorderSize = 0.0f;

        // 间距 - 更宽松的布局
        style.WindowPadding = ImVec2(20, 16);
        style.FramePadding = ImVec2(14, 10);
        style.ItemSpacing = ImVec2(12, 10);
        style.ItemInnerSpacing = ImVec2(10, 8);
        style.ScrollbarSize = 14.0f;
        style.GrabMinSize = 14.0f;

        ImVec4* c = style.Colors;

        // 背景色 - 深色玻璃效果
        const ImVec4 bg_glass = RGBA(12, 14, 20, 230);       // 深色半透明
        const ImVec4 bg_card = RGBA(22, 27, 38, 200);        // 卡片背景
        const ImVec4 bg_hover = RGBA(35, 42, 58, 220);       // 悬停背景

        // 强调色 - 渐变蓝紫
        const ImVec4 accent_primary = RGBA(99, 102, 241);    // Indigo-500
        const ImVec4 accent_hover = RGBA(129, 140, 248);     // Indigo-400
        const ImVec4 accent_glow = RGBA(99, 102, 241, 80);   // 发光效果

        // 成功/警告色
        const ImVec4 success = RGBA(34, 197, 94);            // Green-500
        const ImVec4 warning = RGBA(251, 191, 36);           // Amber-400

        // 文字
        const ImVec4 text_primary = RGBA(248, 250, 252);     // Slate-50
        const ImVec4 text_secondary = RGBA(148, 163, 184);   // Slate-400
        const ImVec4 text_muted = RGBA(100, 116, 139);       // Slate-500

        // 边框 - 微妙的玻璃边缘
        const ImVec4 border_glass = RGBA(255, 255, 255, 15); // 白色微透明
        const ImVec4 border_active = RGBA(99, 102, 241, 100);

        // 窗口
        c[ImGuiCol_WindowBg] = bg_glass;
        c[ImGuiCol_ChildBg] = RGBA(0, 0, 0, 0);
        c[ImGuiCol_PopupBg] = RGBA(18, 22, 32, 245);

        // 文字
        c[ImGuiCol_Text] = text_primary;
        c[ImGuiCol_TextDisabled] = text_muted;

        // 边框
        c[ImGuiCol_Border] = border_glass;
        c[ImGuiCol_BorderShadow] = RGBA(0, 0, 0, 0);

        // 输入框/滑块背景 - 玻璃卡片效果
        c[ImGuiCol_FrameBg] = RGBA(30, 36, 50, 180);
        c[ImGuiCol_FrameBgHovered] = RGBA(45, 55, 75, 200);
        c[ImGuiCol_FrameBgActive] = RGBA(55, 65, 90, 220);

        // 标题栏
        c[ImGuiCol_TitleBg] = RGBA(8, 10, 15, 250);
        c[ImGuiCol_TitleBgActive] = RGBA(12, 15, 22, 250);
        c[ImGuiCol_TitleBgCollapsed] = RGBA(8, 10, 15, 200);

        // 滚动条 - 细腻的玻璃效果
        c[ImGuiCol_ScrollbarBg] = RGBA(15, 18, 25, 100);
        c[ImGuiCol_ScrollbarGrab] = RGBA(60, 70, 90, 150);
        c[ImGuiCol_ScrollbarGrabHovered] = RGBA(80, 95, 120, 180);
        c[ImGuiCol_ScrollbarGrabActive] = accent_primary;

        // 复选框/滑块
        c[ImGuiCol_CheckMark] = accent_primary;
        c[ImGuiCol_SliderGrab] = accent_primary;
        c[ImGuiCol_SliderGrabActive] = accent_hover;

        // 按钮 - 玻璃按钮效果
        c[ImGuiCol_Button] = RGBA(45, 55, 75, 160);
        c[ImGuiCol_ButtonHovered] = RGBA(99, 102, 241, 140);
        c[ImGuiCol_ButtonActive] = accent_primary;

        // 头部/选项
        c[ImGuiCol_Header] = RGBA(45, 55, 75, 140);
        c[ImGuiCol_HeaderHovered] = RGBA(99, 102, 241, 120);
        c[ImGuiCol_HeaderActive] = accent_primary;

        // 分隔线
        c[ImGuiCol_Separator] = RGBA(255, 255, 255, 20);
        c[ImGuiCol_SeparatorHovered] = RGBA(99, 102, 241, 100);
        c[ImGuiCol_SeparatorActive] = accent_primary;

        // 标签页 - 现代标签样式
        c[ImGuiCol_Tab] = RGBA(30, 36, 50, 180);
        c[ImGuiCol_TabHovered] = RGBA(99, 102, 241, 140);
        c[ImGuiCol_TabActive] = accent_primary;
        c[ImGuiCol_TabUnfocused] = RGBA(25, 30, 42, 150);
        c[ImGuiCol_TabUnfocusedActive] = RGBA(70, 75, 160, 180);

        // 调整大小手柄
        c[ImGuiCol_ResizeGrip] = RGBA(0, 0, 0, 0);
        c[ImGuiCol_ResizeGripHovered] = RGBA(99, 102, 241, 100);
        c[ImGuiCol_ResizeGripActive] = accent_primary;

        // 图表
        c[ImGuiCol_PlotLines] = accent_primary;
        c[ImGuiCol_PlotLinesHovered] = accent_hover;
        c[ImGuiCol_PlotHistogram] = RGBA(34, 197, 94);
        c[ImGuiCol_PlotHistogramHovered] = RGBA(74, 222, 128);

        // 表格
        c[ImGuiCol_TableHeaderBg] = RGBA(25, 30, 42, 200);
        c[ImGuiCol_TableBorderStrong] = RGBA(255, 255, 255, 20);
        c[ImGuiCol_TableBorderLight] = RGBA(255, 255, 255, 10);
        c[ImGuiCol_TableRowBg] = RGBA(0, 0, 0, 0);
        c[ImGuiCol_TableRowBgAlt] = RGBA(255, 255, 255, 5);

        // 导航
        c[ImGuiCol_NavHighlight] = accent_primary;
        c[ImGuiCol_NavWindowingHighlight] = RGBA(255, 255, 255, 50);
        c[ImGuiCol_NavWindowingDimBg] = RGBA(0, 0, 0, 150);

        // 模态遮罩
        c[ImGuiCol_ModalWindowDimBg] = RGBA(0, 0, 0, 180);
    }

    // ============================================
    // 主题 1: CyberTeal - 赛博青绿主题
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

    // ============================================
    // 主题 4: NeoMint - 新薄荷主题
    // 清新、活力、护眼
    // ============================================
    inline void ApplyTheme_NeoMint()
    {
        ImGuiStyle& style = ImGui::GetStyle();

        style.WindowRounding = 14.0f;
        style.ChildRounding = 10.0f;
        style.PopupRounding = 10.0f;
        style.FrameRounding = 7.0f;
        style.TabRounding = 7.0f;
        style.ScrollbarRounding = 9.0f;
        style.GrabRounding = 7.0f;

        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.PopupBorderSize = 1.0f;

        style.WindowPadding = ImVec2(18, 14);
        style.FramePadding = ImVec2(12, 9);
        style.ItemSpacing = ImVec2(11, 9);
        style.ItemInnerSpacing = ImVec2(9, 7);
        style.ScrollbarSize = 13.0f;

        ImVec4* c = style.Colors;

        // 背景 - 深绿灰
        const ImVec4 bg_dark = RGBA(13, 20, 18);
        const ImVec4 bg_mid = RGBA(22, 33, 30);
        const ImVec4 bg_light = RGBA(35, 50, 45);

        // 强调色 - 薄荷绿
        const ImVec4 accent = RGBA(52, 211, 153);      // Emerald-400
        const ImVec4 accent_hover = RGBA(110, 231, 183); // Emerald-300
        const ImVec4 accent_dim = RGBA(6, 78, 59);     // Emerald-900

        // 文字
        const ImVec4 text = RGBA(236, 253, 245);       // Emerald-50
        const ImVec4 text_dim = RGBA(134, 239, 172);   // Green-300 dimmed

        // 边框
        const ImVec4 border = RGBA(52, 211, 153, 40);

        c[ImGuiCol_WindowBg] = RGBA(13, 20, 18, 245);
        c[ImGuiCol_ChildBg] = RGBA(0, 0, 0, 0);
        c[ImGuiCol_PopupBg] = RGBA(18, 28, 25, 250);

        c[ImGuiCol_Text] = text;
        c[ImGuiCol_TextDisabled] = RGBA(100, 130, 120);

        c[ImGuiCol_Border] = border;
        c[ImGuiCol_BorderShadow] = RGBA(0, 0, 0, 0);

        c[ImGuiCol_FrameBg] = RGBA(25, 38, 35, 200);
        c[ImGuiCol_FrameBgHovered] = RGBA(35, 55, 50, 220);
        c[ImGuiCol_FrameBgActive] = RGBA(45, 70, 62, 240);

        c[ImGuiCol_TitleBg] = bg_dark;
        c[ImGuiCol_TitleBgActive] = RGBA(16, 25, 22);
        c[ImGuiCol_TitleBgCollapsed] = bg_dark;

        c[ImGuiCol_ScrollbarBg] = RGBA(13, 20, 18, 100);
        c[ImGuiCol_ScrollbarGrab] = RGBA(52, 211, 153, 100);
        c[ImGuiCol_ScrollbarGrabHovered] = RGBA(52, 211, 153, 150);
        c[ImGuiCol_ScrollbarGrabActive] = accent;

        c[ImGuiCol_CheckMark] = accent;
        c[ImGuiCol_SliderGrab] = accent;
        c[ImGuiCol_SliderGrabActive] = accent_hover;

        c[ImGuiCol_Button] = RGBA(35, 55, 50, 180);
        c[ImGuiCol_ButtonHovered] = RGBA(52, 211, 153, 100);
        c[ImGuiCol_ButtonActive] = accent;

        c[ImGuiCol_Header] = RGBA(35, 55, 50, 160);
        c[ImGuiCol_HeaderHovered] = RGBA(52, 211, 153, 100);
        c[ImGuiCol_HeaderActive] = accent;

        c[ImGuiCol_Separator] = border;
        c[ImGuiCol_SeparatorHovered] = RGBA(52, 211, 153, 120);
        c[ImGuiCol_SeparatorActive] = accent;

        c[ImGuiCol_Tab] = RGBA(25, 38, 35, 200);
        c[ImGuiCol_TabHovered] = RGBA(52, 211, 153, 120);
        c[ImGuiCol_TabActive] = accent;
        c[ImGuiCol_TabUnfocused] = RGBA(20, 30, 28, 150);
        c[ImGuiCol_TabUnfocusedActive] = RGBA(6, 78, 59, 180);

        c[ImGuiCol_ResizeGrip] = RGBA(0, 0, 0, 0);
        c[ImGuiCol_ResizeGripHovered] = RGBA(52, 211, 153, 100);
        c[ImGuiCol_ResizeGripActive] = accent;

        c[ImGuiCol_PlotLines] = accent;
        c[ImGuiCol_PlotLinesHovered] = accent_hover;
        c[ImGuiCol_PlotHistogram] = accent;
        c[ImGuiCol_PlotHistogramHovered] = accent_hover;

        c[ImGuiCol_TableHeaderBg] = bg_mid;
        c[ImGuiCol_TableBorderStrong] = border;
        c[ImGuiCol_TableBorderLight] = RGBA(52, 211, 153, 20);
        c[ImGuiCol_TableRowBg] = RGBA(0, 0, 0, 0);
        c[ImGuiCol_TableRowBgAlt] = RGBA(255, 255, 255, 5);

        c[ImGuiCol_NavHighlight] = accent;
        c[ImGuiCol_NavWindowingHighlight] = RGBA(255, 255, 255, 40);
        c[ImGuiCol_NavWindowingDimBg] = RGBA(0, 0, 0, 120);
    }

    // ============================================
    // 主题 5: RoseGold - 玫瑰金主题
    // 优雅、温暖、高端
    // ============================================
    inline void ApplyTheme_RoseGold()
    {
        ImGuiStyle& style = ImGui::GetStyle();

        style.WindowRounding = 14.0f;
        style.ChildRounding = 10.0f;
        style.PopupRounding = 10.0f;
        style.FrameRounding = 7.0f;
        style.TabRounding = 7.0f;
        style.ScrollbarRounding = 9.0f;
        style.GrabRounding = 7.0f;

        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.PopupBorderSize = 1.0f;

        style.WindowPadding = ImVec2(18, 14);
        style.FramePadding = ImVec2(12, 9);
        style.ItemSpacing = ImVec2(11, 9);
        style.ItemInnerSpacing = ImVec2(9, 7);
        style.ScrollbarSize = 13.0f;

        ImVec4* c = style.Colors;

        // 背景 - 深玫瑰灰
        const ImVec4 bg_dark = RGBA(20, 15, 18);
        const ImVec4 bg_mid = RGBA(32, 25, 30);
        const ImVec4 bg_light = RGBA(50, 40, 46);

        // 强调色 - 玫瑰金
        const ImVec4 accent = RGBA(244, 114, 182);     // Pink-400
        const ImVec4 accent_hover = RGBA(251, 146, 191); // Pink-300
        const ImVec4 accent_dim = RGBA(131, 24, 67);   // Pink-900

        // 文字
        const ImVec4 text = RGBA(253, 242, 248);       // Pink-50
        const ImVec4 text_dim = RGBA(180, 150, 165);

        // 边框
        const ImVec4 border = RGBA(244, 114, 182, 40);

        c[ImGuiCol_WindowBg] = RGBA(20, 15, 18, 245);
        c[ImGuiCol_ChildBg] = RGBA(0, 0, 0, 0);
        c[ImGuiCol_PopupBg] = RGBA(28, 22, 26, 250);

        c[ImGuiCol_Text] = text;
        c[ImGuiCol_TextDisabled] = RGBA(130, 110, 120);

        c[ImGuiCol_Border] = border;
        c[ImGuiCol_BorderShadow] = RGBA(0, 0, 0, 0);

        c[ImGuiCol_FrameBg] = RGBA(38, 30, 35, 200);
        c[ImGuiCol_FrameBgHovered] = RGBA(55, 45, 52, 220);
        c[ImGuiCol_FrameBgActive] = RGBA(70, 55, 65, 240);

        c[ImGuiCol_TitleBg] = bg_dark;
        c[ImGuiCol_TitleBgActive] = RGBA(25, 18, 22);
        c[ImGuiCol_TitleBgCollapsed] = bg_dark;

        c[ImGuiCol_ScrollbarBg] = RGBA(20, 15, 18, 100);
        c[ImGuiCol_ScrollbarGrab] = RGBA(244, 114, 182, 100);
        c[ImGuiCol_ScrollbarGrabHovered] = RGBA(244, 114, 182, 150);
        c[ImGuiCol_ScrollbarGrabActive] = accent;

        c[ImGuiCol_CheckMark] = accent;
        c[ImGuiCol_SliderGrab] = accent;
        c[ImGuiCol_SliderGrabActive] = accent_hover;

        c[ImGuiCol_Button] = RGBA(55, 45, 52, 180);
        c[ImGuiCol_ButtonHovered] = RGBA(244, 114, 182, 100);
        c[ImGuiCol_ButtonActive] = accent;

        c[ImGuiCol_Header] = RGBA(55, 45, 52, 160);
        c[ImGuiCol_HeaderHovered] = RGBA(244, 114, 182, 100);
        c[ImGuiCol_HeaderActive] = accent;

        c[ImGuiCol_Separator] = border;
        c[ImGuiCol_SeparatorHovered] = RGBA(244, 114, 182, 120);
        c[ImGuiCol_SeparatorActive] = accent;

        c[ImGuiCol_Tab] = RGBA(38, 30, 35, 200);
        c[ImGuiCol_TabHovered] = RGBA(244, 114, 182, 120);
        c[ImGuiCol_TabActive] = accent;
        c[ImGuiCol_TabUnfocused] = RGBA(28, 22, 26, 150);
        c[ImGuiCol_TabUnfocusedActive] = RGBA(131, 24, 67, 180);

        c[ImGuiCol_ResizeGrip] = RGBA(0, 0, 0, 0);
        c[ImGuiCol_ResizeGripHovered] = RGBA(244, 114, 182, 100);
        c[ImGuiCol_ResizeGripActive] = accent;

        c[ImGuiCol_PlotLines] = accent;
        c[ImGuiCol_PlotLinesHovered] = accent_hover;
        c[ImGuiCol_PlotHistogram] = accent;
        c[ImGuiCol_PlotHistogramHovered] = accent_hover;

        c[ImGuiCol_TableHeaderBg] = bg_mid;
        c[ImGuiCol_TableBorderStrong] = border;
        c[ImGuiCol_TableBorderLight] = RGBA(244, 114, 182, 20);
        c[ImGuiCol_TableRowBg] = RGBA(0, 0, 0, 0);
        c[ImGuiCol_TableRowBgAlt] = RGBA(255, 255, 255, 5);

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
        Themes::ApplyTheme_GlassMorphism();
        break;
    case 1:
        Themes::ApplyTheme_CyberTeal();
        break;
    case 2:
        Themes::ApplyTheme_NeonPurple();
        break;
    case 3:
        Themes::ApplyTheme_OceanBlue();
        break;
    case 4:
        Themes::ApplyTheme_NeoMint();
        break;
    case 5:
        Themes::ApplyTheme_RoseGold();
        break;
    default:
        Themes::ApplyTheme_GlassMorphism();
        break;
    }
}
