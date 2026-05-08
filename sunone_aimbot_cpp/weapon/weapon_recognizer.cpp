#include "weapon_recognizer.h"
#include "sunone_aimbot_cpp.h"
#include <iostream>
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

WeaponRecognizer::WeaponRecognizer() {}

WeaponRecognizer::~WeaponRecognizer() {
    stop();
}

void WeaponRecognizer::configure(const std::string& templatePath, float threshold, int scanIntervalMs) {
    templatePath_ = templatePath;
    threshold_ = threshold;
    scanIntervalMs_ = scanIntervalMs;
}

void WeaponRecognizer::start() {
    if (running_.load()) return;

    // 获取屏幕分辨率
    screenW_ = GetSystemMetrics(SM_CXSCREEN);
    screenH_ = GetSystemMetrics(SM_CYSCREEN);

    // 解析模板路径: 如果相对路径不存在，尝试 exe 所在目录
    std::string resolvedPath = templatePath_;
    if (!fs::exists(resolvedPath)) {
        char exePath[MAX_PATH] = {};
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        fs::path exeDir = fs::path(exePath).parent_path();
        fs::path candidate = exeDir / templatePath_;
        if (fs::exists(candidate)) {
            resolvedPath = candidate.string();
            std::cout << "[WeaponRecognition] Using exe-relative template path: " << resolvedPath << std::endl;
        } else {
            std::cerr << "[WeaponRecognition] Template path not found: " << templatePath_
                      << " (also tried: " << candidate.string() << ")" << std::endl;
            return;
        }
    }

    // 加载模板
    if (!templateMgr_.loadTemplates(resolvedPath, screenW_, screenH_)) {
        std::cerr << "[WeaponRecognition] Failed to load templates" << std::endl;
        return;
    }

    running_ = true;
    thread_ = std::thread(&WeaponRecognizer::recognitionLoop, this);
    std::cout << "[WeaponRecognition] Engine started (GDI capture, interval: " << scanIntervalMs_ << "ms)" << std::endl;
}

void WeaponRecognizer::stop() {
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
    templateMgr_.unloadTemplates();
    std::cout << "[WeaponRecognition] Engine stopped" << std::endl;
}

void WeaponRecognizer::recognizeSingleCategory(const cv::Mat& grayFrame,
    const std::string& regionName, const std::string& category,
    std::string& outName, float& outConf)
{
    if (!templateMgr_.hasRegion(regionName)) return;

    HudRegion region = templateMgr_.getRegion(regionName);
    cv::Rect roi = TemplateManager::clampRoi(region, grayFrame.cols, grayFrame.rows);
    cv::Mat roiMat = grayFrame(roi);

    MatchResult result = templateMgr_.matchCategory(roiMat, category);
    if (result.confidence >= threshold_) {
        outName = result.name;
        outConf = result.confidence;
    } else {
        outName = "None";
        outConf = 0.0f;
    }
}

void WeaponRecognizer::recognizeWeaponSlot(const cv::Mat& grayFrame,
    const std::string& slotPrefix, WeaponInfo& outInfo)
{
    // 武器名称
    recognizeSingleCategory(grayFrame, "weapon_name_" + slotPrefix, "weapons",
                            outInfo.weaponName, outInfo.weaponConfidence);
    // 瞄准镜
    recognizeSingleCategory(grayFrame, "scopes_" + slotPrefix, "scopes",
                            outInfo.scope, outInfo.scopeConfidence);
    // 握把
    recognizeSingleCategory(grayFrame, "grips_" + slotPrefix, "grips",
                            outInfo.grip, outInfo.gripConfidence);
    // 枪口
    recognizeSingleCategory(grayFrame, "muzzles_" + slotPrefix, "muzzles",
                            outInfo.muzzle, outInfo.muzzleConfidence);
    // 枪托
    recognizeSingleCategory(grayFrame, "stocks_" + slotPrefix, "stocks",
                            outInfo.stock, outInfo.stockConfidence);
}

