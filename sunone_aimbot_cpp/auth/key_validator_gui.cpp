#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <winsock2.h>
#include <Windows.h>
#include <windowsx.h>
#include <shellapi.h>

#include <d3d11.h>
#include <dxgi.h>
#include <dwmapi.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <tchar.h>
#include <string>
#include <chrono>
#include <thread>
#include <cmath>
#include <algorithm>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include "key_validator_gui.h"
#include "tg_auth.h"
#include "version/version.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "windowscodecs.lib")

using Microsoft::WRL::ComPtr;
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace KeyValidatorGUI {

static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
static HWND g_hwnd = nullptr;

static ID3D11ShaderResourceView* g_logoTexture = nullptr;
static int g_logoWidth = 0;
static int g_logoHeight = 0;

#define WM_TRAYICON_VALIDATOR (WM_USER + 100)
static NOTIFYICONDATA g_nid = {};
static bool g_trayIconAdded = false;
static HICON g_hTrayIcon = nullptr;

static const int WINDOW_WIDTH = 440;
static const int WINDOW_HEIGHT = 500;

static bool g_shouldClose = false;
static bool g_validationSuccess = false;
static ValidationInfo g_validationInfo;
static char g_keyInput[64] = "";
static std::string g_statusMessage = "";
static bool g_isValidating = false;
static float g_statusColor[3] = {0.5f, 0.5f, 0.5f};
static float g_animTime = 0.0f;
static float g_inputFocusAnim = 0.0f;
static bool g_inputFocused = false;

// 前向声明
static bool CreateDeviceD3D(HWND hWnd);
static void CleanupDeviceD3D();
static void CreateRenderTarget();
static void CleanupRenderTarget();
static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static bool LoadLogoTexture(const char* path);
static void CleanupLogoTexture();

static ImVec4 RGBA(int r, int g, int b, int a = 255) {
    return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

// ========== iOS 26 真实风格绘制 ==========

// iOS 26 卡片: 纯白色, 大圆角, 极微妙阴影
static void DrawiOS26Card(ImDrawList* dl, ImVec2 pMin, ImVec2 pMax, float rounding) {
    // 极微妙的阴影 (iOS 26 几乎看不到阴影)
    for (int i = 3; i >= 1; i--) {
        float expand = (float)i * 1.5f;
        dl->AddRectFilled(
            ImVec2(pMin.x - expand, pMin.y - expand + 1.5f),
            ImVec2(pMax.x + expand, pMax.y + expand + 1.5f),
            IM_COL32(0, 0, 0, 3 + (3 - i) * 2), rounding + expand);
    }
    // 纯白色卡片
    dl->AddRectFilled(pMin, pMax, IM_COL32(255, 255, 255, 255), rounding);
}

// iOS 26 分隔线: 极细, 左侧有缩进
static void DrawiOS26Separator(ImDrawList* dl, float x, float y, float width, float indent = 0.0f) {
    dl->AddLine(ImVec2(x + indent, y), ImVec2(x + width, y), IM_COL32(0, 0, 0, 18), 0.5f);
}

// ========== Logo 加载 ==========
static bool LoadLogoTexture(const char* path) {
    if (!g_pd3dDevice) return false;
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);
    std::wstring wpath(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, path, -1, &wpath[0], wlen);
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return false;
    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(wpath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) return false;
    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) return false;
    UINT width, height;
    frame->GetSize(&width, &height);
    ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) return false;
    hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) return false;
    std::vector<uint8_t> pixels(width * height * 4);
    hr = converter->CopyPixels(nullptr, width * 4, (UINT)pixels.size(), pixels.data());
    if (FAILED(hr)) return false;
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width; desc.Height = height; desc.MipLevels = 1; desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = pixels.data(); initData.SysMemPitch = width * 4;
    ID3D11Texture2D* texture = nullptr;
    hr = g_pd3dDevice->CreateTexture2D(&desc, &initData, &texture);
    if (FAILED(hr)) return false;
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format; srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    hr = g_pd3dDevice->CreateShaderResourceView(texture, &srvDesc, &g_logoTexture);
    texture->Release();
    if (FAILED(hr)) return false;
    g_logoWidth = width; g_logoHeight = height;
    return true;
}

