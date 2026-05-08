#include "recoil_data.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <cmath>

namespace fs = std::filesystem;

// ===== 简易 JSON 解析工具 =====
namespace {

std::string readFileToString(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

size_t skipWS(const std::string& s, size_t p) {
    while (p < s.size() && (s[p]==' '||s[p]=='\t'||s[p]=='\n'||s[p]=='\r')) p++;
    return p;
}

double parseNumber(const std::string& s, size_t& p) {
    p = skipWS(s, p);
    size_t start = p;
    if (p < s.size() && (s[p]=='-'||s[p]=='+')) p++;
    while (p < s.size() && std::isdigit((unsigned char)s[p])) p++;
    if (p < s.size() && s[p]=='.') { p++; while (p < s.size() && std::isdigit((unsigned char)s[p])) p++; }
    return std::stod(s.substr(start, p - start));
}

std::string parseString(const std::string& s, size_t& p) {
    p = skipWS(s, p);
    if (p >= s.size() || s[p] != '"') return "";
    p++;
    std::string result;
    while (p < s.size() && s[p] != '"') { result += s[p]; p++; }
    if (p < s.size()) p++;
    return result;
}

// 跳过一个 JSON 值（字符串、数字、对象、数组）
void skipValue(const std::string& s, size_t& p) {
    p = skipWS(s, p);
    if (p >= s.size()) return;
    if (s[p] == '"') { parseString(s, p); return; }
    if (s[p] == '{') {
        p++; int depth = 1;
        while (p < s.size() && depth > 0) { if (s[p]=='{') depth++; else if (s[p]=='}') depth--; p++; }
        return;
    }
    if (s[p] == '[') {
        p++; int depth = 1;
        while (p < s.size() && depth > 0) { if (s[p]=='[') depth++; else if (s[p]==']') depth--; p++; }
        return;
    }
    // number / bool / null
    while (p < s.size() && s[p]!=',' && s[p]!='}' && s[p]!=']' && s[p]!='\n' && s[p]!='\r' && s[p]!=' ') p++;
}

// 解析 AttachmentMultiplier：数字 → 固定值，数组 → 分段值
// 值可以是: 1.0 或 [[8,0.9],[27,0.8],[40,0.9]]
AttachmentMultiplier parseAttachmentMul(const std::string& s, size_t& p) {
    p = skipWS(s, p);
    AttachmentMultiplier am;
    if (p < s.size() && s[p] == '[') {
        // 分段数组 [[count, mul], ...]
        am.isSegmented = true;
        p++; // skip outer [
        p = skipWS(s, p);
        while (p < s.size() && s[p] != ']') {
            p = skipWS(s, p);
            if (s[p] == '[') {
                p++; // skip inner [
                int count = (int)parseNumber(s, p);
                p = skipWS(s, p); if (p < s.size() && s[p]==',') p++;
                double mul = parseNumber(s, p);
                am.segments.push_back({count, mul});
                p = skipWS(s, p); if (p < s.size() && s[p]==']') p++;
            }
            p = skipWS(s, p);
            if (p < s.size() && s[p]==',') p++;
        }
        if (p < s.size() && s[p]==']') p++;
        // fixedValue 设为最后一段的值作为回退
        am.fixedValue = am.segments.empty() ? 1.0 : am.segments.back().multiplier;
    } else {
        am.fixedValue = parseNumber(s, p);
    }
    return am;
}

// 解析一个 { "key": value, ... } 的配件类别映射
std::unordered_map<std::string, AttachmentMultiplier> parseAttachmentCategory(const std::string& s, size_t& p) {
    std::unordered_map<std::string, AttachmentMultiplier> result;
    p = skipWS(s, p);
    if (p >= s.size() || s[p] != '{') return result;
    p++; // skip {
    p = skipWS(s, p);
    while (p < s.size() && s[p] != '}') {
        std::string key = parseString(s, p);
        p = skipWS(s, p); if (p < s.size() && s[p]==':') p++;
        result[key] = parseAttachmentMul(s, p);
        p = skipWS(s, p); if (p < s.size() && s[p]==',') p++;
        p = skipWS(s, p);
    }
    if (p < s.size()) p++; // skip }
    return result;
}

// 解析 pattern 数组: [[1,23],[2,13],...]
std::vector<RecoilEntry> parsePattern(const std::string& s, size_t& p) {
    std::vector<RecoilEntry> result;
    p = skipWS(s, p);
    if (p >= s.size() || s[p] != '[') return result;
    p++; // skip [
    p = skipWS(s, p);
    while (p < s.size() && s[p] != ']') {
        p = skipWS(s, p);
        if (s[p] == '[') {
            p++;
            int bullet = (int)parseNumber(s, p);
            p = skipWS(s, p); if (p < s.size() && s[p]==',') p++;
            double value = parseNumber(s, p);
            p = skipWS(s, p); if (p < s.size() && s[p]==']') p++;
            result.push_back({bullet, value});
        }
        p = skipWS(s, p);
        if (p < s.size() && s[p]==',') p++;
    }
    if (p < s.size()) p++;
    return result;
}

} // anonymous namespace


// ===== RecoilDatabase 实现 =====

RecoilDatabase& RecoilDatabase::instance() {
    static RecoilDatabase db;
    return db;
}

RecoilDatabase::RecoilDatabase() {}

// 判断武器是否不使用准星偏移
bool RecoilDatabase::isNoFirstShotOffset(const std::string& weapon) const {
    std::string w = resolveWeaponName(weapon);
    return noFirstShotOffset_.count(w) > 0;
}

// 判断武器是否不需要压枪（霰弹枪、狙击枪）
bool RecoilDatabase::isNoRecoilWeapon(const std::string& weapon) const {
    std::string w = resolveWeaponName(weapon);
    return noRecoilWeapons_.count(w) > 0;
}

// 判断武器是否为单发武器（需要连点）
bool RecoilDatabase::isSingleFireWeapon(const std::string& weapon) const {
    std::string w = resolveWeaponName(weapon);
    return singleFireWeapons_.count(w) > 0;
}
// 判断武器是否为始终连点模式（无论全自动/点射，都强制连点）
bool RecoilDatabase::isAlwaysClickWeapon(const std::string& weapon) const {
    std::string w = resolveWeaponName(weapon);
    return alwaysClickWeapons_.count(w) > 0;
}
bool RecoilDatabase::isOffsetWeapon(const std::string& weapon) const {
    std::string w = resolveWeaponName(weapon);
    return offsetWeapons_.count(w) > 0;
}

// 武器别名解析：如果找不到精确匹配，去掉末尾数字再查找
std::string RecoilDatabase::resolveWeaponName(const std::string& weapon) const {
    if (recoilPatterns_.count(weapon)) return weapon;
    // 去掉末尾数字
    std::string base = weapon;
    while (!base.empty() && base.back() >= '0' && base.back() <= '9')
        base.pop_back();
    if (!base.empty() && base != weapon && recoilPatterns_.count(base))
        return base;
    return weapon;
}

double RecoilDatabase::getBaseCoefficient(const std::string& weapon) const {
    std::string w = resolveWeaponName(weapon);
    auto it = baseCoefficients_.find(w);
    return (it != baseCoefficients_.end()) ? it->second : 1.0;
}

int RecoilDatabase::getWeaponInterval(const std::string& weapon) const {
    std::string w = resolveWeaponName(weapon);
    auto it = weaponIntervals_.find(w);
    return (it != weaponIntervals_.end()) ? it->second : 87;
}

int RecoilDatabase::getClickDelay(const std::string& weapon) const {
    std::string w = resolveWeaponName(weapon);
    auto it = clickDelays_.find(w);
    return (it != clickDelays_.end()) ? it->second : 0;
}

double RecoilDatabase::getScopeMultiplier(const std::string& scope) const {
    auto it = scopeMultipliers_.find(scope);
    return (it != scopeMultipliers_.end()) ? it->second : 1.0;
}

const WeaponAttachmentTable* RecoilDatabase::getAttachmentTable(const std::string& weapon) const {
    std::string w = resolveWeaponName(weapon);
    auto it = attachmentTables_.find(w);
    return (it != attachmentTables_.end()) ? &it->second : nullptr;
}

const WeaponRecoilPattern* RecoilDatabase::getRecoilPattern(const std::string& weapon) const {
    std::string w = resolveWeaponName(weapon);
    auto it = recoilPatterns_.find(w);
    return (it != recoilPatterns_.end()) ? &it->second : nullptr;
}

bool RecoilDatabase::loadFromDirectory(const std::string& dirPath) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
        std::cerr << "[RecoilData] Directory not found: " << dirPath << std::endl;
        return false;
    }

    clearAll();
    loadedDir_ = dirPath;

    // 先加载全局配置
    std::string globalPath = (fs::path(dirPath) / "global.json").string();
    if (fs::exists(globalPath)) {
        loadGlobalConfig(globalPath);
        fileTimestamps_[globalPath] = fs::last_write_time(globalPath);
        std::cout << "[RecoilData] Loaded global config: " << globalPath << std::endl;
    }

    // 遍历目录加载武器文件
    int count = 0;
    for (auto& entry : fs::directory_iterator(dirPath)) {
        if (!entry.is_regular_file()) continue;
        std::string filename = entry.path().filename().string();
        if (filename == "global.json") continue;
        std::string ext = entry.path().extension().string();
        if (ext != ".json") continue;

        std::string weaponName = entry.path().stem().string();
        std::string filePath = entry.path().string();
        if (loadWeaponFile(filePath, weaponName)) {
            fileTimestamps_[filePath] = fs::last_write_time(filePath);
            count++;
        }
    }

    lastCheckTime_ = std::chrono::steady_clock::now();
    std::cout << "[RecoilData] Loaded " << count << " weapon(s)" << std::endl;
    return count > 0;
}

