#pragma once

#ifdef COWAI_EXPORTS
#define COWAI_API __declspec(dllexport)
#else
#define COWAI_API __declspec(dllimport)
#endif

extern "C" {
    // Hotkey callback function pointer type
    typedef void (*HotkeyCallbackFunc)(const char* event, int state);

    // Initialize the CowAI core
    COWAI_API void CowAI_Initialize();

    // Shutdown the CowAI core
    COWAI_API void CowAI_Shutdown();

    // Register a hotkey callback
    COWAI_API void CowAI_RegisterHotkeyCallback(HotkeyCallbackFunc callback);

    // Get version information
    COWAI_API const char* CowAI_GetVersion();
}
