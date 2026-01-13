#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <winsock2.h>
#include <Windows.h>

#include "keycodes.h"

#include <string>
#include <unordered_map>

std::unordered_map<std::string, int> KeyCodes::key_code_map =
{
    {"None", 0},
    {"LeftMouseButton", VK_LBUTTON},
    {"RightMouseButton", VK_RBUTTON},
    {"ControlBreak", VK_CANCEL},
    {"MiddleMouseButton", VK_MBUTTON},
    {"X1MouseButton", VK_XBUTTON1},
    {"X2MouseButton", VK_XBUTTON2},
    {"Backspace", VK_BACK},
    {"Tab", VK_TAB},
    {"Clear", VK_CLEAR},
    {"Enter", VK_RETURN},
    {"Pause", VK_PAUSE},
    {"CapsLock", VK_CAPITAL},
    {"Escape", VK_ESCAPE},
    {"Space", VK_SPACE},
    {"PageUp", VK_PRIOR},
    {"PageDown", VK_NEXT},
    {"End", VK_END},
    {"Home", VK_HOME},
    {"LeftArrow", VK_LEFT},
    {"UpArrow", VK_UP},
    {"RightArrow", VK_RIGHT},
    {"DownArrow", VK_DOWN},
    {"Select", VK_SELECT},
    {"Print", VK_PRINT},
    {"Execute", VK_EXECUTE},
    {"PrintScreen", VK_SNAPSHOT},
    {"Ins", VK_INSERT},
    {"Delete", VK_DELETE},
    {"Help", VK_HELP},
    {"Key0", '0'},
    {"Key1", '1'},
    {"Key2", '2'},
    {"Key3", '3'},
    {"Key4", '4'},
    {"Key5", '5'},
    {"Key6", '6'},
    {"Key7", '7'},
    {"Key8", '8'},
    {"Key9", '9'},
    {"A", 'A'},
    {"B", 'B'},
    {"C", 'C'},
    {"D", 'D'},
    {"E", 'E'},
    {"F", 'F'},
    {"G", 'G'},
    {"H", 'H'},
    {"I", 'I'},
    {"J", 'J'},
    {"K", 'K'},
    {"L", 'L'},
    {"M", 'M'},
    {"N", 'N'},
    {"O", 'O'},
    {"P", 'P'},
    {"Q", 'Q'},
    {"R", 'R'},
    {"S", 'S'},
    {"T", 'T'},
    {"U", 'U'},
    {"V", 'V'},
    {"W", 'W'},
    {"X", 'X'},
    {"Y", 'Y'},
    {"Z", 'Z'},
    {"LeftWindowsKey", VK_LWIN},
    {"RightWindowsKey", VK_RWIN},
    {"Application", VK_APPS},
    {"Sleep", VK_SLEEP},
    {"NumpadKey0", VK_NUMPAD0},
    {"NumpadKey1", VK_NUMPAD1},
    {"NumpadKey2", VK_NUMPAD2},
    {"NumpadKey3", VK_NUMPAD3},
    {"NumpadKey4", VK_NUMPAD4},
    {"NumpadKey5", VK_NUMPAD5},
    {"NumpadKey6", VK_NUMPAD6},
    {"NumpadKey7", VK_NUMPAD7},
    {"NumpadKey8", VK_NUMPAD8},
    {"NumpadKey9", VK_NUMPAD9},
    {"Multiply", VK_MULTIPLY},
    {"Add", VK_ADD},
    {"Separator", VK_SEPARATOR},
    {"Subtract", VK_SUBTRACT},
    {"Decimal", VK_DECIMAL},
    {"Divide", VK_DIVIDE},
    {"F1", VK_F1},
    {"F2", VK_F2},
    {"F3", VK_F3},
    {"F4", VK_F4},
    {"F5", VK_F5},
    {"F6", VK_F6},
    {"F7", VK_F7},
    {"F8", VK_F8},
    {"F9", VK_F9},
    {"F10", VK_F10},
    {"F11", VK_F11},
    {"F12", VK_F12},
    {"NumLock", VK_NUMLOCK},
    {"ScrollLock", VK_SCROLL},
    {"LeftShift", VK_LSHIFT},
    {"RightShift", VK_RSHIFT},
    {"LeftControl", VK_LCONTROL},
    {"RightControl", VK_RCONTROL},
    {"LeftAlt", VK_LMENU},
    {"RightAlt", VK_RMENU},
    {"BrowserBack", VK_BROWSER_BACK},
    {"BrowserRefresh", VK_BROWSER_REFRESH},
    {"BrowserStop", VK_BROWSER_STOP},
    {"BrowserSearch", VK_BROWSER_SEARCH},
    {"BrowserFavorites", VK_BROWSER_FAVORITES},
    {"BrowserHome", VK_BROWSER_HOME},
    {"VolumeMute", VK_VOLUME_MUTE},
    {"VolumeDown", VK_VOLUME_DOWN},
    {"VolumeUp", VK_VOLUME_UP},
    {"NextTrack", VK_MEDIA_NEXT_TRACK},
    {"PreviousTrack", VK_MEDIA_PREV_TRACK},
    {"StopMedia", VK_MEDIA_STOP},
    {"PlayMedia", VK_MEDIA_PLAY_PAUSE},
    {"StartMailKey", VK_LAUNCH_MAIL},
    {"SelectMedia", VK_LAUNCH_MEDIA_SELECT},
    {"StartApplication1", VK_LAUNCH_APP1},
    {"StartApplication2", VK_LAUNCH_APP2}
};

