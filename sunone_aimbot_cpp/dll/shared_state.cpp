#include "shared_state.h"
#include <iostream>
#include <cstring>
#include <chrono>

const wchar_t* SharedStateManager::SHARED_MEMORY_NAME = L"Local\\CowAI_SharedState_v2";
const wchar_t* SharedStateManager::SEMAPHORE_NAME = L"Local\\CowAI_Semaphore_v2";

// Global instance
SharedStateManager g_sharedState;

SharedStateManager::SharedStateManager() = default;

SharedStateManager::~SharedStateManager() {
    cleanup();
}

bool SharedStateManager::initialize() {
    if (m_initialized) {
        return true;
    }
    
    // Create semaphore
    m_hSemaphore = CreateSemaphoreW(nullptr, 1, 1, SEMAPHORE_NAME);
    if (!m_hSemaphore) {
        std::cerr << "[SharedState] Failed to create semaphore: " << GetLastError() << std::endl;
        return false;
    }
    
    // Create shared memory
    m_hMapFile = CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        sizeof(SharedEngineState),
        SHARED_MEMORY_NAME
    );
    
    if (!m_hMapFile) {
        std::cerr << "[SharedState] Failed to create file mapping: " << GetLastError() << std::endl;
        CloseHandle(m_hSemaphore);
        m_hSemaphore = nullptr;
        return false;
    }
    
    // Map view
    m_pState = static_cast<SharedEngineState*>(
        MapViewOfFile(m_hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedEngineState))
    );
    
    if (!m_pState) {
        std::cerr << "[SharedState] Failed to map view: " << GetLastError() << std::endl;
        CloseHandle(m_hMapFile);
        CloseHandle(m_hSemaphore);
        m_hMapFile = nullptr;
        m_hSemaphore = nullptr;
        return false;
    }
    
    // Initialize state
    lock();
    memset(m_pState, 0, sizeof(SharedEngineState));
    m_pState->magic = 0x434F5741;
    m_pState->version = 1;
    unlock();
    
    m_initialized = true;
    std::cout << "[SharedState] Initialized successfully" << std::endl;
    return true;
}

void SharedStateManager::cleanup() {
    if (m_pState) {
        UnmapViewOfFile(m_pState);
        m_pState = nullptr;
    }
    if (m_hMapFile) {
        CloseHandle(m_hMapFile);
        m_hMapFile = nullptr;
    }
    if (m_hSemaphore) {
        CloseHandle(m_hSemaphore);
        m_hSemaphore = nullptr;
    }
    m_initialized = false;
}

void SharedStateManager::lock() {
    if (m_hSemaphore) {
        WaitForSingleObject(m_hSemaphore, INFINITE);
    }
}

void SharedStateManager::unlock() {
    if (m_hSemaphore) {
        ReleaseSemaphore(m_hSemaphore, 1, nullptr);
    }
}

void SharedStateManager::updateStatus(float fps, float inferenceMs, float preprocessMs, float postprocessMs,
                                       int detections, bool running, bool paused) {
    if (!m_initialized || !m_pState) return;
    
    lock();
    m_pState->captureFps = fps;
    m_pState->inferenceTimeMs = inferenceMs;
    m_pState->preprocessTimeMs = preprocessMs;
    m_pState->postprocessTimeMs = postprocessMs;
    m_pState->detectionCount = detections;
    m_pState->isRunning = running;
    m_pState->isPaused = paused;
    m_pState->timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    unlock();
}

void SharedStateManager::updateBackendInfo(const std::string& backend, const std::string& captureMethod, 
                                            const std::string& modelName) {
    if (!m_initialized || !m_pState) return;
    
    lock();
    strncpy_s(m_pState->backend, sizeof(m_pState->backend), backend.c_str(), _TRUNCATE);
    strncpy_s(m_pState->captureMethod, sizeof(m_pState->captureMethod), captureMethod.c_str(), _TRUNCATE);
    strncpy_s(m_pState->modelName, sizeof(m_pState->modelName), modelName.c_str(), _TRUNCATE);
    unlock();
}

