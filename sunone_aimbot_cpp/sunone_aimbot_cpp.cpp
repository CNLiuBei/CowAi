#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <winsock2.h>
#include <Windows.h>
#include <shellapi.h>
#include <DbgHelp.h>  // For crash dump

#include <iostream>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <array>
#include <random>
#include <filesystem>
#include <cmath>
#include <limits>
#include <locale>
#include <io.h>
#include <fcntl.h>
#include <timeapi.h>

#include "capture.h"
#include "mouse.h"
#include "sunone_aimbot_cpp.h"
#include "keyboard_listener.h"
#include "overlay.h"
#include "Game_overlay.h"
#include "ghub.h"
#include "other_tools.h"
#include "virtual_camera.h"
#include "mem/gpu_resource_manager.h"
#include "mem/cpu_affinity_manager.h"
#include "ByteTracker.h"
#include "auth/tg_auth.h"
#include "auth/key_validator_gui.h"
#include "dll/shared_state.h"
#include "version/version.h"
#include "pickup/auto_pickup.h"
#include "weapon/recoil_data.h"

#include <wincodec.h>
#include <wrl/client.h>
#include <comdef.h>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "winmm.lib")

using Microsoft::WRL::ComPtr;

// Global crash handler
LONG WINAPI CrashHandler(EXCEPTION_POINTERS* pExceptionInfo)
{
    const char* exceptionName = "Unknown";
    DWORD code = pExceptionInfo->ExceptionRecord->ExceptionCode;
    
    switch (code)
    {
    case EXCEPTION_ACCESS_VIOLATION: exceptionName = "ACCESS_VIOLATION"; break;
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: exceptionName = "ARRAY_BOUNDS_EXCEEDED"; break;
    case EXCEPTION_STACK_OVERFLOW: exceptionName = "STACK_OVERFLOW"; break;
    case EXCEPTION_FLT_DIVIDE_BY_ZERO: exceptionName = "FLT_DIVIDE_BY_ZERO"; break;
    case EXCEPTION_INT_DIVIDE_BY_ZERO: exceptionName = "INT_DIVIDE_BY_ZERO"; break;
    case EXCEPTION_ILLEGAL_INSTRUCTION: exceptionName = "ILLEGAL_INSTRUCTION"; break;
    case EXCEPTION_IN_PAGE_ERROR: exceptionName = "IN_PAGE_ERROR"; break;
    case EXCEPTION_INVALID_DISPOSITION: exceptionName = "INVALID_DISPOSITION"; break;
    case EXCEPTION_NONCONTINUABLE_EXCEPTION: exceptionName = "NONCONTINUABLE_EXCEPTION"; break;
    case EXCEPTION_PRIV_INSTRUCTION: exceptionName = "PRIV_INSTRUCTION"; break;
    }
    
    // Get crash address
    void* crashAddr = pExceptionInfo->ExceptionRecord->ExceptionAddress;
    
    std::cerr << "\n========== CRASH DETECTED ==========" << std::endl;
    std::cerr << "Exception: " << exceptionName << " (0x" << std::hex << code << ")" << std::endl;
    std::cerr << "Address: 0x" << std::hex << (uintptr_t)crashAddr << std::endl;
    
    if (code == EXCEPTION_ACCESS_VIOLATION && pExceptionInfo->ExceptionRecord->NumberParameters >= 2)
    {
        ULONG_PTR accessType = pExceptionInfo->ExceptionRecord->ExceptionInformation[0];
        ULONG_PTR accessAddr = pExceptionInfo->ExceptionRecord->ExceptionInformation[1];
        std::cerr << "Access type: " << (accessType == 0 ? "READ" : (accessType == 1 ? "WRITE" : "EXECUTE")) << std::endl;
        std::cerr << "Accessed address: 0x" << std::hex << accessAddr << std::endl;
    }
    
    std::cerr << "=====================================" << std::endl;
    std::cerr << "Press Enter to exit..." << std::endl;
    std::cin.get();
    
    return EXCEPTION_EXECUTE_HANDLER;
}

std::condition_variable frameCV;
std::atomic<bool> shouldExit(false);
std::atomic<bool> aiming(false);
std::atomic<bool> detectionPaused(false);
std::atomic<bool> norecoilActive(false);
std::mutex configMutex;

#ifdef USE_CUDA
TrtDetector trt_detector;
#else
DirectMLDetector* dml_detector = nullptr;
#endif

MouseThread* globalMouseThread = nullptr;
Config config;

// ByteTracker instance
ByteTracker* byteTracker = nullptr;
std::mutex trackerMutex;

Game_overlay* gameOverlayPtr = nullptr;
std::thread gameOverlayThread;
std::atomic<bool> gameOverlayShouldExit(false);

GhubMouse* gHub = nullptr;
SerialConnection* arduinoSerial = nullptr;
Kmbox_b_Connection* kmboxSerial = nullptr;
KmboxNetConnection* kmboxNetSerial = nullptr;
MakcuConnection* makcuSerial = nullptr;

// Auto Pickup instance
AutoPickup* globalAutoPickup = nullptr;

// Weapon Recognition instance
WeaponRecognizer* globalWeaponRecognizer = nullptr;

// Recoil Controller instance
RecoilController* globalRecoilController = nullptr;

std::atomic<bool> detection_resolution_changed(false);
std::atomic<bool> capture_method_changed(false);
std::atomic<bool> capture_cursor_changed(false);
std::atomic<bool> capture_borders_changed(false);
std::atomic<bool> capture_fps_changed(false);
std::atomic<bool> capture_window_changed(false);
std::atomic<bool> detector_model_changed(false);
std::atomic<bool> show_window_changed(false);
std::atomic<bool> input_method_changed(false);

std::atomic<bool> zooming(false);
std::atomic<bool> shooting(false);

// Weapon slot tracking (主副武器槽位): 0=未知, 1=主武器, 2=副武器
std::atomic<int> currentWeaponSlot(1);  // 默认主武器

static std::string g_lastIconPath;
static int g_iconImageId = 0;
static std::mutex g_iconMutex;

std::string g_iconLastError;

static std::string hr_to_string(HRESULT hr)
{
    _com_error err(hr);
    const wchar_t* ws = err.ErrorMessage();
    int len = WideCharToMultiByte(CP_UTF8, 0, ws, -1, nullptr, 0, nullptr, nullptr);
    std::string s(len > 0 ? len - 1 : 0, '\0');
    if (len > 0) WideCharToMultiByte(CP_UTF8, 0, ws, -1, s.data(), len, nullptr, nullptr);
    return s;
}

static bool IsValidImageFile(const std::wstring& wpath, UINT& outW, UINT& outH, std::string& outErr)
{
    outW = outH = 0;
    outErr.clear();

    static std::once_flag coinit_flag;
    static HRESULT coinit_hr = S_OK;
    std::call_once(coinit_flag, [] {
        coinit_hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        });

    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) { outErr = "WIC factory error: " + hr_to_string(hr); return false; }

    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(wpath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) { outErr = "DecoderFromFilename failed: " + hr_to_string(hr); return false; }

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) { outErr = "GetFrame(0) failed: " + hr_to_string(hr); return false; }

    UINT w = 0, h = 0;
    hr = frame->GetSize(&w, &h);
    if (FAILED(hr)) { outErr = "GetSize failed: " + hr_to_string(hr); return false; }

    const UINT MAX_DIM = 16384;
    if (w == 0 || h == 0 || w > MAX_DIM || h > MAX_DIM)
    {
        outErr = "Invalid image size: " + std::to_string(w) + "x" + std::to_string(h);
        return false;
    }

    ComPtr<IWICFormatConverter> conv;
    hr = factory->CreateFormatConverter(&conv);
    if (FAILED(hr)) { outErr = "CreateFormatConverter failed: " + hr_to_string(hr); return false; }

    hr = conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) { outErr = "Converter Initialize failed: " + hr_to_string(hr); return false; }

    const UINT probe_rows = (std::min)(h, 8u);
    std::vector<uint8_t> probe;
    probe.resize((size_t)w * probe_rows * 4);
    WICRect rect{ 0, 0, (INT)w, (INT)probe_rows };
    hr = conv->CopyPixels(&rect, (UINT)(w * 4), (UINT)probe.size(), probe.data());
    if (FAILED(hr)) { outErr = "CopyPixels failed: " + hr_to_string(hr); return false; }

    outW = w; outH = h;
    return true;
}

inline void SetRandomConsoleTitle()
{
    static constexpr std::array<const wchar_t*, 10> kTitles = {
        L"Microsoft Edge",
        L"Google Chrome",
        L"Notepad",
        L"Windows Terminal",
        L"PowerShell",
        L"Visual Studio Code",
        L"Task Manager",
        L"File Explorer",
        L"Calculator",
        L"Command Prompt",
    };

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dist(0, kTitles.size() - 1);

    ::SetConsoleTitleW(kTitles[dist(gen)]);
}

