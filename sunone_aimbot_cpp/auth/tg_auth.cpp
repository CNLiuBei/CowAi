#include "tg_auth.h"
#include "device_id.h"
#include "http_client.h"
#include "jwt_utils.h"

#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>

namespace TGAuth {

// 状态字符串映射
const char* getStatusString(AuthStatus status) {
    switch (status) {
        case AuthStatus::NOT_INITIALIZED:       return "NOT_INITIALIZED";
        case AuthStatus::CHECKING:              return "CHECKING";
        case AuthStatus::WAITING_FOR_APPROVAL:  return "WAITING_FOR_APPROVAL";
        case AuthStatus::AUTHORIZED:            return "AUTHORIZED";
        case AuthStatus::UNAUTHORIZED:          return "UNAUTHORIZED";
        case AuthStatus::BANNED:                return "BANNED";
        case AuthStatus::DEVICE_LIMIT_EXCEEDED: return "DEVICE_LIMIT_EXCEEDED";
        case AuthStatus::NETWORK_ERROR:         return "NETWORK_ERROR";
        case AuthStatus::TOKEN_EXPIRED:         return "TOKEN_EXPIRED";
        case AuthStatus::SESSION_EXPIRED:       return "SESSION_EXPIRED";
        case AuthStatus::KEY_INVALID:           return "KEY_INVALID";
        case AuthStatus::KEY_EXPIRED:           return "KEY_EXPIRED";
        case AuthStatus::KEY_REVOKED:           return "KEY_REVOKED";
        default:                                return "UNKNOWN";
    }
}

// 错误字符串映射
const char* getErrorString(AuthError error) {
    switch (error) {
        case AuthError::NONE:                   return "NONE";
        case AuthError::NETWORK_ERROR:          return "NETWORK_ERROR";
        case AuthError::TIMEOUT:                return "TIMEOUT";
        case AuthError::SESSION_EXPIRED:        return "SESSION_EXPIRED";
        case AuthError::TOKEN_INVALID:          return "TOKEN_INVALID";
        case AuthError::TOKEN_EXPIRED:          return "TOKEN_EXPIRED";
        case AuthError::DEVICE_MISMATCH:        return "DEVICE_MISMATCH";
        case AuthError::USER_BANNED:            return "USER_BANNED";
        case AuthError::DEVICE_LIMIT_EXCEEDED:  return "DEVICE_LIMIT_EXCEEDED";
        case AuthError::RATE_LIMITED:           return "RATE_LIMITED";
        case AuthError::SERVER_ERROR:           return "SERVER_ERROR";
        case AuthError::PARSE_ERROR:            return "PARSE_ERROR";
        case AuthError::STORAGE_ERROR:          return "STORAGE_ERROR";
        case AuthError::KEY_INVALID:            return "KEY_INVALID";
        case AuthError::KEY_EXPIRED:            return "KEY_EXPIRED";
        case AuthError::KEY_REVOKED:            return "KEY_REVOKED";
        default:                                return "UNKNOWN";
    }
}

// 单例实现
AuthManager& AuthManager::getInstance() {
    static AuthManager instance;
    return instance;
}

AuthManager::AuthManager() = default;

AuthManager::~AuthManager() {
    stopPolling();
}

bool AuthManager::init(const AuthConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_initialized) {
        return true;
    }

    m_config = config;
    
    // 生成设备 ID
    m_device_id = generateDeviceId();
    if (m_device_id.empty()) {
        m_last_error = AuthError::STORAGE_ERROR;
        m_last_error_message = "Failed to generate device ID";
        return false;
    }

    // 设置默认 token 存储路径
    if (m_config.token_storage_path.empty()) {
        m_config.token_storage_path = "auth_token.dat";
    }

    m_initialized = true;
    m_status = AuthStatus::UNAUTHORIZED;
    
    return true;
}