void RecoilDatabase::clearAll() {
    baseCoefficients_.clear();
    weaponIntervals_.clear();
    clickDelays_.clear();
    scopeMultipliers_.clear();
    attachmentTables_.clear();
    recoilPatterns_.clear();
    noFirstShotOffset_.clear();
    noRecoilWeapons_.clear();
    singleFireWeapons_.clear();
    alwaysClickWeapons_.clear();
    offsetWeapons_.clear();
    fileTimestamps_.clear();

    // reset globals to defaults
    globalSensMultiplier_ = 30.0 / 75.2;
    globalVertSensMultiplier_ = 121.0 / 102.5;
    globalBreathMultiplier_ = 1.35;
    globalRecoilMultiplier_ = 18.4 / 100.0;
    firstShotOffset_ = 0.2;
}

bool RecoilDatabase::reload() {
    if (loadedDir_.empty()) return false;
    std::string dir = loadedDir_;
    // loadFromDirectory will lock mutex_ and call clearAll
    return loadFromDirectory(dir);
}

bool RecoilDatabase::checkAndReload() {
    if (loadedDir_.empty()) return false;

    // throttle: check at most every 2 seconds
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - lastCheckTime_).count() < 2) {
        return false;
    }
    lastCheckTime_ = now;

    // check if any file was modified
    bool changed = false;
    try {
        for (auto& [path, oldTime] : fileTimestamps_) {
            if (fs::exists(path)) {
                auto newTime = fs::last_write_time(path);
                if (newTime != oldTime) {
                    changed = true;
                    break;
                }
            }
        }
        // also check for new json files
        if (!changed) {
            for (auto& entry : fs::directory_iterator(loadedDir_)) {
                if (!entry.is_regular_file()) continue;
                if (entry.path().extension().string() != ".json") continue;
                if (fileTimestamps_.find(entry.path().string()) == fileTimestamps_.end()) {
                    changed = true;
                    break;
                }
            }
        }
    } catch (...) {
        return false;
    }

    if (changed) {
        std::cout << "[RecoilData] File change detected, reloading..." << std::endl;
        return reload();
    }
    return false;
}

