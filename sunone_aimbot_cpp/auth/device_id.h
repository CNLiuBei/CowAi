#pragma once

#include <string>

namespace TGAuth {

// 设备 ID 生成模块
class DeviceId {
public:
    // 生成设备唯一标识符
    // 使用 SHA256(cpu_serial + disk_serial + windows_guid)
    static std::string generate();

    // 获取 CPU 序列号/ID
    static std::string getProcessorId();

    // 获取磁盘序列号
    static std::string getDiskSerialNumber();

    // 获取 Windows 机器 GUID
    static std::string getWindowsGuid();

    // SHA256 哈希
    static std::string sha256(const std::string& input);

private:
    DeviceId() = delete;
};

} // namespace TGAuth