static void CleanupLogoTexture() {
    if (g_logoTexture) { g_logoTexture->Release(); g_logoTexture = nullptr; }
    g_logoWidth = 0; g_logoHeight = 0;
}

// ========== iOS 26 真实主题 ==========
static void ApplyAppleTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.ChildRounding = 16.0f;
    style.PopupRounding = 16.0f;
    style.FrameRounding = 10.0f;
    style.ScrollbarRounding = 10.0f;
    style.GrabRounding = 8.0f;
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowPadding = ImVec2(0, 0);
    style.FramePadding = ImVec2(16, 12);
    style.ItemSpacing = ImVec2(12, 10);

    ImVec4* c = style.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg] = RGBA(255, 255, 255, 255);
    c[ImGuiCol_Text] = RGBA(0, 0, 0);                  // iOS 纯黑文字
    c[ImGuiCol_TextDisabled] = RGBA(138, 138, 142);     // iOS secondaryLabel
    c[ImGuiCol_Border] = RGBA(0, 0, 0, 0);
    c[ImGuiCol_BorderShadow] = RGBA(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = RGBA(242, 242, 247);          // iOS systemGray6
    c[ImGuiCol_FrameBgHovered] = RGBA(235, 235, 240);
    c[ImGuiCol_FrameBgActive] = RGBA(229, 229, 234);
    c[ImGuiCol_TitleBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TitleBgActive] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_Button] = RGBA(0, 122, 255);             // iOS systemBlue
    c[ImGuiCol_ButtonHovered] = RGBA(0, 112, 240);
    c[ImGuiCol_ButtonActive] = RGBA(0, 100, 215);
}

// ========== D3D11 ==========
static bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate = {60, 1};
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc = {1, 0};
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL fl;
    const D3D_FEATURE_LEVEL fls[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        fls, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &fl, &g_pd3dDeviceContext)))
        return false;
    CreateRenderTarget();
    return true;
}
static void CleanupDeviceD3D() {
    CleanupRenderTarget(); CleanupLogoTexture();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}
