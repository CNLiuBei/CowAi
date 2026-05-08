#include "recoil_controller.h"
#include <cmath>
#include <algorithm>

RecoilController::RecoilController()
    : lastShotTime_(std::chrono::steady_clock::now()),
      lastPollTime_(std::chrono::steady_clock::now()) {}

void RecoilController::reset() {
    bulletCount_ = 0;
    decimalCache_ = 0.0;
    wasFiring_ = false;
    ratePerMs_ = 0.0;
}

bool RecoilController::hasActiveWeapon() const {
    if (currentWeapon_.empty() || currentWeapon_ == "None") return false;
    return RecoilDatabase::instance().getRecoilPattern(currentWeapon_) != nullptr;
}

double RecoilController::getAttachmentMul(
    const WeaponAttachmentTable* table,
    const std::unordered_map<std::string, AttachmentMultiplier>& category,
    const std::string& key, int bulletIndex) const
{
    auto it = category.find(key);
    if (it != category.end()) return it->second.getMultiplier(bulletIndex);
    auto none = category.find("None");
    if (none != category.end()) return none->second.getMultiplier(bulletIndex);
    return 1.0;
}

double RecoilController::calculateMultiplier(
    const std::string& weapon, const std::string& muzzle,
    const std::string& grip, const std::string& scope,
    const std::string& stock, const std::string& pose,
    bool inCar, int bulletIndex) const
{
    auto& db = RecoilDatabase::instance();

    const WeaponAttachmentTable* table = db.getAttachmentTable(weapon);
    if (!table) table = db.getAttachmentTable("None");
    if (!table) return 1.0;

    double mul = 1.0;
    mul *= db.getBaseCoefficient(weapon);
    mul *= db.getGlobalSensMultiplier();
    mul *= db.getGlobalVertSensMultiplier();
    mul *= db.getGlobalRecoilMultiplier();

    mul *= getAttachmentMul(table, table->poses, pose, bulletIndex);
    mul *= getAttachmentMul(table, table->muzzles, muzzle, bulletIndex);
    mul *= getAttachmentMul(table, table->grips, grip, bulletIndex);
    mul *= db.getScopeMultiplier(scope);
    mul *= getAttachmentMul(table, table->scopes, scope, bulletIndex);
    mul *= getAttachmentMul(table, table->stocks, stock, bulletIndex);
    if (inCar) {
        mul *= getAttachmentMul(table, table->car, "car", bulletIndex);
    }

    return mul;
}

std::pair<int, int> RecoilController::computeRecoil(
    const WeaponState& state, int weaponSlot, bool isFiring)
{
    if (!isFiring) {
        if (wasFiring_) reset();
        return {0, 0};
    }

    WeaponInfo info;
    if (weaponSlot == 2) {
        info = state.getSecondary();
    } else {
        info = state.getPrimary();
    }

    std::string weapon = info.weaponName;
    if (weapon.empty() || weapon == "None") {
        if (wasFiring_) reset();
        return {0, 0};
    }

    auto& db = RecoilDatabase::instance();
    const WeaponRecoilPattern* pattern = db.getRecoilPattern(weapon);
    if (!pattern) {
        return {0, 0};
    }

    if (db.isNoRecoilWeapon(weapon)) {
        return {0, 0};
    }

    auto now = std::chrono::steady_clock::now();
    int intervalMs = db.getWeaponInterval(weapon);

    if (!wasFiring_) {
        wasFiring_ = true;
        bulletCount_ = 0;
        decimalCache_ = 0.0;
        ratePerMs_ = 0.0;
        lastShotTime_ = now;
        lastPollTime_ = now;
        currentWeapon_ = weapon;
        smoothIntervalMs_ = intervalMs;
    }

    // 根据时间推算当前子弹序号
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastShotTime_).count();
    int newBullet = static_cast<int>(std::ceil(static_cast<double>(elapsed) / intervalMs));
    if (newBullet < 1) newBullet = 1;

    // 新子弹到来：计算新的释放速率
    if (newBullet > bulletCount_) {
        int prevBullet = bulletCount_;
        bulletCount_ = newBullet;
        currentWeapon_ = weapon;
        smoothIntervalMs_ = intervalMs;

        const auto& entries = pattern->defaultPattern;
        std::string pose = state.getPose();
        bool inCar = state.getInCar();

        // 累积从 prevBullet+1 到 newBullet 的所有子弹 recoil
        // 如果跳过了多发，把中间子弹的量也加上
        double totalNewRecoil = 0.0;
        int bulletsAdded = 0;
        for (int b = prevBullet + 1; b <= newBullet; b++) {
            double recoilValue = 0.0;
            if (!entries.empty()) {
                for (auto& e : entries) {
                    if (e.bullet == b) {
                        recoilValue = e.value;
                        break;
                    }
                }
                if (recoilValue == 0.0 && b > entries.back().bullet) {
                    recoilValue = entries.back().value;
                }
            }

            if (recoilValue != 0.0) {
                double multiplier = calculateMultiplier(
                    weapon, info.muzzle, info.grip, info.scope, info.stock, pose, inCar, b);
                double rawRecoil = recoilValue * multiplier;

                if (b == 1 && !db.isNoFirstShotOffset(weapon)) {
                    rawRecoil += db.getFirstShotOffset();
                }

                totalNewRecoil += rawRecoil;
            }
            bulletsAdded++;
        }

        // 计算新的释放速率：总像素量 / 总时间跨度(ms)
        // 跳过多发时，时间跨度 = bulletsAdded * intervalMs
        double spanMs = static_cast<double>(bulletsAdded) * intervalMs;
        if (spanMs > 0.0) {
            ratePerMs_ = totalNewRecoil / spanMs;
        }
    }

    // 按实际经过时间均匀释放
    auto dtMs = std::chrono::duration_cast<std::chrono::microseconds>(now - lastPollTime_).count() / 1000.0;
    lastPollTime_ = now;

    if (dtMs <= 0.0 || ratePerMs_ <= 0.0) {
        return {0, 0};
    }

    // 累积本次轮询应释放的浮点像素量
    decimalCache_ += ratePerMs_ * dtMs;

    // 提取整数像素输出，余数保留
    if (decimalCache_ < 1.0) {
        return {0, 0};
    }

    int output = static_cast<int>(decimalCache_);
    decimalCache_ -= output;

    return {0, output};
}
