#include "cowai_api.h"
#include "cowai_core.h"
#include <string>

static const char* COWAI_VERSION = "1.0.0";

extern "C" {
    void CowAI_Initialize() {
        // Initialize the core system
        auto& core = CowaiCore::getInstance();
        core.clearCallbacks();
    }

    void CowAI_Shutdown() {
        // Cleanup
        auto& core = CowaiCore::getInstance();
        core.clearCallbacks();
    }

    void CowAI_RegisterHotkeyCallback(HotkeyCallbackFunc callback) {
        if (callback) {
            auto& core = CowaiCore::getInstance();
            core.registerHotkeyCallback([callback](const char* event, int state) {
                callback(event, state);
            });
        }
    }

    const char* CowAI_GetVersion() {
        return COWAI_VERSION;
    }
}
