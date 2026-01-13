#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <winsock2.h>
#include <Windows.h>

#include <tchar.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <d3d11.h>
#include <dxgi.h>
#include <dwmapi.h>
#include <dcomp.h>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>
#include <chrono>

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

#ifdef USE_CUDA
#include "trt_detector.h"
#endif

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "d3d11.lib")

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

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();

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

std::atomic<bool> overlayVisible(false);

ID3D11ShaderResourceView* body_texture = nullptr;

void load_body_texture();
void release_body_texture();
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

    if (g_dcompVisual) { g_dcompVisual->Release(); g_dcompVisual = NULL; }
    if (g_dcompTarget) { g_dcompTarget->Release(); g_dcompTarget = NULL; }
    if (g_dcompDevice) { g_dcompDevice->Release(); g_dcompDevice = NULL; }

    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
    if (g_pBlendState) { g_pBlendState->Release(); g_pBlendState = nullptr; }
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

    Themes::ApplyTheme_CyberTeal();  // 默认主题
    ApplyThemeByIndex(config.overlay_theme);  // 应用用户选择的主题
    load_body_texture();
}

bool CreateOverlayWindow()
{
    overlayWidth = static_cast<int>(BASE_OVERLAY_WIDTH * config.overlay_ui_scale);
    overlayHeight = static_cast<int>(BASE_OVERLAY_HEIGHT * config.overlay_ui_scale);

    WNDCLASSEX wc = {
        sizeof(WNDCLASSEX),
        CS_CLASSDC,
        WndProc,
        0L,
        0L,
        GetModuleHandle(NULL),
        NULL,
        NULL,
        NULL,
        NULL,
        _T("Chrome"),
        NULL
    };
    ::RegisterClassEx(&wc);

    const DWORD exStyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED;
    const DWORD style = WS_POPUP;

    RECT wr = { 0, 0, overlayWidth, overlayHeight };
    ::AdjustWindowRectEx(&wr, style, FALSE, exStyle);

    const int wndW = wr.right - wr.left;
    const int wndH = wr.bottom - wr.top;

    g_hwnd = ::CreateWindowEx(
        exStyle,
        wc.lpszClassName, _T("Chrome"),
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

    return true;
}

void OverlayThread()
{
    if (!CreateOverlayWindow())
    {
        std::cout << "[覆盖层] 无法创建覆盖层窗口！" << std::endl;
        return;
    }

    SetupImGui();

    overlayVisible.store(false);

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
        const float sidebar_w = 90.0f;

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

        // ========== 顶部标题栏 ==========
        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.09f, 0.16f, 0.95f));
        ImGui::BeginChild("##titlebar", ImVec2(w, drag_h), false, ImGuiWindowFlags_NoScrollbar);
        ImVec2 title_size = ImGui::CalcTextSize(u8"CowAI 配置面板");
        ImGui::SetCursorPos(ImVec2((w - title_size.x) * 0.5f, (drag_h - title_size.y) * 0.5f));
        ImGui::TextColored(ImVec4(0.08f, 0.72f, 0.65f, 1.0f), u8"CowAI 配置面板");
        ImGui::EndChild();
        ImGui::PopStyleColor();

        // ========== 侧边栏 ==========
        static int selected_tab = 0;
        if (selected_tab < 0 || selected_tab > 8) selected_tab = 0;  // 范围保护
        
        static const char* nav_labels[] = {
            u8"捕获", u8"目标", u8"鼠标", u8"AI", u8"按键",
            u8"外观", u8"游戏层", u8"统计", u8"调试"
        };
        
        ImGui::SetCursorPos(ImVec2(0, drag_h));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.04f, 0.06f, 0.12f, 0.98f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 10));
        ImGui::BeginChild("##sidebar", ImVec2(sidebar_w, content_h), false, ImGuiWindowFlags_NoScrollbar);
        
        for (int i = 0; i < 9; i++)
        {
            bool is_selected = (selected_tab == i);
            if (is_selected)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.08f, 0.72f, 0.65f, 0.3f));
            else
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            
            if (ImGui::Button(nav_labels[i], ImVec2(sidebar_w - 12, 28)))
                selected_tab = i;
            
            if (is_selected)
            {
                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 mn = ImGui::GetItemRectMin();
                ImVec2 mx = ImGui::GetItemRectMax();
                dl->AddRectFilled(ImVec2(mn.x - 3, mn.y + 3), ImVec2(mn.x, mx.y - 3), IM_COL32(20, 184, 166, 255), 1.0f);
            }
            
            ImGui::PopStyleColor();
        }
        
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        // ========== 主内容区域 ==========
        ImGui::SetCursorPos(ImVec2(sidebar_w, drag_h));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 10));
        ImGui::BeginChild("##content", ImVec2(w - sidebar_w, content_h), false);
        
        {
            std::lock_guard<std::mutex> lock(configMutex);
            switch (selected_tab)
            {
            case 0: draw_capture_settings(); break;
            case 1: draw_target(); break;
            case 2: draw_mouse(); break;
            case 3: draw_ai(); break;
            case 4: draw_buttons(); break;
            case 5: draw_overlay(); break;
            case 6: draw_game_overlay_settings(); break;
            case 7: draw_stats(); break;
            case 8: draw_debug(); break;
            }
        }
        
        ImGui::EndChild();
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

    CleanupDeviceD3D();
    ::DestroyWindow(g_hwnd);
    ::UnregisterClass(_T("Chrome"), GetModuleHandle(NULL));
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