void WeaponRecognizer::recognitionLoop() {
    auto lastStatusTime = std::chrono::steady_clock::now() - std::chrono::milliseconds(statusIntervalMs_);

    while (running_.load()) {
        auto startTime = std::chrono::steady_clock::now();

        // GDI 截取全屏
        cv::Mat frame = captureScreenGDI();

        if (frame.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(scanIntervalMs_));
            continue;
        }

        // 转灰度
        cv::Mat grayFrame;
        if (frame.channels() == 1) {
            grayFrame = frame;
        } else {
            cv::cvtColor(frame, grayFrame, cv::COLOR_BGR2GRAY);
        }

        // ---- 状态识别（姿态/背包/载具/射击）按慢速间隔执行 ----
        auto now = std::chrono::steady_clock::now();
        auto statusElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastStatusTime).count();

        if (statusElapsed >= statusIntervalMs_) {
            lastStatusTime = now;

            // 识别姿态
            std::string pose;
            float poseConf = 0.0f;
            recognizeSingleCategory(grayFrame, "poses", "poses", pose, poseConf);

            // 识别背包（是否打开）
            std::string bagName;
            float bagConf = 0.0f;
            recognizeSingleCategory(grayFrame, "bag", "bag", bagName, bagConf);
            bool bagOpen = (bagName != "None" && bagConf > 0.0f);

            // 识别载具
            std::string carName;
            float carConf = 0.0f;
            recognizeSingleCategory(grayFrame, "car", "car", carName, carConf);

            // 识别射击状态 (通过像素亮度检测)
            bool shooting = false;
            float shootConf = 0.0f;
            cv::Point shootPx = templateMgr_.getShootPixel();
            if (shootPx.x > 0 && shootPx.y > 0 &&
                shootPx.x < grayFrame.cols && shootPx.y < grayFrame.rows) {
                if (!frame.empty() && shootPx.x < frame.cols && shootPx.y < frame.rows) {
                    cv::Vec3b pixel = frame.at<cv::Vec3b>(shootPx.y, shootPx.x);
                    int brightness = pixel[0] + pixel[1] + pixel[2];
                    shooting = (brightness >= templateMgr_.getShootThreshold());
                    shootConf = shooting ? 1.0f : 0.0f;
                }
            }

            // 更新状态
            {
                std::lock_guard<std::mutex> lock(state_.mutex);
                state_.pose = pose;
                state_.poseConfidence = poseConf;
                state_.isShooting = shooting;
                state_.shootConfidence = shootConf;
                state_.hasBag = bagOpen;
                state_.bagConfidence = bagConf;
                state_.inCar = (carName != "None" && carConf > 0.0f);
                state_.carConfidence = carConf;
            }

            // ---- 武器/配件：仅在背包打开时识别 ----
            if (bagOpen) {
                WeaponInfo primaryInfo;
                recognizeWeaponSlot(grayFrame, "rifle", primaryInfo);

                WeaponInfo secondaryInfo;
                recognizeWeaponSlot(grayFrame, "sniper", secondaryInfo);

                std::lock_guard<std::mutex> lock(state_.mutex);
                state_.primary = primaryInfo;
                state_.secondary = secondaryInfo;
            }
        }

        // 控制扫描间隔
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        auto sleepMs = std::chrono::milliseconds(scanIntervalMs_) - elapsed;
        if (sleepMs.count() > 0) {
            std::this_thread::sleep_for(sleepMs);
        }
    }
}

cv::Mat WeaponRecognizer::captureScreenGDI() {
    if (screenW_ <= 0 || screenH_ <= 0) return cv::Mat();

    HDC hScreen = GetDC(NULL);
    if (!hScreen) return cv::Mat();

    HDC hMemDC = CreateCompatibleDC(hScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, screenW_, screenH_);
    HGDIOBJ hOld = SelectObject(hMemDC, hBitmap);

    BitBlt(hMemDC, 0, 0, screenW_, screenH_, hScreen, 0, 0, SRCCOPY);

    BITMAPINFOHEADER bi = {};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = screenW_;
    bi.biHeight = -screenH_;  // top-down
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    cv::Mat frame(screenH_, screenW_, CV_8UC4);
    GetDIBits(hMemDC, hBitmap, 0, screenH_, frame.data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    SelectObject(hMemDC, hOld);
    DeleteObject(hBitmap);
    DeleteDC(hMemDC);
    ReleaseDC(NULL, hScreen);

    return frame;
}
