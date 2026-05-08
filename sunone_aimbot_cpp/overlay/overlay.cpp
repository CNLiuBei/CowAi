#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <winsock2.h>
#include <Windows.h>
#include <shellapi.h>

#include <tchar.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <d3d11.h>
#include <dxgi.h>
#include <dwmapi.h>
#include <dcomp.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>
#include <chrono>

using Microsoft::WRL::ComPtr;

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <imgui/imgui_internal.h>

#include "overlay.h"
#include "overlay/draw_settings.h"
#include "config.h"
#include "keycodes.h"
#include "sunone_aimbot_cpp.h"
#include "capture.h"
#include "keyboard_listener.h"
#include "other_tools.h"
#include "virtual_camera.h"
#include "version/version.h"
#include "auth/tg_auth.h"

#ifdef USE_CUDA
#include "trt_detector.h"
#else
#include "dml_detector.h"
#endif

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "d3d11.lib")

// 托盘图标相关
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_SHOW 1001
#define ID_TRAY_EXIT 1002
static NOTIFYICONDATA g_nid = {};
static HICON g_hTrayIcon = NULL;
static bool g_trayIconAdded = false;

ID3D11Device* g_pd3dDevice = NULL;
ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
IDXGISwapChain1* g_pSwapChain = NULL;
IDCompositionDevice* g_dcompDevice = NULL;
IDCompositionTarget* g_dcompTarget = NULL;
IDCompositionVisual* g_dcompVisual = NULL;
ID3D11RenderTargetView* g_mainRenderTargetView = NULL;
HWND g_hwnd = NULL;

extern Config config;
extern std::mutex configMutex;
extern std::atomic<bool> shouldExit;

// 用于退出时通知等待的线程
#include "detector/detection_buffer.h"
extern DetectionBuffer detectionBuffer;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
void RemoveTrayIcon();

ID3D11BlendState* g_pBlendState = nullptr;

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

const int BASE_OVERLAY_WIDTH = 720;
const int BASE_OVERLAY_HEIGHT = 500;

int overlayWidth = 0;
int overlayHeight = 0;

static const int DRAG_BAR_HEIGHT_PX = 30;
static const int MIN_OVERLAY_W = 420;
static const int MIN_OVERLAY_H = 300;

std::vector<std::string> availableModels;
std::vector<std::string> key_names;
std::vector<const char*> key_names_cstrs;

std::atomic<bool> overlayVisible(true);

ID3D11ShaderResourceView* body_texture = nullptr;
ID3D11ShaderResourceView* g_logoTexture = nullptr;
int g_logoWidth = 0;
int g_logoHeight = 0;

void load_body_texture();
void release_body_texture();
void load_logo_texture();
void release_logo_texture();
std::vector<std::string> getAvailableModels();

