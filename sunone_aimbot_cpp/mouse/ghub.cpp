#include <iostream>
#include <string>
#include <vector>

#include "ghub.h"

UINT GhubMouse::_ghub_SendInput(UINT nInputs, LPINPUT pInputs)
{
    return SendInput(nInputs, pInputs, sizeof(INPUT));
}

INPUT GhubMouse::_ghub_Input(MOUSEINPUT mi)
{
    INPUT input = { 0 };
    input.type = INPUT_MOUSE;
    input.mi = mi;
    return input;
}

MOUSEINPUT GhubMouse::_ghub_MouseInput(DWORD flags, LONG x, LONG y, DWORD data)
{
    MOUSEINPUT mi = { 0 };
    mi.dx = x;
    mi.dy = y;
    mi.mouseData = data;
    mi.dwFlags = flags;
    return mi;
}

INPUT GhubMouse::_ghub_Mouse(DWORD flags, LONG x, LONG y, DWORD data)
{
    return _ghub_Input(_ghub_MouseInput(flags, x, y, data));
}

GhubMouse::GhubMouse()
{
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    basedir = std::filesystem::path(buffer).parent_path();
    
    // DLL list to try (in order of preference)
    std::vector<std::string> dll_names = {"ghub_mouse.dll", "ghub_device.dll", "logitech.driver.dll", "MouseControl.dll"};
    
    gm = NULL;
    gmok = false;
    dllType = DllType::NONE;
    
    // Function pointers
    fn_moveR_2 = nullptr;
    fn_moveR_3 = nullptr;
    fn_press = nullptr;
    fn_release = nullptr;
    fn_mouse_down = nullptr;
    fn_mouse_up = nullptr;
    fn_mouse_close = nullptr;
    fn_device_close = nullptr;
    
    for (const auto& dll_name : dll_names)
    {
        dlldir = basedir / dll_name;
        
        if (!std::filesystem::exists(dlldir))
        {
            continue;
        }
        
        std::cout << "[Ghub] Trying DLL: " << dlldir.string() << std::endl;
        
        gm = LoadLibraryA(dlldir.string().c_str());
        if (gm == NULL)
        {
            DWORD err = GetLastError();
            std::cerr << "[Ghub] Failed to load DLL, error: " << err << std::endl;
            continue;
        }
        
        // Try ghub_mouse.dll style (mouse_open)
        auto mouse_open = reinterpret_cast<bool(*)()>(GetProcAddress(gm, "mouse_open"));
        if (mouse_open != NULL)
        {
            gmok = mouse_open();
            if (gmok)
            {
                std::cout << "[Ghub] mouse_open success (" << dll_name << ")" << std::endl;
                loadedDllName = dll_name;
                dllType = DllType::GHUB_MOUSE;
                
                // Load function pointers for ghub_mouse.dll
                fn_moveR_2 = reinterpret_cast<bool(*)(int, int)>(GetProcAddress(gm, "moveR"));
                fn_press = reinterpret_cast<bool(*)(int)>(GetProcAddress(gm, "press"));
                fn_release = reinterpret_cast<bool(*)()>(GetProcAddress(gm, "release"));
                fn_mouse_close = reinterpret_cast<bool(*)()>(GetProcAddress(gm, "mouse_close"));
                
                std::cout << "[Ghub] Functions loaded - moveR:" << (fn_moveR_2 ? "OK" : "FAIL") 
                          << " press:" << (fn_press ? "OK" : "FAIL")
                          << " release:" << (fn_release ? "OK" : "FAIL") << std::endl;
                return;
            }
            else
            {
                std::cerr << "[Ghub] mouse_open returned false" << std::endl;
            }
        }
        
        // Try ghub_device.dll / logitech.driver.dll style (device_open)
        auto device_open = reinterpret_cast<int(*)()>(GetProcAddress(gm, "device_open"));
        if (device_open != NULL)
        {
            int result = device_open();
            gmok = (result == 1);
            if (gmok)
            {
                std::cout << "[Ghub] device_open success (" << dll_name << ")" << std::endl;
                loadedDllName = dll_name;
                dllType = DllType::GHUB_DEVICE;
                
                // Load function pointers for ghub_device.dll
                fn_moveR_3 = reinterpret_cast<void(*)(int, int, bool)>(GetProcAddress(gm, "moveR"));
                fn_mouse_down = reinterpret_cast<void(*)(int)>(GetProcAddress(gm, "mouse_down"));
                fn_mouse_up = reinterpret_cast<void(*)(int)>(GetProcAddress(gm, "mouse_up"));
                fn_device_close = reinterpret_cast<void(*)()>(GetProcAddress(gm, "device_close"));
                
                std::cout << "[Ghub] Functions loaded - moveR:" << (fn_moveR_3 ? "OK" : "FAIL") 
                          << " mouse_down:" << (fn_mouse_down ? "OK" : "FAIL")
                          << " mouse_up:" << (fn_mouse_up ? "OK" : "FAIL") << std::endl;
                return;
            }
            else
            {
                std::cerr << "[Ghub] device_open returned " << result << " (expected 1)" << std::endl;
            }
        }
        
        // Try MouseControl.dll style (no init function, just use directly)
        auto move_R = reinterpret_cast<void(*)(int, int)>(GetProcAddress(gm, "move_R"));
        if (move_R != NULL)
        {
            std::cout << "[Ghub] MouseControl.dll detected (" << dll_name << ")" << std::endl;
            loadedDllName = dll_name;
            dllType = DllType::MOUSE_CONTROL;
            gmok = true;
            
            // MouseControl.dll uses different function names
            fn_move_R = move_R;
            fn_click_Left_down = reinterpret_cast<void(*)()>(GetProcAddress(gm, "click_Left_down"));
            fn_click_Left_up = reinterpret_cast<void(*)()>(GetProcAddress(gm, "click_Left_up"));
            
            std::cout << "[Ghub] Functions loaded - move_R:" << (fn_move_R ? "OK" : "FAIL") << std::endl;
            return;
        }
        
        // This DLL doesn't work, free and try next
        FreeLibrary(gm);
        gm = NULL;
    }
    
    if (!gmok)
    {
        std::cerr << "[Ghub] All DLLs failed to initialize" << std::endl;
        std::cerr << "[Ghub] Make sure G HUB (old version 2021.x) is running" << std::endl;
        std::cerr << "[Ghub] Will fallback to Win32 SendInput" << std::endl;
    }
}

