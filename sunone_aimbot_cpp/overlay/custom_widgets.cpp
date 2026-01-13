#include "custom_widgets.h"
#include <imgui_internal.h>

namespace CustomWidgets
{
    bool SliderFloatWithKeys(const char* label, float* v, float v_min, float v_max, const char* format, float step)
    {
        return ImGui::SliderFloat(label, v, v_min, v_max, format);
    }
    
    bool SliderIntWithKeys(const char* label, int* v, int v_min, int v_max, const char* format, int step)
    {
        return ImGui::SliderInt(label, v, v_min, v_max, format);
    }
}
