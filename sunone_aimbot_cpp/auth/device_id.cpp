#include "device_id.h"

#include <windows.h>
#include <wincrypt.h>
#include <intrin.h>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "advapi32.lib")

namespace TGAuth {

std::string DeviceId::generate() {
    std::string combined;
    
    // 获取各硬件标识
    std::string cpu_id = getProcessorId();
    std::string disk_serial = getDiskSerialNumber();
    std::string win_guid = getWindowsGuid();
    
    // 组合所有标识
    combined = cpu_id + disk_serial + win_guid;
    
    if (combined.empty()) {
        return "";
    }
    
    // 返回 SHA256 哈希
    return sha256(combined);
}

std::string DeviceId::getProcessorId() {
    int cpuInfo[4] = {0};
    
    // 获取 CPU 信息
    __cpuid(cpuInfo, 1);
    
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    ss << std::setw(8) << cpuInfo[0];  // EAX
    ss << std::setw(8) << cpuInfo[3];  // EDX
    
    // 获取更多 CPU 特征
    __cpuid(cpuInfo, 0);
    ss << std::setw(8) << cpuInfo[1];  // EBX
    ss << std::setw(8) << cpuInfo[2];  // ECX
    ss << std::setw(8) << cpuInfo[3];  // EDX
    
    return ss.str();
}

std::string DeviceId::getDiskSerialNumber() {
    char volumeName[MAX_PATH + 1] = {0};
    char fileSystemName[MAX_PATH + 1] = {0};
    DWORD serialNumber = 0;
    DWORD maxComponentLen = 0;
    DWORD fileSystemFlags = 0;
    
    // 获取 C 盘卷信息
    if (GetVolumeInformationA(
        "C:\\",
        volumeName, sizeof(volumeName),
        &serialNumber,
        &maxComponentLen,
        &fileSystemFlags,
        fileSystemName, sizeof(fileSystemName)
    )) {
        std::stringstream ss;
        ss << std::hex << std::setfill('0') << std::setw(8) << serialNumber;
        return ss.str();
    }
    
    return "";
}

std::string DeviceId::getWindowsGuid() {
    HKEY hKey;
    char guid[256] = {0};
    DWORD size = sizeof(guid);
    
    // 打开注册表键
    if (RegOpenKeyExA(
        HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Cryptography",
        0,
        KEY_READ | KEY_WOW64_64KEY,
        &hKey
    ) == ERROR_SUCCESS) {
        // 读取 MachineGuid
        if (RegQueryValueExA(
            hKey,
            "MachineGuid",
            NULL,
            NULL,
            (LPBYTE)guid,
            &size
        ) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return std::string(guid);
        }
        RegCloseKey(hKey);
    }
    
    return "";
}

std::string DeviceId::sha256(const std::string& input) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE hash[32] = {0};  // SHA256 = 32 bytes
    DWORD hashLen = 32;
    std::string result;
    
    // 获取加密提供程序
    if (!CryptAcquireContextA(
        &hProv,
        NULL,
        NULL,
        PROV_RSA_AES,
        CRYPT_VERIFYCONTEXT
    )) {
        return "";
    }
    
    // 创建哈希对象
    if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return "";
    }
    
    // 哈希数据
    if (!CryptHashData(
        hHash,
        (const BYTE*)input.c_str(),
        (DWORD)input.length(),
        0
    )) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return "";
    }
    
    // 获取哈希值
    if (!CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return "";
    }
    
    // 转换为十六进制字符串
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (DWORD i = 0; i < hashLen; i++) {
        ss << std::setw(2) << (int)hash[i];
    }
    result = ss.str();
    
    // 清理
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
    
    return result;
}

} // namespace TGAuth