static inline int ClampInt(int v, int lo, int hi)
{
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

void Overlay_SetOpacity(int opacity255)
{
    if (!g_hwnd) return;

    opacity255 = ClampInt(opacity255, 20, 255);

    LONG exStyle = GetWindowLong(g_hwnd, GWL_EXSTYLE);
    if ((exStyle & WS_EX_LAYERED) == 0)
        SetWindowLong(g_hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);

    SetLayeredWindowAttributes(g_hwnd, 0, (BYTE)opacity255, LWA_ALPHA);
}

static UINT GetDpiForWindowSafe(HWND hwnd)
{
    UINT dpi = 96;
    HMODULE user32 = ::GetModuleHandleW(L"user32.dll");
    if (user32)
    {
        auto pGetDpiForWindow = (UINT(WINAPI*)(HWND))::GetProcAddress(user32, "GetDpiForWindow");
        if (pGetDpiForWindow)
            dpi = pGetDpiForWindow(hwnd);
    }
    return dpi;
}

static int GetScaledSystemMetric(int metric, UINT dpi)
{
    const int v = ::GetSystemMetrics(metric);
    return ::MulDiv(v, (int)dpi, 96);
}

bool InitializeBlendState()
{
    D3D11_BLEND_DESC blendDesc;
    ZeroMemory(&blendDesc, sizeof(blendDesc));

    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    HRESULT hr = g_pd3dDevice->CreateBlendState(&blendDesc, &g_pBlendState);
    if (FAILED(hr))
        return false;

    float blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
    g_pd3dDeviceContext->OMSetBlendState(g_pBlendState, blendFactor, 0xffffffff);
    return true;
}

bool CreateDeviceD3D(HWND hWnd)
{
    UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0,
    };

    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        featureLevelArray,
        ARRAYSIZE(featureLevelArray),
        D3D11_SDK_VERSION,
        &g_pd3dDevice,
        &featureLevel,
        &g_pd3dDeviceContext);

    if (FAILED(hr))
        return false;

    IDXGIDevice* dxgiDev = nullptr;
    hr = g_pd3dDevice->QueryInterface(IID_PPV_ARGS(&dxgiDev));
    if (FAILED(hr) || !dxgiDev)
        return false;

    IDXGIAdapter* adapter = nullptr;
    hr = dxgiDev->GetAdapter(&adapter);
    if (FAILED(hr) || !adapter)
    {
        dxgiDev->Release();
        return false;
    }

    IDXGIFactory2* factory2 = nullptr;
    {
        IDXGIFactory* baseFactory = nullptr;
        hr = adapter->GetParent(IID_PPV_ARGS(&baseFactory));
        if (FAILED(hr) || !baseFactory)
        {
            adapter->Release();
            dxgiDev->Release();
            return false;
        }
        hr = baseFactory->QueryInterface(IID_PPV_ARGS(&factory2));
        baseFactory->Release();
    }

    if (FAILED(hr) || !factory2)
    {
        adapter->Release();
        dxgiDev->Release();
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 scd = {};
    scd.Width = overlayWidth;
    scd.Height = overlayHeight;
    scd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scd.SampleDesc.Count = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount = 2;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    scd.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    scd.Scaling = DXGI_SCALING_STRETCH;

    hr = factory2->CreateSwapChainForComposition(
        g_pd3dDevice,
        &scd,
        nullptr,
        &g_pSwapChain);

    factory2->Release();
    adapter->Release();

    if (FAILED(hr) || !g_pSwapChain)
    {
        dxgiDev->Release();
        return false;
    }

    hr = DCompositionCreateDevice(dxgiDev, IID_PPV_ARGS(&g_dcompDevice));
    dxgiDev->Release();
    if (FAILED(hr) || !g_dcompDevice)
        return false;

    hr = g_dcompDevice->CreateTargetForHwnd(hWnd, TRUE, &g_dcompTarget);
    if (FAILED(hr) || !g_dcompTarget)
        return false;

    hr = g_dcompDevice->CreateVisual(&g_dcompVisual);
    if (FAILED(hr) || !g_dcompVisual)
        return false;

    hr = g_dcompVisual->SetContent(g_pSwapChain);
    if (FAILED(hr))
        return false;

    hr = g_dcompTarget->SetRoot(g_dcompVisual);
    if (FAILED(hr))
        return false;

    g_dcompDevice->Commit();

    if (!InitializeBlendState())
        return false;

    CreateRenderTarget();
    return true;
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer = NULL;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    release_logo_texture();

    if (g_dcompVisual) { g_dcompVisual->Release(); g_dcompVisual = NULL; }
    if (g_dcompTarget) { g_dcompTarget->Release(); g_dcompTarget = NULL; }
    if (g_dcompDevice) { g_dcompDevice->Release(); g_dcompDevice = NULL; }

    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
    if (g_pBlendState) { g_pBlendState->Release(); g_pBlendState = nullptr; }
}

// Logo 纹理加载
void load_logo_texture()
{
    if (!g_pd3dDevice) return;
    
    // 获取 exe 所在目录
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string logoPath = exePath;
    size_t lastSlash = logoPath.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        logoPath = logoPath.substr(0, lastSlash + 1) + "Cow.png";
    } else {
        logoPath = "Cow.png";
    }
    
    // 转换为宽字符
    int wlen = MultiByteToWideChar(CP_UTF8, 0, logoPath.c_str(), -1, nullptr, 0);
    std::wstring wpath(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, logoPath.c_str(), -1, &wpath[0], wlen);
    
    // 使用 WIC 加载图片
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    
    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return;
    
    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(wpath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) return;
    
    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) return;
    
    UINT width, height;
    frame->GetSize(&width, &height);
    
    ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) return;
    
    hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) return;
    
    std::vector<uint8_t> pixels(width * height * 4);
    hr = converter->CopyPixels(nullptr, width * 4, (UINT)pixels.size(), pixels.data());
    if (FAILED(hr)) return;
    
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    
    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = pixels.data();
    initData.SysMemPitch = width * 4;
    
    ID3D11Texture2D* texture = nullptr;
    hr = g_pd3dDevice->CreateTexture2D(&desc, &initData, &texture);
    if (FAILED(hr)) return;
    
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    
    hr = g_pd3dDevice->CreateShaderResourceView(texture, &srvDesc, &g_logoTexture);
    texture->Release();
    
    if (SUCCEEDED(hr)) {
        g_logoWidth = width;
        g_logoHeight = height;
    }
}

