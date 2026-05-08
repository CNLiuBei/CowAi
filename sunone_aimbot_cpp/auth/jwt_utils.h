#pragma once

#include <string>
#include <cstdint>

namespace TGAuth {

// JWT Payload 结构
struct JWTPayload {
    std::string sub;    // tg_user_id
    std::string did;    // device_id
    int64_t iat = 0;    // issued at
    int64_t exp = 0;    // expires at
    std::string jti;    // unique token id
};

// JWT 工具类
class JWTUtils {
public:
    // 解码 JWT Token (不验证签名，仅解析 payload)
    static bool decode(const std::string& token, JWTPayload& payload);

    // Base64 URL 解码
    static std::string base64UrlDecode(const std::string& input);

    // Base64 URL 编码
    static std::string base64UrlEncode(const std::string& input);

    // 加密 Token (使用 Windows DPAPI)
    static std::string encrypt(const std::string& data, const std::string& key);

    // 解密 Token (DPAPI，兼容旧版 XOR)
    static std::string decrypt(const std::string& encrypted_data, const std::string& key);

private:
    JWTUtils() = delete;

    // 兼容旧版 XOR 解密 (用于平滑升级)
    static std::string decryptLegacyXOR(const std::string& encrypted_data, const std::string& key);

    // 简单 JSON 解析辅助函数
    static std::string extractJsonString(const std::string& json, const std::string& key);
    static int64_t extractJsonInt(const std::string& json, const std::string& key);
};

} // namespace TGAuth
