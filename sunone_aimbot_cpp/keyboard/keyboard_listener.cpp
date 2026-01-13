#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <winsock2.h>
#include <Windows.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <iostream>

#include "config.h"
#include "SerialConnection.h"
#include "keyboard_listener.h"
#include "mouse.h"
#include "keycodes.h"
#include "sunone_aimbot_cpp.h"
#include "capture.h"
#include "KmboxNetConnection.h"
#include "Makcu.h"
#include "overlay.h"

extern std::atomic<bool> shouldExit;
extern std::atomic<bool> aiming;
extern std::atomic<bool> shooting;
extern std::atomic<bool> zooming;
extern std::atomic<bool> detectionPaused;
extern std::atomic<bool> norecoilActive;

extern MouseThread* globalMouseThread;

const float OFFSET_STEP = 0.01f;
const float NORECOIL_STEP = 5.0f;

// Arrow key vectors
const std::vector<std::string> upArrowKeys = { "UpArrow" };
const std::vector<std::string> downArrowKeys = { "DownArrow" };
const std::vector<std::string> leftArrowKeys = { "LeftArrow" };
const std::vector<std::string> rightArrowKeys = { "RightArrow" };
const std::vector<std::string> shiftKeys = { "LeftShift", "RightShift" };

// Previous key states
bool prevUpArrow = false;
bool prevDownArrow = false;
bool prevLeftArrow = false;
bool prevRightArrow = false;

bool isAnyKeyPressed(const std::vector<std::string>& keys)
{
    bool usePhysicalDevice = false;

    if (makcuSerial && makcuSerial->isOpen()) {
        usePhysicalDevice = true;
    }
    else if (kmboxSerial && kmboxSerial->isOpen()) {
        usePhysicalDevice = true;
    }
    else if (kmboxNetSerial && kmboxNetSerial->isOpen()) {
        usePhysicalDevice = true;
    }
    else if (config.arduino_enable_keys && arduinoSerial && arduinoSerial->isOpen()) {
        usePhysicalDevice = true;
    }

    for (const auto& key_name : keys)
    {
        // Check if it's a combo key (contains +)
        if (key_name.find('+') != std::string::npos) {
            if (KeyCodes::isKeyComboPressed(key_name)) return true;
            continue;
        }
        
        int key_code = KeyCodes::getKeyCode(key_name);
        bool pressed = false;

        // KmboxNet mouse monitor
        if (kmboxNetSerial && kmboxNetSerial->isOpen())
        {
            if (key_name == "LeftMouseButton") {
                int result = kmboxNetSerial->monitorMouseLeft();
                if (result == 1) pressed = true;
            }
            else if (key_name == "RightMouseButton") {
                int result = kmboxNetSerial->monitorMouseRight();
                if (result == 1) pressed = true;
            }
            else if (key_name == "MiddleMouseButton") {
                if (kmboxNetSerial->monitorMouseMiddle() == 1) pressed = true;
            }
            else if (key_name == "X1MouseButton") {
                if (kmboxNetSerial->monitorMouseSide1() == 1) pressed = true;
            }
            else if (key_name == "X2MouseButton") {
                if (kmboxNetSerial->monitorMouseSide2() == 1) pressed = true;
            }
        }

        // MAKCU
        if (!pressed && makcuSerial && makcuSerial->isOpen())
        {
            if (key_name == "LeftMouseButton")       pressed = makcuSerial->shooting_active;
            else if (key_name == "RightMouseButton")  pressed = makcuSerial->zooming_active;
            else if (key_name == "X2MouseButton")     pressed = makcuSerial->aiming_active;
        }

        // Kmbox_b
        if (!pressed && kmboxSerial && kmboxSerial->isOpen())
        {
            if (key_name == "LeftMouseButton")       pressed = kmboxSerial->shooting_active;
            else if (key_name == "RightMouseButton")  pressed = kmboxSerial->zooming_active;
            else if (key_name == "X2MouseButton")     pressed = kmboxSerial->aiming_active;
        }

        // KmboxNet
        if (!pressed && kmboxNetSerial && kmboxNetSerial->isOpen())
        {
            if (key_name == "LeftMouseButton")       pressed = kmboxNetSerial->shooting_active;
            else if (key_name == "RightMouseButton")  pressed = kmboxNetSerial->zooming_active;
            else if (key_name == "X2MouseButton")     pressed = kmboxNetSerial->aiming_active;
        }

        // Arduino
        if (!pressed && config.arduino_enable_keys && arduinoSerial && arduinoSerial->isOpen())
        {
            if (key_name == "LeftMouseButton")       pressed = arduinoSerial->shooting_active;
            else if (key_name == "RightMouseButton")  pressed = arduinoSerial->zooming_active;
            else if (key_name == "X2MouseButton")     pressed = arduinoSerial->aiming_active;
        }

        // Win32 API fallback
        if (!pressed && key_code != -1)
        {
            pressed = (GetAsyncKeyState(key_code) & 0x8000) != 0;
        }

        if (pressed) return true;
    }
    return false;
}