void release_logo_texture()
{
    if (g_logoTexture) { g_logoTexture->Release(); g_logoTexture = nullptr; }
    g_logoWidth = 0;
    g_logoHeight = 0;
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_NCHITTEST:
        {
            POINT pt = { (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam) };
            ::ScreenToClient(hWnd, &pt);

            RECT rc;
            ::GetClientRect(hWnd, &rc);

            if (pt.y >= rc.top && pt.y < rc.top + DRAG_BAR_HEIGHT_PX)
                return HTCAPTION;

            return HTCLIENT;
        }
        
        case WM_TRAYICON:
        {
            if (lParam == WM_LBUTTONDBLCLK) {
                // 双击托盘图标 - 显示/隐藏窗口
                if (IsWindowVisible(hWnd)) {
                    ShowWindow(hWnd, SW_HIDE);
                    overlayVisible.store(false);
                } else {
                    ShowWindow(hWnd, SW_SHOW);
                    SetForegroundWindow(hWnd);
                    overlayVisible.store(true);
                }
            } else if (lParam == WM_RBUTTONUP) {
                // 右键点击 - 显示菜单
                POINT pt;
                GetCursorPos(&pt);
                HMENU hMenu = CreatePopupMenu();
                AppendMenuW(hMenu, MF_STRING, ID_TRAY_SHOW, IsWindowVisible(hWnd) ? L"隐藏" : L"显示");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
                AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"退出");
                SetForegroundWindow(hWnd);
                TrackPopupMenu(hMenu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hWnd, NULL);
                DestroyMenu(hMenu);
            }
            return 0;
        }
        
        case WM_COMMAND:
        {
            switch (LOWORD(wParam)) {
                case ID_TRAY_SHOW:
                    if (IsWindowVisible(hWnd)) {
                        ShowWindow(hWnd, SW_HIDE);
                        overlayVisible.store(false);
                    } else {
                        ShowWindow(hWnd, SW_SHOW);
                        SetForegroundWindow(hWnd);
                        overlayVisible.store(true);
                    }
                    break;
                case ID_TRAY_EXIT:
                    shouldExit.store(true);
                    // 通知所有等待的线程
                    detectionBuffer.cv.notify_all();
                    RemoveTrayIcon();
                    quick_exit(0);
                    break;
            }
            return 0;
        }
    }

    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
        {
            const UINT width = (UINT)LOWORD(lParam);
            const UINT height = (UINT)HIWORD(lParam);

            overlayWidth = (int)width;
            overlayHeight = (int)height;

            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
            if (g_dcompDevice) g_dcompDevice->Commit();
        }
        return 0;

    case WM_DESTROY:
        shouldExit = true;
        detectionBuffer.cv.notify_all();
        RemoveTrayIcon();
        ::PostQuitMessage(0);
        return 0;

    default:
        return ::DefWindowProc(hWnd, msg, wParam, lParam);
    }
}

#include "overlay/themes.h"

void SetupImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = config.overlay_ui_scale;

    io.IniFilename = nullptr;
    io.LogFilename = nullptr;

    // 加载中文字体
    ImFontConfig fontConfig;
    fontConfig.OversampleH = 2;
    fontConfig.OversampleV = 1;
    
    // 尝试加载微软雅黑字体（Windows系统自带）
    const char* fontPaths[] = {
        "C:\\Windows\\Fonts\\msyh.ttc",      // 微软雅黑
        "C:\\Windows\\Fonts\\msyhbd.ttc",    // 微软雅黑粗体
        "C:\\Windows\\Fonts\\simhei.ttf",    // 黑体
        "C:\\Windows\\Fonts\\simsun.ttc",    // 宋体
    };
    
    bool fontLoaded = false;
    for (const char* fontPath : fontPaths)
    {
        if (std::filesystem::exists(fontPath))
        {
            io.Fonts->AddFontFromFileTTF(fontPath, 16.0f, &fontConfig, io.Fonts->GetGlyphRangesChineseFull());
            fontLoaded = true;
            break;
        }
    }
    
    if (!fontLoaded)
    {
        // 如果没有找到中文字体，使用默认字体
        io.Fonts->AddFontDefault();
        std::cout << "[覆盖层] 警告: 未找到中文字体，使用默认字体" << std::endl;
    }

    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    Themes::ApplyTheme_GlassMorphism();  // 默认主题
    ApplyThemeByIndex(config.overlay_theme);  // 应用用户选择的主题
    load_body_texture();
    load_logo_texture();
}