AuthResult AuthManager::checkAuthorization() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    AuthResult result;
    result.status = AuthStatus::UNAUTHORIZED;
    result.error = AuthError::NONE;

    if (!m_initialized) {
        result.error = AuthError::STORAGE_ERROR;
        result.error_message = "AuthManager not initialized";
        return result;
    }

    m_status = AuthStatus::CHECKING;

    // 尝试加载本地 token
    if (loadToken()) {
        // 验证 token
        if (verifyTokenWithServer(m_access_token)) {
            m_status = AuthStatus::AUTHORIZED;
            result.status = AuthStatus::AUTHORIZED;
            result.tg_user_id = m_tg_user_id;
            result.username = m_username;
            result.expires_at = m_expires_at;
            return result;
        }
    }

    // Token 无效或不存在
    m_status = AuthStatus::UNAUTHORIZED;
    result.status = AuthStatus::UNAUTHORIZED;
    return result;
}

std::string AuthManager::requestLogin() {
    if (!m_initialized) {
        m_last_error = AuthError::STORAGE_ERROR;
        m_last_error_message = "AuthManager not initialized";
        return "";
    }

    HttpClient client;
    
    // 构建请求 body
    std::string body = "{\"device_id\":\"" + m_device_id + "\"}";
    
    // 发送请求
    HttpResponse response = client.post(
        m_config.worker_api_url + "/api/request_login",
        body,
        {{"Content-Type", "application/json"}}
    );

    if (!response.success) {
        m_last_error = AuthError::NETWORK_ERROR;
        m_last_error_message = response.error_message;
        return "";
    }

    // 解析响应
    // 简单 JSON 解析 (实际应使用 JSON 库)
    std::string session_id;
    std::string telegram_url;
    
    // 查找 session_id
    size_t pos = response.body.find("\"session_id\"");
    if (pos != std::string::npos) {
        size_t start = response.body.find("\"", pos + 12) + 1;
        size_t end = response.body.find("\"", start);
        if (start != std::string::npos && end != std::string::npos) {
            session_id = response.body.substr(start, end - start);
        }
    }

    // 查找 telegram_url
    pos = response.body.find("\"telegram_url\"");
    if (pos != std::string::npos) {
        size_t start = response.body.find("\"", pos + 14) + 1;
        size_t end = response.body.find("\"", start);
        if (start != std::string::npos && end != std::string::npos) {
            telegram_url = response.body.substr(start, end - start);
        }
    }

    if (session_id.empty() || telegram_url.empty()) {
        m_last_error = AuthError::PARSE_ERROR;
        m_last_error_message = "Failed to parse login response";
        return "";
    }

    m_session_id = session_id;
    m_status = AuthStatus::WAITING_FOR_APPROVAL;
    
    return telegram_url;
}

AuthStatus AuthManager::getStatus() const {
    return m_status;
}

AuthError AuthManager::getLastError() const {
    return m_last_error;
}

std::string AuthManager::getLastErrorMessage() const {
    return m_last_error_message;
}

void AuthManager::startPolling(AuthCallback callback) {
    if (m_polling.load()) {
        return;
    }

    m_poll_callback = callback;
    m_polling.store(true);
    
    m_poll_thread = std::thread(&AuthManager::pollingThread, this);
}

void AuthManager::stopPolling() {
    m_polling.store(false);
    
    if (m_poll_thread.joinable()) {
        m_poll_thread.join();
    }
}

bool AuthManager::isPolling() const {
    return m_polling.load();
}

void AuthManager::logout() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    stopPolling();
    clearToken();
    
    m_access_token.clear();
    m_session_id.clear();
    m_tg_user_id.clear();
    m_username.clear();
    m_expires_at = 0;
    
    m_status = AuthStatus::UNAUTHORIZED;
}

std::string AuthManager::getTgUserId() const {
    return m_tg_user_id;
}

std::string AuthManager::getUsername() const {
    return m_username;
}

int64_t AuthManager::getTokenExpiresAt() const {
    return m_expires_at;
}