// 创建并初始化控制台窗口（用于 Windows 子系统应用）
inline void CreateAndInitConsole()
{
    // 先尝试附加到父进程的控制台（从命令行启动时）
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
    {
        // 如果失败，创建新控制台
        AllocConsole();
    }
    
    // 重定向标准输入输出到控制台
    FILE* fp = nullptr;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    freopen_s(&fp, "CONIN$", "r", stdin);
    
    // 设置控制台编码为 UTF-8
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    setlocale(LC_ALL, ".UTF-8");
    
    // 启用虚拟终端处理以支持 ANSI 转义序列
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
    
    // 设置随机控制台标题
    SetRandomConsoleTitle();
    
    // 同步 C++ 流
    std::ios::sync_with_stdio(true);
    std::cout.clear();
    std::cerr.clear();
    std::cin.clear();
}

void createInputDevices()
{
    if (arduinoSerial)
    {
        delete arduinoSerial;
        arduinoSerial = nullptr;
    }

    if (gHub)
    {
        gHub->mouse_close();
        delete gHub;
        gHub = nullptr;
    }

    if (kmboxSerial)
    {
        delete kmboxSerial;
        kmboxSerial = nullptr;
    }

    if (kmboxNetSerial)
    {
        delete kmboxNetSerial;
        kmboxNetSerial = nullptr;
    }

    if (makcuSerial)
    {
        delete makcuSerial;
        makcuSerial = nullptr;
    }

    if (config.input_method == "ARDUINO")
    {
        std::cout << "[鼠标] 使用 Arduino 输入方式" << std::endl;
        arduinoSerial = new SerialConnection(config.arduino_port, config.arduino_baudrate);
    }
    else if (config.input_method == "GHUB")
    {
        std::cout << "[鼠标] 使用 Ghub 输入方式" << std::endl;
        gHub = new GhubMouse();
        // 测试移动来验证是否工作
        if (!gHub->mouse_xy(0, 0))
        {
            std::cerr << "[Ghub] 打开鼠标失败，回退到 Win32" << std::endl;
            delete gHub;
            gHub = nullptr;
        }
        else
        {
            std::cout << "[Ghub] 初始化成功" << std::endl;
        }
    }
    else if (config.input_method == "KMBOX_B")
    {
        std::cout << "[鼠标] 使用 KMBOX_B 输入方式" << std::endl;
        kmboxSerial = new Kmbox_b_Connection(config.kmbox_b_port, config.kmbox_b_baudrate);
        if (!kmboxSerial->isOpen())
        {
            std::cerr << "[Kmbox] 连接 Kmbox 串口失败" << std::endl;
            delete kmboxSerial;
            kmboxSerial = nullptr;
        }
    }
    else if (config.input_method == "KMBOX_NET")
    {
        std::cout << "[鼠标] 使用 KMBOX_NET 输入方式" << std::endl;
        kmboxNetSerial = new KmboxNetConnection(config.kmbox_net_ip, config.kmbox_net_port, config.kmbox_net_uuid);
        if (!kmboxNetSerial->isOpen())
        {
            std::cerr << "[KmboxNet] 连接失败" << std::endl;
            delete kmboxNetSerial; kmboxNetSerial = nullptr;
        }
    }
    else if (config.input_method == "MAKCU")
    {
        std::cout << "[鼠标] 使用 MAKCU 输入方式" << std::endl;
        makcuSerial = new MakcuConnection(config.makcu_port, config.makcu_baudrate);
        if (!makcuSerial->isOpen())
        {
            std::cerr << "[Makcu] 连接失败" << std::endl;
            delete makcuSerial;
            makcuSerial = nullptr;
        }
    }
    else
    {
        std::cout << "[鼠标] 使用默认 Win32 输入方式" << std::endl;
    }
}

void assignInputDevices()
{
    if (globalMouseThread)
    {
        globalMouseThread->setSerialConnection(arduinoSerial);
        globalMouseThread->setGHubMouse(gHub);
        globalMouseThread->setKmboxConnection(kmboxSerial);
        globalMouseThread->setKmboxNetConnection(kmboxNetSerial);
        globalMouseThread->setMakcuConnection(makcuSerial);
    }
}

void handleEasyNoRecoil(MouseThread& mouseThread)
{
    // 调试：每500ms输出一次压枪状态
    static auto lastDebug = std::chrono::steady_clock::now();
    auto nowDbg = std::chrono::steady_clock::now();
    bool doDebug = std::chrono::duration_cast<std::chrono::milliseconds>(nowDbg - lastDebug).count() >= 500;

    // 压枪触发条件：easynorecoil 开启 + norecoilActive（右键+左键 或 单独压枪键）
    if (config.easynorecoil && norecoilActive.load())
    {
        int recoil_x = 0;
        int recoil_y = 0;

        // 优先使用弹道压枪（武器识别开启且有有效武器数据时）
        bool useBallistic = false;
        if (config.weapon_recognition_enabled && globalRecoilController && globalWeaponRecognizer
            && globalWeaponRecognizer->isRunning())
        {
            auto [dx, dy] = globalRecoilController->computeRecoil(
                globalWeaponRecognizer->getState(), currentWeaponSlot.load(), true);
            if (doDebug) {
                int slot = currentWeaponSlot.load();
                WeaponInfo winfo = (slot == 2)
                    ? globalWeaponRecognizer->getState().getSecondary()
                    : globalWeaponRecognizer->getState().getPrimary();
                std::cout << "[RECOIL-DBG] weapon=" << winfo.weaponName
                          << " hasActive=" << globalRecoilController->hasActiveWeapon()
                          << " bullet=" << globalRecoilController->getBulletCount()
                          << " dx=" << dx << " dy=" << dy << std::endl;
            }
            if (globalRecoilController->hasActiveWeapon()) {
                useBallistic = true;
                recoil_x = dx;
                recoil_y = dy;
            }
        }
        else if (doDebug) {
            std::cout << "[RECOIL-DBG] SKIP ballistic: wr_enabled=" << config.weapon_recognition_enabled
                      << " ctrl=" << (globalRecoilController != nullptr)
                      << " recog=" << (globalWeaponRecognizer != nullptr)
                      << " running=" << (globalWeaponRecognizer ? globalWeaponRecognizer->isRunning() : false) << std::endl;
        }

        // 没有弹道数据时回退到简易固定压枪
        if (!useBallistic) {
            recoil_y = static_cast<int>(config.easynorecoilstrength);
            if (doDebug) std::cout << "[RECOIL-DBG] fallback easy=" << recoil_y << std::endl;
        }

        if (doDebug) lastDebug = nowDbg;

        if (recoil_x == 0 && recoil_y == 0) return;

        // Use queueMove to ensure recoil compensation is tracked in accumulated movement
        std::lock_guard<std::mutex> lock(mouseThread.input_method_mutex);
        
        if (arduinoSerial)
        {
            arduinoSerial->move(recoil_x, recoil_y);
        }
        else if (gHub)
        {
            gHub->mouse_xy(recoil_x, recoil_y);
        }
        else if (kmboxSerial)
        {
            kmboxSerial->move(recoil_x, recoil_y);
        }
        else if (kmboxNetSerial)
        {
            kmboxNetSerial->move(recoil_x, recoil_y);
        }
        else if (makcuSerial)
        {
            makcuSerial->move(recoil_x, recoil_y);
        }
        else
        {
            INPUT input = { 0 };
            input.type = INPUT_MOUSE;
            input.mi.dx = recoil_x;
            input.mi.dy = recoil_y;
            input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_VIRTUALDESK;
            SendInput(1, &input, sizeof(INPUT));
        }
        
        // Track recoil movement in accumulated mouse movement
        mouseThread.addAccumulatedMovement(recoil_x, recoil_y);
    }
    else
    {
        // 松开射击时重置弹道计数
        if (globalRecoilController) {
            globalRecoilController->reset();
        }
    }

    // === 始终连点逻辑（always_click_weapons）===
    // 与压枪同条件触发：norecoilActive（右键+左键）
    static std::chrono::steady_clock::time_point clickLastTime;
    static bool clickActive = false;

    if (config.easynorecoil && norecoilActive.load()
        && config.weapon_recognition_enabled && globalWeaponRecognizer
        && globalWeaponRecognizer->isRunning())
    {
        int slot = currentWeaponSlot.load();
        WeaponInfo winfo = (slot == 2)
            ? globalWeaponRecognizer->getState().getSecondary()
            : globalWeaponRecognizer->getState().getPrimary();

        if (!winfo.weaponName.empty() && winfo.weaponName != "None"
            && RecoilDatabase::instance().isAlwaysClickWeapon(winfo.weaponName))
        {
            auto now = std::chrono::steady_clock::now();
            if (!clickActive)
            {
                clickLastTime = now;
                clickActive = true;
                std::cout << "[AlwaysClick] Activated for weapon: " << winfo.weaponName << std::endl;
            }
            else
            {
                int weaponInterval = RecoilDatabase::instance().getWeaponInterval(winfo.weaponName);
                int clickDelay = RecoilDatabase::instance().getClickDelay(winfo.weaponName);
                float clickInterval = (clickDelay > 0) ? static_cast<float>(clickDelay)
                                    : (weaponInterval > 0) ? static_cast<float>(weaponInterval)
                                    : config.auto_shoot_interval;
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - clickLastTime).count();
                if (elapsed >= clickInterval)
                {
                    std::cout << "[AlwaysClick] ForceClick! interval=" << clickInterval << "ms" << std::endl;
                    mouseThread.forceClick();
                    clickLastTime = now;
                }
            }
        }
        else
        {
            clickActive = false;
        }
    }
    else
    {
        clickActive = false;
    }
}

