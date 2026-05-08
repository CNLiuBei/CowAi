#pragma once

#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <winsock2.h>
#include <Windows.h>

#include <atomic>
#include <thread>
#include <string>
#include <memory>
#include <opencv2/opencv.hpp>
#include "weapon_state.h"
#include "template_manager.h"

class WeaponRecognizer {
public:
    WeaponRecognizer();
    ~WeaponRecognizer();

    void start();
    void stop();
    bool isRunning() const { return running_.load(); }

    void configure(const std::string& templatePath, float threshold, int scanIntervalMs);

    WeaponState& getState() { return state_; }
    const WeaponState& getState() const { return state_; }

private:
    void recognitionLoop();
    void recognizeWeaponSlot(const cv::Mat& grayFrame, const std::string& slotPrefix, WeaponInfo& outInfo);
    void recognizeSingleCategory(const cv::Mat& grayFrame, const std::string& regionName,
                                  const std::string& category, std::string& outName, float& outConf);

    std::atomic<bool> running_{false};
    std::thread thread_;

    TemplateManager templateMgr_;
    WeaponState state_;

    std::string templatePath_ = "peijian";
    float threshold_ = 0.75f;
    int scanIntervalMs_ = 100;
    int statusIntervalMs_ = 500;  // 姿态/背包/载具/射击的识别间隔

    int screenW_ = 0;
    int screenH_ = 0;
    cv::Mat captureScreenGDI();  // GDI 截屏（不与 DDA 冲突）
};
