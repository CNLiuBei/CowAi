#ifndef CUSTOM_WIDGETS_H
#define CUSTOM_WIDGETS_H

#include "imgui/imgui.h"

namespace CustomWidgets
{
    // Custom SliderFloat with keyboard arrow support
    // Press left/right arrow to adjust by step, hold to accelerate
    bool SliderFloatWithKeys(const char* label, float* v, float v_min, float v_max, const char* format = "%.3f", float step = 0.0f);
    
    // Custom SliderInt with keyboard arrow support
    bool SliderIntWithKeys(const char* label, int* v, int v_min, int v_max, const char* format = "%d", int step = 1);
}

#endif // CUSTOM_WIDGETS_H