// 独立压枪线程：高频轮询（每5ms），不受YOLO帧率影响
void recoilThreadFunction()
{
    timeBeginPeriod(1);  // 提高定时器精度到1ms
    while (!shouldExit)
    {
        if (globalMouseThread) {
            handleEasyNoRecoil(*globalMouseThread);
        }
        // 弹道数据热重载
        RecoilDatabase::instance().checkAndReload();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    timeEndPeriod(1);
}

void mouseThreadFunction(MouseThread& mouseThread)
{
    int lastVersion = -1;
    static int debugCounter = 0;

    while (!shouldExit)
    {
        std::vector<cv::Rect> boxes;
        std::vector<int> classes;
        std::vector<float> scores;

        {
            std::unique_lock<std::mutex> lock(detectionBuffer.mutex);
            detectionBuffer.cv.wait(lock, [&] {
                return detectionBuffer.version > lastVersion || shouldExit;
                }
            );

            if (shouldExit) break;
            boxes = detectionBuffer.boxes;
            classes = detectionBuffer.classes;
            scores = detectionBuffer.scores;
            lastVersion = detectionBuffer.version;
        }

        // Debug output every 100 frames (disabled for release)
        // debugCounter++;
        // if (debugCounter % 100 == 0)
        // {
        //     std::cout << "[DEBUG] Frame " << debugCounter << ", detections: " << boxes.size() << std::endl;
        // }

        // Update shared state for Qt Panel IPC
        if (g_sharedState.isInitialized())
        {
            // Update status
            float inferenceMs = 0.0f;
            float preprocessMs = 0.0f;
            float postprocessMs = 0.0f;
            
#ifdef USE_CUDA
            inferenceMs = static_cast<float>(trt_detector.lastInferenceTime.count());
            preprocessMs = static_cast<float>(trt_detector.lastPreprocessTime.count());
            postprocessMs = static_cast<float>(trt_detector.lastPostprocessTime.count());
#else
            if (dml_detector) {
                inferenceMs = static_cast<float>(dml_detector->lastInferenceTimeDML.count());
                preprocessMs = static_cast<float>(dml_detector->lastPreprocessTimeDML.count());
                postprocessMs = static_cast<float>(dml_detector->lastPostprocessTimeDML.count());
            }
#endif
            
            g_sharedState.updateStatus(
                static_cast<float>(captureFps.load()),
                inferenceMs,
                preprocessMs,
                postprocessMs,
                static_cast<int>(boxes.size()),
                !shouldExit.load(),
                detectionPaused.load()
            );
            
            // Update detections
            if (!boxes.empty())
            {
                std::vector<SharedEngineState::Detection> sharedDets;
                sharedDets.reserve(boxes.size());
                for (size_t i = 0; i < boxes.size(); i++)
                {
                    SharedEngineState::Detection det;
                    det.x = boxes[i].x;
                    det.y = boxes[i].y;
                    det.width = boxes[i].width;
                    det.height = boxes[i].height;
                    det.classId = (i < classes.size()) ? classes[i] : 0;
                    det.confidence = (i < scores.size()) ? scores[i] : 0.0f;
                    det.trackId = -1;
                    sharedDets.push_back(det);
                }
                g_sharedState.updateDetections(sharedDets.data(), static_cast<int>(sharedDets.size()));
            }
            
            // Update preview frame for Qt Panel (throttled to ~10 FPS)
            static auto lastFrameUpdate = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFrameUpdate).count() >= 100)
            {
                lastFrameUpdate = now;
                std::lock_guard<std::mutex> lk(frameMutex);
                if (!latestFrame.empty())
                {
                    // Convert BGR to RGB for Qt
                    cv::Mat rgb;
                    cv::cvtColor(latestFrame, rgb, cv::COLOR_BGR2RGB);
                    g_sharedState.updatePreviewFrame(rgb.data, rgb.cols, rgb.rows);
                }
            }
            
            // Check control requests from Qt Panel
            if (g_sharedState.checkRequestPause())
            {
                detectionPaused.store(true);
            }
            if (g_sharedState.checkRequestResume())
            {
                detectionPaused.store(false);
            }
            if (g_sharedState.checkRequestReloadConfig())
            {
                std::lock_guard<std::mutex> cfgLock(configMutex);
                config.loadConfig();
                detection_resolution_changed.store(true);
            }
            std::string newModelPath;
            if (g_sharedState.checkRequestReloadModel(newModelPath))
            {
                if (!newModelPath.empty())
                {
                    std::lock_guard<std::mutex> cfgLock(configMutex);
                    config.ai_model = newModelPath;
                    detector_model_changed.store(true);
                }
            }
            if (g_sharedState.checkRequestReloadCapture())
            {
                capture_method_changed.store(true);
            }
            if (g_sharedState.checkRequestReloadInput())
            {
                input_method_changed.store(true);
            }
        }

        if (input_method_changed.load())
        {
            createInputDevices();
            assignInputDevices();
            input_method_changed.store(false);
        }

        if (detection_resolution_changed.load())
        {
            {
                std::lock_guard<std::mutex> cfgLock(configMutex);
                mouseThread.updateConfig(
                    config.detection_resolution,
                    config.fovX,
                    config.fovY,
                    config.minSpeedMultiplier,
                    config.maxSpeedMultiplier,
                    config.predictionInterval,
                    config.auto_shoot,
                    config.bScope_multiplier
                );
            }
            
            // Reset ByteTracker on resolution change
            if (byteTracker)
            {
                std::lock_guard<std::mutex> lock(trackerMutex);
                byteTracker->reset();
            }
            
            detection_resolution_changed.store(false);
        }

        // Apply ByteTrack if enabled
        std::vector<cv::Rect> trackedBoxes;
        std::vector<int> trackedClasses;
        std::vector<int> trackedIds;
        std::vector<std::pair<float, float>> trackedVelocities;
        
        if (config.enable_bytetrack && byteTracker && !boxes.empty())
        {
            std::vector<TrackerDetection> detections;
            for (size_t i = 0; i < boxes.size(); i++)
            {
                TrackerDetection det;
                det.rect = boxes[i];
                det.classId = classes[i];
                det.score = (i < scores.size()) ? scores[i] : config.confidence_threshold;
                detections.push_back(det);
            }
            
            std::vector<TrackedObject> tracked;
            {
                std::lock_guard<std::mutex> lock(trackerMutex);
                tracked = byteTracker->update(detections);
            }
            
            if (!tracked.empty())
            {
                for (const auto& obj : tracked)
                {
                    // Use predicted position for smoother tracking
                    trackedBoxes.push_back(obj.predictedRect);
                    trackedClasses.push_back(obj.classId);
                    trackedIds.push_back(obj.trackId);
                    trackedVelocities.push_back({obj.velocityX, obj.velocityY});
                }
            }
            else
            {
                // ByteTrack returned no results, fallback to raw detections
                trackedBoxes = boxes;
                trackedClasses = classes;
                // No trackIds available - target lock won't work without ByteTrack
                trackedIds.clear();
                trackedVelocities.clear();
            }
        }
        else
        {
            trackedBoxes = boxes;
            trackedClasses = classes;
            // No trackIds available - target lock won't work without ByteTrack
            trackedIds.clear();
            trackedVelocities.clear();
        }

        // Target lock state
        static int lockedTrackId = -1;
        static int lockedBodyTrackId = -1;  // Remember associated body when locked to head
        static int lockLostFrames = 0;
        static bool wasAiming = false;
        
        // Last known target position for brief disappearance handling
        static double lastPivotX = 0.0;
        static double lastPivotY = 0.0;
        static double lastVelX = 0.0;
        static double lastVelY = 0.0;
        static bool hasLastPosition = false;
        
        bool isAiming = aiming.load() || (config.auto_shoot && shooting.load());
        bool aimJustPressed = isAiming && !wasAiming;
        
        // Determine if target lock should be enabled based on weapon slot
        // 根据当前武器槽位决定是否启用目标锁定
        bool weaponLockEnabled = true;
        int weaponSlot = currentWeaponSlot.load();
        if (weaponSlot == 1) {
            weaponLockEnabled = config.weapon_primary_lock_enabled;
        } else if (weaponSlot == 2) {
            weaponLockEnabled = config.weapon_secondary_lock_enabled;
        }
        
        // Final target lock state: global setting AND weapon-specific setting
        bool effectiveTargetLock = config.enable_target_lock && weaponLockEnabled;
        
        // Reset lock when aim key is pressed (re-acquire target)
        if (aimJustPressed && effectiveTargetLock)
        {
            lockedTrackId = -1;
            lockedBodyTrackId = -1;
            lockLostFrames = 0;
        }
        
        int currentLockedId = (effectiveTargetLock && config.enable_bytetrack && !trackedIds.empty()) ? lockedTrackId : -1;
        
        auto target = sortTargets(
            trackedBoxes,
            trackedClasses,
            trackedIds,
            config.detection_resolution,
            config.detection_resolution,
            config.disable_headshot,
            currentLockedId,
            config.target_lock_threshold,
            lockedBodyTrackId,
            trackedVelocities.empty() ? nullptr : &trackedVelocities
        );

        if (target)
        {
            // Save last known position
            lastPivotX = target->pivotX;
            lastPivotY = target->pivotY;
            lastVelX = target->velocityX;
            lastVelY = target->velocityY;
            hasLastPosition = true;
            
            // Update locked target
            if (effectiveTargetLock && config.enable_bytetrack && isAiming)
            {
                if (lockedTrackId < 0)
                {
                    // No locked target, lock to current
                    lockedTrackId = target->trackId;
                    lockedBodyTrackId = target->associatedBodyTrackId;
                    lockLostFrames = 0;
                }
                else if (target->trackId == lockedTrackId)
                {
                    // Still tracking locked target
                    lockLostFrames = 0;
                    // Update body association if we have a better one now
                    if (target->associatedBodyTrackId >= 0 && lockedBodyTrackId < 0)
                    {
                        lockedBodyTrackId = target->associatedBodyTrackId;
                    }
                }
                else if (target->trackId >= 0)
                {
                    // sortTargets returned a different target (intentional switch, e.g., body->head)
                    // Update lock immediately without waiting
                    lockedTrackId = target->trackId;
                    // Remember associated body if switching to head
                    if (target->associatedBodyTrackId >= 0)
                    {
                        lockedBodyTrackId = target->associatedBodyTrackId;
                    }
                    lockLostFrames = 0;
                }
                else
                {
                    // Target has no valid trackId, count as lost
                    lockLostFrames++;
                    if (lockLostFrames > config.target_lock_hold_frames)
                    {
                        lockedTrackId = -1;
                        lockedBodyTrackId = -1;
                        lockLostFrames = 0;
                    }
                }
            }
            
            mouseThread.setLastTargetTime(std::chrono::steady_clock::now());
            mouseThread.setTargetDetected(true);

            auto futurePositions = mouseThread.predictFuturePositions(
                target->pivotX,
                target->pivotY,
                config.prediction_futurePositions
            );
            mouseThread.storeFuturePositions(futurePositions);
        }
        else
        {
            // No target detected this frame
            if (effectiveTargetLock && isAiming)
            {
                lockLostFrames++;
                if (lockLostFrames > config.target_lock_hold_frames)
                {
                    // Target lost for too long, reset lock
                    lockedTrackId = -1;
                    lockedBodyTrackId = -1;
                    lockLostFrames = 0;
                    hasLastPosition = false;
                    // Only clear positions when truly lost
                    mouseThread.clearFuturePositions();
                    mouseThread.setTargetDetected(false);
                }
                // else: Keep last position during brief disappearance (don't clear)
            }
            else
            {
                // Not aiming or lock disabled, clear immediately
                hasLastPosition = false;
                mouseThread.clearFuturePositions();
                mouseThread.setTargetDetected(false);
            }
        }
        
        // Reset lock when not aiming
        if (!isAiming && effectiveTargetLock)
        {
            lockedTrackId = -1;
            lockedBodyTrackId = -1;
            lockLostFrames = 0;
            hasLastPosition = false;
        }
        
        wasAiming = isAiming;

        // Auto shoot logic
        static std::chrono::steady_clock::time_point auto_shoot_last_fire;
        static bool auto_shoot_active = false;
        static int on_target_frames = 0;  // Count consecutive frames on target
        const int MIN_ON_TARGET_FRAMES = 1;  // Require 1 frame on target before firing (fast response)

        // Check if aim assist is enabled for current weapon slot
        // 武器槽位设置只影响手动瞄准，不影响自动射击
        bool isManualAiming = aiming.load();  // 手动瞄准（右键）
        bool isAutoShootAiming = config.auto_shoot && shooting.load();  // 自动射击瞄准
        
        // 手动瞄准时检查武器槽位设置，自动射击始终启用
        bool weaponAimEnabled = isAutoShootAiming || (isManualAiming && weaponLockEnabled);

        if (isAiming && weaponAimEnabled)
        {
            if (target)
            {
                // Pass shooting state for crosshair offset compensation
                // 根据武器类型判断是否应用准星偏移：步枪和机枪需要，其他不用
                // 检测实际开枪状态：button_shoot(自动射击) 或 左键按下(手动射击)
                bool currentlyShooting = shooting.load() || (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
                bool useOffset = false;
                if (currentlyShooting && config.weapon_recognition_enabled && globalWeaponRecognizer && globalWeaponRecognizer->isRunning())
                {
                    int slot = currentWeaponSlot.load();
                    WeaponInfo winfo = (slot == 2)
                        ? globalWeaponRecognizer->getState().getSecondary()
                        : globalWeaponRecognizer->getState().getPrimary();
                    if (!winfo.weaponName.empty() && winfo.weaponName != "None")
                    {
                        useOffset = RecoilDatabase::instance().isOffsetWeapon(winfo.weaponName);
                    }
                }
                mouseThread.moveMousePivot(target->pivotX, target->pivotY, target->velocityX, target->velocityY, useOffset);

                if (config.auto_shoot && shooting)
                {
                    // 自动开枪：头身都能触发开枪
                    // 分别找最近的身体目标和头部目标
                    double centerX = config.detection_resolution / 2.0;
                    double centerY = config.detection_resolution / 2.0;
                    
                    int bodyIdx = -1, headIdx = -1;
                    double nearestBodyDist = std::numeric_limits<double>::max();
                    double nearestHeadDist = std::numeric_limits<double>::max();
                    
                    for (size_t i = 0; i < trackedBoxes.size(); i++)
                    {
                        double bx = trackedBoxes[i].x + trackedBoxes[i].width / 2.0;
                        if (trackedClasses[i] == config.class_player)
                        {
                            double by = trackedBoxes[i].y + trackedBoxes[i].height * config.body_y_offset;
                            double dist = std::sqrt((bx - centerX) * (bx - centerX) + (by - centerY) * (by - centerY));
                            if (dist < nearestBodyDist) { nearestBodyDist = dist; bodyIdx = static_cast<int>(i); }
                        }
                        else if (trackedClasses[i] == config.class_head)
                        {
                            double by = trackedBoxes[i].y + trackedBoxes[i].height * config.head_y_offset;
                            double dist = std::sqrt((bx - centerX) * (bx - centerX) + (by - centerY) * (by - centerY));
                            if (dist < nearestHeadDist) { nearestHeadDist = dist; headIdx = static_cast<int>(i); }
                        }
                    }
                    
                    // 构建候选列表：身体优先，头部其次，最后用当前target
                    struct AutoShootCandidate {
                        double pivotX, pivotY, w, h;
                        int classId;
                    };
                    std::vector<AutoShootCandidate> candidates;
                    
                    if (bodyIdx >= 0)
                    {
                        double w = static_cast<double>(trackedBoxes[bodyIdx].width);
                        double h = static_cast<double>(trackedBoxes[bodyIdx].height);
                        candidates.push_back({
                            trackedBoxes[bodyIdx].x + w / 2.0,
                            trackedBoxes[bodyIdx].y + h * config.body_y_offset,
                            w, h, config.class_player
                        });
                    }
                    if (headIdx >= 0)
                    {
                        double w = static_cast<double>(trackedBoxes[headIdx].width);
                        double h = static_cast<double>(trackedBoxes[headIdx].height);
                        candidates.push_back({
                            trackedBoxes[headIdx].x + w / 2.0,
                            trackedBoxes[headIdx].y + h * config.head_y_offset,
                            w, h, config.class_head
                        });
                    }
                    if (candidates.empty())
                    {
                        candidates.push_back({
                            target->pivotX, target->pivotY,
                            static_cast<double>(target->w), static_cast<double>(target->h), target->classId
                        });
                    }

                    // 遍历候选目标，任一在准星上即开枪
                    bool anyOnTarget = false;
                    for (auto& cand : candidates)
                    {
                        // 自动开枪独立视野范围检查
                        double autoFovPx = config.auto_shoot_fov;
                        double fdx = std::abs(cand.pivotX - centerX);
                        double fdy = std::abs(cand.pivotY - centerY);
                        if (fdx > autoFovPx || fdy > autoFovPx) continue;

                        double detectW = cand.w;
                        double detectH = cand.h;
                        if (cand.classId == config.class_head)
                        {
                            detectW = std::max(detectW * 2.0, 40.0);
                            detectH = std::max(detectH * 2.0, 40.0);
                        }
                        
                        if (mouseThread.check_pivot_in_scope(cand.pivotX, cand.pivotY, detectW, detectH, config.bScope_multiplier))
                        {
                            anyOnTarget = true;
                            break;
                        }
                    }

                    if (anyOnTarget)
                    {
                        on_target_frames++;
                        if (on_target_frames >= MIN_ON_TARGET_FRAMES)
                        {
                            // 获取当前武器名，判断是否为单发武器或始终连点武器
                            bool isSingleFire = false;
                            bool isAlwaysClick = false;
                            if (config.weapon_recognition_enabled && globalWeaponRecognizer && globalWeaponRecognizer->isRunning())
                            {
                                int slot = currentWeaponSlot.load();
                                WeaponInfo winfo = (slot == 2)
                                    ? globalWeaponRecognizer->getState().getSecondary()
                                    : globalWeaponRecognizer->getState().getPrimary();
                                if (!winfo.weaponName.empty() && winfo.weaponName != "None")
                                {
                                    isSingleFire = RecoilDatabase::instance().isSingleFireWeapon(winfo.weaponName);
                                    isAlwaysClick = RecoilDatabase::instance().isAlwaysClickWeapon(winfo.weaponName);
                                }
                            }
                            bool needClick = isSingleFire || isAlwaysClick;

                            auto now = std::chrono::steady_clock::now();
                            if (!auto_shoot_active)
                            {
                                mouseThread.pressMouseDirect();
                                auto_shoot_last_fire = now;
                                auto_shoot_active = true;
                            }
                            else if (needClick)
                            {
                                // 始终连点武器使用武器自身射击间隔，单发武器使用配置间隔
                                float clickInterval = config.auto_shoot_interval;
                                if (isAlwaysClick && !isSingleFire)
                                {
                                    int weaponInterval = RecoilDatabase::instance().getWeaponInterval(
                                        [&]() -> std::string {
                                            int slot = currentWeaponSlot.load();
                                            WeaponInfo winfo = (slot == 2)
                                                ? globalWeaponRecognizer->getState().getSecondary()
                                                : globalWeaponRecognizer->getState().getPrimary();
                                            return winfo.weaponName;
                                        }());
                                    if (weaponInterval > 0) clickInterval = static_cast<float>(weaponInterval);
                                }
                                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - auto_shoot_last_fire).count();
                                if (elapsed >= clickInterval)
                                {
                                    mouseThread.releaseMouse();
                                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                                    mouseThread.pressMouseDirect();
                                    auto_shoot_last_fire = now;
                                }
                            }
                        }
                    }
                    else
                    {
                        on_target_frames = 0;
                        if (auto_shoot_active)
                        {
                            mouseThread.releaseMouse();
                            auto_shoot_active = false;
                        }
                    }
                }
                else
                {
                    // Shooting key released or auto_shoot disabled
                    on_target_frames = 0;
                    if (auto_shoot_active)
                    {
                        mouseThread.releaseMouse();
                        auto_shoot_active = false;
                    }
                }
            }
            else
            {
                // Target not detected this frame - stop moving
                on_target_frames = 0;
                
                if (config.auto_shoot && auto_shoot_active)
                {
                    mouseThread.releaseMouse();
                    auto_shoot_active = false;
                }
            }
        }
        else
        {
            on_target_frames = 0;
            if (config.auto_shoot && auto_shoot_active)
            {
                mouseThread.releaseMouse();
                auto_shoot_active = false;
            }
        }

        mouseThread.checkAndResetPredictions();
    }
}

