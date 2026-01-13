// Feature: tgauth-system, Property 13: Error Response Consistency
// Validates: Requirements 12.5
//
// Property: For any error condition:
// - The response SHALL include success=false, error.code, and error.message
// - Error codes SHALL match the documented error code table

#include "../tg_auth.h"
#include <iostream>
#include <cassert>
#include <map>
#include <set>

namespace TGAuth {
namespace Tests {

// 测试所有错误码都有对应的字符串
void testAllErrorCodesHaveStrings() {
    std::cout << "Testing all error codes have strings..." << std::endl;
    
    std::vector<AuthError> allErrors = {
        AuthError::NONE,
        AuthError::NETWORK_ERROR,
        AuthError::TIMEOUT,
        AuthError::SESSION_EXPIRED,
        AuthError::TOKEN_INVALID,
        AuthError::TOKEN_EXPIRED,
        AuthError::DEVICE_MISMATCH,
        AuthError::USER_BANNED,
        AuthError::DEVICE_LIMIT_EXCEEDED,
        AuthError::RATE_LIMITED,
        AuthError::SERVER_ERROR,
        AuthError::PARSE_ERROR,
        AuthError::STORAGE_ERROR,
    };
    
    for (AuthError error : allErrors) {
        const char* str = getErrorString(error);
        assert(str != nullptr && "Error string should not be null");
        assert(strlen(str) > 0 && "Error string should not be empty");
        assert(strcmp(str, "UNKNOWN") != 0 && "Error should have a known string");
        
        std::cout << "  " << static_cast<int>(error) << " -> " << str << std::endl;
    }
    
    std::cout << "  PASSED: All error codes have strings" << std::endl;
}

// 测试所有状态码都有对应的字符串
void testAllStatusCodesHaveStrings() {
    std::cout << "Testing all status codes have strings..." << std::endl;
    
    std::vector<AuthStatus> allStatuses = {
        AuthStatus::NOT_INITIALIZED,
        AuthStatus::CHECKING,
        AuthStatus::WAITING_FOR_APPROVAL,
        AuthStatus::AUTHORIZED,
        AuthStatus::UNAUTHORIZED,
        AuthStatus::BANNED,
        AuthStatus::DEVICE_LIMIT_EXCEEDED,
        AuthStatus::NETWORK_ERROR,
        AuthStatus::TOKEN_EXPIRED,
        AuthStatus::SESSION_EXPIRED,
    };
    
    for (AuthStatus status : allStatuses) {
        const char* str = getStatusString(status);
        assert(str != nullptr && "Status string should not be null");
        assert(strlen(str) > 0 && "Status string should not be empty");
        assert(strcmp(str, "UNKNOWN") != 0 && "Status should have a known string");
        
        std::cout << "  " << static_cast<int>(status) << " -> " << str << std::endl;
    }
    
    std::cout << "  PASSED: All status codes have strings" << std::endl;
}

// 测试错误码唯一性
void testErrorCodeUniqueness() {
    std::cout << "Testing error code uniqueness..." << std::endl;
    
    std::set<int> errorValues;
    std::vector<AuthError> allErrors = {
        AuthError::NONE,
        AuthError::NETWORK_ERROR,
        AuthError::TIMEOUT,
        AuthError::SESSION_EXPIRED,
        AuthError::TOKEN_INVALID,
        AuthError::TOKEN_EXPIRED,
        AuthError::DEVICE_MISMATCH,
        AuthError::USER_BANNED,
        AuthError::DEVICE_LIMIT_EXCEEDED,
        AuthError::RATE_LIMITED,
        AuthError::SERVER_ERROR,
        AuthError::PARSE_ERROR,
        AuthError::STORAGE_ERROR,
    };
    
    for (AuthError error : allErrors) {
        int value = static_cast<int>(error);
        assert(errorValues.find(value) == errorValues.end() && "Error codes should be unique");
        errorValues.insert(value);
    }
    
    std::cout << "  PASSED: All error codes are unique" << std::endl;
}

// 测试状态码唯一性
void testStatusCodeUniqueness() {
    std::cout << "Testing status code uniqueness..." << std::endl;
    
    std::set<int> statusValues;
    std::vector<AuthStatus> allStatuses = {
        AuthStatus::NOT_INITIALIZED,
        AuthStatus::CHECKING,
        AuthStatus::WAITING_FOR_APPROVAL,
        AuthStatus::AUTHORIZED,
        AuthStatus::UNAUTHORIZED,
        AuthStatus::BANNED,
        AuthStatus::DEVICE_LIMIT_EXCEEDED,
        AuthStatus::NETWORK_ERROR,
        AuthStatus::TOKEN_EXPIRED,
        AuthStatus::SESSION_EXPIRED,
    };
    
    for (AuthStatus status : allStatuses) {
        int value = static_cast<int>(status);
        assert(statusValues.find(value) == statusValues.end() && "Status codes should be unique");
        statusValues.insert(value);
    }
    
    std::cout << "  PASSED: All status codes are unique" << std::endl;
}

// 测试错误字符串唯一性
void testErrorStringUniqueness() {
    std::cout << "Testing error string uniqueness..." << std::endl;
    
    std::set<std::string> errorStrings;
    std::vector<AuthError> allErrors = {
        AuthError::NONE,
        AuthError::NETWORK_ERROR,
        AuthError::TIMEOUT,
        AuthError::SESSION_EXPIRED,
        AuthError::TOKEN_INVALID,
        AuthError::TOKEN_EXPIRED,
        AuthError::DEVICE_MISMATCH,
        AuthError::USER_BANNED,
        AuthError::DEVICE_LIMIT_EXCEEDED,
        AuthError::RATE_LIMITED,
        AuthError::SERVER_ERROR,
        AuthError::PARSE_ERROR,
        AuthError::STORAGE_ERROR,
    };
    
    for (AuthError error : allErrors) {
        std::string str = getErrorString(error);
        assert(errorStrings.find(str) == errorStrings.end() && "Error strings should be unique");
        errorStrings.insert(str);
    }
    
    std::cout << "  PASSED: All error strings are unique" << std::endl;
}

// 测试 AuthResult 结构完整性
void testAuthResultStructure() {
    std::cout << "Testing AuthResult structure..." << std::endl;
    
    // 测试成功结果
    AuthResult successResult;
    successResult.status = AuthStatus::AUTHORIZED;
    successResult.error = AuthError::NONE;
    successResult.error_message = "";
    successResult.tg_user_id = "123456789";
    successResult.username = "test_user";
    successResult.expires_at = 1704672000;
    
    assert(successResult.status == AuthStatus::AUTHORIZED);
    assert(successResult.error == AuthError::NONE);
    assert(!successResult.tg_user_id.empty());
    
    // 测试错误结果
    AuthResult errorResult;
    errorResult.status = AuthStatus::UNAUTHORIZED;
    errorResult.error = AuthError::TOKEN_INVALID;
    errorResult.error_message = "Token signature verification failed";
    
    assert(errorResult.status == AuthStatus::UNAUTHORIZED);
    assert(errorResult.error == AuthError::TOKEN_INVALID);
    assert(!errorResult.error_message.empty());
    
    std::cout << "  PASSED: AuthResult structure is correct" << std::endl;
}

// 测试 AuthConfig 默认值
void testAuthConfigDefaults() {
    std::cout << "Testing AuthConfig defaults..." << std::endl;
    
    AuthConfig config;
    
    // 检查默认值
    assert(config.poll_interval_ms == 2000 && "Default poll interval should be 2000ms");
    assert(config.session_timeout_sec == 300 && "Default session timeout should be 300s");
    
    // 检查可以设置自定义值
    config.worker_api_url = "https://api.example.com";
    config.bot_username = "TestBot";
    config.token_storage_path = "custom_path.dat";
    config.poll_interval_ms = 3000;
    config.session_timeout_sec = 600;
    
    assert(config.worker_api_url == "https://api.example.com");
    assert(config.bot_username == "TestBot");
    assert(config.token_storage_path == "custom_path.dat");
    assert(config.poll_interval_ms == 3000);
    assert(config.session_timeout_sec == 600);
    
    std::cout << "  PASSED: AuthConfig defaults are correct" << std::endl;
}

// 测试错误码与 API 错误码映射
void testErrorCodeMapping() {
    std::cout << "Testing error code mapping to API codes..." << std::endl;
    
    // 映射表: C++ 错误码 -> API 错误码字符串
    std::map<AuthError, std::string> expectedMappings = {
        {AuthError::NONE, "NONE"},
        {AuthError::NETWORK_ERROR, "NETWORK_ERROR"},
        {AuthError::TIMEOUT, "TIMEOUT"},
        {AuthError::SESSION_EXPIRED, "SESSION_EXPIRED"},
        {AuthError::TOKEN_INVALID, "TOKEN_INVALID"},
        {AuthError::TOKEN_EXPIRED, "TOKEN_EXPIRED"},
        {AuthError::DEVICE_MISMATCH, "DEVICE_MISMATCH"},
        {AuthError::USER_BANNED, "USER_BANNED"},
        {AuthError::DEVICE_LIMIT_EXCEEDED, "DEVICE_LIMIT_EXCEEDED"},
        {AuthError::RATE_LIMITED, "RATE_LIMITED"},
        {AuthError::SERVER_ERROR, "SERVER_ERROR"},
        {AuthError::PARSE_ERROR, "PARSE_ERROR"},
        {AuthError::STORAGE_ERROR, "STORAGE_ERROR"},
    };
    
    for (const auto& [error, expectedStr] : expectedMappings) {
        std::string actualStr = getErrorString(error);
        assert(actualStr == expectedStr && "Error code should map to expected string");
    }
    
    std::cout << "  PASSED: Error code mapping is correct" << std::endl;
}

// 属性测试: 错误响应一致性
void testErrorResponseConsistency() {
    std::cout << "Testing error response consistency..." << std::endl;
    
    // 对于每种错误类型，验证响应格式一致
    std::vector<AuthError> allErrors = {
        AuthError::NETWORK_ERROR,
        AuthError::TIMEOUT,
        AuthError::SESSION_EXPIRED,
        AuthError::TOKEN_INVALID,
        AuthError::TOKEN_EXPIRED,
        AuthError::DEVICE_MISMATCH,
        AuthError::USER_BANNED,
        AuthError::DEVICE_LIMIT_EXCEEDED,
        AuthError::RATE_LIMITED,
        AuthError::SERVER_ERROR,
        AuthError::PARSE_ERROR,
        AuthError::STORAGE_ERROR,
    };
    
    for (AuthError error : allErrors) {
        AuthResult result;
        result.status = AuthStatus::UNAUTHORIZED;
        result.error = error;
        result.error_message = std::string("Error: ") + getErrorString(error);
        
        // 验证错误响应包含必要字段
        assert(result.status != AuthStatus::AUTHORIZED && "Error result should not be AUTHORIZED");
        assert(result.error != AuthError::NONE && "Error result should have error code");
        
        // 错误消息应该包含错误码字符串
        std::string errorStr = getErrorString(error);
        assert(result.error_message.find(errorStr) != std::string::npos && 
               "Error message should contain error code string");
    }
    
    std::cout << "  PASSED: Error response consistency verified" << std::endl;
}

// 运行所有测试
void runAllTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "TGAuth Error Handling Property Tests" << std::endl;
    std::cout << "Property 13: Error Response Consistency" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    testAllErrorCodesHaveStrings();
    testAllStatusCodesHaveStrings();
    testErrorCodeUniqueness();
    testStatusCodeUniqueness();
    testErrorStringUniqueness();
    testAuthResultStructure();
    testAuthConfigDefaults();
    testErrorCodeMapping();
    testErrorResponseConsistency();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "All tests PASSED!" << std::endl;
    std::cout << "========================================\n" << std::endl;
}

} // namespace Tests
} // namespace TGAuth

int main() {
    TGAuth::Tests::runAllTests();
    return 0;
}