bool RecoilDatabase::loadGlobalConfig(const std::string& path) {
    std::string json = readFileToString(path);
    if (json.empty()) return false;

    size_t p = 0;
    p = skipWS(json, p);
    if (p >= json.size() || json[p] != '{') return false;
    p++; // skip {

    while (p < json.size() && json[p] != '}') {
        p = skipWS(json, p);
        std::string key = parseString(json, p);
        p = skipWS(json, p); if (p < json.size() && json[p]==':') p++;
        p = skipWS(json, p);

        if (key == "sensitivity_multiplier") {
            if (json[p] == '[') {
                p++;
                double a = parseNumber(json, p);
                p = skipWS(json, p); if (p < json.size() && json[p]==',') p++;
                double b = parseNumber(json, p);
                p = skipWS(json, p); if (p < json.size() && json[p]==']') p++;
                globalSensRatio = {a, b};
                globalSensMultiplier_ = a / b;
            } else {
                double v = parseNumber(json, p);
                globalSensMultiplier_ = v;
                globalSensRatio = {v, 1.0};
            }
        } else if (key == "vertical_sensitivity_multiplier") {
            if (json[p] == '[') {
                p++;
                double a = parseNumber(json, p);
                p = skipWS(json, p); if (p < json.size() && json[p]==',') p++;
                double b = parseNumber(json, p);
                p = skipWS(json, p); if (p < json.size() && json[p]==']') p++;
                globalVertSensRatio = {a, b};
                globalVertSensMultiplier_ = a / b;
            } else {
                double v = parseNumber(json, p);
                globalVertSensMultiplier_ = v;
                globalVertSensRatio = {v, 1.0};
            }
        } else if (key == "breath_multiplier") {
            globalBreathMultiplier_ = parseNumber(json, p);
        } else if (key == "recoil_multiplier") {
            if (json[p] == '[') {
                p++;
                double a = parseNumber(json, p);
                p = skipWS(json, p); if (p < json.size() && json[p]==',') p++;
                double b = parseNumber(json, p);
                p = skipWS(json, p); if (p < json.size() && json[p]==']') p++;
                globalRecoilRatio = {a, b};
                globalRecoilMultiplier_ = a / b;
            } else {
                double v = parseNumber(json, p);
                globalRecoilMultiplier_ = v;
                globalRecoilRatio = {v, 1.0};
            }
        } else if (key == "first_shot_offset") {
            firstShotOffset_ = parseNumber(json, p);
        } else if (key == "scope_multipliers") {
            p = skipWS(json, p);
            if (p < json.size() && json[p] == '{') {
                p++;
                p = skipWS(json, p);
                while (p < json.size() && json[p] != '}') {
                    std::string scopeName = parseString(json, p);
                    p = skipWS(json, p); if (p < json.size() && json[p]==':') p++;
                    p = skipWS(json, p);
                    if (json[p] == '[') {
                        p++;
                        double a = parseNumber(json, p);
                        p = skipWS(json, p); if (p < json.size() && json[p]==',') p++;
                        double b = parseNumber(json, p);
                        p = skipWS(json, p); if (p < json.size() && json[p]==']') p++;
                        scopeMultipliers_[scopeName] = a / b;
                        scopeRatioValues[scopeName] = {true, 0, a, b};
                    } else {
                        double v = parseNumber(json, p);
                        scopeMultipliers_[scopeName] = v;
                        scopeRatioValues[scopeName] = {false, v, 0, 0};
                    }
                    p = skipWS(json, p); if (p < json.size() && json[p]==',') p++;
                    p = skipWS(json, p);
                }
                if (p < json.size()) p++; // skip }
            }
        } else if (key == "no_first_shot_offset") {
            // 解析字符串数组 ["MP5","UZI",...]
            p = skipWS(json, p);
            if (p < json.size() && json[p] == '[') {
                p++;
                p = skipWS(json, p);
                while (p < json.size() && json[p] != ']') {
                    std::string name = parseString(json, p);
                    if (!name.empty()) noFirstShotOffset_.insert(name);
                    p = skipWS(json, p); if (p < json.size() && json[p]==',') p++;
                    p = skipWS(json, p);
                }
                if (p < json.size()) p++;
            }
        } else if (key == "no_recoil_weapons") {
            p = skipWS(json, p);
            if (p < json.size() && json[p] == '[') {
                p++;
                p = skipWS(json, p);
                while (p < json.size() && json[p] != ']') {
                    std::string name = parseString(json, p);
                    if (!name.empty()) noRecoilWeapons_.insert(name);
                    p = skipWS(json, p); if (p < json.size() && json[p]==',') p++;
                    p = skipWS(json, p);
                }
                if (p < json.size()) p++;
            }
        } else if (key == "single_fire_weapons") {
            p = skipWS(json, p);
            if (p < json.size() && json[p] == '[') {
                p++;
                p = skipWS(json, p);
                while (p < json.size() && json[p] != ']') {
                    std::string name = parseString(json, p);
                    if (!name.empty()) singleFireWeapons_.insert(name);
                    p = skipWS(json, p); if (p < json.size() && json[p]==',') p++;
                    p = skipWS(json, p);
                }
                if (p < json.size()) p++;
            }
        } else if (key == "offset_weapons") {
            p = skipWS(json, p);
            if (p < json.size() && json[p] == '[') {
                p++;
                p = skipWS(json, p);
                while (p < json.size() && json[p] != ']') {
                    std::string name = parseString(json, p);
                    if (!name.empty()) offsetWeapons_.insert(name);
                    p = skipWS(json, p); if (p < json.size() && json[p]==',') p++;
                    p = skipWS(json, p);
                }
                if (p < json.size()) p++;
            }
        } else if (key == "always_click_weapons") {
            p = skipWS(json, p);
            if (p < json.size() && json[p] == '[') {
                p++;
                p = skipWS(json, p);
                while (p < json.size() && json[p] != ']') {
                    std::string name = parseString(json, p);
                    if (!name.empty()) alwaysClickWeapons_.insert(name);
                    p = skipWS(json, p); if (p < json.size() && json[p]==',') p++;
                    p = skipWS(json, p);
                }
                if (p < json.size()) p++;
            }
        } else {
            skipValue(json, p);
        }

        p = skipWS(json, p); if (p < json.size() && json[p]==',') p++;
        p = skipWS(json, p);
    }
    return true;
}