static void gameOverlayRenderLoop()
{
    const int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);

    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    HMONITOR hPrimary = MonitorFromPoint(POINT{ 0,0 }, MONITOR_DEFAULTTOPRIMARY);
    GetMonitorInfo(hPrimary, &mi);
    RECT pr = mi.rcMonitor;
    const int pw = pr.right - pr.left;
    const int ph = pr.bottom - pr.top;

    const int offX = pr.left - vx;
    const int offY = pr.top - vy;

    while (!gameOverlayShouldExit.load())
    {
        // Game Overlay 在以下任一条件满足时启用：
        // 1. game_overlay_enabled 手动开启
        // 2. weapon_recognition_enabled 武器识别开启（用于显示武器名HUD）
        bool needOverlay = config.game_overlay_enabled || config.weapon_recognition_enabled;

        if (!needOverlay)
        {
            if (gameOverlayPtr)
            {
                gameOverlayPtr->Stop();
                delete gameOverlayPtr;
                gameOverlayPtr = nullptr;
                std::lock_guard<std::mutex> lk(g_iconMutex);
                g_lastIconPath.clear();
                g_iconImageId = 0;
                g_iconLastError.clear();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            continue;
        }

        if (!gameOverlayPtr)
        {
            gameOverlayPtr = new Game_overlay();
            gameOverlayPtr->SetWindowBounds(0, 0, pw, ph);
            gameOverlayPtr->SetMaxFPS(config.game_overlay_max_fps > 0 ? (unsigned)config.game_overlay_max_fps : 0);
            gameOverlayPtr->Start();
        }
        else if (!gameOverlayPtr->IsRunning())
        {
            gameOverlayPtr->SetWindowBounds(0, 0, pw, ph);
            gameOverlayPtr->SetMaxFPS(config.game_overlay_max_fps > 0 ? (unsigned)config.game_overlay_max_fps : 0);
            gameOverlayPtr->Start();
        }

        if (!gameOverlayPtr || !gameOverlayPtr->IsRunning())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            continue;
        }

        gameOverlayPtr->SetMaxFPS(config.game_overlay_max_fps > 0 ? (unsigned)config.game_overlay_max_fps : 0);

        const int detRes = config.detection_resolution;
        if (detRes <= 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        int regionW = detRes;
        int regionH = detRes;

        if (regionW > pw) regionW = pw;
        if (regionH > ph) regionH = ph;

        const int baseX = (pw - regionW) / 2;
        const int baseY = (ph - regionH) / 2;

        const float scaleX = 1.0f;
        const float scaleY = 1.0f;

        std::vector<cv::Rect> boxesCopy;
        std::vector<int> classesCopy;
        {
            std::lock_guard<std::mutex> lk(detectionBuffer.mutex);
            boxesCopy = detectionBuffer.boxes;
            classesCopy = detectionBuffer.classes;
        }

        decltype(globalMouseThread->getFuturePositions()) futurePts;
        if (config.game_overlay_draw_future && globalMouseThread)
            futurePts = globalMouseThread->getFuturePositions();

        if (config.game_overlay_icon_enabled)
        {
            std::lock_guard<std::mutex> lk(g_iconMutex);
            if (config.game_overlay_icon_path != g_lastIconPath)
            {
                if (g_iconImageId != 0)
                {
                    gameOverlayPtr->UnloadImage(g_iconImageId);
                    g_iconImageId = 0;
                }
                g_lastIconPath = config.game_overlay_icon_path;
                std::error_code fsErr;
                std::filesystem::path p;
                try
                {
                    p = std::filesystem::u8path(g_lastIconPath);
                }
                catch (const std::exception&)
                {
                    p = std::filesystem::path(g_lastIconPath);
                }
                const bool hasFile = !g_lastIconPath.empty() && p.has_filename() && std::filesystem::is_regular_file(p, fsErr);
                if (fsErr)
                {
                    g_iconImageId = 0;
                    g_iconLastError = "[游戏覆盖层] 读取图标路径失败: " + g_lastIconPath + " (" + fsErr.message() + ")";
                    std::cerr << g_iconLastError << std::endl;
                }
                else if (hasFile)
                {
                    const std::wstring wpath = p.wstring();
                    g_iconLastError.clear();

                    UINT iw = 0, ih = 0;
                    std::string verr;
                    if (!IsValidImageFile(wpath, iw, ih, verr))
                    {
                        g_iconImageId = 0;
                        g_iconLastError = "[游戏覆盖层] 无效图片 '" + g_lastIconPath + "': " + verr;
                        std::cerr << g_iconLastError << std::endl;
                    }
                    else
                    {
                        try
                        {
                            int id = gameOverlayPtr->LoadImageFromFile(wpath);
                            if (id != 0)
                            {
                                g_iconImageId = id;
                                std::cout << "[游戏覆盖层] 已加载图标 (" << iw << "x" << ih << "): " << g_lastIconPath << std::endl;
                            }
                            else
                            {
                                g_iconImageId = 0;
                                g_iconLastError = "[游戏覆盖层] 加载图标失败 (返回值为0): " + g_lastIconPath;
                                std::cerr << g_iconLastError << std::endl;
                            }
                        }
                        catch (const std::exception& e)
                        {
                            g_iconImageId = 0;
                            g_iconLastError = std::string("[游戏覆盖层] 加载图标时发生异常: ") + e.what();
                            std::cerr << g_iconLastError << std::endl;
                        }
                        catch (...)
                        {
                            g_iconImageId = 0;
                            g_iconLastError = "[游戏覆盖层] 加载图标时发生未知异常";
                            std::cerr << g_iconLastError << std::endl;
                        }
                    }
                }
                else
                {
                    g_iconImageId = 0;
                    g_iconLastError = "[游戏覆盖层] 图标文件未找到: " + g_lastIconPath;
                    std::cerr << g_iconLastError << std::endl;
                }
            }
        }

        gameOverlayPtr->BeginFrame();

        // 以下内容仅在 Game Overlay 手动开启时绘制
        if (config.game_overlay_enabled)
        {

        // CAPTURE FRAME
        if (config.game_overlay_draw_frame)
        {
            int A = config.game_overlay_frame_a;
            int R = config.game_overlay_frame_r;
            int G = config.game_overlay_frame_g;
            int B = config.game_overlay_frame_b;
            auto clamp255 = [](int& v) { if (v < 0) v = 0; else if (v > 255) v = 255; };
            clamp255(A); clamp255(R); clamp255(G); clamp255(B);
            const uint32_t col =
                (uint32_t(A) << 24) |
                (uint32_t(R) << 16) |
                (uint32_t(G) << 8) |
                uint32_t(B);

            float thickness = config.game_overlay_frame_thickness;
            if (thickness <= 0.f) thickness = 1.f;

            {
                gameOverlayPtr->AddRect(
                    { static_cast<float>(baseX),
                      static_cast<float>(baseY),
                      static_cast<float>(regionW),
                      static_cast<float>(regionH) },
                    col, thickness);
            }
        }

        // BOXES
        if (config.game_overlay_draw_boxes && !boxesCopy.empty())
        {
            int A = config.game_overlay_box_a;
            int R = config.game_overlay_box_r;
            int G = config.game_overlay_box_g;
            int B = config.game_overlay_box_b;
            auto clamp255 = [](int& v) { if (v < 0) v = 0; else if (v > 255) v = 255; };
            clamp255(A); clamp255(R); clamp255(G); clamp255(B);
            const uint32_t col =
                (uint32_t(A) << 24) |
                (uint32_t(R) << 16) |
                (uint32_t(G) << 8) |
                uint32_t(B);

            float thickness = config.game_overlay_box_thickness;
            if (thickness <= 0.f) thickness = 1.f;

            for (const auto& b : boxesCopy)
            {
                if (b.width <= 0 || b.height <= 0) continue;

                int bx = std::max(0, std::min(b.x, detRes));
                int by = std::max(0, std::min(b.y, detRes));
                int bw = std::max(0, std::min(b.width, detRes - bx));
                int bh = std::max(0, std::min(b.height, detRes - by));
                if (bw == 0 || bh == 0) continue;

                float x = baseX + bx * scaleX;
                float y = baseY + by * scaleY;
                float w = bw * scaleX;
                float h = bh * scaleY;

                if (x + w < baseX || y + h < baseY ||
                    x > baseX + regionW || y > baseY + regionH)
                    continue;

                gameOverlayPtr->AddRect({ x, y, w, h }, col, thickness);
            }
        }

        // FUTURE POINTS
        if (config.game_overlay_draw_future && !futurePts.empty())
        {
            const int total = static_cast<int>(futurePts.size());
            const int baseA = std::max(5, std::min(255, config.game_overlay_box_a));

            for (int i = 0; i < total; ++i)
            {
                float alphaFactor =
                    std::exp(-config.game_overlay_future_alpha_falloff *
                        (static_cast<float>(i) / static_cast<float>(total)));

                int a = static_cast<int>(baseA * alphaFactor);
                if (a < 12) a = 12;

                const uint32_t col =
                    (uint32_t(a) << 24) |
                    (uint32_t(255 - (i * 255 / total)) << 16) |
                    (uint32_t(50) << 8) |
                    (uint32_t(i * 255 / total));

                float px = static_cast<float>(baseX) + static_cast<float>(futurePts[i].first) * scaleX;
                float py = static_cast<float>(baseY) + static_cast<float>(futurePts[i].second) * scaleY;

                if (px < baseX - 40 || py < baseY - 40 ||
                    px > baseX + regionW + 40 || py > baseY + regionH + 40)
                    continue;

                gameOverlayPtr->FillCircle({ px, py, config.game_overlay_future_point_radius }, col);
            }
        }

        // ICONS
        if (config.game_overlay_icon_enabled && g_iconImageId != 0 && !boxesCopy.empty())
        {
            const int iconW = config.game_overlay_icon_width;
            const int iconH = config.game_overlay_icon_height;
            const float offXIcon = config.game_overlay_icon_offset_x;
            const float offYIcon = config.game_overlay_icon_offset_y;
            std::string anchor = config.game_overlay_icon_anchor;
            int i = 0;
            for (const auto& b : boxesCopy)
            {
                // temporary: only draw for players
                if (classesCopy[i] != 0)
                {
                    continue;
                }

                if (b.width <= 0 || b.height <= 0) continue;
                int bx = std::max(0, std::min(b.x, detRes));
                int by = std::max(0, std::min(b.y, detRes));
                int bw = std::max(0, std::min(b.width, detRes - bx));
                int bh = std::max(0, std::min(b.height, detRes - by));
                if (bw == 0 || bh == 0) continue;

                float boxX = baseX + bx * scaleX;
                float boxY = baseY + by * scaleY;
                float boxW = bw * scaleX;
                float boxH = bh * scaleY;

                float drawX = boxX;
                float drawY = boxY;

                if (anchor == "center")
                {
                    drawX = boxX + boxW / 2.0f - iconW / 2.0f;
                    drawY = boxY + boxH / 2.0f - iconH / 2.0f;
                }
                else if (anchor == "top" || anchor == "head")
                {
                    drawX = boxX + boxW / 2.0f - iconW / 2.0f;
                    drawY = boxY - iconH;
                }
                else if (anchor == "bottom")
                {
                    drawX = boxX + boxW / 2.0f - iconW / 2.0f;
                    drawY = boxY + boxH;
                }
                else
                {
                    drawX = boxX + boxW / 2.0f - iconW / 2.0f;
                    drawY = boxY + boxH / 2.0f - iconH / 2.0f;
                }

                drawX += offXIcon;
                drawY += offYIcon;

                gameOverlayPtr->DrawImage(g_iconImageId, drawX, drawY, (float)iconW, (float)iconH, 1.0f);
                i++;
            }
        }

        } // end if (config.game_overlay_enabled)

        // WEAPON HUD (top center, current weapon + attachments + pose)
        if (config.weapon_recognition_enabled && globalWeaponRecognizer && globalWeaponRecognizer->isRunning())
        {
            int slot = currentWeaponSlot.load();
            WeaponInfo winfo = (slot == 2)
                ? globalWeaponRecognizer->getState().getSecondary()
                : globalWeaponRecognizer->getState().getPrimary();
            std::string pose = globalWeaponRecognizer->getState().getPose();

            auto toCN = [](const std::string& key) -> std::wstring {
                if (key == "stand")  return L"\x7AD9\x7ACB";
                if (key == "down")   return L"\x8E72\x4E0B";
                if (key == "crawl")  return L"\x8DB4\x4E0B";
                if (key == "reddot") return L"\x7EA2\x70B9";
                if (key == "quanxi") return L"\x5168\x606F";
                if (key == "x2")     return L"2\x500D";
                if (key == "x3")     return L"3\x500D";
                if (key == "x4")     return L"4\x500D";
                if (key == "x6")     return L"6\x500D";
                if (key == "x8")     return L"8\x500D";
                if (key == "xy1")    return L"\x6D88\x7130\x5668";
                if (key == "xy2")    return L"\x6D88\x7130\x5668(\x72D9)";
                if (key == "xy3")    return L"\x6D88\x7130\x5668(\x51B2)";
                if (key == "xx")     return L"\x6D88\x97F3\x5668";
                if (key == "xx1")    return L"\x6D88\x97F3\x5668(\x51B2)";
                if (key == "bc1")    return L"\x8865\x507F\x5668";
                if (key == "bc2")    return L"\x8865\x507F\x5668(\x72D9)";
                if (key == "bc3")    return L"\x8865\x507F\x5668(\x51B2)";
                if (key == "zt")     return L"\x5236\x9000\x5668";
                if (key == "angle")  return L"\x76F4\x89D2";
                if (key == "red")    return L"\x7EA2\x70B9\x63E1\x628A";
                if (key == "thumb")  return L"\x62C7\x6307";
                if (key == "light")  return L"\x8F7B\x578B";
                if (key == "line")   return L"\x5782\x76F4";
                if (key == "heavy")  return L"\x91CD\x578B\x67AA\x6258";
                if (key == "normal") return L"\x6218\x672F\x67AA\x6258";
                if (key == "pg")     return L"\x67AA\x6258(PG)";
                return std::wstring(key.begin(), key.end());
            };

            if (!winfo.weaponName.empty() && winfo.weaponName != "None")
            {
                std::wstring line(winfo.weaponName.begin(), winfo.weaponName.end());
                auto app = [&](const std::string& val) {
                    if (!val.empty() && val != "None") {
                        line += L" | ";
                        line += toCN(val);
                    }
                };
                app(winfo.scope); app(winfo.muzzle); app(winfo.grip); app(winfo.stock);
                if (!pose.empty() && pose != "None") {
                    line += L" | ";
                    line += toCN(pose);
                }

                float textW = line.size() * 7.0f;
                float tx = (pw - textW) / 2.0f;
                float ty = 4.0f;
                gameOverlayPtr->AddText(tx + 1, ty + 1, line, 14.0f, ARGB(180, 0, 0, 0));
                gameOverlayPtr->AddText(tx, ty, line, 14.0f, ARGB(255, 0, 255, 128));
            }
        }

        gameOverlayPtr->EndFrame();

        unsigned fpsCap = (unsigned)config.game_overlay_max_fps;
        if (boxesCopy.empty() && futurePts.empty())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
            continue;
        }
        if (fpsCap > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(1000 / fpsCap));
        else
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (gameOverlayPtr)
    {
        gameOverlayPtr->Stop();
        delete gameOverlayPtr;
        gameOverlayPtr = nullptr;
    }
}