int KeyCodes::getKeyCode(const std::string& key_name) {
    std::unordered_map<std::string, int>::iterator it = key_code_map.find(key_name);
    if (it != key_code_map.end())
        return it->second;
    else
        return -1;
}

std::string KeyCodes::getKeyName(int vk_code) {
    if (vk_code == 0) return "None";
    std::unordered_map<std::string, int>::iterator it;
    for (it = key_code_map.begin(); it != key_code_map.end(); ++it) {
        if (it->second == vk_code) {
            return it->first;
        }
    }
    return "Unknown";
}

int KeyCodes::detectPressedKey() {
    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) return VK_LBUTTON;
    if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) return VK_RBUTTON;
    if (GetAsyncKeyState(VK_MBUTTON) & 0x8000) return VK_MBUTTON;
    if (GetAsyncKeyState(VK_XBUTTON1) & 0x8000) return VK_XBUTTON1;
    if (GetAsyncKeyState(VK_XBUTTON2) & 0x8000) return VK_XBUTTON2;
    
    std::unordered_map<std::string, int>::iterator it;
    for (it = key_code_map.begin(); it != key_code_map.end(); ++it) {
        if (it->second != 0 && it->second != VK_LBUTTON) {
            if (GetAsyncKeyState(it->second) & 0x8000) {
                return it->second;
            }
        }
    }
    return 0;
}