bool RecoilDatabase::loadWeaponFile(const std::string& path, const std::string& weaponName) {
    std::string json = readFileToString(path);
    if (json.empty()) return false;

    size_t p = 0;
    p = skipWS(json, p);
    if (p >= json.size() || json[p] != '{') return false;
    p++;

    double baseCoef = 1.0;
    int intervalMs = 87;
    int clickDelayMs = 0;
    WeaponAttachmentTable table;
    WeaponRecoilPattern pattern;

    while (p < json.size() && json[p] != '}') {
        p = skipWS(json, p);
        std::string key = parseString(json, p);
        p = skipWS(json, p); if (p < json.size() && json[p]==':') p++;
        p = skipWS(json, p);

        if (key == "base_coefficient") {
            baseCoef = parseNumber(json, p);
        } else if (key == "interval_ms") {
            intervalMs = (int)parseNumber(json, p);
        } else if (key == "click_delay_ms") {
            clickDelayMs = (int)parseNumber(json, p);
        } else if (key == "attachments") {
            // { "poses": {...}, "muzzles": {...}, ... }
            if (json[p] == '{') {
                p++;
                p = skipWS(json, p);
                while (p < json.size() && json[p] != '}') {
                    std::string catName = parseString(json, p);
                    p = skipWS(json, p); if (p < json.size() && json[p]==':') p++;
                    p = skipWS(json, p);

                    if (catName == "poses") table.poses = parseAttachmentCategory(json, p);
                    else if (catName == "muzzles") table.muzzles = parseAttachmentCategory(json, p);
                    else if (catName == "grips") table.grips = parseAttachmentCategory(json, p);
                    else if (catName == "scopes") table.scopes = parseAttachmentCategory(json, p);
                    else if (catName == "stocks") table.stocks = parseAttachmentCategory(json, p);
                    else if (catName == "car") table.car = parseAttachmentCategory(json, p);
                    else skipValue(json, p);

                    p = skipWS(json, p); if (p < json.size() && json[p]==',') p++;
                    p = skipWS(json, p);
                }
                if (p < json.size()) p++; // skip }
            }
        } else if (key == "pattern") {
            pattern.defaultPattern = parsePattern(json, p);
        } else {
            skipValue(json, p);
        }

        p = skipWS(json, p); if (p < json.size() && json[p]==',') p++;
        p = skipWS(json, p);
    }

    baseCoefficients_[weaponName] = baseCoef;
    weaponIntervals_[weaponName] = intervalMs;
    if (clickDelayMs > 0) clickDelays_[weaponName] = clickDelayMs;
    attachmentTables_[weaponName] = std::move(table);
    recoilPatterns_[weaponName] = std::move(pattern);
    return true;
}