bool AuthManager::isTokenExpiringSoon(int hours) const {
    if (m_expires_at == 0) {
        return true;
    }
    
    auto now = std::chrono::system_clock::now();
    auto now_ts = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()
    ).count();
    
    int64_t threshold = hours * 3600;
    return (m_expires_at - now_ts) < threshold;
}

std::string AuthManager::getKeyType() const {
    return m_key_type;
}

int64_t AuthManager::getKeyExpiresAt() const {
    return m_key_expires_at;
}

bool AuthManager::isKeyAuth() const {
    return m_is_key_auth;
}

AuthResult AuthManager::activateKey(const std::string& keyCode) {
    AuthResult result;
    result.status = AuthStatus::UNAUTHORIZED;
    result.error = AuthError::NONE;

    if (!m_initialized) {
        result.error = AuthError::STORAGE_ERROR;
        result.error_message = "AuthManager not initialized";
        return result;
    }

    HttpClient client;
    
    // 构建请求 body
    std::string body = "{\"key_code\":\"" + keyCode + "\",\"device_id\":\"" + m_device_id + "\"}";
    
    // 发送请求
    HttpResponse response = client.post(
        m_config.worker_api_url + "/api/activate_key",
        body,
        {{"Content-Type", "application/json"}}
    );

    if (!response.success) {
        result.status = AuthStatus::NETWORK_ERROR;
        result.error = AuthError::NETWORK_ERROR;
        result.error_message = response.error_message;
        m_last_error = AuthError::NETWORK_ERROR;
        m_last_error_message = response.error_message;
        return result;
    }

    // 检查响应
    if (response.body.find("\"success\":true") != std::string::npos ||
        response.body.find("\"success\": true") != std::string::npos) {
        
        // 提取 access_token
        size_t pos = response.body.find("\"access_token\"");
        if (pos != std::string::npos) {
            size_t start = response.body.find("\"", pos + 14) + 1;
            size_t end = response.body.find("\"", start);
            if (start != std::string::npos && end != std::string::npos) {
                m_access_token = response.body.substr(start, end - start);
            }
        }

        // 提取 key_type
        pos = response.body.find("\"key_type\"");
        if (pos != std::string::npos) {
            size_t start = response.body.find("\"", pos + 10) + 1;
            size_t end = response.body.find("\"", start);
            if (start != std::string::npos && end != std::string::npos) {
                m_key_type = response.body.substr(start, end - start);
            }
        }

        // 提取 key_expires_at
        pos = response.body.find("\"key_expires_at\"");
        if (pos != std::string::npos) {
            size_t start = response.body.find(":", pos) + 1;
            size_t end = response.body.find_first_of(",}", start);
            if (start != std::string::npos && end != std::string::npos) {
                std::string expires_str = response.body.substr(start, end - start);
                // 去除空格
                expires_str.erase(0, expires_str.find_first_not_of(" \t\n\r"));
                expires_str.erase(expires_str.find_last_not_of(" \t\n\r") + 1);
                if (expires_str != "null") {
                    m_key_expires_at = std::stoll(expires_str);
                }
            }
        }

        if (!m_access_token.empty() && parseJWT(m_access_token)) {
            saveToken(m_access_token);
            m_key_code = keyCode;
            m_is_key_auth = true;
            m_status = AuthStatus::AUTHORIZED;
            
            result.status = AuthStatus::AUTHORIZED;
            result.tg_user_id = m_tg_user_id;
            result.key_type = m_key_type;
            result.expires_at = m_expires_at;
            result.key_expires_at = m_key_expires_at;
            return result;
        }
    }

    // Parse error response - 匹配服务端返回的中文错误信息
    // 卡密不存在
    if (response.body.find("KEY_INVALID") != std::string::npos ||
        response.body.find("not found") != std::string::npos ||
        response.body.find("\xE5\x8D\xA1\xE5\xAF\x86\xE4\xB8\x8D\xE5\xAD\x98\xE5\x9C\xA8") != std::string::npos) {
        result.status = AuthStatus::KEY_INVALID;
        result.error = AuthError::KEY_INVALID;
        result.error_message = "\xE5\x8D\xA1\xE5\xAF\x86\xE4\xB8\x8D\xE5\xAD\x98\xE5\x9C\xA8";  // 卡密不存在
    }
    // 卡密已过期
    else if (response.body.find("KEY_EXPIRED") != std::string::npos ||
             response.body.find("\xE5\x8D\xA1\xE5\xAF\x86\xE5\xB7\xB2\xE8\xBF\x87\xE6\x9C\x9F") != std::string::npos) {
        result.status = AuthStatus::KEY_EXPIRED;
        result.error = AuthError::KEY_EXPIRED;
        result.error_message = "\xE5\x8D\xA1\xE5\xAF\x86\xE5\xB7\xB2\xE8\xBF\x87\xE6\x9C\x9F";  // 卡密已过期
    }
    // 卡密已被撤销
    else if (response.body.find("KEY_REVOKED") != std::string::npos ||
             response.body.find("revoked") != std::string::npos ||
             response.body.find("\xE5\x8D\xA1\xE5\xAF\x86\xE5\xB7\xB2\xE8\xA2\xAB\xE6\x92\xA4\xE9\x94\x80") != std::string::npos) {
        result.status = AuthStatus::KEY_REVOKED;
        result.error = AuthError::KEY_REVOKED;
        result.error_message = "\xE5\x8D\xA1\xE5\xAF\x86\xE5\xB7\xB2\xE8\xA2\xAB\xE6\x92\xA4\xE9\x94\x80";  // 卡密已被撤销
    }
    // 设备数量已达上限
    else if (response.body.find("DEVICE_LIMIT") != std::string::npos ||
             response.body.find("device limit") != std::string::npos ||
             response.body.find("\xE8\xAE\xBE\xE5\xA4\x87\xE6\x95\xB0\xE9\x87\x8F\xE5\xB7\xB2\xE8\xBE\xBE\xE4\xB8\x8A\xE9\x99\x90") != std::string::npos) {
        result.status = AuthStatus::DEVICE_LIMIT_EXCEEDED;
        result.error = AuthError::DEVICE_LIMIT_EXCEEDED;
        result.error_message = "\xE8\xAE\xBE\xE5\xA4\x87\xE6\x95\xB0\xE9\x87\x8F\xE5\xB7\xB2\xE8\xBE\xBE\xE4\xB8\x8A\xE9\x99\x90\xEF\xBC\x8C\xE8\xAF\xB7\xE8\x81\x94\xE7\xB3\xBB\xE7\xAE\xA1\xE7\x90\x86\xE5\x91\x98\xE8\xA7\xA3\xE7\xBB\x91";  // 设备数量已达上限，请联系管理员解绑
    }
    // 尝试从响应中提取 error 字段
    else {
        // 查找 "error":"xxx" 格式
        size_t errPos = response.body.find("\"error\"");
        if (errPos != std::string::npos) {
            size_t start = response.body.find("\"", errPos + 7) + 1;
            size_t end = response.body.find("\"", start);
            if (start != std::string::npos && end != std::string::npos && end > start) {
                result.error_message = response.body.substr(start, end - start);
            } else {
                result.error_message = "\xE6\xBF\x80\xE6\xB4\xBB\xE5\xA4\xB1\xE8\xB4\xA5";  // 激活失败
            }
        } else {
            result.error_message = "\xE6\xBF\x80\xE6\xB4\xBB\xE5\xA4\xB1\xE8\xB4\xA5\xEF\xBC\x8C\xE8\xAF\xB7\xE6\xA3\x80\xE6\x9F\xA5\xE7\xBD\x91\xE7\xBB\x9C";  // 激活失败，请检查网络
        }
        result.error = AuthError::SERVER_ERROR;
    }

    m_last_error = result.error;
    m_last_error_message = result.error_message;
    return result;
}

