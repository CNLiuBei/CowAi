#ifndef CUSTOM_WIDGETS_H
#define CUSTOM_WIDGETS_H

#include "imgui/imgui.h"

namespace CustomWidgets
{
    // ========== 滑块组件 ==========
    bool SliderFloatWithKeys(const char* label, float* v, float v_min, float v_max, const char* format = "%.3f", float step = 0.0f);
    bool SliderIntWithKeys(const char* label, int* v, int v_min, int v_max, const char* format = "%d", int step = 1);
    
    // ========== 现代化组件 ==========
    
    // 带图标的分组标题
    void SectionHeader(const char* icon, const char* label);
    
    // 带描述的复选框
    bool CheckboxWithDesc(const char* label, bool* v, const char* description);
    
    // 现代化滑块（带数值显示）
    bool ModernSliderFloat(const char* label, float* v, float v_min, float v_max, const char* format = "%.2f");
    bool ModernSliderInt(const char* label, int* v, int v_min, int v_max);
    
    // 状态指示器
    void StatusIndicator(const char* label, bool active, const char* activeText = "ON", const char* inactiveText = "OFF");
    
    // 带颜色的标签
    void ColoredLabel(const char* label, ImVec4 color);
    
    // 信息卡片
    void InfoCard(const char* title, const char* value, ImVec4 accentColor);
    
    // 进度条（带百分比）
    void ProgressBarEx(float fraction, const ImVec2& size, const char* overlay = nullptr);
    
    // 分隔线（带标签）
    void SeparatorWithText(const char* text);
    
    // 工具提示辅助
    void HelpMarker(const char* desc);
    
    // 带图标的按钮
    bool IconButton(const char* icon, const char* tooltip = nullptr);
    
    // 开关按钮（Toggle）
    bool ToggleButton(const char* label, bool* v);
    
    // 渐变背景卡片开始/结束
    void BeginCard(const char* id, float rounding = 8.0f);
    void EndCard();
}

#endif // CUSTOM_WIDGETS_H
