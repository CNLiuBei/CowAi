#include "custom_widgets.h"
#include <imgui_internal.h>
#include <cstdio>

namespace CustomWidgets
{
    // ========== 原有滑块组件 ==========
    bool SliderFloatWithKeys(const char* label, float* v, float v_min, float v_max, const char* format, float step)
    {
        return ImGui::SliderFloat(label, v, v_min, v_max, format);
    }
    
    bool SliderIntWithKeys(const char* label, int* v, int v_min, int v_max, const char* format, int step)
    {
        return ImGui::SliderInt(label, v, v_min, v_max, format);
    }

    // ========== 分组标题 ==========
    void SectionHeader(const char* icon, const char* label)
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.39f, 0.40f, 0.95f, 1.0f));  // Indigo
        
        char buf[256];
        snprintf(buf, sizeof(buf), "%s  %s", icon, label);
        ImGui::Text("%s", buf);
        
        ImGui::PopStyleColor();
        
        // 下划线
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        float width = ImGui::GetContentRegionAvail().x;
        draw_list->AddLine(
            ImVec2(p.x, p.y),
            ImVec2(p.x + width * 0.3f, p.y),
            IM_COL32(99, 102, 241, 180), 2.0f
        );
        ImGui::Spacing();
        ImGui::Spacing();
    }

    // ========== 带描述的复选框 ==========
    bool CheckboxWithDesc(const char* label, bool* v, const char* description)
    {
        bool changed = ImGui::Checkbox(label, v);
        if (description && description[0])
        {
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 25.0f);
                ImGui::TextUnformatted(description);
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
        }
        return changed;
    }

    // ========== 现代化滑块 ==========
    bool ModernSliderFloat(const char* label, float* v, float v_min, float v_max, const char* format)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 6.0f);
        
        bool changed = ImGui::SliderFloat(label, v, v_min, v_max, format);
        
        ImGui::PopStyleVar(2);
        return changed;
    }

    bool ModernSliderInt(const char* label, int* v, int v_min, int v_max)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 6.0f);
        
        bool changed = ImGui::SliderInt(label, v, v_min, v_max);
        
        ImGui::PopStyleVar(2);
        return changed;
    }

    // ========== 状态指示器 ==========
    void StatusIndicator(const char* label, bool active, const char* activeText, const char* inactiveText)
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        
        // 圆点
        float radius = 5.0f;
        ImU32 color = active ? IM_COL32(34, 197, 94, 255) : IM_COL32(239, 68, 68, 255);
        draw_list->AddCircleFilled(ImVec2(p.x + radius, p.y + ImGui::GetTextLineHeight() * 0.5f), radius, color);
        
        // 发光效果
        if (active)
        {
            draw_list->AddCircle(ImVec2(p.x + radius, p.y + ImGui::GetTextLineHeight() * 0.5f), radius + 2, IM_COL32(34, 197, 94, 100), 0, 2.0f);
        }
        
        ImGui::Dummy(ImVec2(radius * 2 + 8, 0));
        ImGui::SameLine();
        
        ImGui::Text("%s: ", label);
        ImGui::SameLine();
        
        if (active)
            ImGui::TextColored(ImVec4(0.13f, 0.77f, 0.37f, 1.0f), "%s", activeText);
        else
            ImGui::TextColored(ImVec4(0.94f, 0.27f, 0.27f, 1.0f), "%s", inactiveText);
    }

    // ========== 带颜色的标签 ==========
    void ColoredLabel(const char* label, ImVec4 color)
    {
        ImGui::TextColored(color, "%s", label);
    }

    // ========== 信息卡片 ==========
    void InfoCard(const char* title, const char* value, ImVec4 accentColor)
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        float width = ImGui::GetContentRegionAvail().x;
        float height = 50.0f;
        
        // 背景
        draw_list->AddRectFilled(
            p, ImVec2(p.x + width, p.y + height),
            IM_COL32(30, 36, 50, 200), 8.0f
        );
        
        // 左侧强调条
        draw_list->AddRectFilled(
            p, ImVec2(p.x + 4, p.y + height),
            ImGui::ColorConvertFloat4ToU32(accentColor), 4.0f
        );
        
        // 文字
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 14);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8);
        ImGui::TextDisabled("%s", title);
        
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 14);
        ImGui::TextColored(accentColor, "%s", value);
        
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8);
    }

    // ========== 进度条 ==========
    void ProgressBarEx(float fraction, const ImVec2& size, const char* overlay)
    {
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.39f, 0.40f, 0.95f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        
        ImGui::ProgressBar(fraction, size, overlay);
        
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    // ========== 分隔线（带标签）==========
    void SeparatorWithText(const char* text)
    {
        ImGui::Spacing();
        
        float width = ImGui::GetContentRegionAvail().x;
        ImVec2 textSize = ImGui::CalcTextSize(text);
        float lineWidth = (width - textSize.x - 20) * 0.5f;
        
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        float y = p.y + textSize.y * 0.5f;
        
        // 左线
        draw_list->AddLine(
            ImVec2(p.x, y),
            ImVec2(p.x + lineWidth, y),
            IM_COL32(255, 255, 255, 30), 1.0f
        );
        
        // 文字
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + lineWidth + 10);
        ImGui::TextDisabled("%s", text);
        
        // 右线
        ImVec2 p2 = ImGui::GetCursorScreenPos();
        draw_list->AddLine(
            ImVec2(p.x + lineWidth + textSize.x + 20, y),
            ImVec2(p.x + width, y),
            IM_COL32(255, 255, 255, 30), 1.0f
        );
        
        ImGui::Spacing();
    }

    // ========== 帮助标记 ==========
    void HelpMarker(const char* desc)
    {
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 25.0f);
            ImGui::TextUnformatted(desc);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    // ========== 图标按钮 ==========
    bool IconButton(const char* icon, const char* tooltip)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 8));
        bool clicked = ImGui::Button(icon);
        ImGui::PopStyleVar();
        
        if (tooltip && ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("%s", tooltip);
            ImGui::EndTooltip();
        }
        
        return clicked;
    }

    // ========== 开关按钮 ==========
    bool ToggleButton(const char* label, bool* v)
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        
        float height = ImGui::GetFrameHeight();
        float width = height * 1.8f;
        float radius = height * 0.5f - 2.0f;
        
        ImGui::InvisibleButton(label, ImVec2(width, height));
        bool clicked = ImGui::IsItemClicked();
        if (clicked)
            *v = !*v;
        
        // 背景
        ImU32 bg_color = *v ? IM_COL32(99, 102, 241, 255) : IM_COL32(60, 70, 90, 255);
        draw_list->AddRectFilled(p, ImVec2(p.x + width, p.y + height), bg_color, height * 0.5f);
        
        // 圆点
        float t = *v ? 1.0f : 0.0f;
        float circle_x = p.x + radius + 2 + t * (width - radius * 2 - 4);
        draw_list->AddCircleFilled(ImVec2(circle_x, p.y + height * 0.5f), radius, IM_COL32(255, 255, 255, 255));
        
        ImGui::SameLine();
        ImGui::Text("%s", label);
        
        return clicked;
    }

    // ========== 卡片容器 ==========
    void BeginCard(const char* id, float rounding)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, rounding);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.10f, 0.14f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 0.05f));
        ImGui::BeginChild(id, ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysAutoResize);
    }

    void EndCard()
    {
        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();
    }
}
