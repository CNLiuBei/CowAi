#include "cowai_core.h"
#include <iostream>

CowaiCore& CowaiCore::getInstance() {
    static CowaiCore instance;
    return instance;
}

void CowaiCore::registerHotkeyCallback(HotkeyCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_.push_back(callback);
}

void CowaiCore::invokeHotkeyCallback(const char* event, int state) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Call all registered callbacks
    for (auto& callback : callbacks_) {
        if (callback) {
            try {
                callback(event, state);
            }
            catch (const std::exception& e) {
                std::cerr << "Error in hotkey callback: " << e.what() << std::endl;
            }
        }
    }
}

void CowaiCore::clearCallbacks() {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_.clear();
}
