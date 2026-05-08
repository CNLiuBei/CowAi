#pragma once

#include <string>
#include <cstdint>

namespace KeyValidatorGUI {

// 验证结果
enum class ValidationResult {
    RESULT_SUCCESS,         // 验证成功
    RESULT_CANCELLED,       // 用户取消
    RESULT_KEY_INVALID,     // 卡密无效
    RESULT_KEY_EXPIRED,     // 卡密过期
    RESULT_KEY_REVOKED,     // 卡密已撤销
    RESULT_DEVICE_LIMIT,    // 设备数量超限
    RESULT_NETWORK_ERROR,   // 网络错误
    RESULT_ERROR            // 其他错误
};

// 验证信息
struct ValidationInfo {
    ValidationResult result;
    std::string key_type;       // "time" 或 "permanent"
    int64_t expires_at;         // 过期时间戳 (0 = 永久)
    std::string error_message;
    
    ValidationInfo() : result(ValidationResult::RESULT_ERROR), expires_at(0) {}
};

// 显示卡密验证窗口
// 返回 true 表示验证成功，false 表示失败或取消
// info 输出验证信息
bool ShowKeyValidatorWindow(ValidationInfo& info);

// 检查是否已有有效授权（跳过验证窗口）
bool HasValidAuthorization();

} // namespace KeyValidatorGUI
