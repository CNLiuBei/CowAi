// Feature: tgauth-system, Property 4: Device ID Determinism
// Validates: Requirements 5.1
//
// Property: For any set of hardware identifiers, the generated device_id 
// SHALL be deterministic - same inputs always produce same outputs.

#include "../device_id.h"
#include <iostream>
#include <string>
#include <set>
#include <cassert>

namespace TGAuth {
namespace Tests {

// 测试 SHA256 确定性
// Property: SHA256(x) == SHA256(x) for all x
void testSHA256Determinism() {
    std::cout << "Testing SHA256 determinism..." << std::endl;
    
    // 测试多个输入
    std::vector<std::string> inputs = {
        "test",
        "hello world",
        "12345678901234567890",
        "",
        "特殊字符测试",
        std::string(1000, 'a'),  // 长字符串
    };
    
    for (const auto& input : inputs) {
        std::string hash1 = DeviceId::sha256(input);
        std::string hash2 = DeviceId::sha256(input);
        
        assert(hash1 == hash2 && "SHA256 should be deterministic");
        assert(hash1.length() == 64 && "SHA256 should produce 64 hex chars");
    }
    
    std::cout << "  PASSED: SHA256 is deterministic" << std::endl;
}

// 测试 SHA256 唯一性 (不同输入产生不同输出)
// Property: SHA256(x) != SHA256(y) for x != y (collision resistance)
void testSHA256Uniqueness() {
    std::cout << "Testing SHA256 uniqueness..." << std::endl;
    
    std::set<std::string> hashes;
    std::vector<std::string> inputs = {
        "test1",
        "test2",
        "Test1",  // 大小写敏感
        "test1 ", // 空格敏感
        " test1",
        "1test",
    };
    
    for (const auto& input : inputs) {
        std::string hash = DeviceId::sha256(input);
        assert(hashes.find(hash) == hashes.end() && "SHA256 should produce unique hashes");
        hashes.insert(hash);
    }
    
    std::cout << "  PASSED: SHA256 produces unique hashes for different inputs" << std::endl;
}

// 测试 Device ID 生成确定性
// Property: generate() returns same value on same hardware
void testDeviceIdDeterminism() {
    std::cout << "Testing Device ID determinism..." << std::endl;
    
    // 多次调用应返回相同结果
    std::string id1 = DeviceId::generate();
    std::string id2 = DeviceId::generate();
    std::string id3 = DeviceId::generate();
    
    assert(!id1.empty() && "Device ID should not be empty");
    assert(id1 == id2 && "Device ID should be deterministic");
    assert(id2 == id3 && "Device ID should be deterministic");
    assert(id1.length() == 64 && "Device ID should be 64 hex chars (SHA256)");
    
    std::cout << "  PASSED: Device ID is deterministic" << std::endl;
    std::cout << "  Generated ID: " << id1.substr(0, 16) << "..." << std::endl;
}

// 测试硬件信息获取函数
void testHardwareInfoFunctions() {
    std::cout << "Testing hardware info functions..." << std::endl;
    
    std::string cpuId = DeviceId::getProcessorId();
    std::string diskSerial = DeviceId::getDiskSerialNumber();
    std::string winGuid = DeviceId::getWindowsGuid();
    
    // 至少有一个应该返回非空值
    bool hasInfo = !cpuId.empty() || !diskSerial.empty() || !winGuid.empty();
    assert(hasInfo && "At least one hardware identifier should be available");
    
    std::cout << "  CPU ID: " << (cpuId.empty() ? "(empty)" : cpuId.substr(0, 16) + "...") << std::endl;
    std::cout << "  Disk Serial: " << (diskSerial.empty() ? "(empty)" : diskSerial) << std::endl;
    std::cout << "  Windows GUID: " << (winGuid.empty() ? "(empty)" : winGuid.substr(0, 16) + "...") << std::endl;
    
    // 多次调用应返回相同结果
    assert(cpuId == DeviceId::getProcessorId() && "CPU ID should be deterministic");
    assert(diskSerial == DeviceId::getDiskSerialNumber() && "Disk serial should be deterministic");
    assert(winGuid == DeviceId::getWindowsGuid() && "Windows GUID should be deterministic");
    
    std::cout << "  PASSED: Hardware info functions are deterministic" << std::endl;
}

// 测试 Device ID 格式
void testDeviceIdFormat() {
    std::cout << "Testing Device ID format..." << std::endl;
    
    std::string id = DeviceId::generate();
    
    // 应该是 64 个十六进制字符
    assert(id.length() == 64 && "Device ID should be 64 characters");
    
    // 所有字符应该是十六进制
    for (char c : id) {
        bool isHex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        assert(isHex && "Device ID should only contain hex characters");
    }
    
    std::cout << "  PASSED: Device ID format is valid" << std::endl;
}

// 属性测试: 组合输入的确定性
// Property: SHA256(a + b + c) is deterministic for any a, b, c
void testCombinedInputDeterminism() {
    std::cout << "Testing combined input determinism..." << std::endl;
    
    // 模拟不同的硬件组合
    std::vector<std::tuple<std::string, std::string, std::string>> testCases = {
        {"cpu1", "disk1", "guid1"},
        {"cpu2", "disk2", "guid2"},
        {"", "disk1", "guid1"},      // 缺少 CPU
        {"cpu1", "", "guid1"},       // 缺少磁盘
        {"cpu1", "disk1", ""},       // 缺少 GUID
    };
    
    for (const auto& [cpu, disk, guid] : testCases) {
        std::string combined = cpu + disk + guid;
        if (combined.empty()) continue;
        
        std::string hash1 = DeviceId::sha256(combined);
        std::string hash2 = DeviceId::sha256(combined);
        
        assert(hash1 == hash2 && "Combined hash should be deterministic");
    }
    
    std::cout << "  PASSED: Combined input hashing is deterministic" << std::endl;
}

// 运行所有测试
void runAllTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "TGAuth Device ID Property Tests" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    testSHA256Determinism();
    testSHA256Uniqueness();
    testDeviceIdDeterminism();
    testHardwareInfoFunctions();
    testDeviceIdFormat();
    testCombinedInputDeterminism();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "All tests PASSED!" << std::endl;
    std::cout << "========================================\n" << std::endl;
}

} // namespace Tests
} // namespace TGAuth

// 主函数
int main() {
    TGAuth::Tests::runAllTests();
    return 0;
}
