#define COWAI_EXPORTS
#include "cowai_engine_api.h"
#include "config_mapping.h"
#include "../config/config.h"
#include "../capture/capture.h"
#include "../detector/detection_buffer.h"
#include "../mouse/mouse.h"
#include "../tracker/ByteTracker.h"
#include "../keyboard/keyboard_listener.h"

#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>
#include <cstring>

// External globals from sunone_aimbot_cpp.cpp
extern Config config;
extern MouseThread* globalMouseThread;
extern ByteTracker* byteTracker;
extern DetectionBuffer detectionBuffer;
extern std::atomic<bool> shouldExit;
extern std::atomic<bool> aiming;
extern std::atomic<bool> detectionPaused;
extern std::atomic<bool> detection_resolution_changed;
extern std::atomic<bool> capture_method_changed;
extern std::atomic<bool> detector_model_changed;
extern std::atomic<bool> input_method_changed;
extern std::atomic<int> captureFps;
extern std::mutex configMutex;
extern std::mutex trackerMutex;

// Engine state
static std::atomic<bool> g_engineInitialized{false};
static std::atomic<bool> g_engineRunning{false};
static std::atomic<bool> g_enginePaused{false};
static std::thread g_captureThread;
static std::thread g_detectionThread;
static std::thread g_mouseThread;

// Callbacks
static CowAI_StatusCallback g_statusCallback = nullptr;
static CowAI_DetectionCallback g_detectionCallback = nullptr;
static CowAI_ErrorCallback g_errorCallback = nullptr;
static CowAI_HotkeyCallback g_hotkeyCallback = nullptr;
static std::mutex g_callbackMutex;

// Version
static const char* COWAI_ENGINE_VERSION = "2.0.0";

// Helper to copy string safely
static void safeCopyString(char* dest, size_t destSize, const std::string& src) {
    strncpy_s(dest, destSize, src.c_str(), _TRUNCATE);
}

// Status update thread
static std::thread g_statusThread;
static std::atomic<bool> g_statusThreadRunning{false};

