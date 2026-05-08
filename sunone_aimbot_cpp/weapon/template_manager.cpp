#include "template_manager.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <regex>

namespace fs = std::filesystem;

// ---- 简易 JSON 解析辅助 (仅支持 config.json 的简单格式) ----
namespace {

std::string readFileToString(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// 从 JSON 字符串中提取 "key": [x, y, w, h] 数组
bool parseIntArray(const std::string& json, const std::string& key, std::vector<int>& out) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return false;

    auto bracket = json.find('[', pos);
    if (bracket == std::string::npos) return false;
    auto end = json.find(']', bracket);
    if (end == std::string::npos) return false;

    std::string arr = json.substr(bracket + 1, end - bracket - 1);
    std::stringstream ss(arr);
    std::string token;
    out.clear();
    while (std::getline(ss, token, ',')) {
        try { out.push_back(std::stoi(token)); }
        catch (...) { return false; }
    }
    return true;
}

// 从 JSON 中提取 "key": { "x": val, "y": val }
bool parsePointObject(const std::string& json, const std::string& key, int& x, int& y) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return false;

    auto brace = json.find('{', pos);
    if (brace == std::string::npos) return false;
    auto end = json.find('}', brace);
    if (end == std::string::npos) return false;

    std::string obj = json.substr(brace, end - brace + 1);

    std::regex rx("\"x\"\\s*:\\s*(-?\\d+)");
    std::regex ry("\"y\"\\s*:\\s*(-?\\d+)");
    std::smatch mx, my;
    if (std::regex_search(obj, mx, rx) && std::regex_search(obj, my, ry)) {
        x = std::stoi(mx[1]);
        y = std::stoi(my[1]);
        return true;
    }
    return false;
}

// 从 JSON 中提取 "key": intValue
bool parseIntValue(const std::string& json, const std::string& key, int& val) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return false;

    auto colon = json.find(':', pos);
    if (colon == std::string::npos) return false;

    size_t start = colon + 1;
    while (start < json.size() && (json[start] == ' ' || json[start] == '\t')) start++;

    std::string numStr;
    while (start < json.size() && (std::isdigit(json[start]) || json[start] == '-')) {
        numStr += json[start++];
    }
    if (numStr.empty()) return false;
    try { val = std::stoi(numStr); return true; }
    catch (...) { return false; }
}

// 解析分辨率目录名 "19201080" -> (1920, 1080)
bool parseResolutionDir(const std::string& name, int& w, int& h) {
    // 已知分辨率格式: 宽度3-4位 + 高度3-4位
    // 尝试常见分割点
    static const int knownWidths[] = { 1728, 1920, 2304, 2560, 3440, 3840 };
    for (int kw : knownWidths) {
        std::string prefix = std::to_string(kw);
        if (name.substr(0, prefix.size()) == prefix) {
            std::string rest = name.substr(prefix.size());
            try {
                h = std::stoi(rest);
                w = kw;
                return true;
            } catch (...) {}
        }
    }
    // 通用: 尝试前4位为宽度
    if (name.size() >= 7) {
        try {
            w = std::stoi(name.substr(0, 4));
            h = std::stoi(name.substr(4));
            return true;
        } catch (...) {}
    }
    return false;
}

} // anonymous namespace

// ---- TemplateManager 实现 ----

std::string TemplateManager::findBestResolution(const std::string& basePath, int screenW, int screenH) {
    std::string exact = std::to_string(screenW) + std::to_string(screenH);

    if (fs::exists(fs::path(basePath) / exact)) {
        return exact;
    }

    std::string best;
    int64_t bestDiff = INT64_MAX;
    int64_t targetPixels = (int64_t)screenW * screenH;

    try {
        for (auto& entry : fs::directory_iterator(basePath)) {
            if (!entry.is_directory()) continue;
            std::string dirName = entry.path().filename().string();
            int dw, dh;
            if (!parseResolutionDir(dirName, dw, dh)) continue;

            int64_t diff = std::abs((int64_t)dw * dh - targetPixels);
            if (diff < bestDiff) {
                bestDiff = diff;
                best = dirName;
            }
        }
    } catch (...) {}

    if (!best.empty()) {
        std::cout << "[WeaponRecognition] 未找到精确分辨率 " << exact
                  << "，使用最接近的: " << best << std::endl;
    }
    return best;
}

cv::Rect TemplateManager::clampRoi(const HudRegion& region, int frameWidth, int frameHeight) {
    int x = std::max(0, region.x);
    int y = std::max(0, region.y);
    int w = region.width;
    int h = region.height;

    if (x + w > frameWidth) w = frameWidth - x;
    if (y + h > frameHeight) h = frameHeight - y;
    if (w <= 0 || h <= 0) {
        // 返回一个最小有效区域
        x = std::min(x, frameWidth - 1);
        y = std::min(y, frameHeight - 1);
        w = 1;
        h = 1;
    }
    return cv::Rect(x, y, w, h);
}