static void CreateRenderTarget() {
    ID3D11Texture2D* bb; g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&bb));
    g_pd3dDevice->CreateRenderTargetView(bb, nullptr, &g_mainRenderTargetView); bb->Release();
}
static void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}
static void RemoveTrayIcon() {
    if (g_trayIconAdded) { Shell_NotifyIcon(NIM_DELETE, &g_nid); g_trayIconAdded = false; }
}

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_NCHITTEST: {
        POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(hWnd, &pt);
        if (pt.y < 60 && pt.x < WINDOW_WIDTH - 50) return HTCAPTION;
    } break;
    case WM_TRAYICON_VALIDATOR:
        if (lParam == WM_LBUTTONDBLCLK) { ShowWindow(hWnd, SW_SHOW); SetForegroundWindow(hWnd); }
        return 0;
    case WM_CLOSE:
        g_shouldClose = true;
        return 0;
    case WM_DESTROY:
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// ========== 验证逻辑 ==========
static void DoValidation() {
    g_isValidating = true;
    g_statusMessage = "\xE6\xAD\xA3\xE5\x9C\xA8\xE9\xAA\x8C\xE8\xAF\x81...";  // 正在验证...
    g_statusColor[0] = 0.0f; g_statusColor[1] = 0.48f; g_statusColor[2] = 1.0f;  // Apple 蓝

    auto& auth = TGAuth::AuthManager::getInstance();
    std::string keyCode = g_keyInput;
    size_t start = keyCode.find_first_not_of(" \t\n\r");
    size_t end = keyCode.find_last_not_of(" \t\n\r");
    if (start != std::string::npos && end != std::string::npos)
        keyCode = keyCode.substr(start, end - start + 1);

    if (keyCode.empty()) {
        g_statusMessage = "\xE8\xAF\xB7\xE8\xBE\x93\xE5\x85\xA5\xE5\x8D\xA1\xE5\xAF\x86";  // 请输入卡密
        g_statusColor[0] = 1.0f; g_statusColor[1] = 0.23f; g_statusColor[2] = 0.19f;  // Apple 红
        g_isValidating = false;
        return;
    }

    TGAuth::AuthResult result = auth.activateKey(keyCode);
    g_isValidating = false;

    switch (result.status) {
    case TGAuth::AuthStatus::AUTHORIZED:
        g_validationSuccess = true;
        g_validationInfo.result = ValidationResult::RESULT_SUCCESS;
        g_validationInfo.key_type = result.key_type;
        g_validationInfo.expires_at = result.key_expires_at;
        g_statusMessage = "\xE9\xAA\x8C\xE8\xAF\x81\xE6\x88\x90\xE5\x8A\x9F";  // 验证成功
        g_statusColor[0] = 0.2f; g_statusColor[1] = 0.78f; g_statusColor[2] = 0.35f;  // Apple 绿
        std::thread([]() { std::this_thread::sleep_for(std::chrono::milliseconds(1000)); g_shouldClose = true; }).detach();
        break;
    case TGAuth::AuthStatus::KEY_INVALID:
        g_validationInfo.result = ValidationResult::RESULT_KEY_INVALID;
        g_statusMessage = "\xE5\x8D\xA1\xE5\xAF\x86\xE6\x97\xA0\xE6\x95\x88";
        g_statusColor[0] = 1.0f; g_statusColor[1] = 0.23f; g_statusColor[2] = 0.19f;
        break;
    case TGAuth::AuthStatus::KEY_EXPIRED:
        g_validationInfo.result = ValidationResult::RESULT_KEY_EXPIRED;
        g_statusMessage = "\xE5\x8D\xA1\xE5\xAF\x86\xE5\xB7\xB2\xE8\xBF\x87\xE6\x9C\x9F";
        g_statusColor[0] = 1.0f; g_statusColor[1] = 0.58f; g_statusColor[2] = 0.0f;  // Apple 橙
        break;
    case TGAuth::AuthStatus::KEY_REVOKED:
        g_validationInfo.result = ValidationResult::RESULT_KEY_REVOKED;
        g_statusMessage = "\xE5\x8D\xA1\xE5\xAF\x86\xE5\xB7\xB2\xE8\xA2\xAB\xE6\x92\xA4\xE9\x94\x80";
        g_statusColor[0] = 1.0f; g_statusColor[1] = 0.23f; g_statusColor[2] = 0.19f;
        break;
    case TGAuth::AuthStatus::DEVICE_LIMIT_EXCEEDED:
        g_validationInfo.result = ValidationResult::RESULT_DEVICE_LIMIT;
        g_statusMessage = "\xE8\xAE\xBE\xE5\xA4\x87\xE6\x95\xB0\xE9\x87\x8F\xE5\xB7\xB2\xE8\xBE\xBE\xE4\xB8\x8A\xE9\x99\x90";
        g_statusColor[0] = 1.0f; g_statusColor[1] = 0.58f; g_statusColor[2] = 0.0f;
        break;
    case TGAuth::AuthStatus::NETWORK_ERROR:
        g_validationInfo.result = ValidationResult::RESULT_NETWORK_ERROR;
        g_statusMessage = "\xE7\xBD\x91\xE7\xBB\x9C\xE9\x94\x99\xE8\xAF\xAF";
        g_statusColor[0] = 1.0f; g_statusColor[1] = 0.58f; g_statusColor[2] = 0.0f;
        break;
    default:
        g_validationInfo.result = ValidationResult::RESULT_ERROR;
        g_validationInfo.error_message = result.error_message;
        g_statusMessage = result.error_message.empty() ? "\xE9\xAA\x8C\xE8\xAF\x81\xE5\xA4\xB1\xE8\xB4\xA5" : result.error_message;
        g_statusColor[0] = 1.0f; g_statusColor[1] = 0.23f; g_statusColor[2] = 0.19f;
        break;
    }
}

bool HasValidAuthorization() {
    auto& auth = TGAuth::AuthManager::getInstance();
    if (auth.getStatus() == TGAuth::AuthStatus::NOT_INITIALIZED) return false;
    return auth.checkAuthorization().status == TGAuth::AuthStatus::AUTHORIZED;
}