static void statusUpdateLoop() {
    while (g_statusThreadRunning.load()) {
        if (g_statusCallback) {
            CowAI_EngineStatus status = {};
            CowAI_Engine_GetStatus(&status);
            
            std::lock_guard<std::mutex> lock(g_callbackMutex);
            if (g_statusCallback) {
                g_statusCallback(&status);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// Detection callback thread
static std::thread g_detectionCallbackThread;
static std::atomic<bool> g_detectionCallbackRunning{false};

static void detectionCallbackLoop() {
    int lastVersion = -1;
    
    while (g_detectionCallbackRunning.load()) {
        std::vector<cv::Rect> boxes;
        std::vector<int> classes;
        std::vector<float> scores;
        int version;
        
        {
            std::unique_lock<std::mutex> lock(detectionBuffer.mutex);
            if (detectionBuffer.cv.wait_for(lock, std::chrono::milliseconds(100), [&] {
                return detectionBuffer.version > lastVersion || !g_detectionCallbackRunning.load();
            })) {
                if (!g_detectionCallbackRunning.load()) break;
                
                boxes = detectionBuffer.boxes;
                classes = detectionBuffer.classes;
                scores = detectionBuffer.scores;
                version = detectionBuffer.version;
                lastVersion = version;
            } else {
                continue;
            }
        }
        
        if (g_detectionCallback && !boxes.empty()) {
            std::vector<CowAI_Detection> detections;
            detections.reserve(boxes.size());
            
            for (size_t i = 0; i < boxes.size(); i++) {
                CowAI_Detection det = {};
                det.x = boxes[i].x;
                det.y = boxes[i].y;
                det.width = boxes[i].width;
                det.height = boxes[i].height;
                det.classId = (i < classes.size()) ? classes[i] : 0;
                det.confidence = (i < scores.size()) ? scores[i] : 0.0f;
                det.trackId = -1;
                detections.push_back(det);
            }
            
            std::lock_guard<std::mutex> lock(g_callbackMutex);
            if (g_detectionCallback) {
                g_detectionCallback(detections.data(), (int)detections.size(), 
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count());
            }
        }
    }
}

extern "C" {

bool CowAI_Engine_Initialize(const char* configPath) {
    if (g_engineInitialized.load()) {
        return true;
    }
    
    try {
        // Load config
        std::string path = configPath ? configPath : "config.ini";
        if (!config.loadConfig(path)) {
            if (g_errorCallback) {
                g_errorCallback("Failed to load config file");
            }
            return false;
        }
        
        g_engineInitialized.store(true);
        return true;
    }
    catch (const std::exception& e) {
        if (g_errorCallback) {
            g_errorCallback(e.what());
        }
        return false;
    }
}

void CowAI_Engine_Shutdown() {
    CowAI_Engine_Stop();
    
    // Cleanup
    g_engineInitialized.store(false);
}

bool CowAI_Engine_Start() {
    if (!g_engineInitialized.load()) {
        if (g_errorCallback) {
            g_errorCallback("Engine not initialized");
        }
        return false;
    }
    
    if (g_engineRunning.load()) {
        return true;
    }
    
    shouldExit.store(false);
    g_engineRunning.store(true);
    g_enginePaused.store(false);
    
    // Start status update thread
    g_statusThreadRunning.store(true);
    g_statusThread = std::thread(statusUpdateLoop);
    
    // Start detection callback thread
    g_detectionCallbackRunning.store(true);
    g_detectionCallbackThread = std::thread(detectionCallbackLoop);
    
    return true;
}

void CowAI_Engine_Stop() {
    if (!g_engineRunning.load()) {
        return;
    }
    
    shouldExit.store(true);
    g_engineRunning.store(false);
    g_enginePaused.store(false);
    
    // Stop status thread
    g_statusThreadRunning.store(false);
    if (g_statusThread.joinable()) {
        g_statusThread.join();
    }
    
    // Stop detection callback thread
    g_detectionCallbackRunning.store(false);
    detectionBuffer.cv.notify_all();
    if (g_detectionCallbackThread.joinable()) {
        g_detectionCallbackThread.join();
    }
}

void CowAI_Engine_Pause() {
    g_enginePaused.store(true);
    detectionPaused.store(true);
}

void CowAI_Engine_Resume() {
    g_enginePaused.store(false);
    detectionPaused.store(false);
}

bool CowAI_Engine_IsRunning() {
    return g_engineRunning.load();
}

bool CowAI_Engine_IsPaused() {
    return g_enginePaused.load();
}

void CowAI_Engine_ReloadConfig() {
    std::lock_guard<std::mutex> lock(configMutex);
    config.loadConfig("config.ini");
    detection_resolution_changed.store(true);
}

void CowAI_Engine_ReloadModel(const char* modelPath) {
    if (modelPath) {
        std::lock_guard<std::mutex> lock(configMutex);
        config.ai_model = modelPath;
    }
    detector_model_changed.store(true);
}

void CowAI_Engine_ReloadCapture() {
    capture_method_changed.store(true);
}

void CowAI_Engine_ReloadInputMethod() {
    input_method_changed.store(true);
}

void CowAI_Engine_SetStatusCallback(CowAI_StatusCallback callback) {
    std::lock_guard<std::mutex> lock(g_callbackMutex);
    g_statusCallback = callback;
}

void CowAI_Engine_SetDetectionCallback(CowAI_DetectionCallback callback) {
    std::lock_guard<std::mutex> lock(g_callbackMutex);
    g_detectionCallback = callback;
}

void CowAI_Engine_SetErrorCallback(CowAI_ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(g_callbackMutex);
    g_errorCallback = callback;
}

void CowAI_Engine_SetHotkeyCallback(CowAI_HotkeyCallback callback) {
    std::lock_guard<std::mutex> lock(g_callbackMutex);
    g_hotkeyCallback = callback;
}

void CowAI_Engine_GetStatus(CowAI_EngineStatus* status) {
    if (!status) return;
    
    memset(status, 0, sizeof(CowAI_EngineStatus));
    
    status->captureFps = static_cast<float>(captureFps.load());
    status->isRunning = g_engineRunning.load();
    status->isPaused = g_enginePaused.load();
    
    {
        std::lock_guard<std::mutex> lock(detectionBuffer.mutex);
        status->detectionCount = static_cast<int>(detectionBuffer.boxes.size());
    }
    
    {
        std::lock_guard<std::mutex> lock(configMutex);
        safeCopyString(status->backend, sizeof(status->backend), config.backend);
        safeCopyString(status->captureMethod, sizeof(status->captureMethod), config.capture_method);
        safeCopyString(status->modelName, sizeof(status->modelName), config.ai_model);
    }
}

const char* CowAI_Engine_GetVersion() {
    return COWAI_ENGINE_VERSION;
}

bool CowAI_Config_Save(const char* path) {
    std::lock_guard<std::mutex> lock(configMutex);
    return config.saveConfig(path ? path : "config.ini");
}

bool CowAI_Config_Load(const char* path) {
    std::lock_guard<std::mutex> lock(configMutex);
    return config.loadConfig(path ? path : "config.ini");
}

} // extern "C"