bool CreateOverlayWindow()
{
    overlayWidth = static_cast<int>(BASE_OVERLAY_WIDTH * config.overlay_ui_scale);
    overlayHeight = static_cast<int>(BASE_OVERLAY_HEIGHT * config.overlay_ui_scale);

    // 从 Cow.png 加载图标
    HICON hIcon = NULL;
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string iconPath = exePath;
    size_t lastSlash = iconPath.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        iconPath = iconPath.substr(0, lastSlash + 1) + "Cow.png";
    } else {
        iconPath = "Cow.png";
    }
    
    // 使用 GDI+ 或 WIC 加载 PNG 并转换为 HICON
    // 简化方案：使用 LoadImage 加载 ICO，或者用 WIC 转换
    // 这里尝试加载同目录下的 Cow.ico，如果没有则使用默认图标
    std::string icoPath = iconPath.substr(0, iconPath.length() - 4) + ".ico";
    hIcon = (HICON)LoadImageA(NULL, icoPath.c_str(), IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
    if (!hIcon) {
        // 尝试从 PNG 创建图标 (使用 WIC)
        int wlen = MultiByteToWideChar(CP_UTF8, 0, iconPath.c_str(), -1, nullptr, 0);
        std::wstring wpath(wlen, 0);
        MultiByteToWideChar(CP_UTF8, 0, iconPath.c_str(), -1, &wpath[0], wlen);
        
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        ComPtr<IWICImagingFactory> factory;
        if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) {
            ComPtr<IWICBitmapDecoder> decoder;
            if (SUCCEEDED(factory->CreateDecoderFromFilename(wpath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder))) {
                ComPtr<IWICBitmapFrameDecode> frame;
                if (SUCCEEDED(decoder->GetFrame(0, &frame))) {
                    UINT width, height;
                    frame->GetSize(&width, &height);
                    
                    ComPtr<IWICFormatConverter> converter;
                    if (SUCCEEDED(factory->CreateFormatConverter(&converter))) {
                        if (SUCCEEDED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom))) {
                            // 缩放到图标大小
                            UINT iconSize = 32;
                            ComPtr<IWICBitmapScaler> scaler;
                            if (SUCCEEDED(factory->CreateBitmapScaler(&scaler))) {
                                if (SUCCEEDED(scaler->Initialize(converter.Get(), iconSize, iconSize, WICBitmapInterpolationModeHighQualityCubic))) {
                                    std::vector<uint8_t> pixels(iconSize * iconSize * 4);
                                    if (SUCCEEDED(scaler->CopyPixels(nullptr, iconSize * 4, (UINT)pixels.size(), pixels.data()))) {
                                        // 创建 HBITMAP
                                        BITMAPINFO bmi = {};
                                        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                                        bmi.bmiHeader.biWidth = iconSize;
                                        bmi.bmiHeader.biHeight = -(int)iconSize;  // 负数表示自上而下
                                        bmi.bmiHeader.biPlanes = 1;
                                        bmi.bmiHeader.biBitCount = 32;
                                        bmi.bmiHeader.biCompression = BI_RGB;
                                        
                                        void* bits = nullptr;
                                        HBITMAP hBitmap = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
                                        if (hBitmap && bits) {
                                            memcpy(bits, pixels.data(), pixels.size());
                                            
                                            // 创建掩码位图
                                            HBITMAP hMask = CreateBitmap(iconSize, iconSize, 1, 1, NULL);
                                            
                                            ICONINFO ii = {};
                                            ii.fIcon = TRUE;
                                            ii.hbmMask = hMask;
                                            ii.hbmColor = hBitmap;
                                            hIcon = CreateIconIndirect(&ii);
                                            
                                            DeleteObject(hMask);
                                            DeleteObject(hBitmap);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    WNDCLASSEX wc = {
        sizeof(WNDCLASSEX),
        CS_CLASSDC,
        WndProc,
        0L,
        0L,
        GetModuleHandle(NULL),
        hIcon,      // 窗口图标
        NULL,
        NULL,
        NULL,
        _T("CowAIOverlay"),
        hIcon       // 小图标
    };
    ::RegisterClassEx(&wc);

    // 移除 WS_EX_TOOLWINDOW 以显示任务栏图标
    const DWORD exStyle = WS_EX_TOPMOST | WS_EX_LAYERED;
    const DWORD style = WS_POPUP;

    RECT wr = { 0, 0, overlayWidth, overlayHeight };
    ::AdjustWindowRectEx(&wr, style, FALSE, exStyle);

    const int wndW = wr.right - wr.left;
    const int wndH = wr.bottom - wr.top;

    g_hwnd = ::CreateWindowEx(
        exStyle,
        wc.lpszClassName, _T("CowAI"),
        style,
        0, 0, wndW, wndH,
        NULL, NULL, wc.hInstance, NULL);

    if (g_hwnd == NULL)
        return false;

    BOOL dwm = FALSE;
    if (SUCCEEDED(DwmIsCompositionEnabled(&dwm)) && dwm)
    {
        MARGINS m = { -1, -1, -1, -1 };
        DwmExtendFrameIntoClientArea(g_hwnd, &m);
    }

    if (config.overlay_opacity <= 20)  config.overlay_opacity = 20;
    if (config.overlay_opacity >= 256) config.overlay_opacity = 255;

    Overlay_SetOpacity(config.overlay_opacity);

    if (!CreateDeviceD3D(g_hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return false;
    }

    // 创建托盘图标
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize = sizeof(NOTIFYICONDATA);
    g_nid.hWnd = g_hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = hIcon ? hIcon : LoadIcon(NULL, IDI_APPLICATION);
    wcscpy_s(g_nid.szTip, L"CowAI - 双击显示/隐藏");
    
    if (Shell_NotifyIcon(NIM_ADD, &g_nid)) {
        g_trayIconAdded = true;
        g_hTrayIcon = hIcon;
    }

    return true;
}

// 移除托盘图标
void RemoveTrayIcon()
{
    if (g_trayIconAdded) {
        Shell_NotifyIcon(NIM_DELETE, &g_nid);
        g_trayIconAdded = false;
    }
    if (g_hTrayIcon) {
        DestroyIcon(g_hTrayIcon);
        g_hTrayIcon = NULL;
    }
}

void OverlayThread()
{
    if (!CreateOverlayWindow())
    {
        std::cout << "[覆盖层] 无法创建覆盖层窗口！" << std::endl;
        return;
    }

    SetupImGui();

    overlayVisible.store(true);
    
    // 默认显示配置面板
    ShowWindow(g_hwnd, SW_SHOW);
    SetForegroundWindow(g_hwnd);

    for (const auto& pair : KeyCodes::key_code_map)
        key_names.push_back(pair.first);

    std::sort(key_names.begin(), key_names.end());
    key_names_cstrs.reserve(key_names.size());
    for (const auto& name : key_names)
        key_names_cstrs.push_back(name.c_str());

    availableModels = getAvailableModels(false);

    MSG msg;
    ZeroMemory(&msg, sizeof(msg));

    while (!shouldExit)
    {
        while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
            {
                shouldExit = true;
                break;
            }
        }
        if (shouldExit) break;

        if (isAnyKeyPressed(config.button_open_overlay) & 0x1)
        {
            bool currentVisible = overlayVisible.load();
            overlayVisible.store(!currentVisible);

            if (overlayVisible.load())
            {
                ShowWindow(g_hwnd, SW_SHOW);
                SetForegroundWindow(g_hwnd);
            }
            else
            {
                ShowWindow(g_hwnd, SW_HIDE);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        if (!overlayVisible.load())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        const float w = (float)overlayWidth;
        const float h = (float)overlayHeight;
        const float drag_h = (float)DRAG_BAR_HEIGHT_PX;
        const float content_h = (h > drag_h + 10) ? (h - drag_h) : 10.0f;  // 确保最小高度
        const float sidebar_w = 100.0f;  // 稍微加宽侧边栏

        // ========== 单一主窗口 ==========
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        
        bool tempVisible = overlayVisible.load();
        ImGui::Begin("##main", &tempVisible,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings);
        if (!tempVisible) overlayVisible.store(false);

        // ========== 顶部标题栏 - 现代化设计 ==========
        ImGui::SetCursorPos(ImVec2(0, 0));
        ImDrawList* title_dl = ImGui::GetWindowDrawList();
        ImVec2 title_p = ImGui::GetCursorScreenPos();
        
        // 标题栏渐变背景
        title_dl->AddRectFilledMultiColor(
            title_p, ImVec2(title_p.x + w, title_p.y + drag_h),
            IM_COL32(15, 18, 28, 250), IM_COL32(15, 18, 28, 250),
            IM_COL32(22, 27, 38, 250), IM_COL32(22, 27, 38, 250)
        );
        
        // 底部发光线
        title_dl->AddLine(
            ImVec2(title_p.x, title_p.y + drag_h - 1),
            ImVec2(title_p.x + w, title_p.y + drag_h - 1),
            IM_COL32(99, 102, 241, 60), 1.0f
        );
        
        ImGui::BeginChild("##titlebar", ImVec2(w, drag_h), false, ImGuiWindowFlags_NoScrollbar);
        
        // Logo/图标区域
        float logoSize = 20.0f;
        float logoX = title_p.x + 12;
        float logoY = title_p.y + (drag_h - logoSize) * 0.5f;
        
        if (g_logoTexture) {
            // 使用图片 Logo
            title_dl->AddImageRounded(
                (ImTextureID)g_logoTexture,
                ImVec2(logoX, logoY),
                ImVec2(logoX + logoSize, logoY + logoSize),
                ImVec2(0, 0), ImVec2(1, 1),
                IM_COL32(255, 255, 255, 255), 4.0f
            );
        } else {
            // 备用：绘制圆形
            title_dl->AddCircleFilled(
                ImVec2(logoX + logoSize * 0.5f, logoY + logoSize * 0.5f),
                logoSize * 0.4f, IM_COL32(99, 102, 241, 255)
            );
        }
        
        // 标题文字
        ImVec2 title_size = ImGui::CalcTextSize(u8"CowAI");
        ImGui::SetCursorPos(ImVec2(40, (drag_h - title_size.y) * 0.5f));
        ImGui::TextColored(ImVec4(0.97f, 0.98f, 0.99f, 1.0f), u8"CowAI");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.58f, 0.64f, 0.72f, 1.0f), u8"Panel");
        
        // 显示到期时间
        ImGui::SameLine();
        ImGui::SetCursorPosX(200);
        auto& auth = TGAuth::AuthManager::getInstance();
        int64_t expires_at = auth.getKeyExpiresAt();
        if (expires_at == 0) {
            ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), u8"[永久授权]");
        } else if (expires_at > 0) {
            std::time_t t = static_cast<std::time_t>(expires_at);
            std::tm tm_info;
            localtime_s(&tm_info, &t);
            char buf[64];
            std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm_info);
            ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.3f, 1.0f), u8"到期: %s", buf);
        } else {
            // expires_at < 0 表示解析失败，显示已授权但不显示具体时间
            ImGui::TextColored(ImVec4(0.4f, 0.7f, 0.9f, 1.0f), u8"[已授权]");
        }
        
        ImGui::EndChild();

        // ========== 侧边栏 - 现代化导航 ==========
        static int selected_tab = 0;
        if (selected_tab < 0 || selected_tab > 10) selected_tab = 0;  // 范围保护
        
        // 导航项：图标 + 标签
        static const char* nav_icons[] = {
            u8"[C]", u8"[T]", u8"[M]", u8"[A]", u8"[K]",
            /* u8"[U]", u8"[G]", */ u8"[P]", u8"[S]", u8"[D]"
        };
        static const char* nav_labels[] = {
            u8"捕获", u8"目标", u8"鼠标", u8"AI", u8"按键",
            /* u8"外观", u8"游戏层", */ u8"拾取", u8"武器识别", u8"统计", u8"调试"
        };
        
        // 标签索引映射：nav index -> 实际 draw 函数
        static const int tab_mapping[] = { 0, 1, 2, 3, 4, /* 5=外观, 6=游戏层 */ 7, 8, 9, 10 };
        static const int nav_count = 9;
        
        ImGui::SetCursorPos(ImVec2(0, drag_h));
        ImDrawList* sidebar_dl = ImGui::GetWindowDrawList();
        ImVec2 sidebar_p = ImGui::GetCursorScreenPos();
        
        // 侧边栏背景
        sidebar_dl->AddRectFilled(
            sidebar_p, ImVec2(sidebar_p.x + sidebar_w, sidebar_p.y + content_h),
            IM_COL32(12, 14, 20, 250), 0.0f
        );
        
        // 右侧分隔线
        sidebar_dl->AddLine(
            ImVec2(sidebar_p.x + sidebar_w - 1, sidebar_p.y),
            ImVec2(sidebar_p.x + sidebar_w - 1, sidebar_p.y + content_h),
            IM_COL32(255, 255, 255, 15), 1.0f
        );
        
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 12));
        ImGui::BeginChild("##sidebar", ImVec2(sidebar_w, content_h), false, ImGuiWindowFlags_NoScrollbar);
        
        for (int i = 0; i < nav_count; i++)
        {
            bool is_selected = (selected_tab == i);
            
            ImVec2 btn_pos = ImGui::GetCursorScreenPos();
            float btn_w = sidebar_w - 16;
            float btn_h = 32;
            
            // 选中状态背景
            if (is_selected)
            {
                sidebar_dl->AddRectFilled(
                    ImVec2(btn_pos.x, btn_pos.y),
                    ImVec2(btn_pos.x + btn_w, btn_pos.y + btn_h),
                    IM_COL32(99, 102, 241, 40), 6.0f
                );
                // 左侧指示条
                sidebar_dl->AddRectFilled(
                    ImVec2(btn_pos.x - 4, btn_pos.y + 4),
                    ImVec2(btn_pos.x - 1, btn_pos.y + btn_h - 4),
                    IM_COL32(99, 102, 241, 255), 2.0f
                );
            }
            
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.39f, 0.40f, 0.95f, 0.15f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.39f, 0.40f, 0.95f, 0.25f));
            
            if (ImGui::Button(nav_labels[i], ImVec2(btn_w, btn_h)))
                selected_tab = i;
            
            ImGui::PopStyleColor(3);
        }
        
        // 底部版本信息
        ImGui::SetCursorPosY(content_h - 30);
        ImGui::TextColored(ImVec4(0.4f, 0.45f, 0.55f, 1.0f), u8"  v%s", APP_VERSION);
        
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        // ========== 主内容区域 ==========
        ImGui::SetCursorPos(ImVec2(sidebar_w, drag_h));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 14));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.055f, 0.08f, 0.95f));
        ImGui::BeginChild("##content", ImVec2(w - sidebar_w, content_h), false);
        
        {
            std::lock_guard<std::mutex> lock(configMutex);
            int mapped = (selected_tab >= 0 && selected_tab < nav_count) ? tab_mapping[selected_tab] : 0;
            switch (mapped)
            {
            case 0: draw_capture_settings(); break;
            case 1: draw_target(); break;
            case 2: draw_mouse(); break;
            case 3: draw_ai(); break;
            case 4: draw_buttons(); break;
            // case 5: draw_overlay(); break;
            // case 6: draw_game_overlay_settings(); break;
            case 7: draw_pickup_settings(); break;
            case 8: draw_weapon_recognition_settings(); break;
            case 9: draw_stats(); break;
            case 10: draw_debug(); break;
            }
        }
        
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::Render();

        const float clear_color_with_alpha[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        HRESULT result = g_pSwapChain->Present(0, 0);
        if (result == DXGI_STATUS_OCCLUDED || result == DXGI_ERROR_ACCESS_LOST)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    release_body_texture();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    RemoveTrayIcon();
    CleanupDeviceD3D();
    ::DestroyWindow(g_hwnd);
    ::UnregisterClass(_T("CowAIOverlay"), GetModuleHandle(NULL));
}

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPTSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    std::thread overlay(OverlayThread);
    overlay.join();
    return 0;
}