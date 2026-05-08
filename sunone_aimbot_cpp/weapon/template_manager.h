#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <opencv2/opencv.hpp>

struct MatchResult {
    std::string name = "None";
    float confidence = 0.0f;
};

struct HudRegion {
    int x = 0, y = 0, width = 0, height = 0;
};

class TemplateManager {
public:
    bool loadTemplates(const std::string& basePath, int screenWidth, int screenHeight);
    void unloadTemplates();
    bool isLoaded() const { return loaded_; }

    MatchResult matchCategory(const cv::Mat& grayRoi, const std::string& category) const;

    HudRegion getRegion(const std::string& regionName) const;
    bool hasRegion(const std::string& regionName) const;

    cv::Point getShootPixel() const { return shootPixel_; }
    int getShootThreshold() const { return shootThreshold_; }

    // 公开给测试用
    static std::string findBestResolution(const std::string& basePath, int screenW, int screenH);
    static cv::Rect clampRoi(const HudRegion& region, int frameWidth, int frameHeight);

private:
    bool loadConfigJson(const std::string& jsonPath);
    bool loadCategoryTemplates(const std::string& dirPath, const std::string& category);

    // category -> [(name, grayTemplate)]
    std::unordered_map<std::string, std::vector<std::pair<std::string, cv::Mat>>> templates_;
    std::unordered_map<std::string, HudRegion> regions_;
    cv::Point shootPixel_{0, 0};
    int shootThreshold_ = 760;
    bool loaded_ = false;
};
