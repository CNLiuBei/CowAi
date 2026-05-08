#ifndef GHUB_H
#define GHUB_H

#include <filesystem>
#include <windows.h>
#include <string>

class GhubMouse
{
public:
    GhubMouse();
    ~GhubMouse();
    bool mouse_xy(int x, int y);
    bool mouse_down(int key = 1);
    bool mouse_up(int key = 1);
    bool mouse_close();
    
    std::string loadedDllName;  // Name of loaded DLL

private:
    std::filesystem::path basedir;
    std::filesystem::path dlldir;
    HMODULE gm;
    bool gmok;
    
    // DLL type enum
    enum class DllType {
        NONE,
        GHUB_MOUSE,      // ghub_mouse.dll
        GHUB_DEVICE,     // ghub_device.dll / logitech.driver.dll
        MOUSE_CONTROL    // MouseControl.dll
    };
    DllType dllType;
    
    // Function pointers for ghub_mouse.dll
    bool (*fn_moveR_2)(int, int);
    bool (*fn_press)(int);
    bool (*fn_release)();
    bool (*fn_mouse_close)();
    
    // Function pointers for ghub_device.dll
    void (*fn_moveR_3)(int, int, bool);
    void (*fn_mouse_down)(int);
    void (*fn_mouse_up)(int);
    void (*fn_device_close)();
    
    // Function pointers for MouseControl.dll
    void (*fn_move_R)(int, int);
    void (*fn_click_Left_down)();
    void (*fn_click_Left_up)();

    static UINT _ghub_SendInput(UINT nInputs, LPINPUT pInputs);
    static INPUT _ghub_Input(MOUSEINPUT mi);
    static MOUSEINPUT _ghub_MouseInput(DWORD flags, LONG x, LONG y, DWORD data);
    static INPUT _ghub_Mouse(DWORD flags, LONG x = 0, LONG y = 0, DWORD data = 0);
};

#endif // GHUB_H
