#pragma once
#include <string>
#include <mutex>

struct WeaponInfo {
    std::string weaponName = "None";
    std::string scope = "None";
    std::string grip = "None";
    std::string muzzle = "None";
    std::string stock = "None";
    float weaponConfidence = 0.0f;
    float scopeConfidence = 0.0f;
    float gripConfidence = 0.0f;
    float muzzleConfidence = 0.0f;
    float stockConfidence = 0.0f;
};

struct WeaponState {
    mutable std::mutex mutex;

    WeaponInfo primary;
    WeaponInfo secondary;
    std::string pose = "None";
    bool isShooting = false;
    bool hasBag = false;
    bool inCar = false;
    float poseConfidence = 0.0f;
    float shootConfidence = 0.0f;
    float bagConfidence = 0.0f;
    float carConfidence = 0.0f;

    WeaponInfo getPrimary() const {
        std::lock_guard<std::mutex> lock(mutex);
        return primary;
    }
    WeaponInfo getSecondary() const {
        std::lock_guard<std::mutex> lock(mutex);
        return secondary;
    }
    std::string getPose() const {
        std::lock_guard<std::mutex> lock(mutex);
        return pose;
    }
    bool getIsShooting() const {
        std::lock_guard<std::mutex> lock(mutex);
        return isShooting;
    }
    bool getHasBag() const {
        std::lock_guard<std::mutex> lock(mutex);
        return hasBag;
    }
    bool getInCar() const {
        std::lock_guard<std::mutex> lock(mutex);
        return inCar;
    }
};
