// draw_dataset.cpp - Dataset collection UI
#pragma execution_character_set("utf-8")
#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <winsock2.h>
#include <Windows.h>

#include "imgui/imgui.h"
#include "config.h"
#include "sunone_aimbot_cpp.h"
#include "dataset/dataset_collector.h"

extern Config config;
extern DatasetCollector* datasetCollector;

void draw_dataset_settings()
{
    ImGui::SeparatorText("\xE6\x95\xB0\xE6\x8D\xAE\xE9\x9B\x86\xE9\x87\x87\xE9\x9B\x86\xE8\xAE\xBE\xE7\xBD\xAE");
    
    bool enabled = config.dataset_enabled;
    if (ImGui::Checkbox("\xE5\x90\xAF\xE7\x94\xA8\xE6\x95\xB0\xE6\x8D\xAE\xE9\x9B\x86\xE9\x87\x87\xE9\x9B\x86", &enabled))
    {
        config.dataset_enabled = enabled;
        if (datasetCollector)
        {
            datasetCollector->EnableDatasetMode(enabled);
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("\xE5\x90\xAF\xE7\x94\xA8\xE5\x90\x8E\xEF\xBC\x8C\xE6\x8C\x89\xE4\xBD\x8F\xE7\x83\xAD\xE9\x94\xAE\xE6\x97\xB6\xE4\xBC\x9A\xE8\x87\xAA\xE5\x8A\xA8\xE9\x87\x87\xE9\x9B\x86\xE6\xA3\x80\xE6\xB5\x8B\xE6\x95\xB0\xE6\x8D\xAE");
        ImGui::EndTooltip();
    }
    
    static char outputDir[256];
    strncpy_s(outputDir, config.dataset_output_dir.c_str(), sizeof(outputDir) - 1);
    ImGui::InputText("\xE8\xBE\x93\xE5\x87\xBA\xE7\x9B\xAE\xE5\xBD\x95", outputDir, sizeof(outputDir));
    if (ImGui::IsItemDeactivatedAfterEdit())
    {
        config.dataset_output_dir = outputDir;
    }
    
    ImGui::SeparatorText("\xE9\x87\x87\xE9\x9B\x86\xE7\xBB\x9F\xE8\xAE\xA1");
    
    if (datasetCollector)
    {
        DatasetStats stats = datasetCollector->GetCurrentStats();
        
        ImGui::Columns(2, "stats_columns", false);
        
        ImGui::Text("\xE5\xB7\xB2\xE4\xBF\x9D\xE5\xAD\x98: %d", stats.savedCount);
        ImGui::Text("\xE5\xB7\xB2\xE4\xB8\xA2\xE5\xBC\x83: %d", stats.discardedCount);
        ImGui::Text("\xE5\x8E\xBB\xE9\x87\x8D\xE4\xB8\xA2\xE5\xBC\x83: %d", stats.deduplicatedCount);
        
        ImGui::NextColumn();

        ImGui::Text("\xE8\xB4\xA8\xE9\x87\x8F\xE8\xBF\x87\xE6\xBB\xA4: %d", stats.qualityRejectedCount);
        ImGui::Text("\xE9\xAA\x8C\xE8\xAF\x81\xE8\xBF\x87\xE6\xBB\xA4: %d", stats.validationRejectedCount);
        ImGui::Text("\xE9\xA2\x91\xE7\x8E\x87\xE9\x99\x90\xE5\x88\xB6: %d", stats.rateLimitedCount);
        
        ImGui::Columns(1);
        
        ImGui::Separator();
        ImGui::Text("Head: %d | Body: %d | Paired: %d", 
            stats.headCount, stats.bodyCount, stats.pairedCount);
        
        if (config.dataset_upload_enabled)
        {
            ImGui::Text("\xE5\x8C\x85: %d \xE5\xB7\xB2\xE5\x88\x9B\xE5\xBB\xBA | %d \xE5\xB7\xB2\xE4\xB8\x8A\xE4\xBC\xA0 | %d \xE5\xA4\xB1\xE8\xB4\xA5", 
                stats.packagesCreated, stats.packagesUploaded, stats.uploadFailures);
        }
        
        ImGui::Separator();
        bool isEnabled = datasetCollector->IsEnabled();
        bool isHotkeyPressed = datasetCollector->IsHotkeyPressed();
        
        if (isEnabled)
        {
            if (isHotkeyPressed)
            {
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "\xE2\x97\x8F \xE6\xAD\xA3\xE5\x9C\xA8\xE9\x87\x87\xE9\x9B\x86...");
            }
            else
            {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.2f, 1.0f), "\xE2\x97\x8F \xE7\xAD\x89\xE5\xBE\x85\xE7\x83\xAD\xE9\x94\xAE");
            }
        }
        else
        {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "\xE2\x97\x8B \xE5\xB7\xB2\xE7\xA6\x81\xE7\x94\xA8");
        }
    }
    else
    {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "\xE6\x95\xB0\xE6\x8D\xAE\xE9\x9B\x86\xE9\x87\x87\xE9\x9B\x86\xE5\x99\xA8\xE6\x9C\xAA\xE5\x88\x9D\xE5\xA7\x8B\xE5\x8C\x96");
    }

    ImGui::SeparatorText("\xE9\xAA\x8C\xE8\xAF\x81\xE9\x98\x88\xE5\x80\xBC");
    
    ImGui::SliderFloat("Head \xE7\xBD\xAE\xE4\xBF\xA1\xE5\xBA\xA6", &config.dataset_head_confidence, 0.1f, 1.0f, "%.2f");
    ImGui::SliderFloat("Body \xE7\xBD\xAE\xE4\xBF\xA1\xE5\xBA\xA6", &config.dataset_body_confidence, 0.1f, 1.0f, "%.2f");
    ImGui::SliderInt("Head \xE6\x9C\x80\xE5\xB0\x8F\xE5\xB0\xBA\xE5\xAF\xB8", &config.dataset_head_min_size, 1, 50);
    ImGui::SliderInt("Body \xE6\x9C\x80\xE5\xB0\x8F\xE9\xAB\x98\xE5\xBA\xA6", &config.dataset_body_min_height, 1, 100);
    ImGui::SliderFloat("\xE5\x90\x8C\xE4\xBA\xBA IoU \xE9\x98\x88\xE5\x80\xBC", &config.dataset_same_person_iou, 0.0f, 1.0f, "%.2f");
    
    ImGui::SeparatorText("\xE8\xB4\xA8\xE9\x87\x8F\xE8\xBF\x87\xE6\xBB\xA4");
    
    float laplacian = static_cast<float>(config.dataset_laplacian_threshold);
    if (ImGui::SliderFloat("Laplacian \xE9\x98\x88\xE5\x80\xBC", &laplacian, 10.0f, 500.0f, "%.0f"))
    {
        config.dataset_laplacian_threshold = static_cast<double>(laplacian);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("\xE8\xB6\x8A\xE4\xBD\x8E\xE8\xB6\x8A\xE4\xB8\xA5\xE6\xA0\xBC\xEF\xBC\x8C\xE8\xBF\x87\xE6\xBB\xA4\xE6\xA8\xA1\xE7\xB3\x8A\xE5\x9B\xBE\xE5\x83\x8F");
        ImGui::EndTooltip();
    }
    
    ImGui::SliderFloat("\xE8\xBF\x87\xE6\x9B\x9D\xE9\x98\x88\xE5\x80\xBC", &config.dataset_overexposure_threshold, 0.1f, 1.0f, "%.2f");
    ImGui::SliderFloat("\xE6\xAC\xA0\xE6\x9B\x9D\xE9\x98\x88\xE5\x80\xBC", &config.dataset_underexposure_threshold, 0.1f, 1.0f, "%.2f");

    ImGui::SeparatorText("\xE5\x8E\xBB\xE9\x87\x8D\xE8\xAE\xBE\xE7\xBD\xAE");
    
    ImGui::SliderInt("\xE5\x93\x88\xE5\xB8\x8C\xE7\xBC\x93\xE5\x86\xB2\xE5\x8C\xBA\xE5\xA4\xA7\xE5\xB0\x8F", &config.dataset_hash_buffer_size, 100, 2000);
    ImGui::SliderFloat("\xE5\x9B\xBE\xE5\x83\x8F\xE7\x9B\xB8\xE4\xBC\xBC\xE5\xBA\xA6\xE9\x98\x88\xE5\x80\xBC", &config.dataset_image_similarity, 0.5f, 1.0f, "%.2f");
    ImGui::SliderFloat("\xE7\x9B\xAE\xE6\xA0\x87 IoU \xE9\x98\x88\xE5\x80\xBC", &config.dataset_target_iou, 0.5f, 1.0f, "%.2f");
    ImGui::SliderInt("\xE7\x9B\xAE\xE6\xA0\x87\xE4\xBD\x8D\xE7\xA7\xBB\xE9\x98\x88\xE5\x80\xBC", &config.dataset_target_displacement, 1, 50);
    
    ImGui::SeparatorText("\xE9\xA2\x91\xE7\x8E\x87\xE6\x8E\xA7\xE5\x88\xB6");
    
    ImGui::SliderInt("\xE5\x86\xB7\xE5\x8D\xB4\xE6\x97\xB6\xE9\x97\xB4 (ms)", &config.dataset_cooldown_ms, 50, 2000);
    
    ImGui::SeparatorText("\xE6\x89\x93\xE5\x8C\x85\xE4\xB8\x8A\xE4\xBC\xA0 (WebDAV)");
    
    ImGui::SliderInt("\xE6\x89\x93\xE5\x8C\x85\xE5\xA4\xA7\xE5\xB0\x8F", &config.dataset_package_size, 50, 1000);
    
    ImGui::Checkbox("\xE5\x90\xAF\xE7\x94\xA8\xE4\xB8\x8A\xE4\xBC\xA0", &config.dataset_upload_enabled);
    
    if (config.dataset_upload_enabled)
    {
        static char webdavEndpoint[256];
        strncpy_s(webdavEndpoint, config.dataset_webdav_endpoint.c_str(), sizeof(webdavEndpoint) - 1);
        ImGui::InputText("WebDAV \xE7\xAB\xAF\xE7\x82\xB9", webdavEndpoint, sizeof(webdavEndpoint));
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            config.dataset_webdav_endpoint = webdavEndpoint;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("\xE4\xBE\x8B\xE5\xA6\x82: https://example.com/dav");
            ImGui::EndTooltip();
        }
        
        static char webdavRemotePath[128];
        strncpy_s(webdavRemotePath, config.dataset_webdav_remote_path.c_str(), sizeof(webdavRemotePath) - 1);
        ImGui::InputText("\xE8\xBF\x9C\xE7\xA8\x8B\xE8\xB7\xAF\xE5\xBE\x84", webdavRemotePath, sizeof(webdavRemotePath));
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            config.dataset_webdav_remote_path = webdavRemotePath;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("\xE4\xBE\x8B\xE5\xA6\x82: /aim");
            ImGui::EndTooltip();
        }
        
        static char webdavUsername[128];
        strncpy_s(webdavUsername, config.dataset_webdav_username.c_str(), sizeof(webdavUsername) - 1);
        ImGui::InputText("\xE7\x94\xA8\xE6\x88\xB7\xE5\x90\x8D", webdavUsername, sizeof(webdavUsername));
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            config.dataset_webdav_username = webdavUsername;
        }
        
        static char webdavPassword[128];
        strncpy_s(webdavPassword, config.dataset_webdav_password.c_str(), sizeof(webdavPassword) - 1);
        ImGui::InputText("\xE5\xAF\x86\xE7\xA0\x81", webdavPassword, sizeof(webdavPassword), ImGuiInputTextFlags_Password);
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            config.dataset_webdav_password = webdavPassword;
        }
        
        ImGui::Checkbox("\xE4\xBD\xBF\xE7\x94\xA8 HTTPS", &config.dataset_webdav_use_ssl);
    }

    ImGui::Separator();
    if (ImGui::Button("\xE5\xBA\x94\xE7\x94\xA8\xE9\x85\x8D\xE7\xBD\xAE"))
    {
        if (datasetCollector)
        {
            DatasetConfig dsConfig;
            dsConfig.enabled = config.dataset_enabled;
            dsConfig.outputDirectory = config.dataset_output_dir;
            dsConfig.classHead = config.class_head;      // 从主配置读取类别 ID
            dsConfig.classBody = config.class_player;    // class_player 对应 body
            dsConfig.headConfidenceThreshold = config.dataset_head_confidence;
            dsConfig.bodyConfidenceThreshold = config.dataset_body_confidence;
            dsConfig.headMinSize = config.dataset_head_min_size;
            dsConfig.bodyMinHeight = config.dataset_body_min_height;
            dsConfig.samePersonIouThreshold = config.dataset_same_person_iou;
            dsConfig.laplacianThreshold = config.dataset_laplacian_threshold;
            dsConfig.overexposureThreshold = config.dataset_overexposure_threshold;
            dsConfig.underexposureThreshold = config.dataset_underexposure_threshold;
            dsConfig.hashBufferSize = config.dataset_hash_buffer_size;
            dsConfig.imageSimilarityThreshold = config.dataset_image_similarity;
            dsConfig.targetIouThreshold = config.dataset_target_iou;
            dsConfig.targetDisplacementThreshold = config.dataset_target_displacement;
            dsConfig.cooldownMs = config.dataset_cooldown_ms;
            dsConfig.packageSize = config.dataset_package_size;
            dsConfig.uploadEnabled = config.dataset_upload_enabled;
            dsConfig.uploadEndpoint = config.dataset_upload_endpoint;
            
            datasetCollector->LoadConfig(dsConfig);
            
            // Configure WebDAV
            WebDAVConfig webdavConfig;
            webdavConfig.endpoint = config.dataset_webdav_endpoint;
            webdavConfig.username = config.dataset_webdav_username;
            webdavConfig.password = config.dataset_webdav_password;
            webdavConfig.remotePath = config.dataset_webdav_remote_path;
            webdavConfig.useSSL = config.dataset_webdav_use_ssl;
            datasetCollector->ConfigureWebDAV(webdavConfig);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("\xE4\xBF\x9D\xE5\xAD\x98\xE9\x85\x8D\xE7\xBD\xAE"))
    {
        config.saveConfig("config.ini");
    }
}