// ===== 新增方法 =====

std::vector<std::string> RecoilDatabase::getWeaponNames() const {
    std::vector<std::string> names;
    names.reserve(recoilPatterns_.size());
    for (auto& [k, v] : recoilPatterns_) names.push_back(k);
    std::sort(names.begin(), names.end());
    return names;
}

WeaponAttachmentTable* RecoilDatabase::getAttachmentTableMut(const std::string& weapon) {
    auto it = attachmentTables_.find(weapon);
    return (it != attachmentTables_.end()) ? &it->second : nullptr;
}

WeaponRecoilPattern* RecoilDatabase::getRecoilPatternMut(const std::string& weapon) {
    auto it = recoilPatterns_.find(weapon);
    return (it != recoilPatterns_.end()) ? &it->second : nullptr;
}

// ===== JSON 序列化辅助 =====
namespace {

std::string fmtNum(double v) {
    char buf[64];
    if (v == (int)v) snprintf(buf, sizeof(buf), "%d", (int)v);
    else snprintf(buf, sizeof(buf), "%.4g", v);
    return buf;
}

std::string serializeAttachmentMul(const AttachmentMultiplier& am) {
    if (am.isSegmented && !am.segments.empty()) {
        std::string s = "[";
        for (size_t i = 0; i < am.segments.size(); i++) {
            if (i > 0) s += ",";
            s += "[" + fmtNum((double)am.segments[i].count) + "," + fmtNum(am.segments[i].multiplier) + "]";
        }
        s += "]";
        return s;
    }
    return fmtNum(am.fixedValue);
}

std::string serializeAttachmentCategory(const std::unordered_map<std::string, AttachmentMultiplier>& cat) {
    std::string s = "{";
    bool first = true;
    for (auto& [k, v] : cat) {
        if (!first) s += ",";
        s += "\"" + k + "\":" + serializeAttachmentMul(v);
        first = false;
    }
    s += "}";
    return s;
}

} // anonymous namespace

