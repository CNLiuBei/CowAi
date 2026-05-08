#pragma once

#include <string>
#include <functional>
#include <map>
#include <mutex>

// CowaiCore - Core class for managing hotkey callbacks and DLL functionality
class CowaiCore {
public:
    // Callback type: void callback(const char* event, int state)
    using HotkeyCallback = std::function<void(const char*, int)>;

    // Get singleton instance
    static CowaiCore& getInstance();

    // Register a hotkey callback
    void registerHotkeyCallback(HotkeyCallback callback);

    // Invoke hotkey callback
    void invokeHotkeyCallback(const char* event, int state);

    // Clear all callbacks
    void clearCallbacks();

private:
    CowaiCore() = default;
    ~CowaiCore() = default;

    // Prevent copying
    CowaiCore(const CowaiCore&) = delete;
    CowaiCore& operator=(const CowaiCore&) = delete;

    std::vector<HotkeyCallback> callbacks_;
    std::mutex mutex_;
};