int main(int argc, char* argv[])
{
    // ========== 调试模式：启用控制台窗口 ==========
    // CreateAndInitConsole();
    // std::cout << "[DEBUG] Console initialized, starting CowAI..." << std::endl;
    
    // 注册崩溃处理器
    SetUnhandledExceptionFilter(CrashHandler);
    // ========== 调试模式结束 ==========

    // 全局异常处理 - 捕获所有未处理的异常
    try
    {

    // ========== 版本检查（最先执行）==========
    {
        std::string newVersion, downloadUrl, changelog;
        if (Version::CheckForUpdate(newVersion, downloadUrl, changelog))
        {
            // 版本过旧，显示更新提示后强制退出
            Version::ShowUpdateDialog(newVersion, downloadUrl, changelog);
            std::cout << "[DEBUG] Version check failed, press Enter to exit..." << std::endl;
            // std::cin.get();
            return -1;
        }
    }
    // std::cout << "[DEBUG] Version check passed" << std::endl;
    // ========== 版本检查结束 ==========

    // Windows 子系统应用 - 无控制台窗口
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_FATAL);

    // 加载配置（静默加载，失败时显示消息框）
    if (!config.loadConfig())
    {
        // std::cerr << "[DEBUG] Failed to load config!" << std::endl;
        MessageBoxW(NULL, L"\xE5\x8A\xA0\xE8\xBD\xBD\xE9\x85\x8D\xE7\xBD\xAE\xE6\x96\x87\xE4\xBB\xB6\xE5\xA4\xB1\xE8\xB4\xA5\xEF\xBC\x81", L"CowAI", MB_OK | MB_ICONERROR);
        // std::cout << "Press Enter to exit..." << std::endl;
        // std::cin.get();
        return -1;
    }
    // std::cout << "[DEBUG] Config loaded successfully" << std::endl;

    // ========== TGAuth 授权检查 (已禁用) ==========
    if (false && config.auth_enabled)
    {
        TGAuth::AuthConfig authConfig;
        authConfig.worker_api_url = "https://cowai.trueliu.com";
        
        TGAuth::AuthManager& auth = TGAuth::AuthManager::getInstance();
        if (!auth.init(authConfig))
        {
            MessageBoxW(NULL, L"\xE5\x88\x9D\xE5\xA7\x8B\xE5\x8C\x96\xE6\x8E\x88\xE6\x9D\x83\xE6\xA8\xA1\xE5\x9D\x97\xE5\xA4\xB1\xE8\xB4\xA5\xEF\xBC\x81", L"CowAI", MB_OK | MB_ICONERROR);
            return -1;
        }
        
        // 先检查本地 token
        TGAuth::AuthResult result = auth.checkAuthorization();
        
        if (result.status == TGAuth::AuthStatus::AUTHORIZED)
        {
            // 验证成功，继续运行（不创建控制台）
        }
        else
        {
            // 需要输入卡密 - 使用 GUI 验证器
            #ifdef USE_KEY_VALIDATOR_GUI
            KeyValidatorGUI::ValidationInfo validationInfo;
            bool validated = KeyValidatorGUI::ShowKeyValidatorWindow(validationInfo);
            
            if (!validated)
            {
                return -1;
            }
            #else
            // 没有 GUI 验证器且未授权，显示错误并退出
            MessageBoxW(NULL, L"\xE6\x9C\xAA\xE6\x8E\x88\xE6\x9D\x83\xEF\xBC\x8C\xE8\xAF\xB7\xE8\x81\x94\xE7\xB3\xBB\xE7\xAE\xA1\xE7\x90\x86\xE5\x91\x98", L"CowAI", MB_OK | MB_ICONERROR);
            return -1;
            #endif
        }
    }
    // ========== TGAuth 授权检查结束 ==========

    CPUAffinityManager cpuManager;

    if (config.cpuCoreReserveCount > 0)
    {
        if (!cpuManager.reserveCPUCores(config.cpuCoreReserveCount)) return -1;
    }

    if (config.systemMemoryReserveMB > 0)
    {
        if (!cpuManager.reserveSystemMemory(config.systemMemoryReserveMB)) return -1;
    }

    try
    {
#ifdef USE_CUDA
        int cuda_runtime_version = 0;
        cudaError_t runtime_status = cudaRuntimeGetVersion(&cuda_runtime_version);

        if (runtime_status != cudaSuccess)
        {
            std::cerr << "[主程序] CUDA 运行时检查失败: " << cudaGetErrorString(runtime_status) << std::endl;
            std::cin.get();
            return -1;
        }

        if (config.verbose)
            std::cout << "[CUDA] 版本: " << cuda_runtime_version << std::endl;

        const int required_cuda_version = 13010;
        if (cuda_runtime_version < required_cuda_version)
        {
            int required_major = required_cuda_version / 1000;
            int required_minor = (required_cuda_version % 1000) / 10;
            int runtime_major = cuda_runtime_version / 1000;
            int runtime_minor = (cuda_runtime_version % 1000) / 10;
            std::cerr << "[主程序] 需要 CUDA " << required_major << "." << required_minor
                << "，检测到 " << runtime_major << "." << runtime_minor << std::endl;
            std::cin.get();
            return -1;
        }

        GPUResourceManager gpuManager;
        if (config.gpuMemoryReserveMB > 0)
        {
            if (!gpuManager.reserveGPUMemory(config.gpuMemoryReserveMB)) return -1;
        }
        
        if (config.enableGpuExclusiveMode)
        {
            if (!gpuManager.setGPUExclusiveMode()) return -1;
        }

        int cuda_devices = 0;
        if (cudaGetDeviceCount(&cuda_devices) != cudaSuccess || cuda_devices == 0)
        {
            std::cerr << "[主程序] 需要 CUDA 但未找到设备" << std::endl;
            std::cin.get();
            return -1;
        }
#endif
        if (!CreateDirectory(L"screenshots", NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        {
            std::cout << "[主程序] 创建截图文件夹失败" << std::endl;
            std::cin.get();
            return -1;
        }

        if (!CreateDirectory(L"models", NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        {
            std::cout << "[主程序] 创建模型文件夹失败" << std::endl;
            std::cin.get();
            return -1;
        }

        if (config.capture_method == "virtual_camera")
        {
            auto cams = VirtualCameraCapture::GetAvailableVirtualCameras();
            if (!cams.empty())
            {
                if (config.virtual_camera_name == "None" ||
                    std::find(cams.begin(), cams.end(), config.virtual_camera_name) == cams.end())
                {
                    config.virtual_camera_name = cams[0];
                    config.saveConfig("config.ini");
                    std::cout << "[主程序] 设置 virtual_camera_name = " << config.virtual_camera_name << std::endl;
                }
                std::cout << "[主程序] 已加载虚拟摄像头: " << cams.size() << " 个" << std::endl;
            }
            else
            {
                std::cerr << "[主程序] 未找到虚拟摄像头" << std::endl;
            }
        }

        std::string modelPath = "models/" + config.ai_model;

        if (!std::filesystem::exists(utf8_to_path(modelPath)))
        {
            std::cerr << "[主程序] 指定的模型不存在: " << modelPath << std::endl;

            std::vector<std::string> modelFiles = getModelFiles();

            if (!modelFiles.empty())
            {
                config.ai_model = modelFiles[0];
                config.saveConfig();
                std::cout << "[主程序] 已加载第一个可用模型: " << config.ai_model << std::endl;
            }
            else
            {
                MessageBoxW(NULL, 
                    L"models \x76EE\x5F55\x4E2D\x672A\x627E\x5230\x6A21\x578B\x6587\x4EF6\xFF01\n\n"
                    L"\x8BF7\x5C06 .onnx \x6A21\x578B\x6587\x4EF6\x653E\x5165 models \x76EE\x5F55",
                    L"CowAI", MB_OK | MB_ICONERROR);
                return -1;
            }
        }

        createInputDevices();

        // Initialize SharedState for Qt Panel IPC
        if (g_sharedState.initialize())
        {
            g_sharedState.updateBackendInfo(config.backend, config.capture_method, config.ai_model);
            std::cout << "[SharedState] IPC initialized for Qt Panel" << std::endl;
        }

        // Initialize ByteTracker
        byteTracker = new ByteTracker(
            config.bytetrack_track_thresh,
            config.bytetrack_high_thresh,
            config.bytetrack_match_thresh,
            config.bytetrack_max_time_lost
        );
        std::cout << "[ByteTrack] Tracker initialized" << std::endl;

        // Initialize AutoPickup (一键拾取)
        globalAutoPickup = new AutoPickup();
        globalAutoPickup->setInputDevices(arduinoSerial, gHub, kmboxSerial, kmboxNetSerial, makcuSerial);
        
        // Configure pickup based on screen type
        int startX, startY, spacingY, targetX, targetY;
        switch (config.pickup_screen_type)
        {
        case 2:  // 2560x1080
            startX = 7000;
            startY = 55000;
            spacingY = 3755;
            targetX = 50000;
            targetY = 55000;
            break;
        case 3:  // 2560x1440
            startX = 7000;
            startY = 55000;
            spacingY = 2816;
            targetX = 50000;
            targetY = 55000;
            break;
        default:  // 1920x1080
            startX = 7000;
            startY = 55000;
            spacingY = 3755;
            targetX = 50000;
            targetY = 55000;
            break;
        }
        globalAutoPickup->configure(1, config.pickup_items_per_loop, startX, startY, 
                                    spacingY, targetX, targetY, config.pickup_delay_ms);
        std::cout << "[拾取] 一键拾取已初始化" << std::endl;

        // Initialize Weapon Recognition (武器识别)
        globalWeaponRecognizer = new WeaponRecognizer();
        globalWeaponRecognizer->configure(
            config.weapon_template_path,
            config.weapon_recognition_threshold,
            config.weapon_scan_interval_ms);
        if (config.weapon_recognition_enabled) {
            globalWeaponRecognizer->start();
        }
        std::cout << "[武器识别] 武器识别模块已初始化" << std::endl;

        // Initialize Recoil Controller (弹道压枪)
        RecoilDatabase::instance().loadFromDirectory("recoil");
        globalRecoilController = new RecoilController();
        std::cout << "[弹道压枪] 弹道压枪模块已初始化" << std::endl;

        MouseThread mouseThread(
            config.detection_resolution,
            config.fovX,
            config.fovY,
            config.minSpeedMultiplier,
            config.maxSpeedMultiplier,
            config.predictionInterval,
            config.auto_shoot,
            config.bScope_multiplier,
            arduinoSerial,
            gHub,
            kmboxSerial,
            kmboxNetSerial
        );

        globalMouseThread = &mouseThread;
        assignInputDevices();

        std::vector<std::string> availableModels = getAvailableModels(false);

        if (!config.ai_model.empty())
        {
            std::string modelPath = "models/" + config.ai_model;
            auto modelFsPath = utf8_to_path(modelPath);
            if (!std::filesystem::exists(modelFsPath))
            {
                if (!availableModels.empty())
                {
                    config.ai_model = availableModels[0];
                    config.saveConfig("config.ini");
                }
                else
                {
                    MessageBoxW(NULL,
                        L"\x672A\x627E\x5230\x6A21\x578B\x6587\x4EF6\xFF01\n\n"
                        L"\x8BF7\x5C06 .onnx \x6A21\x578B\x6587\x4EF6\x653E\x5165 models \x76EE\x5F55",
                        L"CowAI", MB_OK | MB_ICONERROR);
                    std::cin.get();
                    return -1;
                }
            }
            else
            {
                uintmax_t fileSize = std::filesystem::file_size(modelFsPath);
                if (fileSize == 0)
                {
                    MessageBoxW(NULL,
                        L"\x6A21\x578B\x6587\x4EF6\x4E3A\x7A7A\xFF01\n\n"
                        L"\x8BF7\x68C0\x67E5 models \x76EE\x5F55\x4E2D\x7684 .onnx \x6587\x4EF6\x662F\x5426\x5B8C\x6574",
                        L"CowAI", MB_OK | MB_ICONERROR);
                    return -1;
                }
            }
        }
        else
        {
            if (!availableModels.empty())
            {
                config.ai_model = availableModels[0];
                config.saveConfig();
            }
            else
            {
                MessageBoxW(NULL,
                    L"\x672A\x627E\x5230\x6A21\x578B\x6587\x4EF6\xFF01\n\n"
                    L"\x8BF7\x5C06 .onnx \x6A21\x578B\x6587\x4EF6\x653E\x5165 models \x76EE\x5F55",
                    L"CowAI", MB_OK | MB_ICONERROR);
                return -1;
            }
        }
        
        std::cout << "[\xE6\xA8\xA1\xE5\x9E\x8B] " << config.ai_model << std::endl;

#ifdef USE_CUDA
        std::cout << "[\xE6\xA3\x80\xE6\xB5\x8B\xE5\x99\xA8] TensorRT" << std::endl;
        trt_detector.initialize("models/" + config.ai_model);
#else
        std::cout << "[\xE6\xA3\x80\xE6\xB5\x8B\xE5\x99\xA8] DirectML" << std::endl;
        dml_detector = new DirectMLDetector("models/" + config.ai_model);
#endif

        detection_resolution_changed.store(true);
        detectionPaused.store(false);  // 确保启动时检测未暂停

        welcome_message();

        std::thread keyThread(keyboardListener);
        std::thread capThread(captureThread, config.detection_resolution, config.detection_resolution);

#ifdef USE_CUDA
        std::thread trt_detThread(&TrtDetector::inferenceThread, &trt_detector);
#else
        std::thread dml_detThread(&DirectMLDetector::dmlInferenceThread, dml_detector);
#endif
        std::thread mouseMovThread(mouseThreadFunction, std::ref(mouseThread));
        std::thread recoilThread(recoilThreadFunction);
        std::thread overlayThread(OverlayThread);

        if (config.game_overlay_enabled || config.weapon_recognition_enabled)
        {
            gameOverlayPtr = new Game_overlay();
            int pw = GetSystemMetrics(SM_CXSCREEN);
            int ph = GetSystemMetrics(SM_CYSCREEN);
            gameOverlayPtr->SetWindowBounds(0, 0, pw, ph);
            gameOverlayPtr->SetMaxFPS(config.game_overlay_max_fps > 0 ? (unsigned)config.game_overlay_max_fps : 0);
            gameOverlayPtr->Start();
            gameOverlayShouldExit.store(false);
            gameOverlayThread = std::thread(gameOverlayRenderLoop);
        }

        keyThread.join();
        capThread.join();

#ifdef USE_CUDA
        trt_detThread.join();
#else
        dml_detThread.join();
        delete dml_detector;
        dml_detector = nullptr;
#endif
        mouseMovThread.join();
        recoilThread.join();
        overlayThread.join();

        if (arduinoSerial)
        {
            delete arduinoSerial;
        }

        if (gHub)
        {
            gHub->mouse_close();
            delete gHub;
        }

        if (byteTracker)
        {
            delete byteTracker;
            byteTracker = nullptr;
        }

        if (globalAutoPickup)
        {
            globalAutoPickup->stop();
            delete globalAutoPickup;
            globalAutoPickup = nullptr;
        }

        if (globalWeaponRecognizer)
        {
            globalWeaponRecognizer->stop();
            delete globalWeaponRecognizer;
            globalWeaponRecognizer = nullptr;
        }

        if (globalRecoilController)
        {
            delete globalRecoilController;
            globalRecoilController = nullptr;
        }

        gameOverlayShouldExit.store(true);
        if (gameOverlayThread.joinable()) gameOverlayThread.join();
        if (gameOverlayPtr)
        {
            gameOverlayPtr->Stop();
            delete gameOverlayPtr;
            gameOverlayPtr = nullptr;
        }

        // std::cout << "[DEBUG] Program exiting normally, press Enter..." << std::endl;
        // std::cin.get();
        return 0;
    }
    catch (const std::exception& e)
    {
        // std::cerr << "[主程序] 主线程发生错误: " << e.what() << std::endl;
        // std::cout << "按回车键退出...";
        // std::cin.get();
        return -1;
    }

    } // 全局 try 结束
    catch (const std::exception& e)
    {
        std::cerr << "[FATAL] Unhandled exception: " << e.what() << std::endl;
        std::cout << "Press Enter to exit..." << std::endl;
        std::cin.get();
        return -1;
    }
    catch (...)
    {
        std::cerr << "[FATAL] Unknown exception occurred!" << std::endl;
        std::cout << "Press Enter to exit..." << std::endl;
        std::cin.get();
        return -1;
    }
}