// ========== 主窗口 ==========
bool ShowKeyValidatorWindow(ValidationInfo& info) {
    g_shouldClose = false; g_validationSuccess = false; g_isValidating = false;
    g_statusMessage = ""; g_animTime = 0.0f; g_inputFocusAnim = 0.0f;
    g_statusColor[0] = 0.5f; g_statusColor[1] = 0.5f; g_statusColor[2] = 0.5f;
    memset(g_keyInput, 0, sizeof(g_keyInput));
    g_validationInfo = ValidationInfo();
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    // 加载图标
    HICON hIcon = NULL;
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string iconPath = exePath;
    size_t lastSlash = iconPath.find_last_of("\\/");
    iconPath = (lastSlash != std::string::npos) ? iconPath.substr(0, lastSlash + 1) + "Cow.png" : "Cow.png";
    
    std::string icoPath = iconPath.substr(0, iconPath.length() - 4) + ".ico";
    hIcon = (HICON)LoadImageA(NULL, icoPath.c_str(), IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
    if (!hIcon) {
        int wlen = MultiByteToWideChar(CP_ACP, 0, iconPath.c_str(), -1, nullptr, 0);
        std::wstring wpath(wlen, 0);
        MultiByteToWideChar(CP_ACP, 0, iconPath.c_str(), -1, &wpath[0], wlen);
        ComPtr<IWICImagingFactory> factory;
        if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) {
            ComPtr<IWICBitmapDecoder> decoder;
            if (SUCCEEDED(factory->CreateDecoderFromFilename(wpath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder))) {
                ComPtr<IWICBitmapFrameDecode> frame;
                if (SUCCEEDED(decoder->GetFrame(0, &frame))) {
                    ComPtr<IWICFormatConverter> converter;
                    if (SUCCEEDED(factory->CreateFormatConverter(&converter)) &&
                        SUCCEEDED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom))) {
                        const UINT iconSize = 32;
                        ComPtr<IWICBitmapScaler> scaler;
                        if (SUCCEEDED(factory->CreateBitmapScaler(&scaler)) &&
                            SUCCEEDED(scaler->Initialize(converter.Get(), iconSize, iconSize, WICBitmapInterpolationModeHighQualityCubic))) {
                            std::vector<BYTE> pixels(iconSize * iconSize * 4);
                            if (SUCCEEDED(scaler->CopyPixels(nullptr, iconSize * 4, (UINT)pixels.size(), pixels.data()))) {
                                BITMAPV5HEADER bi = {};
                                bi.bV5Size = sizeof(bi); bi.bV5Width = iconSize; bi.bV5Height = -(LONG)iconSize;
                                bi.bV5Planes = 1; bi.bV5BitCount = 32; bi.bV5Compression = BI_BITFIELDS;
                                bi.bV5RedMask = 0x00FF0000; bi.bV5GreenMask = 0x0000FF00;
                                bi.bV5BlueMask = 0x000000FF; bi.bV5AlphaMask = 0xFF000000;
                                HDC hdc = GetDC(NULL); void* bits = nullptr;
                                HBITMAP hBitmap = CreateDIBSection(hdc, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &bits, NULL, 0);
                                if (hBitmap && bits) {
                                    memcpy(bits, pixels.data(), pixels.size());
                                    HBITMAP hMask = CreateBitmap(iconSize, iconSize, 1, 1, NULL);
                                    ICONINFO ii = {}; ii.fIcon = TRUE; ii.hbmMask = hMask; ii.hbmColor = hBitmap;
                                    hIcon = CreateIconIndirect(&ii);
                                    DeleteObject(hMask); DeleteObject(hBitmap);
                                }
                                ReleaseDC(NULL, hdc);
                            }
                        }
                    }
                }
            }
        }
    }

    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L,
        GetModuleHandle(nullptr), hIcon, nullptr, nullptr, nullptr, _T("CowAIKeyValidator"), hIcon };
    RegisterClassEx(&wc);

    int posX = (GetSystemMetrics(SM_CXSCREEN) - WINDOW_WIDTH) / 2;
    int posY = (GetSystemMetrics(SM_CYSCREEN) - WINDOW_HEIGHT) / 2;
    g_hwnd = CreateWindowEx(WS_EX_TOPMOST | WS_EX_APPWINDOW, wc.lpszClassName, _T("CowAI"),
        WS_POPUP | WS_VISIBLE, posX, posY, WINDOW_WIDTH, WINDOW_HEIGHT, nullptr, nullptr, wc.hInstance, nullptr);
    if (!g_hwnd) { info.result = ValidationResult::RESULT_ERROR; info.error_message = "Failed to create window"; return false; }

    if (hIcon) { SendMessage(g_hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon); SendMessage(g_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon); }

    // DWM 圆角
    HMODULE dwmapi = LoadLibrary(_T("dwmapi.dll"));
    if (dwmapi) {
        typedef HRESULT(WINAPI* Fn)(HWND, DWORD, LPCVOID, DWORD);
        auto fn = (Fn)GetProcAddress(dwmapi, "DwmSetWindowAttribute");
        if (fn) { DWORD v = 2; fn(g_hwnd, 33, &v, sizeof(v)); }
        FreeLibrary(dwmapi);
    }

    // 托盘
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize = sizeof(NOTIFYICONDATA); g_nid.hWnd = g_hwnd; g_nid.uID = 2;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON_VALIDATOR;
    g_nid.hIcon = hIcon ? hIcon : LoadIcon(NULL, IDI_APPLICATION);
    wcscpy_s(g_nid.szTip, L"CowAI");
    if (Shell_NotifyIcon(NIM_ADD, &g_nid)) { g_trayIconAdded = true; g_hTrayIcon = hIcon; }

    if (!CreateDeviceD3D(g_hwnd)) {
        CleanupDeviceD3D(); DestroyWindow(g_hwnd); UnregisterClass(wc.lpszClassName, wc.hInstance);
        info.result = ValidationResult::RESULT_ERROR; info.error_message = "Failed to create D3D device"; return false;
    }
    LoadLogoTexture(iconPath.c_str());
    ShowWindow(g_hwnd, SW_SHOWDEFAULT); UpdateWindow(g_hwnd);

    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); io.IniFilename = nullptr; io.LogFilename = nullptr;
    const char* fontPaths[] = {"C:\\Windows\\Fonts\\msyh.ttc", "C:\\Windows\\Fonts\\simhei.ttf"};
    bool fontLoaded = false;
    for (auto fp : fontPaths) {
        if (GetFileAttributesA(fp) != INVALID_FILE_ATTRIBUTES) {
            io.Fonts->AddFontFromFileTTF(fp, 15.0f, nullptr, io.Fonts->GetGlyphRangesChineseFull());
            fontLoaded = true; break;
        }
    }
    if (!fontLoaded) io.Fonts->AddFontDefault();
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
    ApplyAppleTheme();

    MSG msg; ZeroMemory(&msg, sizeof(msg));
    auto lastTime = std::chrono::steady_clock::now();

    while (!g_shouldClose) {
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg); DispatchMessage(&msg);
            if (msg.message == WM_QUIT) g_shouldClose = true;
        }
        if (g_shouldClose) break;

        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;
        g_animTime += dt;

        float focusTarget = g_inputFocused ? 1.0f : 0.0f;
        g_inputFocusAnim += (focusTarget - g_inputFocusAnim) * dt * 10.0f;

        ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2((float)WINDOW_WIDTH, (float)WINDOW_HEIGHT));
        ImGui::Begin("##v", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wp = ImGui::GetWindowPos();
        float W = (float)WINDOW_WIDTH, H = (float)WINDOW_HEIGHT;

        // ========== 背景: iOS 26 浅灰色 (systemGroupedBackground) ==========
        dl->AddRectFilled(wp, ImVec2(wp.x + W, wp.y + H), IM_COL32(242, 242, 247, 255));

        // ========== Logo ==========
        float logoSize = 52.0f;
        float logoCX = wp.x + W / 2.0f;
        float logoCY = wp.y + 44.0f;

        // Logo 圆形容器 - iOS 26 风格纯白圆形
        float logoR = logoSize / 2 + 4;
        // 微妙阴影
        dl->AddCircleFilled(ImVec2(logoCX, logoCY + 1.0f), logoR + 1.0f, IM_COL32(0, 0, 0, 8), 48);
        dl->AddCircleFilled(ImVec2(logoCX, logoCY), logoR, IM_COL32(255, 255, 255, 255), 48);

        if (g_logoTexture) {
            dl->AddImageRounded((ImTextureID)g_logoTexture,
                ImVec2(logoCX - logoSize/2, logoCY - logoSize/2),
                ImVec2(logoCX + logoSize/2, logoCY + logoSize/2),
                ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), logoSize/2);
        } else {
            dl->AddCircleFilled(ImVec2(logoCX, logoCY), logoSize/2, IM_COL32(0, 122, 255, 255), 32);
            dl->AddText(ImVec2(logoCX - 6, logoCY - 8), IM_COL32(255, 255, 255, 255), "C");
        }

        // ========== 标题 ==========
        ImGui::SetCursorPos(ImVec2(0, 78));
        const char* title = "CowAI";
        float titleW = ImGui::CalcTextSize(title).x;
        ImGui::SetCursorPosX((W - titleW) / 2.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, RGBA(0, 0, 0));
        ImGui::Text("%s", title);
        ImGui::PopStyleColor();

        // 副标题
        const char* subtitle = "\xE5\x8D\xA1\xE5\xAF\x86\xE9\xAA\x8C\xE8\xAF\x81";  // 卡密验证
        float subW = ImGui::CalcTextSize(subtitle).x;
        ImGui::SetCursorPosX((W - subW) / 2.0f);
        ImGui::TextColored(ImVec4(0.54f, 0.54f, 0.56f, 1.0f), "%s", subtitle);

        // ========== 关闭按钮 ==========
        ImGui::SetCursorPos(ImVec2(W - 40, 8));
        // iOS 26 风格: 灰色圆形背景 + X
        ImVec2 closeCenter = ImVec2(wp.x + W - 40 + 13, wp.y + 8 + 13);
        dl->AddCircleFilled(closeCenter, 13, IM_COL32(0, 0, 0, 15), 24);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, RGBA(0, 0, 0, 20));
        ImGui::PushStyleColor(ImGuiCol_Text, RGBA(138, 138, 142));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 13.0f);
        if (ImGui::Button("X", ImVec2(26, 26))) {
            g_shouldClose = true; g_validationInfo.result = ValidationResult::RESULT_CANCELLED;
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        // ========== 主卡片 (iOS 26 纯白卡片) ==========
        float cardMargin = 20.0f;
        float cardTop = 118.0f;
        float cardBottom = 340.0f;
        ImVec2 cardMin = ImVec2(wp.x + cardMargin, wp.y + cardTop);
        ImVec2 cardMax = ImVec2(wp.x + W - cardMargin, wp.y + cardBottom);
        DrawiOS26Card(dl, cardMin, cardMax, 16.0f);

        // 输入标签
        float padX = cardMargin + 16.0f;
        ImGui::SetCursorPos(ImVec2(padX, cardTop + 16.0f));
        ImGui::TextColored(ImVec4(0.24f, 0.24f, 0.26f, 0.6f), "\xE8\xAF\xB7\xE8\xBE\x93\xE5\x85\xA5\xE6\x82\xA8\xE7\x9A\x84\xE5\x8D\xA1\xE5\xAF\x86\xEF\xBC\x9A");  // 请输入您的卡密：

        // 输入框 - iOS 26 风格: systemGray6 背景, 大圆角
        float inputW = W - 2 * padX;
        ImGui::SetCursorPos(ImVec2(padX, cardTop + 40.0f));
        ImGui::PushItemWidth(inputW);

        // 输入框聚焦 - iOS 26 蓝色边框
        ImVec2 inputMin = ImVec2(wp.x + padX, wp.y + cardTop + 40.0f);
        ImVec2 inputMax = ImVec2(inputMin.x + inputW, inputMin.y + 38.0f);
        if (g_inputFocusAnim > 0.01f) {
            int focusAlpha = (int)(g_inputFocusAnim * 180);
            dl->AddRect(ImVec2(inputMin.x - 1.5f, inputMin.y - 1.5f), ImVec2(inputMax.x + 1.5f, inputMax.y + 1.5f),
                IM_COL32(0, 122, 255, focusAlpha), 11.0f, 0, 2.0f);
        }

        // iOS 26 输入框: 浅灰背景, 无边框
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14, 10));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, RGBA(242, 242, 247));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, RGBA(235, 235, 240));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, RGBA(229, 229, 234));
        ImGui::PushStyleColor(ImGuiCol_Border, RGBA(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

        bool enterPressed = ImGui::InputText("##key", g_keyInput, sizeof(g_keyInput), ImGuiInputTextFlags_EnterReturnsTrue);
        g_inputFocused = ImGui::IsItemActive();

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(4);
        ImGui::PopItemWidth();

        // 状态消息
        if (!g_statusMessage.empty()) {
            ImGui::SetCursorPos(ImVec2(padX, cardTop + 88.0f));
            ImGui::TextColored(ImVec4(g_statusColor[0], g_statusColor[1], g_statusColor[2], 1.0f), "%s", g_statusMessage.c_str());
        }

        // ========== 按钮 - iOS 26 风格 ==========
        float btnY = cardTop + 116.0f;
        float btnW = (inputW - 12.0f) / 2.0f;
        float btnH = 40.0f;

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 10));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);

        if (g_isValidating) {
            ImGui::SetCursorPos(ImVec2(padX, btnY));
            ImGui::PushStyleColor(ImGuiCol_Button, RGBA(0, 122, 255, 120));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, RGBA(0, 122, 255, 120));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, RGBA(0, 122, 255, 120));
            ImGui::PushStyleColor(ImGuiCol_Text, RGBA(255, 255, 255));
            const char* dots[] = {"\xE9\xAA\x8C\xE8\xAF\x81\xE4\xB8\xAD.", "\xE9\xAA\x8C\xE8\xAF\x81\xE4\xB8\xAD..", "\xE9\xAA\x8C\xE8\xAF\x81\xE4\xB8\xAD..."};
            ImGui::Button(dots[((int)(g_animTime * 3.0f)) % 3], ImVec2(inputW, btnH));
            ImGui::PopStyleColor(4);
        } else {
            // 激活按钮 - iOS systemBlue
            ImGui::SetCursorPos(ImVec2(padX, btnY));
            ImGui::PushStyleColor(ImGuiCol_Button, RGBA(0, 122, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, RGBA(0, 112, 240));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, RGBA(0, 100, 215));
            ImGui::PushStyleColor(ImGuiCol_Text, RGBA(255, 255, 255));
            if (ImGui::Button("\xE6\xBF\x80\xE6\xB4\xBB", ImVec2(btnW, btnH)) || enterPressed) {
                std::thread(DoValidation).detach();
            }
            ImGui::PopStyleColor(4);

            ImGui::SameLine(0, 12);

            // 取消按钮 - iOS systemGray5
            ImGui::PushStyleColor(ImGuiCol_Button, RGBA(229, 229, 234));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, RGBA(220, 220, 225));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, RGBA(210, 210, 215));
            ImGui::PushStyleColor(ImGuiCol_Text, RGBA(0, 0, 0));
            if (ImGui::Button("\xE5\x8F\x96\xE6\xB6\x88", ImVec2(btnW, btnH))) {
                g_shouldClose = true; g_validationInfo.result = ValidationResult::RESULT_CANCELLED;
            }
            ImGui::PopStyleColor(4);
        }
        ImGui::PopStyleVar(2);

        // ========== 购买卡密按钮 - iOS 26 橙色 ==========
        float buyY = cardBottom + 16.0f;
        float buyW = W - 2 * cardMargin;
        float buyH = 40.0f;
        ImVec2 buyMin = ImVec2(wp.x + cardMargin, wp.y + buyY);
        ImVec2 buyMax = ImVec2(buyMin.x + buyW, buyMin.y + buyH);
        // 微妙阴影
        for (int i = 2; i >= 1; i--) {
            float ex = (float)i * 1.5f;
            dl->AddRectFilled(ImVec2(buyMin.x - ex, buyMin.y - ex + 1.0f), ImVec2(buyMax.x + ex, buyMax.y + ex + 1.0f),
                IM_COL32(0, 0, 0, 3), 10.0f + ex);
        }

        ImGui::SetCursorPos(ImVec2(cardMargin, buyY));
        ImGui::PushStyleColor(ImGuiCol_Button, RGBA(255, 149, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, RGBA(255, 135, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, RGBA(230, 125, 0));
        ImGui::PushStyleColor(ImGuiCol_Text, RGBA(255, 255, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 10));
        if (ImGui::Button("\xE8\xB4\xAD\xE4\xB9\xB0\xE5\x8D\xA1\xE5\xAF\x86", ImVec2(buyW, buyH))) {
            ShellExecuteA(NULL, "open", "https://cowai.trueliu.com", NULL, NULL, SW_SHOWNORMAL);
        }
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);

        // ========== QQ群 (iOS 26 白色卡片) ==========
        float qqY = buyY + 52.0f;
        ImVec2 qqMin = ImVec2(wp.x + cardMargin, wp.y + qqY);
        ImVec2 qqMax = ImVec2(wp.x + W - cardMargin, wp.y + qqY + 40.0f);
        DrawiOS26Card(dl, qqMin, qqMax, 12.0f);

        // QQ 图标 - iOS 26 风格蓝色圆形
        float qx = wp.x + cardMargin + 12, qy = wp.y + qqY + 10;
        dl->AddCircleFilled(ImVec2(qx + 10, qy + 10), 10, IM_COL32(0, 122, 255, 255), 24);
        dl->AddText(ImVec2(qx + 5, qy + 3), IM_COL32(255, 255, 255, 255), "Q");

        ImGui::SetCursorPos(ImVec2(cardMargin + 36, qqY + 10));
        ImGui::TextColored(ImVec4(0.56f, 0.56f, 0.58f, 1.0f), "QQ\xE7\xBE\xA4\xEF\xBC\x9A");
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, RGBA(0, 0, 0, 8));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, RGBA(0, 0, 0, 15));
        ImGui::PushStyleColor(ImGuiCol_Text, RGBA(0, 122, 255));  // Apple 蓝色链接
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        if (ImGui::Button("972035976##qq")) {
            if (OpenClipboard(g_hwnd)) {
                EmptyClipboard();
                const char* qq = "972035976";
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, strlen(qq) + 1);
                if (hMem) { memcpy(GlobalLock(hMem), qq, strlen(qq) + 1); GlobalUnlock(hMem); SetClipboardData(CF_TEXT, hMem); }
                CloseClipboard();
                g_statusMessage = "\xE5\xB7\xB2\xE5\xA4\x8D\xE5\x88\xB6QQ\xE7\xBE\xA4\xE5\x8F\xB7";
                g_statusColor[0] = 0.2f; g_statusColor[1] = 0.78f; g_statusColor[2] = 0.35f;
            }
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("\xE7\x82\xB9\xE5\x87\xBB\xE5\xA4\x8D\xE5\x88\xB6");
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);

        // ========== 版本 ==========
        ImGui::SetCursorPos(ImVec2(0, H - 26));
        std::string ver = std::string("CowAI v") + APP_VERSION;
        float verW = ImGui::CalcTextSize(ver.c_str()).x;
        ImGui::SetCursorPosX((W - verW) / 2.0f);
        ImGui::TextColored(ImVec4(0.56f, 0.56f, 0.58f, 0.5f), "%s", ver.c_str());

        ImGui::End();

        ImGui::Render();
        const float cc[4] = {242.0f/255.0f, 242.0f/255.0f, 247.0f/255.0f, 1.0f};  // iOS systemGroupedBackground
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, cc);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
    }

    // 清理顺序很重要：先关 ImGui，再关 D3D，最后销毁窗口
    RemoveTrayIcon();
    CleanupLogoTexture();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    ::DestroyWindow(g_hwnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);
    g_hwnd = nullptr;

    // 排空残留消息，防止影响后续窗口的消息循环
    MSG drainMsg;
    while (PeekMessage(&drainMsg, nullptr, 0, 0, PM_REMOVE)) {
        // 消费所有残留消息
    }

    // 平衡 COM 引用计数
    CoUninitialize();

    info = g_validationInfo;
    return g_validationSuccess;
}

} // namespace KeyValidatorGUI
