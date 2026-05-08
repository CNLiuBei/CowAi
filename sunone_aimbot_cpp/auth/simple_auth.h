#pragma once

#include <string>
#include <atomic>
#include <functional>

namespace SimpleAuth {

// 授权状态
enum class AuthStatus {
    NOT_CHECKED,        // 未检查
    CHECKING,           // 检查中
    VALID,              // 有效
    INVALID,            // 无效
    EXPIRED,            // 已过期
    NETWORK_ERROR       // 网络错误
};

// 授权结果
struct AuthResult {
    AuthStatus status = AuthStatus::NOT_CHECKED;
    std::string key_code;
    std::string key_type;       // "trial", "monthly", "permanent"
    int64_t expires_at = 0;     // Unix timestamp, 0 = permanent
    std::string error_message;
};

// 授权管理器
class AuthManager {
public:
    static AuthManager& getInstance();
    
    // 初始化
    bool init(const std::string& api_url);
    
    // 快速检查（本地缓存，不阻塞）
    AuthResult quickCheck();
    
    // 激活卡密（会联网验证）
    AuthResult activate(const std::string& key_code);
    
    // 验证卡密（会联网验证）
    AuthResult verify();
    
    // 清除本地授权
    void clearAuth();
    
    // 获取设备ID
    std::string getDeviceId() const { return m_device_id; }
    
private:
    AuthManager() = default;
    ~AuthManager() = default;
    AuthManager(const AuthManager&) = delete;
    AuthManager& operator=(const AuthManager&) = delete;
    
    // 保存/加载本地授权信息
    bool saveLocal(const AuthResult& result);
    bool loadLocal(AuthResult& result);
    
    // 网络请求
    bool httpPost(const std::string& url, const std::string& body, std::string& response, std::string& error);
    
    std::string m_api_url;
    std::string m_device_id;
    std::string m_auth_file = "auth.dat";
    std::atomic<bool> m_initialized{false};
};

} // namespace SimpleAuth