bool RecoilDatabase::saveGlobalConfig() const {
    if (loadedDir_.empty()) return false;
    std::string path = (fs::path(loadedDir_) / "global.json").string();
    std::ofstream f(path);
    if (!f.is_open()) return false;

    f << "{\n";
    f << "    \"sensitivity_multiplier\": " << fmtNum(globalSensMultiplier_) << ",\n";
    f << "    \"vertical_sensitivity_multiplier\": " << fmtNum(globalVertSensMultiplier_) << ",\n";
    f << "    \"breath_multiplier\": " << fmtNum(globalBreathMultiplier_) << ",\n";
    f << "    \"recoil_multiplier\": " << fmtNum(globalRecoilMultiplier_) << ",\n";
    f << "    \"first_shot_offset\": " << fmtNum(firstShotOffset_) << ",\n";
    f << "    \"scope_multipliers\": {";
    bool first = true;
    for (auto& [name, entry] : scopeRatioValues) {
        if (!first) f << ",";
        f << "\n        \"" << name << "\": ";
        auto smIt = scopeMultipliers_.find(name);
        double smVal = (smIt != scopeMultipliers_.end()) ? smIt->second : (entry.isRatio ? (entry.b != 0 ? entry.a / entry.b : 1.0) : entry.fixed);
        f << fmtNum(smVal);
        first = false;
    }
    f << "\n    }\n";

    // no_first_shot_offset
    if (!noFirstShotOffset_.empty()) {
        f << ",\n    \"no_first_shot_offset\": [";
        bool firstNfo = true;
        for (auto& name : noFirstShotOffset_) {
            if (!firstNfo) f << ",";
            f << "\"" << name << "\"";
            firstNfo = false;
        }
        f << "]";
    }

    // no_recoil_weapons
    if (!noRecoilWeapons_.empty()) {
        f << ",\n    \"no_recoil_weapons\": [";
        bool firstNr = true;
        for (auto& name : noRecoilWeapons_) {
            if (!firstNr) f << ",";
            f << "\"" << name << "\"";
            firstNr = false;
        }
        f << "]";
    }

    // single_fire_weapons
    if (!singleFireWeapons_.empty()) {
        f << ",\n    \"single_fire_weapons\": [";
        bool firstSf = true;
        for (auto& name : singleFireWeapons_) {
            if (!firstSf) f << ",";
            f << "\"" << name << "\"";
            firstSf = false;
        }
        f << "]";
    }

    // offset_weapons
    if (!offsetWeapons_.empty()) {
        f << ",\n    \"offset_weapons\": [";
        bool firstOw = true;
        for (auto& name : offsetWeapons_) {
            if (!firstOw) f << ",";
            f << "\"" << name << "\"";
            firstOw = false;
        }
        f << "]";
    }

    f << "\n}\n";
    f.close();
    return true;
}