void SharedStateManager::updateDetections(const SharedEngineState::Detection* dets, int count) {
    if (!m_initialized || !m_pState || !dets) return;
    
    lock();
    int copyCount = (count > SharedEngineState::MAX_DETECTIONS) ? SharedEngineState::MAX_DETECTIONS : count;
    m_pState->numDetections = copyCount;
    memcpy(m_pState->detections, dets, copyCount * sizeof(SharedEngineState::Detection));
    unlock();
}

bool SharedStateManager::checkRequestStart() {
    if (!m_initialized || !m_pState) return false;
    
    lock();
    bool result = m_pState->requestStart;
    m_pState->requestStart = false;
    unlock();
    return result;
}

bool SharedStateManager::checkRequestStop() {
    if (!m_initialized || !m_pState) return false;
    
    lock();
    bool result = m_pState->requestStop;
    m_pState->requestStop = false;
    unlock();
    return result;
}

bool SharedStateManager::checkRequestPause() {
    if (!m_initialized || !m_pState) return false;
    
    lock();
    bool result = m_pState->requestPause;
    m_pState->requestPause = false;
    unlock();
    return result;
}

bool SharedStateManager::checkRequestResume() {
    if (!m_initialized || !m_pState) return false;
    
    lock();
    bool result = m_pState->requestResume;
    m_pState->requestResume = false;
    unlock();
    return result;
}

bool SharedStateManager::checkRequestReloadConfig() {
    if (!m_initialized || !m_pState) return false;
    
    lock();
    bool result = m_pState->requestReloadConfig;
    m_pState->requestReloadConfig = false;
    unlock();
    return result;
}

bool SharedStateManager::checkRequestReloadModel(std::string& outModelPath) {
    if (!m_initialized || !m_pState) return false;
    
    lock();
    bool result = m_pState->requestReloadModel;
    if (result) {
        outModelPath = m_pState->newModelPath;
    }
    m_pState->requestReloadModel = false;
    unlock();
    return result;
}

bool SharedStateManager::checkRequestReloadCapture() {
    if (!m_initialized || !m_pState) return false;
    
    lock();
    bool result = m_pState->requestReloadCapture;
    m_pState->requestReloadCapture = false;
    unlock();
    return result;
}

bool SharedStateManager::checkRequestReloadInput() {
    if (!m_initialized || !m_pState) return false;
    
    lock();
    bool result = m_pState->requestReloadInput;
    m_pState->requestReloadInput = false;
    unlock();
    return result;
}

void SharedStateManager::updatePreviewFrame(const uint8_t* rgbData, int width, int height) {
    if (!m_initialized || !m_pState || !rgbData) return;
    if (width <= 0 || height <= 0) return;
    
    lock();
    
    // Store original dimensions for aspect ratio info
    m_pState->frameWidth = PREVIEW_WIDTH;
    m_pState->frameHeight = PREVIEW_HEIGHT;
    m_pState->frameTimestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Simple nearest-neighbor resize to PREVIEW_WIDTH x PREVIEW_HEIGHT
    float scaleX = static_cast<float>(width) / PREVIEW_WIDTH;
    float scaleY = static_cast<float>(height) / PREVIEW_HEIGHT;
    
    for (int y = 0; y < PREVIEW_HEIGHT; y++) {
        int srcY = static_cast<int>(y * scaleY);
        if (srcY >= height) srcY = height - 1;
        
        for (int x = 0; x < PREVIEW_WIDTH; x++) {
            int srcX = static_cast<int>(x * scaleX);
            if (srcX >= width) srcX = width - 1;
            
            int srcIdx = (srcY * width + srcX) * 3;
            int dstIdx = (y * PREVIEW_WIDTH + x) * 3;
            
            m_pState->frameData[dstIdx + 0] = rgbData[srcIdx + 0]; // R
            m_pState->frameData[dstIdx + 1] = rgbData[srcIdx + 1]; // G
            m_pState->frameData[dstIdx + 2] = rgbData[srcIdx + 2]; // B
        }
    }
    
    unlock();
}