AuthResult AuthManager::verifyKey(const std::string& keyCode) {
    AuthResult result;
    result.status = AuthStatus::UNAUTHORIZED;
    result.error = AuthError::NONE;

    if (!m_initialized) {
        result.error = AuthError::STORAGE_ERROR;
        result.error_message = "AuthManager not initialized";
        return result;
    }

    HttpClient client;
    
    // 构建请求 body
    std::string body = "{\"key_code\":\"" + keyCode + "\",\"device_id\":\"" + m_device_id + "\"}";
    
    // 发送请求
    HttpResponse response = client.post(
        m_config.worker_api_url + "/api/verify_key",
        body,
        {{"Content-Type", "application/json"}}
    );

    if (!response.success) {
        result.status = AuthStatus::NETWORK_ERROR;
        result.error = AuthError::NETWORK_ERROR;
        result.error_message = response.error_message;
        return result;
    }

    if (response.body.find("\"success\":true") != std::string::npos ||
        response.body.find("\"success\": true") != std::string::npos) {
        result.status = AuthStatus::AUTHORIZED;
        
        // 提取 key_type
        size_t pos = response.body.find("\"key_type\"");
        if (pos != std::string::npos) {
            size_t start = response.body.find("\"", pos + 10) + 1;
            size_t end = response.body.find("\"", start);
            if (start != std::string::npos && end != std::string::npos) {
                result.key_type = response.body.substr(start, end - start);
            }
        }

        // 提取 expires_at
        pos = response.body.find("\"expires_at\"");
        if (pos != std::string::npos) {
            size_t start = response.body.find(":", pos) + 1;
            size_t end = response.body.find_first_of(",}", start);
            if (start != std::string::npos && end != std::string::npos) {
                std::string expires_str = response.body.substr(start, end - start);
                expires_str.erase(0, expires_str.find_first_not_of(" \t\n\r"));
                expires_str.erase(expires_str.find_last_not_of(" \t\n\r") + 1);
                if (expires_str != "null") {
                    result.key_expires_at = std::stoll(expires_str);
                }
            }
        }

        return result;
    }

    // Parse error response
    if (response.body.find("KEY_INVALID") != std::string::npos ||
        response.body.find("not found") != std::string::npos) {
        result.status = AuthStatus::KEY_INVALID;
        result.error = AuthError::KEY_INVALID;
    }
    else if (response.body.find("KEY_EXPIRED") != std::string::npos ||
             response.body.find("expired") != std::string::npos) {
        result.status = AuthStatus::KEY_EXPIRED;
        result.error = AuthError::KEY_EXPIRED;
    }
    else if (response.body.find("not bound") != std::string::npos ||
             response.body.find("DEVICE_MISMATCH") != std::string::npos) {
        result.status = AuthStatus::UNAUTHORIZED;
        result.error = AuthError::DEVICE_MISMATCH;
    }

    return result;
}