bool TemplateManager::loadConfigJson(const std::string& jsonPath) {
    std::string json = readFileToString(jsonPath);
    if (json.empty()) {
        std::cerr << "[WeaponRecognition] 无法读取 config.json: " << jsonPath << std::endl;
        return false;
    }

    // 提取 "regions" 对象的内容 (config.json 中 region 嵌套在 regions 对象内)
    std::string regionsJson = json;
    auto regionsPos = json.find("\"regions\"");
    if (regionsPos != std::string::npos) {
        auto brace = json.find('{', regionsPos);
        if (brace != std::string::npos) {
            int depth = 1;
            size_t end = brace + 1;
            while (end < json.size() && depth > 0) {
                if (json[end] == '{') depth++;
                else if (json[end] == '}') depth--;
                end++;
            }
            regionsJson = json.substr(brace, end - brace);
        }
    }

    // 解析 regions
    static const char* regionNames[] = {
        "poses", "weapon_name_rifle", "weapon_name_sniper",
        "muzzles_rifle", "muzzles_sniper", "grips_rifle", "grips_sniper",
        "scopes_rifle", "scopes_sniper", "stocks_rifle", "stocks_sniper",
        "bag", "car", "cursor"
    };

    for (const char* name : regionNames) {
        std::vector<int> arr;
        if (parseIntArray(regionsJson, name, arr) && arr.size() == 4) {
            regions_[name] = { arr[0], arr[1], arr[2], arr[3] };
        }
    }

    // 解析 shoot_pixel
    int sx, sy;
    if (parsePointObject(json, "shoot_pixel", sx, sy)) {
        shootPixel_ = cv::Point(sx, sy);
    }

    // 解析 shoot_threshold
    parseIntValue(json, "shoot_threshold", shootThreshold_);

    return !regions_.empty();
}

bool TemplateManager::loadCategoryTemplates(const std::string& dirPath, const std::string& category) {
    if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) return false;

    auto& vec = templates_[category];
    for (auto& entry : fs::directory_iterator(dirPath)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != ".png" && ext != ".jpg" && ext != ".bmp") continue;

        cv::Mat img = cv::imread(entry.path().string(), cv::IMREAD_GRAYSCALE);
        if (img.empty()) {
            std::cerr << "[WeaponRecognition] 无法加载模板: " << entry.path().string() << std::endl;
            continue;
        }
        std::string name = entry.path().stem().string();
        vec.emplace_back(name, img);
    }
    return !vec.empty();
}

bool TemplateManager::loadTemplates(const std::string& basePath, int screenWidth, int screenHeight) {
    unloadTemplates();

    std::string resDir = findBestResolution(basePath, screenWidth, screenHeight);
    if (resDir.empty()) {
        std::cerr << "[WeaponRecognition] 未找到任何可用分辨率目录: " << basePath << std::endl;
        return false;
    }

    fs::path resPath = fs::path(basePath) / resDir;

    // 加载 config.json
    fs::path configPath = resPath / "config.json";
    if (!loadConfigJson(configPath.string())) {
        std::cerr << "[WeaponRecognition] 加载 config.json 失败: " << configPath.string() << std::endl;
        return false;
    }

    // 加载各类别模板
    fs::path templatesDir = resPath / "weapon_templates";
    static const char* categories[] = {
        "weapons", "scopes", "grips", "muzzles", "stocks",
        "poses", "shoot", "bag", "car"
    };

    int totalLoaded = 0;
    for (const char* cat : categories) {
        fs::path catDir = templatesDir / cat;
        if (loadCategoryTemplates(catDir.string(), cat)) {
            totalLoaded += (int)templates_[cat].size();
        }
    }

    if (totalLoaded == 0) {
        std::cerr << "[WeaponRecognition] 未加载任何模板" << std::endl;
        return false;
    }

    std::cout << "[WeaponRecognition] 已加载 " << totalLoaded << " 个模板 (分辨率: " << resDir << ")" << std::endl;
    loaded_ = true;
    return true;
}

void TemplateManager::unloadTemplates() {
    templates_.clear();
    regions_.clear();
    shootPixel_ = cv::Point(0, 0);
    shootThreshold_ = 760;
    loaded_ = false;
}

MatchResult TemplateManager::matchCategory(const cv::Mat& grayRoi, const std::string& category) const {
    MatchResult best;
    auto it = templates_.find(category);
    if (it == templates_.end() || it->second.empty()) return best;
    if (grayRoi.empty()) return best;

    for (const auto& [name, tmpl] : it->second) {
        // 模板不能大于 ROI
        if (tmpl.cols > grayRoi.cols || tmpl.rows > grayRoi.rows) continue;

        cv::Mat result;
        cv::matchTemplate(grayRoi, tmpl, result, cv::TM_CCOEFF_NORMED);

        double minVal, maxVal;
        cv::minMaxLoc(result, &minVal, &maxVal);

        if ((float)maxVal > best.confidence) {
            best.confidence = (float)maxVal;
            best.name = name;
        }
    }
    return best;
}

HudRegion TemplateManager::getRegion(const std::string& regionName) const {
    auto it = regions_.find(regionName);
    if (it != regions_.end()) return it->second;
    return {};
}

bool TemplateManager::hasRegion(const std::string& regionName) const {
    return regions_.count(regionName) > 0;
}
