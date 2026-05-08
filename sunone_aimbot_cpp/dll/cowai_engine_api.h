#pragma once

#ifdef COWAI_EXPORTS
#define COWAI_API __declspec(dllexport)
#else
#define COWAI_API __declspec(dllimport)
#endif

#include <cstdint>

extern "C" {
    // Engine status structure
    struct CowAI_EngineStatus {
        float captureFps;
        float inferenceTimeMs;
        float preprocessTimeMs;
        float postprocessTimeMs;
        int detectionCount;
        bool isRunning;
        bool isPaused;
        char backend[32];
        char captureMethod[32];
        char modelName[256];
    };

    // Detection result structure
    struct CowAI_Detection {
        int x, y, width, height;
        int classId;
        float confidence;
        int trackId;
    };

    // Callback types
    typedef void (*CowAI_StatusCallback)(const CowAI_EngineStatus* status);
    typedef void (*CowAI_DetectionCallback)(const CowAI_Detection* detections, int count, int64_t timestamp);
    typedef void (*CowAI_ErrorCallback)(const char* error);
    typedef void (*CowAI_HotkeyCallback)(const char* event, int state);

    // ============ Engine Lifecycle ============
    
    // Initialize engine with config file path
    COWAI_API bool CowAI_Engine_Initialize(const char* configPath);
    
    // Shutdown engine and release resources
    COWAI_API void CowAI_Engine_Shutdown();
    
    // Start detection loop
    COWAI_API bool CowAI_Engine_Start();
    
    // Stop detection loop
    COWAI_API void CowAI_Engine_Stop();
    
    // Pause detection (keeps resources loaded)
    COWAI_API void CowAI_Engine_Pause();
    
    // Resume detection
    COWAI_API void CowAI_Engine_Resume();
    
    // Check if engine is running
    COWAI_API bool CowAI_Engine_IsRunning();
    
    // Check if engine is paused
    COWAI_API bool CowAI_Engine_IsPaused();

    // ============ Hot Reload ============
    
    // Reload configuration from file
    COWAI_API void CowAI_Engine_ReloadConfig();
    
    // Reload AI model
    COWAI_API void CowAI_Engine_ReloadModel(const char* modelPath);
    
    // Reload capture method
    COWAI_API void CowAI_Engine_ReloadCapture();
    
    // Reload input method (mouse/keyboard)
    COWAI_API void CowAI_Engine_ReloadInputMethod();

    // ============ Callbacks ============
    
    // Register status update callback
    COWAI_API void CowAI_Engine_SetStatusCallback(CowAI_StatusCallback callback);
    
    // Register detection callback
    COWAI_API void CowAI_Engine_SetDetectionCallback(CowAI_DetectionCallback callback);
    
    // Register error callback
    COWAI_API void CowAI_Engine_SetErrorCallback(CowAI_ErrorCallback callback);
    
    // Register hotkey callback
    COWAI_API void CowAI_Engine_SetHotkeyCallback(CowAI_HotkeyCallback callback);

    // ============ Status Query ============
    
    // Get current engine status
    COWAI_API void CowAI_Engine_GetStatus(CowAI_EngineStatus* status);
    
    // Get version string
    COWAI_API const char* CowAI_Engine_GetVersion();

    // ============ Config Access ============
    
    // Save current config to file
    COWAI_API bool CowAI_Config_Save(const char* path);
    
    // Load config from file
    COWAI_API bool CowAI_Config_Load(const char* path);
}
