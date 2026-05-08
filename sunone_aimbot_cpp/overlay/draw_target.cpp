#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <winsock2.h>
#include <Windows.h>

#include "d3d11.h"
#include "imgui/imgui.h"

#include "overlay.h"
#include "draw_settings.h"
#include "sunone_aimbot_cpp.h"
#include "other_tools.h"
#include "memory_images.h"

ID3D11ShaderResourceView* bodyTexture = nullptr;
ImVec2 bodyImageSize;

bool prev_disable_headshot = config.disable_headshot;
float prev_body_y_offset = config.body_y_offset;
float prev_head_y_offset = config.head_y_offset;
bool prev_auto_aim = config.auto_aim;
bool prev_easynorecoil = config.easynorecoil;
float prev_easynorecoilstrength = config.easynorecoilstrength;

// Preset values for common games
struct OffsetPreset {
    const char* name;
    float body_offset;
    float head_offset;
};

static const OffsetPreset presets[] = {
    { u8"默认", 0.15f, 0.35f },
    // { u8"CS2", 0.20f, 0.40f },
    // { u8"Valorant", 0.18f, 0.38f },
    // { u8"Apex", 0.25f, 0.45f },
    // { u8"COD", 0.22f, 0.42f },
};
static const int presetCount = sizeof(presets) / sizeof(presets[0]);

// Track class ID changes for auto-save
int prev_class_player = config.class_player;
int prev_class_head = config.class_head;

