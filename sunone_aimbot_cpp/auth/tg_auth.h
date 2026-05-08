#pragma once

#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>

namespace TGAuth {

// 授权状态枚举
enum class AuthStatus {
    NOT_INITIALIZED,        // 未初始化
    CHECKING,               // 正在检查
    WAITING_FOR_APPROVAL,   // 等待用户授权
    AUTHORIZED,             // 已授权
    UNAUTHORIZED,           // 未授权
    BANNED,                 // 已被封禁
    DEVICE_LIMIT_EXCEEDED,  // 超出设备限制
    NETWORK_ERROR,          // 网络错误
    TOKEN_EXPIRED,          // Token 已过期
    SESSION_EXPIRED,        // 会话已过期
    KEY_INVALID,            // 卡密无效
    KEY_EXPIRED,            // 卡密已过期
    KEY_REVOKED,            // 卡密已撤销
};

// 错误码枚举
enum class AuthError {
    NONE = 0,
    NETWORK_ERROR = 1,
    TIMEOUT = 2,
    SESSION_EXPIRED = 3,
    TOKEN_INVALID = 4,
    TOKEN_EXPIRED = 5,
    DEVICE_MISMATCH = 6,
    USER_BANNED = 7,
    DEVICE_LIMIT_EXCEEDED = 8,
    RATE_LIMITED = 9,
    SERVER_ERROR = 10,
    PARSE_ERROR = 11,
    STORAGE_ERROR = 12,
    KEY_INVALID = 13,
    KEY_EXPIRED = 14,
    KEY_REVOKED = 15
};

// 配置结构
struct AuthConfig {
    std::string worker_api_url;      // Cloudflare Worker URL
    std::string bot_username;         // Telegram Bot 用户名
    int poll_interval_ms = 2000;      // 轮询间隔 (毫秒)
    int session_timeout_sec = 300;    // 会话超时时间 (秒)
};

// 授权结果结构
struct AuthResult {
    AuthStatus status;
    AuthError error;
    std::string error_message;
    std::string tg_user_id;
    std::string username;
    std::string key_type;             // 卡密类型: "time" 或 "permanent"
    int64_t expires_at;
    int64_t key_expires_at;           // 卡密过期时间 (0 表示永久)
};

// 授权回调函数类型
using AuthCallback = std::function<void(const AuthResult&)>;

// 授权管理器类
class AuthManager {
public:
    // 获取单例实例
    static AuthManager& getInstance();

    // 禁止拷贝和赋值
    AuthManager(const AuthManager&) = delete;
    AuthManager& operator=(const AuthManager&) = delete;

    // 初始化授权模块
    bool init(const AuthConfig& config);

    // 检查是否已授权（同步，用于启动检查）
    AuthResult checkAuthorization();

    // 请求登录，返回 Telegram 登录 URL
    std::string requestLogin();

    // 使用卡密激活
    AuthResult activateKey(const std::string& keyCode);

    // 验证卡密
    AuthResult verifyKey(const std::string& keyCode);

    // 获取当前登录状态
    AuthStatus getStatus() const;

    // 获取最后一次错误
    AuthError getLastError() const;
    std::string getLastErrorMessage() const;

    // 开始轮询登录状态（异步）
    void startPolling(AuthCallback callback);

    // 停止轮询
    void stopPolling();

    // 是否正在轮询
    bool isPolling() const;

    // 登出，清除本地 token
    void logout();

    // 获取当前用户信息
    std::string getTgUserId() const;
    std::string getUsername() const;

    // 获取 Token 过期时间
    int64_t getTokenExpiresAt() const;

    // 检查 Token 是否即将过期（默认 24 小时内）
    bool isTokenExpiringSoon(int hours = 24) const;

    // 获取卡密类型
    std::string getKeyType() const;

    // 获取卡密过期时间
    int64_t getKeyExpiresAt() const;

    // 是否使用卡密登录
    bool isKeyAuth() const;

    // 解绑当前设备
    AuthResult unbindDevice(const std::string& keyCode);

    // 获取保存的卡密
    std::string getSavedKeyCode();

private:
    AuthManager();
    ~AuthManager();

    // 生成设备 ID
    std::string generateDeviceId();

    // 卡密存储操作
    bool loadKeyCode();
    bool saveKeyCode(const std::string& keyCode);
    bool clearKeyCode();

    // Token 验证
    bool verifyTokenWithServer(const std::string& token);

    // 解析 JWT Token
    bool parseJWT(const std::string& token);

    // 轮询线程函数
    void pollingThread();

    // 成员变量
    AuthConfig m_config;
    std::string m_device_id;
    std::string m_access_token;
    std::string m_session_id;
    std::string m_tg_user_id;
    std::string m_username;
    std::string m_key_code;           // 保存的卡密
    std::string m_key_type;           // 卡密类型
    int64_t m_expires_at = 0;
    int64_t m_key_expires_at = 0;     // 卡密过期时间
    bool m_is_key_auth = false;       // 是否使用卡密登录

    AuthStatus m_status = AuthStatus::NOT_INITIALIZED;
    AuthError m_last_error = AuthError::NONE;
    std::string m_last_error_message;

    std::atomic<bool> m_polling{false};
    std::thread m_poll_thread;
    AuthCallback m_poll_callback;
    mutable std::mutex m_mutex;

    bool m_initialized = false;
};

// 辅助函数：获取状态字符串
const char* getStatusString(AuthStatus status);

// 辅助函数：获取错误字符串
const char* getErrorString(AuthError error);

} // namespace TGAuth