GhubMouse::~GhubMouse()
{
    if (gm != NULL)
    {
        FreeLibrary(gm);
    }
}

bool GhubMouse::mouse_xy(int x, int y)
{
    if (gmok && gm != NULL)
    {
        switch (dllType)
        {
        case DllType::GHUB_MOUSE:
            if (fn_moveR_2)
            {
                bool result = fn_moveR_2(x, y);
                return result;
            }
            break;
            
        case DllType::GHUB_DEVICE:
            if (fn_moveR_3)
            {
                fn_moveR_3(x, y, false);  // false = relative move
                return true;
            }
            break;
            
        case DllType::MOUSE_CONTROL:
            if (fn_move_R)
            {
                fn_move_R(x, y);
                return true;
            }
            break;
            
        default:
            break;
        }
    }
    
    // Fallback to Win32 SendInput
    INPUT input = _ghub_Mouse(MOUSEEVENTF_MOVE, x, y);
    return _ghub_SendInput(1, &input) == 1;
}

bool GhubMouse::mouse_down(int key)
{
    if (gmok && gm != NULL)
    {
        switch (dllType)
        {
        case DllType::GHUB_MOUSE:
            if (fn_press)
            {
                return fn_press(key);
            }
            break;
            
        case DllType::GHUB_DEVICE:
            if (fn_mouse_down)
            {
                fn_mouse_down(key);
                return true;
            }
            break;
            
        case DllType::MOUSE_CONTROL:
            if (key == 1 && fn_click_Left_down)
            {
                fn_click_Left_down();
                return true;
            }
            break;
            
        default:
            break;
        }
    }
    
    // Fallback to Win32
    DWORD flag = (key == 1) ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_RIGHTDOWN;
    INPUT input = _ghub_Mouse(flag);
    return _ghub_SendInput(1, &input) == 1;
}

bool GhubMouse::mouse_up(int key)
{
    if (gmok && gm != NULL)
    {
        switch (dllType)
        {
        case DllType::GHUB_MOUSE:
            if (fn_release)
            {
                return fn_release();
            }
            break;
            
        case DllType::GHUB_DEVICE:
            if (fn_mouse_up)
            {
                fn_mouse_up(key);
                return true;
            }
            break;
            
        case DllType::MOUSE_CONTROL:
            if (key == 1 && fn_click_Left_up)
            {
                fn_click_Left_up();
                return true;
            }
            break;
            
        default:
            break;
        }
    }
    
    // Fallback to Win32
    DWORD flag = (key == 1) ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_RIGHTUP;
    INPUT input = _ghub_Mouse(flag);
    return _ghub_SendInput(1, &input) == 1;
}

bool GhubMouse::mouse_close()
{
    if (gmok && gm != NULL)
    {
        switch (dllType)
        {
        case DllType::GHUB_MOUSE:
            if (fn_mouse_close)
            {
                return fn_mouse_close();
            }
            break;
            
        case DllType::GHUB_DEVICE:
            if (fn_device_close)
            {
                fn_device_close();
                return true;
            }
            break;
            
        default:
            break;
        }
    }
    return false;
}