std::string KeyCodes::detectPressedKeyCombo() {
    std::string combo = "";
    
    // Check keyboard modifiers
    bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    
    // Check mouse button modifiers
    bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    bool rmb = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    bool mmb = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
    bool xmb1 = (GetAsyncKeyState(VK_XBUTTON1) & 0x8000) != 0;
    bool xmb2 = (GetAsyncKeyState(VK_XBUTTON2) & 0x8000) != 0;
    
    // Count pressed mouse buttons
    int mouse_count = (lmb ? 1 : 0) + (rmb ? 1 : 0) + (mmb ? 1 : 0) + (xmb1 ? 1 : 0) + (xmb2 ? 1 : 0);
    
    // Find main key
    int main_key = 0;
    
    // If multiple mouse buttons pressed, use the last one as main key
    if (mouse_count >= 2) {
        // Build combo with mouse modifiers (order: RMB, MMB, XMB1, XMB2, then LMB as main)
        if (rmb) combo += "RightMouseButton+";
        if (mmb) combo += "MiddleMouseButton+";
        if (xmb1) combo += "X1MouseButton+";
        if (xmb2) combo += "X2MouseButton+";
        if (lmb) {
            combo += "LeftMouseButton";
            return combo;
        }
        // Remove trailing +
        if (!combo.empty() && combo.back() == '+') {
            combo.pop_back();
        }
        return combo;
    }
    
    // Single mouse button or keyboard key
    if (lmb) main_key = VK_LBUTTON;
    else if (rmb) main_key = VK_RBUTTON;
    else if (mmb) main_key = VK_MBUTTON;
    else if (xmb1) main_key = VK_XBUTTON1;
    else if (xmb2) main_key = VK_XBUTTON2;
    else {
        // Check other keys
        std::unordered_map<std::string, int>::iterator it;
        for (it = key_code_map.begin(); it != key_code_map.end(); ++it) {
            int vk = it->second;
            if (vk != 0 && vk != VK_LBUTTON &&
                vk != VK_CONTROL && vk != VK_LCONTROL && vk != VK_RCONTROL &&
                vk != VK_SHIFT && vk != VK_LSHIFT && vk != VK_RSHIFT &&
                vk != VK_MENU && vk != VK_LMENU && vk != VK_RMENU) {
                if (GetAsyncKeyState(vk) & 0x8000) {
                    main_key = vk;
                    break;
                }
            }
        }
    }
    
    // If only modifier pressed, return empty
    if (main_key == 0) return "";
    
    // Build combo string with keyboard modifiers
    if (ctrl) combo += "Ctrl+";
    if (shift) combo += "Shift+";
    if (alt) combo += "Alt+";
    combo += getKeyName(main_key);
    
    return combo;
}

bool KeyCodes::isKeyComboPressed(const std::string& combo) {
    if (combo.empty() || combo == "None") return false;
    
    bool need_ctrl = false;
    bool need_shift = false;
    bool need_alt = false;
    bool need_rmb = false;
    bool need_mmb = false;
    bool need_xmb1 = false;
    bool need_xmb2 = false;
    std::string main_key_name = combo;
    
    // Parse combo string
    while (true) {
        if (main_key_name.substr(0, 5) == "Ctrl+") {
            need_ctrl = true;
            main_key_name = main_key_name.substr(5);
        } else if (main_key_name.substr(0, 6) == "Shift+") {
            need_shift = true;
            main_key_name = main_key_name.substr(6);
        } else if (main_key_name.substr(0, 4) == "Alt+") {
            need_alt = true;
            main_key_name = main_key_name.substr(4);
        } else if (main_key_name.substr(0, 17) == "RightMouseButton+") {
            need_rmb = true;
            main_key_name = main_key_name.substr(17);
        } else if (main_key_name.substr(0, 18) == "MiddleMouseButton+") {
            need_mmb = true;
            main_key_name = main_key_name.substr(18);
        } else if (main_key_name.substr(0, 14) == "X1MouseButton+") {
            need_xmb1 = true;
            main_key_name = main_key_name.substr(14);
        } else if (main_key_name.substr(0, 14) == "X2MouseButton+") {
            need_xmb2 = true;
            main_key_name = main_key_name.substr(14);
        } else {
            break;
        }
    }
    
    // Check keyboard modifiers
    bool ctrl_pressed = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    bool shift_pressed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    bool alt_pressed = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    
    if (need_ctrl != ctrl_pressed) return false;
    if (need_shift != shift_pressed) return false;
    if (need_alt != alt_pressed) return false;
    
    // Check mouse button modifiers
    if (need_rmb && !(GetAsyncKeyState(VK_RBUTTON) & 0x8000)) return false;
    if (need_mmb && !(GetAsyncKeyState(VK_MBUTTON) & 0x8000)) return false;
    if (need_xmb1 && !(GetAsyncKeyState(VK_XBUTTON1) & 0x8000)) return false;
    if (need_xmb2 && !(GetAsyncKeyState(VK_XBUTTON2) & 0x8000)) return false;
    
    // Check main key
    int vk = getKeyCode(main_key_name);
    if (vk <= 0) return false;
    
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}
