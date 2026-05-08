#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <set>
#include <filesystem>
#include <chrono>

// 分段乘数：前 count 发使用 multiplier
struct RecoilSegment {
    int count;
    double multiplier;
};

// 配件乘数：可以是固定值或分段值
struct AttachmentMultiplier {
    bool isSegmented = false;
    double fixedValue = 1.0;
    std::vector<RecoilSegment> segments;

    double getMultiplier(int bulletIndex) const {
        if (!isSegmented) return fixedValue;
        for (auto& seg : segments) {
            if (bulletIndex <= seg.count) return seg.multiplier;
        }
        return segments.empty() ? 1.0 : segments.back().multiplier;
    }
};

// 单把武器的配件乘数表
struct WeaponAttachmentTable {
    std::unordered_map<std::string, AttachmentMultiplier> poses;
    std::unordered_map<std::string, AttachmentMultiplier> muzzles;
    std::unordered_map<std::string, AttachmentMultiplier> grips;
    std::unordered_map<std::string, AttachmentMultiplier> scopes;
    std::unordered_map<std::string, AttachmentMultiplier> stocks;
    std::unordered_map<std::string, AttachmentMultiplier> car;
};

// 弹道模式：每发子弹 {bulletIndex, recoilValue}
struct RecoilEntry {
    int bullet;
    double value;
};

struct WeaponRecoilPattern {
    std::vector<RecoilEntry> defaultPattern;
    // std::vector<RecoilEntry> burstPattern; // DMR连射模式（暂时禁用）
};

// 全局弹道数据库
class RecoilDatabase {
public:
    static RecoilDatabase& instance();

    // 从外部 JSON 文件夹加载弹道数据
    bool loadFromDirectory(const std::string& dirPath);

    // 热重载：重新加载所有弹道数据（线程安全）
    bool reload();

    // 检查文件是否有变化，有则自动重载（在主循环中定期调用）
    bool checkAndReload();

    // 获取上次加载的目录路径
    std::string getLoadedDirectory() const { return loadedDir_; }

    // 获取读写锁（外部需要在读取数据时加锁）
    std::mutex& getMutex() { return mutex_; }

    double getGlobalSensMultiplier() const { return globalSensMultiplier_; }
    double getGlobalVertSensMultiplier() const { return globalVertSensMultiplier_; }
    double getGlobalBreathMultiplier() const { return globalBreathMultiplier_; }
    double getGlobalRecoilMultiplier() const { return globalRecoilMultiplier_; }
    double getFirstShotOffset() const { return firstShotOffset_; }
    bool isNoFirstShotOffset(const std::string& weapon) const;
    bool isNoRecoilWeapon(const std::string& weapon) const;
    bool isSingleFireWeapon(const std::string& weapon) const;
    bool isAlwaysClickWeapon(const std::string& weapon) const;
    bool isOffsetWeapon(const std::string& weapon) const;
    double getBaseCoefficient(const std::string& weapon) const;
    int getWeaponInterval(const std::string& weapon) const;
    int getClickDelay(const std::string& weapon) const;
    double getScopeMultiplier(const std::string& scope) const;

    const WeaponAttachmentTable* getAttachmentTable(const std::string& weapon) const;
    const WeaponRecoilPattern* getRecoilPattern(const std::string& weapon) const;

    // 武器别名解析（去掉末尾数字匹配，如 TOM1 -> TOM）
    std::string resolveWeaponName(const std::string& weapon) const;

    int getLoadedWeaponCount() const { return static_cast<int>(recoilPatterns_.size()); }

    // 获取所有武器名列表
    std::vector<std::string> getWeaponNames() const;

    // 可写访问（编辑器用）
    void setGlobalSensMultiplier(double v) { globalSensMultiplier_ = v; }
    void setGlobalVertSensMultiplier(double v) { globalVertSensMultiplier_ = v; }
    void setGlobalBreathMultiplier(double v) { globalBreathMultiplier_ = v; }
    void setGlobalRecoilMultiplier(double v) { globalRecoilMultiplier_ = v; }
    void setFirstShotOffset(double v) { firstShotOffset_ = v; }

    void setBaseCoefficient(const std::string& weapon, double v) { baseCoefficients_[weapon] = v; }
    void setWeaponInterval(const std::string& weapon, int v) { weaponIntervals_[weapon] = v; }

    WeaponAttachmentTable* getAttachmentTableMut(const std::string& weapon);
    WeaponRecoilPattern* getRecoilPatternMut(const std::string& weapon);
    std::unordered_map<std::string, double>& getScopeMultipliersMut() { return scopeMultipliers_; }

    // 保存到 JSON 文件
    bool saveGlobalConfig() const;
    bool saveWeaponFile(const std::string& weaponName) const;
    bool saveAll() const;

    // 原始比率值（用于编辑器显示/保存）
    struct RatioValue { double a; double b; };
    RatioValue globalSensRatio = {30.0, 75.2};
    RatioValue globalVertSensRatio = {121.0, 102.5};
    RatioValue globalRecoilRatio = {18.4, 100.0};
    struct ScopeRatioEntry { bool isRatio; double fixed; double a; double b; };
    std::unordered_map<std::string, ScopeRatioEntry> scopeRatioValues;

private:
    RecoilDatabase();

    bool loadGlobalConfig(const std::string& path);
    bool loadWeaponFile(const std::string& path, const std::string& weaponName);
    void clearAll();

    std::string loadedDir_;
    std::mutex mutex_;
    std::unordered_map<std::string, std::filesystem::file_time_type> fileTimestamps_;
    std::chrono::steady_clock::time_point lastCheckTime_;

    double globalSensMultiplier_ = 30.0 / 75.2;
    double globalVertSensMultiplier_ = 121.0 / 102.5;
    double globalBreathMultiplier_ = 1.35;
    double globalRecoilMultiplier_ = 18.4 / 100.0;
    double firstShotOffset_ = 0.2;

    std::unordered_map<std::string, double> baseCoefficients_;
    std::unordered_map<std::string, int> weaponIntervals_;
    std::unordered_map<std::string, int> clickDelays_;
    std::unordered_map<std::string, double> scopeMultipliers_;
    std::unordered_map<std::string, WeaponAttachmentTable> attachmentTables_;
    std::unordered_map<std::string, WeaponRecoilPattern> recoilPatterns_;
    std::set<std::string> noFirstShotOffset_;
    std::set<std::string> noRecoilWeapons_;
    std::set<std::string> singleFireWeapons_;
    std::set<std::string> alwaysClickWeapons_;
    std::set<std::string> offsetWeapons_;
};
