#include "jwt_utils.h"

#include <windows.h>
#include <wincrypt.h>
#include <dpapi.h>
#include <sstream>
#include <vector>
#include <algorithm>

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "crypt32.lib")  // for DPAPI

namespace TGAuth {

bool JWTUtils::decode(const std::string& token, JWTPayload& payload) {
    // JWT 格式: header.payload.signature
    // 我们只需要解析 payload 部分
    
    size_t first_dot = token.find('.');
    if (first_dot == std::string::npos) {
        return false;
    }
    
    size_t second_dot = token.find('.', first_dot + 1);
    if (second_dot == std::string::npos) {
        return false;
    }
    
    // 提取 payload 部分
    std::string payload_b64 = token.substr(first_dot + 1, second_dot - first_dot - 1);
    
    // Base64 URL 解码
    std::string payload_json = base64UrlDecode(payload_b64);
    if (payload_json.empty()) {
        return false;
    }
    
    // 解析 JSON
    payload.sub = extractJsonString(payload_json, "sub");
    payload.did = extractJsonString(payload_json, "did");
    payload.jti = extractJsonString(payload_json, "jti");
    payload.iat = extractJsonInt(payload_json, "iat");
    payload.exp = extractJsonInt(payload_json, "exp");
    
    // 至少需要 sub 和 exp
    if (payload.sub.empty() || payload.exp == 0) {
        return false;
    }
    
    return true;
}

std::string JWTUtils::base64UrlDecode(const std::string& input) {
    // 将 Base64 URL 转换为标准 Base64
    std::string base64 = input;
    
    // 替换 URL 安全字符
    std::replace(base64.begin(), base64.end(), '-', '+');
    std::replace(base64.begin(), base64.end(), '_', '/');
    
    // 添加填充
    while (base64.length() % 4 != 0) {
        base64 += '=';
    }
    
    // 使用 Windows CryptoAPI 解码
    DWORD decoded_len = 0;
    if (!CryptStringToBinaryA(
        base64.c_str(),
        (DWORD)base64.length(),
        CRYPT_STRING_BASE64,
        NULL,
        &decoded_len,
        NULL,
        NULL
    )) {
        return "";
    }
    
    std::vector<BYTE> decoded(decoded_len);
    if (!CryptStringToBinaryA(
        base64.c_str(),
        (DWORD)base64.length(),
        CRYPT_STRING_BASE64,
        decoded.data(),
        &decoded_len,
        NULL,
        NULL
    )) {
        return "";
    }
    
    return std::string(decoded.begin(), decoded.begin() + decoded_len);
}

std::string JWTUtils::base64UrlEncode(const std::string& input) {
    // 使用 Windows CryptoAPI 编码
    DWORD encoded_len = 0;
    if (!CryptBinaryToStringA(
        (const BYTE*)input.c_str(),
        (DWORD)input.length(),
        CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
        NULL,
        &encoded_len
    )) {
        return "";
    }
    
    std::vector<char> encoded(encoded_len);
    if (!CryptBinaryToStringA(
        (const BYTE*)input.c_str(),
        (DWORD)input.length(),
        CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
        encoded.data(),
        &encoded_len
    )) {
        return "";
    }
    
    std::string result(encoded.data(), encoded_len);
    
    // 移除填充并转换为 URL 安全字符
    while (!result.empty() && result.back() == '=') {
        result.pop_back();
    }
    std::replace(result.begin(), result.end(), '+', '-');
    std::replace(result.begin(), result.end(), '/', '_');
    
    return result;
}

std::string JWTUtils::encrypt(const std::string& data, const std::string& key) {
    // 使用 Windows DPAPI 加密 (机器绑定，比 XOR 安全得多)
    // key 参数作为附加熵 (additional entropy)
    
    if (data.empty()) {
        return "";
    }
    
    DATA_BLOB inputBlob;
    inputBlob.pbData = (BYTE*)data.c_str();
    inputBlob.cbData = (DWORD)data.length();
    
    DATA_BLOB entropyBlob;
    entropyBlob.pbData = (BYTE*)key.c_str();
    entropyBlob.cbData = (DWORD)key.length();
    
    DATA_BLOB outputBlob;
    
    // CRYPTPROTECT_LOCAL_MACHINE: 同一台机器上的任何用户都能解密
    // 不加此标志则只有当前用户能解密
    if (!CryptProtectData(&inputBlob, NULL, &entropyBlob, NULL, NULL, 
                          CRYPTPROTECT_LOCAL_MACHINE, &outputBlob)) {
        return "";
    }
    
    std::string encrypted((char*)outputBlob.pbData, outputBlob.cbData);
    LocalFree(outputBlob.pbData);
    
    // Base64 编码以便存储
    return base64UrlEncode(encrypted);
}

std::string JWTUtils::decrypt(const std::string& encrypted_data, const std::string& key) {
    if (encrypted_data.empty()) {
        return "";
    }
    
    // Base64 解码
    std::string decoded = base64UrlDecode(encrypted_data);
    if (decoded.empty()) {
        // 兼容旧版 XOR 加密的 token：尝试 XOR 解密
        return decryptLegacyXOR(encrypted_data, key);
    }
    
    DATA_BLOB inputBlob;
    inputBlob.pbData = (BYTE*)decoded.c_str();
    inputBlob.cbData = (DWORD)decoded.length();
    
    DATA_BLOB entropyBlob;
    entropyBlob.pbData = (BYTE*)key.c_str();
    entropyBlob.cbData = (DWORD)key.length();
    
    DATA_BLOB outputBlob;
    
    if (!CryptUnprotectData(&inputBlob, NULL, &entropyBlob, NULL, NULL, 0, &outputBlob)) {
        // DPAPI 解密失败，尝试兼容旧版 XOR 格式
        return decryptLegacyXOR(encrypted_data, key);
    }
    
    std::string decrypted((char*)outputBlob.pbData, outputBlob.cbData);
    LocalFree(outputBlob.pbData);
    
    return decrypted;
}

std::string JWTUtils::decryptLegacyXOR(const std::string& encrypted_data, const std::string& key) {
    // 兼容旧版 XOR 加密格式，用于平滑升级
    if (encrypted_data.empty() || key.empty()) {
        return "";
    }
    
    std::string decoded = base64UrlDecode(encrypted_data);
    if (decoded.empty()) {
        return "";
    }
    
    std::string decrypted;
    decrypted.reserve(decoded.length());
    
    for (size_t i = 0; i < decoded.length(); i++) {
        decrypted += decoded[i] ^ key[i % key.length()];
    }
    
    // 简单验证：JWT token 应该以 "eyJ" 开头
    if (decrypted.length() > 3 && decrypted.substr(0, 3) == "eyJ") {
        return decrypted;
    }
    
    return "";
}

std::string JWTUtils::extractJsonString(const std::string& json, const std::string& key) {
    // 简单 JSON 字符串提取
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) {
        return "";
    }
    
