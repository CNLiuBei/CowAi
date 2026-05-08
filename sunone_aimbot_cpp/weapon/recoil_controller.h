#pragma once
#include <string>
#include <atomic>
#include <chrono>
#include "weapon_state.h"
#include "recoil_data.h"

// 压枪控制器：根据武器识别结果 + 弹道数据计算每发子弹的压枪量
// 使用平滑分配：将每发子弹的压枪量均匀分配到子弹间隔内，避免卡顿
class RecoilController {
public:
    RecoilController();

    // 每次轮询调用（~5ms）：返回本次应施加的平滑压枪偏移量 {dx, dy}
    std::pair<int, int> computeRecoil(const WeaponState& state, int weaponSlot, bool isFiring);

    void reset();
    bool hasActiveWeapon() const;

    std::string getCurrentWeapon() const { return currentWeapon_; }
    int getBulletCount() const { return bulletCount_; }

private:
    double calculateMultiplier(const std::string& weapon, const std::string& muzzle,
                               const std::string& grip, const std::string& scope,
                               const std::string& stock, const std::string& pose,
                               bool inCar, int bulletIndex) const;

    double getAttachmentMul(const WeaponAttachmentTable* table,
                            const std::unordered_map<std::string, AttachmentMultiplier>& category,
                            const std::string& key, int bulletIndex) const;

    std::string currentWeapon_;
    int bulletCount_ = 0;
    double decimalCache_ = 0.0;
    std::chrono::steady_clock::time_point lastShotTime_;
    bool wasFiring_ = false;

    // 平滑分配状态：将每发子弹的recoil按时间均匀释放
    double ratePerMs_ = 0.0;           // 当前像素/毫秒 释放速率
    int smoothIntervalMs_ = 87;        // 当前武器子弹间隔(ms)
    std::chrono::steady_clock::time_point lastPollTime_;  // 上次轮询时间
};