bool isAnyKeyPressedWin32Only(const std::vector<std::string>& keys)
{
    for (const auto& key_name : keys)
    {
        // Check if it's a combo key (contains +)
        if (key_name.find('+') != std::string::npos) {
            if (KeyCodes::isKeyComboPressed(key_name)) return true;
            continue;
        }
        
        int key_code = KeyCodes::getKeyCode(key_name);
        if (key_code != -1 && (GetAsyncKeyState(key_code) & 0x8000))
            return true;
    }
    return false;
}

void keyboardListener()
{
    while (!shouldExit)
    {
        // Aiming
        if (!config.auto_aim)
        {
            aiming = isAnyKeyPressed(config.button_targeting) ||
                (config.arduino_enable_keys && arduinoSerial && arduinoSerial->isOpen() && arduinoSerial->aiming_active) ||
                (kmboxSerial && kmboxSerial->isOpen() && kmboxSerial->aiming_active) ||
                (kmboxNetSerial && kmboxNetSerial->isOpen() && kmboxNetSerial->aiming_active) ||
                (makcuSerial && makcuSerial->isOpen() && makcuSerial->aiming_active);
        }
        else
        {
            aiming = true;
        }

        // Shooting
        shooting = isAnyKeyPressed(config.button_shoot) ||
            (config.arduino_enable_keys && arduinoSerial && arduinoSerial->isOpen() && arduinoSerial->shooting_active) ||
            (kmboxSerial && kmboxSerial->isOpen() && kmboxSerial->shooting_active) ||
            (kmboxNetSerial && kmboxNetSerial->isOpen() && kmboxNetSerial->shooting_active) ||
            (makcuSerial && makcuSerial->isOpen() && makcuSerial->shooting_active);

        // Zooming
        zooming = isAnyKeyPressed(config.button_zoom) ||
            (config.arduino_enable_keys && arduinoSerial && arduinoSerial->isOpen() && arduinoSerial->zooming_active) ||
            (kmboxSerial && kmboxSerial->isOpen() && kmboxSerial->zooming_active) ||
            (kmboxNetSerial && kmboxNetSerial->isOpen() && kmboxNetSerial->zooming_active) ||
            (makcuSerial && makcuSerial->isOpen() && makcuSerial->zooming_active);

        // NoRecoil - control panel open disables recoil control
        if (overlayVisible.load())
        {
            norecoilActive.store(false);
        }
        else if (config.norecoil_mode == 0)
        {
            // Mode 0: Right mouse button + Left mouse button
            bool rightPressed = isAnyKeyPressed(std::vector<std::string>{"RightMouseButton"});
            bool leftPressed = isAnyKeyPressed(std::vector<std::string>{"LeftMouseButton"});
            norecoilActive.store(rightPressed && leftPressed);
        }
        else
        {
            // Mode 1: Custom key
            norecoilActive.store(isAnyKeyPressed(config.button_norecoil));
        }

        // Exit - Win32
        if (isAnyKeyPressedWin32Only(config.button_exit))
        {
            shouldExit = true;
            quick_exit(0);
        }

        // Pause detection - Win32
        static bool pausePressed = false;
        if (isAnyKeyPressedWin32Only(config.button_pause))
        {
            if (!pausePressed)
            {
                detectionPaused = !detectionPaused;
                pausePressed = true;
            }
        }
        else
        {
            pausePressed = false;
        }

        // Reload config - Win32
        static bool reloadPressed = false;
        if (isAnyKeyPressedWin32Only(config.button_reload_config))
        {
            if (!reloadPressed)
            {
                config.loadConfig();

                if (globalMouseThread)
                {
                    globalMouseThread->updateConfig(
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
                reloadPressed = true;
            }
        }
        else
        {
            reloadPressed = false;
        }

        // Open overlay - Win32
        static bool overlayPressed = false;
        if (isAnyKeyPressedWin32Only(config.button_open_overlay))
        {
            if (!overlayPressed)
            {
                overlayPressed = true;
            }
        }
        else
        {
            overlayPressed = false;
        }

        // Arrow key detection - Win32
        bool upArrow = isAnyKeyPressedWin32Only(upArrowKeys);
        bool downArrow = isAnyKeyPressedWin32Only(downArrowKeys);
        bool leftArrow = isAnyKeyPressedWin32Only(leftArrowKeys);
        bool rightArrow = isAnyKeyPressedWin32Only(rightArrowKeys);
        bool shiftKey = isAnyKeyPressedWin32Only(shiftKeys);

        // Adjust norecoil strength based on up/down arrow keys (always enabled when easynorecoil is on)
        static DWORD upArrowPressTime = 0;
        static DWORD downArrowPressTime = 0;
        static DWORD lastUpRepeat = 0;
        static DWORD lastDownRepeat = 0;
        
        if (config.easynorecoil)
        {
            DWORD now = GetTickCount();
            DWORD repeatDelay = 300;
            DWORD repeatInterval = 50;
            
            if (upArrow)
            {
                if (!prevUpArrow)
                {
                    config.easynorecoilstrength = std::max(0.1f, config.easynorecoilstrength - 0.1f);
                    upArrowPressTime = now;
                    lastUpRepeat = now;
                }
                else if (now - upArrowPressTime > repeatDelay && now - lastUpRepeat > repeatInterval)
                {
                    config.easynorecoilstrength = std::max(0.1f, config.easynorecoilstrength - 0.1f);
                    lastUpRepeat = now;
                }
            }
            
            if (downArrow)
            {
                if (!prevDownArrow)
                {
                    config.easynorecoilstrength = std::min(500.0f, config.easynorecoilstrength + 0.1f);
                    downArrowPressTime = now;
                    lastDownRepeat = now;
                }
                else if (now - downArrowPressTime > repeatDelay && now - lastDownRepeat > repeatInterval)
                {
                    config.easynorecoilstrength = std::min(500.0f, config.easynorecoilstrength + 0.1f);
                    lastDownRepeat = now;
                }
            }
        }

        // Adjust offsets based on arrow keys and shift combination
        if (config.enable_arrows_settings)
        {
            if (upArrow && !prevUpArrow)
            {
                if (shiftKey)
                {
                    // Shift + Up Arrow: Decrease head offset
                    config.head_y_offset = std::max(0.0f, config.head_y_offset - OFFSET_STEP);
                }
                else
                {
                    // Up Arrow: Decrease body offset
                    config.body_y_offset = std::max(0.0f, config.body_y_offset - OFFSET_STEP);
                }
            }
            if (downArrow && !prevDownArrow)
            {
                if (shiftKey)
                {
                    // Shift + Down Arrow: Increase head offset
                    config.head_y_offset = std::min(1.0f, config.head_y_offset + OFFSET_STEP);
                }
                else
                {
                    // Down Arrow: Increase body offset
                    config.body_y_offset = std::min(1.0f, config.body_y_offset + OFFSET_STEP);
                }
            }
        }
        
        // Update previous key states
        prevUpArrow = upArrow;
        prevDownArrow = downArrow;
        prevLeftArrow = leftArrow;
        prevRightArrow = rightArrow;

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}