std::string AuthManager::generateDeviceId() {
    return DeviceId::generate();
}

bool AuthManager::loadToken() {
    std::ifstream file(m_config.token_storage_path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string encrypted_token = buffer.str();
    file.close();

    if (encrypted_token.empty()) {
        return false;
    }

    // 解密 token (使用设备 ID 作为密钥)
    m_access_token = JWTUtils::decrypt(encrypted_token, m_device_id);
    
    if (m_access_token.empty()) {
        return false;
    }

    // 解析 JWT 获取用户信息
    return parseJWT(m_access_token);
}

bool AuthManager::saveToken(const std::string& token) {
    // 加密 token (使用设备 ID 作为密钥)
    std::string encrypted_token = JWTUtils::encrypt(token, m_device_id);
    
    if (encrypted_token.empty()) {
        return false;
    }

    std::ofstream file(m_config.token_storage_path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    file << encrypted_token;
    file.close();
    
    return true;
}

bool AuthManager::clearToken() {
    std::remove(m_config.token_storage_path.c_str());
    return true;
}

bool AuthManager::verifyTokenWithServer(const std::string& token) {
    HttpClient client;
    
    HttpResponse response = client.post(
        m_config.worker_api_url + "/api/verify_token",
        "{}",
        {
            {"Content-Type", "application/json"},
            {"Authorization", "Bearer " + token},
            {"X-Device-ID", m_device_id}
        }
    );

    if (!response.success) {
        m_last_error = AuthError::NETWORK_ERROR;
        m_last_error_message = response.error_message;
        return false;
    }

    // 检查响应中的 success 字段
    if (response.body.find("\"success\":true") != std::string::npos ||
        response.body.find("\"success\": true") != std::string::npos) {
        return true;
    }

    // 检查错误类型 - 包括卡密相关错误
    if (response.body.find("USER_BANNED") != std::string::npos) {
        m_status = AuthStatus::BANNED;
        m_last_error = AuthError::USER_BANNED;
        m_last_error_message = "\xE7\x94\xA8\xE6\x88\xB7\xE5\xB7\xB2\xE8\xA2\xAB\xE5\xB0\x81\xE7\xA6\x81";  // 用户已被封禁
    } else if (response.body.find("KEY_REVOKED") != std::string::npos ||
               response.body.find("\xE5\x8D\xA1\xE5\xAF\x86\xE5\xB7\xB2\xE8\xA2\xAB\xE6\x92\xA4\xE9\x94\x80") != std::string::npos) {
        m_status = AuthStatus::KEY_REVOKED;
        m_last_error = AuthError::KEY_REVOKED;
        m_last_error_message = "\xE5\x8D\xA1\xE5\xAF\x86\xE5\xB7\xB2\xE8\xA2\xAB\xE6\x92\xA4\xE9\x94\x80";  // 卡密已被撤销
    } else if (response.body.find("KEY_EXPIRED") != std::string::npos ||
               response.body.find("\xE5\x8D\xA1\xE5\xAF\x86\xE5\xB7\xB2\xE8\xBF\x87\xE6\x9C\x9F") != std::string::npos) {
        m_status = AuthStatus::KEY_EXPIRED;
        m_last_error = AuthError::KEY_EXPIRED;
        m_last_error_message = "\xE5\x8D\xA1\xE5\xAF\x86\xE5\xB7\xB2\xE8\xBF\x87\xE6\x9C\x9F";  // 卡密已过期
    } else if (response.body.find("DEVICE_UNBOUND") != std::string::npos ||
               response.body.find("\xE8\xAE\xBE\xE5\xA4\x87\xE5\xB7\xB2\xE8\xA7\xA3\xE7\xBB\x91") != std::string::npos) {
        m_status = AuthStatus::UNAUTHORIZED;
        m_last_error = AuthError::DEVICE_MISMATCH;
        m_last_error_message = "\xE8\xAE\xBE\xE5\xA4\x87\xE5\xB7\xB2\xE8\xA7\xA3\xE7\xBB\x91";  // 设备已解绑
    } else if (response.body.find("TOKEN_EXPIRED") != std::string::npos) {
        m_status = AuthStatus::TOKEN_EXPIRED;
        m_last_error = AuthError::TOKEN_EXPIRED;
        m_last_error_message = "\xE6\x8E\x88\xE6\x9D\x83\xE5\xB7\xB2\xE8\xBF\x87\xE6\x9C\x9F";  // 授权已过期
    } else if (response.body.find("DEVICE_MISMATCH") != std::string::npos) {
        m_last_error = AuthError::DEVICE_MISMATCH;
        m_last_error_message = "\xE8\xAE\xBE\xE5\xA4\x87\xE4\xB8\x8D\xE5\x8C\xB9\xE9\x85\x8D";  // 设备不匹配
    } else {
        m_last_error = AuthError::TOKEN_INVALID;
        m_last_error_message = "\xE6\x8E\x88\xE6\x9D\x83\xE6\x97\xA0\xE6\x95\x88";  // 授权无效
    }

    // 清除本地 token
    clearToken();

    return false;
}

bool AuthManager::parseJWT(const std::string& token) {
    JWTPayload payload;
    if (!JWTUtils::decode(token, payload)) {
        return false;
    }

    m_tg_user_id = payload.sub;
    m_expires_at = payload.exp;
    
    // 检查是否过期
    auto now = std::chrono::system_clock::now();
    auto now_ts = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()
    ).count();
    
    if (payload.exp < now_ts) {
        m_last_error = AuthError::TOKEN_EXPIRED;
        return false;
    }

    // 检查设备 ID 是否匹配
    if (payload.did != m_device_id) {
        m_last_error = AuthError::DEVICE_MISMATCH;
        return false;
    }

    return true;
}

void AuthManager::pollingThread() {
    HttpClient client;
    
    while (m_polling.load()) {
        // 发送检查请求
        HttpResponse response = client.get(
            m_config.worker_api_url + "/api/check_login?session_id=" + m_session_id
        );

        AuthResult result;
        result.error = AuthError::NONE;

        if (!response.success) {
            result.status = AuthStatus::NETWORK_ERROR;
            result.error = AuthError::NETWORK_ERROR;
            result.error_message = response.error_message;
            
            if (m_poll_callback) {
                m_poll_callback(result);
            }
            
            std::this_thread::sleep_for(
                std::chrono::milliseconds(m_config.poll_interval_ms)
            );
            continue;
        }

        // 解析状态
        if (response.body.find("\"status\":\"pending\"") != std::string::npos ||
            response.body.find("\"status\": \"pending\"") != std::string::npos) {
            result.status = AuthStatus::WAITING_FOR_APPROVAL;
        }
        else if (response.body.find("\"status\":\"approved\"") != std::string::npos ||
                 response.body.find("\"status\": \"approved\"") != std::string::npos) {
            // 提取 access_token
            size_t pos = response.body.find("\"access_token\"");
            if (pos != std::string::npos) {
                size_t start = response.body.find("\"", pos + 14) + 1;
                size_t end = response.body.find("\"", start);
                if (start != std::string::npos && end != std::string::npos) {
                    m_access_token = response.body.substr(start, end - start);
                }
            }

            if (!m_access_token.empty() && parseJWT(m_access_token)) {
                saveToken(m_access_token);
                m_status = AuthStatus::AUTHORIZED;
                result.status = AuthStatus::AUTHORIZED;
                result.tg_user_id = m_tg_user_id;
                result.username = m_username;
                result.expires_at = m_expires_at;
                
                m_polling.store(false);
            } else {
                result.status = AuthStatus::UNAUTHORIZED;
                result.error = AuthError::PARSE_ERROR;
            }
        }
        else if (response.body.find("\"status\":\"rejected\"") != std::string::npos) {
            result.status = AuthStatus::UNAUTHORIZED;
            m_status = AuthStatus::UNAUTHORIZED;
            m_polling.store(false);
        }
        else if (response.body.find("\"status\":\"expired\"") != std::string::npos ||
                 response.body.find("SESSION_EXPIRED") != std::string::npos) {
            result.status = AuthStatus::SESSION_EXPIRED;
            result.error = AuthError::SESSION_EXPIRED;
            m_status = AuthStatus::SESSION_EXPIRED;
            m_polling.store(false);
        }
        else if (response.body.find("DEVICE_LIMIT_EXCEEDED") != std::string::npos) {
            result.status = AuthStatus::DEVICE_LIMIT_EXCEEDED;
            result.error = AuthError::DEVICE_LIMIT_EXCEEDED;
            m_status = AuthStatus::DEVICE_LIMIT_EXCEEDED;
            m_polling.store(false);
        }
        else if (response.body.find("USER_BANNED") != std::string::npos) {
            result.status = AuthStatus::BANNED;
            result.error = AuthError::USER_BANNED;
            m_status = AuthStatus::BANNED;
            m_polling.store(false);
        }

        if (m_poll_callback) {
            m_poll_callback(result);
        }

        if (m_polling.load()) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(m_config.poll_interval_ms)
            );
        }
    }
}

} // namespace TGAuth