bool RecoilDatabase::saveWeaponFile(const std::string& weaponName) const {
    if (loadedDir_.empty()) return false;
    std::string path = (fs::path(loadedDir_) / (weaponName + ".json")).string();

    auto bcIt = baseCoefficients_.find(weaponName);
    auto intIt = weaponIntervals_.find(weaponName);
    auto attIt = attachmentTables_.find(weaponName);
    auto patIt = recoilPatterns_.find(weaponName);
    if (patIt == recoilPatterns_.end()) return false;

    std::ofstream f(path);
    if (!f.is_open()) return false;

    double baseCoef = (bcIt != baseCoefficients_.end()) ? bcIt->second : 1.0;
    int interval = (intIt != weaponIntervals_.end()) ? intIt->second : 87;

    f << "{\n";
    f << "    \"base_coefficient\": " << fmtNum(baseCoef) << ",\n";
    f << "    \"interval_ms\": " << interval << ",\n";

    if (attIt != attachmentTables_.end()) {
        auto& t = attIt->second;
        f << "    \"attachments\": {\n";
        f << "        \"poses\": " << serializeAttachmentCategory(t.poses) << ",\n";
        f << "        \"muzzles\": " << serializeAttachmentCategory(t.muzzles) << ",\n";
        f << "        \"grips\": " << serializeAttachmentCategory(t.grips) << ",\n";
        f << "        \"scopes\": " << serializeAttachmentCategory(t.scopes) << ",\n";
        f << "        \"stocks\": " << serializeAttachmentCategory(t.stocks) << ",\n";
        f << "        \"car\": " << serializeAttachmentCategory(t.car) << "\n";
        f << "    },\n";
    }

    f << "    \"pattern\": [";
    auto& pat = patIt->second.defaultPattern;
    for (size_t i = 0; i < pat.size(); i++) {
        if (i > 0) f << ",";
        f << "[" << pat[i].bullet << "," << fmtNum(pat[i].value) << "]";
    }
    f << "]\n";
    f << "}\n";
    f.close();
    return true;
}

bool RecoilDatabase::saveAll() const {
    if (loadedDir_.empty()) return false;
    bool ok = saveGlobalConfig();
    for (auto& [name, _] : recoilPatterns_) {
        if (!saveWeaponFile(name)) ok = false;
    }
    return ok;
}
