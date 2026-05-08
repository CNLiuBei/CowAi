#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <winsock2.h>
#include <Windows.h>

#include <string.h>
#include <algorithm>

#include <imgui/imgui.h>
#include "imgui/imgui_internal.h"

#include "config.h"
#include "sunone_aimbot_cpp.h"
#include "capture.h"
#include "other_tools.h"
#include "virtual_camera.h"
#include "draw_settings.h"
#include "overlay.h"

bool disable_winrt_futures = checkwin1903();
int monitors = get_active_monitors();

static std::vector<std::string> virtual_cameras;
static char virtual_camera_filter_buf[128] = "";
static char gstreamer_pipeline_buf[1024] = "";

void ensureVirtualCamerasLoaded()
{
    if (virtual_cameras.empty())
    {
        virtual_cameras = VirtualCameraCapture::GetAvailableVirtualCameras();
    }
}

void draw_capture_settings()
{
    static const int allowed_resolutions[] = { 160, 320, 640 };
    static int current_resolution_idx = 1;

    for (int i = 0; i < 3; ++i)
        if (config.detection_resolution == allowed_resolutions[i])
            current_resolution_idx = i;

    if (ImGui::Combo(u8"检测分辨率", &current_resolution_idx, "160\0""320\0""640\0"))
    {
        config.detection_resolution = allowed_resolutions[current_resolution_idx];
        detection_resolution_changed.store(true);
        detector_model_changed.store(true);

        globalMouseThread->updateConfig(
            config.detection_resolution,
            config.fovX,
            config.fovY,
            config.minSpeedMultiplier,
            config.maxSpeedMultiplier,
            config.predictionInterval,
            config.auto_shoot,
            config.bScope_multiplier);
        config.saveConfig();
    }

    if (ImGui::SliderInt(u8"捕获帧率", &config.capture_fps, 0, 240))
    {
        capture_fps_changed.store(true);
        config.saveConfig();
    }

    if (config.capture_fps == 0)
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(255, 0, 0, 255), u8"-> 已禁用");
    }

    if (config.capture_fps == 0 || config.capture_fps >= 61)
    {
        ImGui::TextColored(ImVec4(255, 255, 0, 255), u8"警告：过高的帧率可能会影响性能。");
    }

    std::vector<std::string> captureMethodOptions = { "duplication_api", "winrt", "virtual_camera", "gstreamer" };
    std::vector<const char*> captureMethodItems;

    for (const auto& option : captureMethodOptions)
    {
        captureMethodItems.push_back(option.c_str());
    }

    int currentcaptureMethodIndex = 0;
    for (size_t i = 0; i < captureMethodOptions.size(); ++i)
    {
        if (captureMethodOptions[i] == config.capture_method)
        {
            currentcaptureMethodIndex = static_cast<int>(i);
            break;
        }
    }

    if (ImGui::Combo(u8"捕获方式", &currentcaptureMethodIndex, captureMethodItems.data(), static_cast<int>(captureMethodItems.size()))) {
        config.capture_method = captureMethodOptions[currentcaptureMethodIndex];
        config.saveConfig();
        capture_method_changed.store(true);
    }

    if (config.capture_method == "winrt")
    {
        {
            std::vector<std::string> targetOptions = { "monitor", "window" };
            int currentTargetIndex = (config.capture_target == "window") ? 1 : 0;
            if (ImGui::Combo(u8"捕获目标 (WinRT)", &currentTargetIndex,
                [](void* data, int idx, const char** out_text) {
                    const auto* v = static_cast<std::vector<std::string>*>(data);
                    if (idx < 0 || idx >= (int)v->size()) return false;
                    *out_text = v->at(idx).c_str();
                    return true;
                }, (void*)&targetOptions, (int)targetOptions.size()))
            {
                config.capture_target = targetOptions[currentTargetIndex];
                config.saveConfig();
                capture_method_changed.store(true);
            }
        }

        if (config.capture_target == "window")
        {
            static bool initTitle = false;
            static char titleBuf[256];
            if (!initTitle)
            {
                memset(titleBuf, 0, sizeof(titleBuf));
                std::string t = config.capture_window_title;
                if (t.size() >= sizeof(titleBuf)) t = t.substr(0, sizeof(titleBuf) - 1);
                memcpy(titleBuf, t.c_str(), t.size());
                initTitle = true;
            }

            ImGui::InputText(u8"窗口标题包含", titleBuf, IM_ARRAYSIZE(titleBuf));
            ImGui::SameLine();
            if (ImGui::Button(u8"使用当前窗口"))
            {
                wchar_t wbuf[512]{};
                HWND fg = ::GetForegroundWindow();
                if (fg && ::GetWindowTextW(fg, wbuf, (int)std::size(wbuf)) > 0)
                {
                    std::wstring ws(wbuf);
                    std::string s(ws.begin(), ws.end());
                    memset(titleBuf, 0, sizeof(titleBuf));
                    auto copy = s.substr(0, sizeof(titleBuf) - 1);
                    memcpy(titleBuf, copy.c_str(), copy.size());
                }
            }
            if (ImGui::Button(u8"应用窗口目标"))
            {
                config.capture_window_title = titleBuf;
                config.saveConfig();
                capture_method_changed.store(true);
            }
        }

        if (disable_winrt_futures)
        {
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        }

        if (ImGui::Checkbox(u8"捕获边框", &config.capture_borders))
        {
            capture_borders_changed.store(true);
            config.saveConfig();
        }

        if (ImGui::Checkbox(u8"捕获光标", &config.capture_cursor))
        {
            capture_cursor_changed.store(true);
            config.saveConfig();
        }

        if (disable_winrt_futures)
        {
            ImGui::PopStyleVar();
            ImGui::PopItemFlag();
        }
    }

    if (config.capture_method == "duplication_api" || (config.capture_method == "winrt" && config.capture_target != "window"))
    {
        std::vector<std::string> monitorNames;
        if (monitors == -1)
        {
            monitorNames.push_back("Monitor 1");
        }
        else
        {
            for (int i = -1; i < monitors; ++i)
            {
                monitorNames.push_back("Monitor " + std::to_string(i + 1));
            }
        }

        std::vector<const char*> monitorItems;
        for (const auto& name : monitorNames)
        {
            monitorItems.push_back(name.c_str());
        }

        if (ImGui::Combo(u8"捕获显示器", &config.monitor_idx, monitorItems.data(), static_cast<int>(monitorItems.size())))
        {
            config.saveConfig();
            capture_method_changed.store(true);
        }
    }

    if (config.capture_method == "virtual_camera")
    {
        ensureVirtualCamerasLoaded();
        ImGui::Text(u8"选择虚拟摄像头：");

        // Filter
        ImGui::Text(u8"筛选：");
        if (ImGui::InputText("##VCFilter", virtual_camera_filter_buf, IM_ARRAYSIZE(virtual_camera_filter_buf)))
        {

        }

        std::string filter_lower = virtual_camera_filter_buf;
        std::transform(filter_lower.begin(), filter_lower.end(), filter_lower.begin(), ::tolower);

        // Filter list
        std::vector<int> filtered_indices;
        for (int i = 0; i < static_cast<int>(virtual_cameras.size()); ++i)
        {
            std::string name_lower = virtual_cameras[i];
            std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
            if (filter_lower.empty() || name_lower.find(filter_lower) != std::string::npos)
            {
                filtered_indices.push_back(i);
            }
        }

        if (!filtered_indices.empty())
        {
            int currentIndex = 0;
            for (int fi = 0; fi < static_cast<int>(filtered_indices.size()); ++fi)
            {
                if (virtual_cameras[filtered_indices[fi]] == config.virtual_camera_name)
                {
                    currentIndex = fi;
                    break;
                }
            }

            // Build items
            std::vector<const char*> items;
            items.reserve(filtered_indices.size());
            for (int idx : filtered_indices)
            {
                items.push_back(virtual_cameras[idx].c_str());
            }

            if (ImGui::Combo("##virtual_camera_combo", &currentIndex, items.data(), static_cast<int>(items.size())))
            {
                config.virtual_camera_name = virtual_cameras[filtered_indices[currentIndex]];
                config.saveConfig();
                capture_method_changed.store(true);
            }
        }
        else
        {
            ImGui::TextDisabled(u8"没有匹配的虚拟摄像头");
        }

        ImGui::SameLine();
        if (ImGui::Button(u8"刷新"))
        {
            VirtualCameraCapture::ClearCachedCameraList();
            virtual_cameras = VirtualCameraCapture::GetAvailableVirtualCameras(true);
            virtual_camera_filter_buf[0] = '\0';
        }

        if (ImGui::SliderInt(u8"虚拟摄像头宽度", &config.virtual_camera_width, 128, 3840))
        {
            config.saveConfig();
            capture_method_changed.store(true);
        }

        if (ImGui::SliderInt(u8"虚拟摄像头高度", &config.virtual_camera_heigth, 128, 2160))
        {
            config.saveConfig();
            capture_method_changed.store(true);
        }
    }

    if (config.capture_method == "gstreamer")
    {
        static bool initPipeline = false;
        if (!initPipeline)
        {
            memset(gstreamer_pipeline_buf, 0, sizeof(gstreamer_pipeline_buf));
            std::string pipeline = config.gstreamer_pipeline;
            if (pipeline.size() >= sizeof(gstreamer_pipeline_buf))
                pipeline = pipeline.substr(0, sizeof(gstreamer_pipeline_buf) - 1);
            memcpy(gstreamer_pipeline_buf, pipeline.c_str(), pipeline.size());
            initPipeline = true;
        }

        ImGui::TextWrapped(u8"GStreamer 管道（需要 appsink）。");
        ImGui::InputText("##gstreamer_pipeline", gstreamer_pipeline_buf, IM_ARRAYSIZE(gstreamer_pipeline_buf));
        if (ImGui::Button(u8"应用 GStreamer 管道"))
        {
            config.gstreamer_pipeline = gstreamer_pipeline_buf;
            config.saveConfig();
            capture_method_changed.store(true);
        }
    }
}