void draw_target()
{
    // Model Class ID Section
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), u8"[模型类别设置]");
    
    ImGui::PushItemWidth(200);
    ImGui::InputInt(u8"身体类别ID", &config.class_player);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(u8"模型输出的身体/玩家类别ID (通常为0或1)");
    
    ImGui::SameLine();
    ImGui::InputInt(u8"头部类别ID", &config.class_head);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(u8"模型输出的头部类别ID (通常为0或1)");
    ImGui::PopItemWidth();
    
    // Clamp class IDs to valid range
    if (config.class_player < 0) config.class_player = 0;
    if (config.class_head < 0) config.class_head = 0;
    
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), u8"提示: 类别ID需与模型训练时的标签对应");
    
    ImGui::Separator();

    // Aim Mode Section
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), u8"[瞄准模式]");
    ImGui::Checkbox(u8"禁用爆头 (仅瞄身体)", &config.disable_headshot);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(u8"启用后将只瞄准身体，忽略头部目标");
    
    ImGui::SameLine();
    ImGui::Checkbox(u8"自动瞄准", &config.auto_aim);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(u8"启用后按住瞄准键时自动跟踪目标");

    ImGui::Separator();

    // Preset Section
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), u8"[快速预设]");
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
    for (int i = 0; i < presetCount; i++)
    {
        if (i > 0) ImGui::SameLine();
        if (ImGui::Button(presets[i].name, ImVec2(60, 0)))
        {
            config.body_y_offset = presets[i].body_offset;
            config.head_y_offset = presets[i].head_offset;
        }
    }
    ImGui::PopStyleVar();
    
    ImGui::Separator();

    // Body Offset Section
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), u8"[身体瞄准点]");
    
    ImGui::PushItemWidth(180);
    ImGui::SliderFloat(u8"##body_slider", &config.body_y_offset, 0.0f, 1.0f, u8"%.2f");
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::PushItemWidth(50);
    ImGui::InputFloat(u8"##body_input", &config.body_y_offset, 0.0f, 0.0f, "%.2f");
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button(u8"重置##body", ImVec2(45, 0)))
        config.body_y_offset = 0.15f;
    
    // Clamp body offset
    if (config.body_y_offset < 0.0f) config.body_y_offset = 0.0f;
    if (config.body_y_offset > 1.0f) config.body_y_offset = 1.0f;

    // Head Offset Section
    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), u8"[头部瞄准点]");
    
    ImGui::PushItemWidth(180);
    ImGui::SliderFloat(u8"##head_slider", &config.head_y_offset, 0.0f, 1.0f, u8"%.2f");
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::PushItemWidth(50);
    ImGui::InputFloat(u8"##head_input", &config.head_y_offset, 0.0f, 0.0f, "%.2f");
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button(u8"重置##head", ImVec2(45, 0)))
        config.head_y_offset = 0.35f;
    
    // Clamp head offset
    if (config.head_y_offset < 0.0f) config.head_y_offset = 0.0f;
    if (config.head_y_offset > 1.0f) config.head_y_offset = 1.0f;

    // Keyboard shortcuts hint
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), u8"快捷键: 方向键调整身体 | Shift+方向键调整头部");

    ImGui::Separator();

    // Visual Preview Section
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), u8"[预览]");
    
    if (bodyTexture)
    {
        // Draw preview image
        ImGui::Image((void*)bodyTexture, bodyImageSize);

        ImVec2 image_pos = ImGui::GetItemRectMin();
        ImVec2 image_size = ImGui::GetItemRectSize();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        // Body aim line (red)
        float normalized_body_value = (config.body_y_offset - 1.0f) / 1.0f;
        float body_line_y = image_pos.y + (1.0f + normalized_body_value) * image_size.y;
        ImVec2 body_line_start = ImVec2(image_pos.x, body_line_y);
        ImVec2 body_line_end = ImVec2(image_pos.x + image_size.x, body_line_y);
        
        // Draw body line with glow effect
        draw_list->AddLine(body_line_start, body_line_end, IM_COL32(255, 100, 100, 100), 6.0f);
        draw_list->AddLine(body_line_start, body_line_end, IM_COL32(255, 50, 50, 255), 2.0f);
        
        // Body crosshair marker
        float center_x = image_pos.x + image_size.x / 2;
        draw_list->AddCircleFilled(ImVec2(center_x, body_line_y), 5.0f, IM_COL32(255, 50, 50, 255));
        draw_list->AddCircle(ImVec2(center_x, body_line_y), 8.0f, IM_COL32(255, 255, 255, 200), 0, 1.5f);
        
        // Head aim line (green)
        float body_y_pos_at_015 = image_pos.y + (1.0f + (0.15f - 1.0f) / 1.0f) * image_size.y;
        float head_top_pos = image_pos.y;
        float head_line_y = head_top_pos + (config.head_y_offset * (body_y_pos_at_015 - head_top_pos));
        
        ImVec2 head_line_start = ImVec2(image_pos.x, head_line_y);
        ImVec2 head_line_end = ImVec2(image_pos.x + image_size.x, head_line_y);
        
        // Draw head line with glow effect
        draw_list->AddLine(head_line_start, head_line_end, IM_COL32(100, 255, 100, 100), 6.0f);
        draw_list->AddLine(head_line_start, head_line_end, IM_COL32(50, 255, 50, 255), 2.0f);
        
        // Head crosshair marker
        draw_list->AddCircleFilled(ImVec2(center_x, head_line_y), 5.0f, IM_COL32(50, 255, 50, 255));
        draw_list->AddCircle(ImVec2(center_x, head_line_y), 8.0f, IM_COL32(255, 255, 255, 200), 0, 1.5f);
        
        // Labels with background
        const char* body_label = u8"身体";
        const char* head_label = u8"头部";
        ImVec2 body_label_pos = ImVec2(body_line_end.x + 8, body_line_y - 8);
        ImVec2 head_label_pos = ImVec2(head_line_end.x + 8, head_line_y - 8);
        
        // Label backgrounds
        ImVec2 body_text_size = ImGui::CalcTextSize(body_label);
        ImVec2 head_text_size = ImGui::CalcTextSize(head_label);
        draw_list->AddRectFilled(
            ImVec2(body_label_pos.x - 2, body_label_pos.y - 2),
            ImVec2(body_label_pos.x + body_text_size.x + 2, body_label_pos.y + body_text_size.y + 2),
            IM_COL32(0, 0, 0, 180), 3.0f);
        draw_list->AddRectFilled(
            ImVec2(head_label_pos.x - 2, head_label_pos.y - 2),
            ImVec2(head_label_pos.x + head_text_size.x + 2, head_label_pos.y + head_text_size.y + 2),
            IM_COL32(0, 0, 0, 180), 3.0f);
        
        draw_list->AddText(body_label_pos, IM_COL32(255, 100, 100, 255), body_label);
        draw_list->AddText(head_label_pos, IM_COL32(100, 255, 100, 255), head_label);
        
        // Current values display
        char body_val[32], head_val[32];
        snprintf(body_val, sizeof(body_val), "%.0f%%", config.body_y_offset * 100);
        snprintf(head_val, sizeof(head_val), "%.0f%%", config.head_y_offset * 100);
        
        ImVec2 body_val_size = ImGui::CalcTextSize(body_val);
        ImVec2 head_val_size = ImGui::CalcTextSize(head_val);
        
        draw_list->AddText(ImVec2(image_pos.x - body_val_size.x - 5, body_line_y - 7), IM_COL32(255, 100, 100, 255), body_val);
        draw_list->AddText(ImVec2(image_pos.x - head_val_size.x - 5, head_line_y - 7), IM_COL32(100, 255, 100, 255), head_val);
    }
    else
    {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), u8"预览图片加载失败");
    }
    
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), u8"提示: 不同游戏的模型大小不同，请根据实际效果调整");

    // Auto-save on change
    if (prev_disable_headshot != config.disable_headshot ||
        prev_body_y_offset != config.body_y_offset ||
        prev_head_y_offset != config.head_y_offset ||
        prev_auto_aim != config.auto_aim ||
        prev_easynorecoil != config.easynorecoil ||
        prev_easynorecoilstrength != config.easynorecoilstrength ||
        prev_class_player != config.class_player ||
        prev_class_head != config.class_head)
    {
        prev_disable_headshot = config.disable_headshot;
        prev_body_y_offset = config.body_y_offset;
        prev_head_y_offset = config.head_y_offset;
        prev_auto_aim = config.auto_aim;
        prev_easynorecoil = config.easynorecoil;
        prev_easynorecoilstrength = config.easynorecoilstrength;
        prev_class_player = config.class_player;
        prev_class_head = config.class_head;
        config.saveConfig();
    }
}

void load_body_texture()
{
    int image_width = 0;
    int image_height = 0;

    std::string body_image = std::string(bodyImageBase64_1) + std::string(bodyImageBase64_2) + std::string(bodyImageBase64_3);

    bool ret = LoadTextureFromMemory(body_image, g_pd3dDevice, &bodyTexture, &image_width, &image_height);
    if (!ret)
    {
        std::cerr << "[Overlay] Can't load image!" << std::endl;
    }
    else
    {
        bodyImageSize = ImVec2((float)image_width, (float)image_height);
    }
}

void release_body_texture()
{
    if (bodyTexture)
    {
        bodyTexture->Release();
        bodyTexture = nullptr;
    }
}