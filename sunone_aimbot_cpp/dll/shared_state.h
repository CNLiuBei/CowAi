#pragma once

#include <Windows.h>
#include <cstdint>
#include <string>
#include <mutex>

// Preview frame dimensions (must match qt_panel/engine/shared_memory.h)
constexpr int PREVIEW_WIDTH = 320;
constexpr int PREVIEW_HEIGHT = 180;
constexpr int PREVIEW_CHANNELS = 3; // RGB
constexpr int PREVIEW_SIZE = PREVIEW_WIDTH * PREVIEW_HEIGHT * PREVIEW_CHANNELS;

// Shared memory structure for IPC between Qt Panel and CowAI Engine
// Must match qt_panel/engine/shared_memory.h exactly!
#pragma pack(push, 1)
struct SharedEngineState {
    // Magic number for validation
    uint32_t magic = 0x434F5741; // "COWA"
    uint32_t version = 2; // Bumped version for frame support
    
    // Engine status
    float captureFps;
    float inferenceTimeMs;
    float preprocessTimeMs;
    float postprocessTimeMs;
    int32_t detectionCount;
    bool isRunning;
    bool isPaused;
    
    // Backend info
    char backend[32];
    char captureMethod[32];
    char modelName[256];
    
    // Control flags (Panel -> Engine)
    bool requestStart;
    bool requestStop;
    bool requestPause;
    bool requestResume;
    bool requestReloadConfig;
    bool requestReloadModel;
    bool requestReloadCapture;
    bool requestReloadInput;
    
    // New model path for reload
    char newModelPath[256];
    
    // Detection results (Engine -> Panel)
    static const int MAX_DETECTIONS = 64;
    int32_t numDetections;
    struct Detection {
        int32_t x, y, width, height;
        int32_t classId;
        float confidence;
        int32_t trackId;
    } detections[MAX_DETECTIONS];
    
    // Timestamp
    int64_t timestamp;
    
    // Preview frame data (320x180 RGB)
    uint32_t frameWidth;
    uint32_t frameHeight;
    uint64_t frameTimestamp;
    uint8_t frameData[PREVIEW_SIZE];
    
    // Padding for future use
    uint8_t reserved[128];
};
#pragma pack(pop)

class SharedStateManager {
public:
    SharedStateManager();
    ~SharedStateManager();
    
    // Initialize shared memory (create)
    bool initialize();
    
    // Cleanup
    void cleanup();
    
    // Check if initialized
    bool isInitialized() const { return m_initialized; }
    
    // Update engine status
    void updateStatus(float fps, float inferenceMs, float preprocessMs, float postprocessMs,
                      int detections, bool running, bool paused);
    
    // Update backend info
    void updateBackendInfo(const std::string& backend, const std::string& captureMethod, 
                           const std::string& modelName);
    
    // Update detections
    void updateDetections(const SharedEngineState::Detection* dets, int count);
    
    // Check and clear control flags
    bool checkRequestStart();
    bool checkRequestStop();
    bool checkRequestPause();
    bool checkRequestResume();
    bool checkRequestReloadConfig();
    bool checkRequestReloadModel(std::string& outModelPath);
    bool checkRequestReloadCapture();
    bool checkRequestReloadInput();
    
    // Update preview frame (resized to 320x180 RGB)
    void updatePreviewFrame(const uint8_t* rgbData, int width, int height);

private:
    HANDLE m_hMapFile = nullptr;
    HANDLE m_hSemaphore = nullptr;
    SharedEngineState* m_pState = nullptr;
    bool m_initialized = false;
    
    static const wchar_t* SHARED_MEMORY_NAME;
    static const wchar_t* SEMAPHORE_NAME;
    
    void lock();
    void unlock();
};

// Global instance
extern SharedStateManager g_sharedState;
