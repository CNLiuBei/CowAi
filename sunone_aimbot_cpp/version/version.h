#pragma once
#include <string>

// 当前版本号
#define APP_VERSION "2.1.1"
#define APP_VERSION_MAJOR 2
#define APP_VERSION_MINOR 1
#define APP_VERSION_PATCH 1

// 版本检查
namespace Version {
    // 检查更新，返回是否有新版本
    bool CheckForUpdate(std::string& newVersion, std::string& downloadUrl, std::string& changelog);
    
    // 显示更新提示窗口
    void ShowUpdateDialog(const std::string& newVersion, const std::string& downloadUrl, const std::string& changelog);
    
    // 获取当前版本
    inline const char* GetVersion() { return APP_VERSION; }
}