    // 找到冒号
    pos = json.find(':', pos + search.length());
    if (pos == std::string::npos) {
        return "";
    }
    
    // 跳过空白
    pos++;
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) {
        pos++;
    }
    
    // 检查是否是字符串
    if (pos >= json.length() || json[pos] != '"') {
        return "";
    }
    
    // 找到字符串结束
    size_t start = pos + 1;
    size_t end = start;
    while (end < json.length()) {
        if (json[end] == '"' && (end == start || json[end - 1] != '\\')) {
            break;
        }
        end++;
    }
    
    if (end >= json.length()) {
        return "";
    }
    
    return json.substr(start, end - start);
}

int64_t JWTUtils::extractJsonInt(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) {
        return 0;
    }
    
    // 找到冒号
    pos = json.find(':', pos + search.length());
    if (pos == std::string::npos) {
        return 0;
    }
    
    // 跳过空白
    pos++;
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) {
        pos++;
    }
    
    // 提取数字
    std::string num_str;
    while (pos < json.length() && (isdigit(json[pos]) || json[pos] == '-')) {
        num_str += json[pos];
        pos++;
    }
    
    if (num_str.empty()) {
        return 0;
    }
    
    try {
        return std::stoll(num_str);
    } catch (...) {
        return 0;
    }
}

} // namespace TGAuth
