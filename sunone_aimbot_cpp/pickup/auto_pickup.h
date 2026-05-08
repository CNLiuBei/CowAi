#ifndef AUTO_PICKUP_H
#define AUTO_PICKUP_H

#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <winsock2.h>
#include <Windows.h>

#include <atomic>
#include <thread>
#include <chrono>

#include "mouse/SerialConnection.h"
#include "mouse/Kmbox_b.h"
#include "mouse/kmboxNetConnection.h"
#include "mouse/Makcu.h"
#include "mouse/ghub.h"

class AutoPickup
{
private:
    std::atomic<bool> isRunning{ false };
    std::thread pickupThread;
    
    // Input device pointers (use main program's devices)
    SerialConnection* arduino;
    GhubMouse* gHub;
    Kmbox_b_Connection* kmbox;
    KmboxNetConnection* kmboxNet;
    MakcuConnection* makcu;
    
    // Pickup parameters (matching original Lua script)
    int screenType;      // 1=1920x1080, 2=2560x1080, 3=2560x1440
    int itemsPerLoop;    // Original default: 13
    int delayMs;         // Optional delay between items
    
    // Screen resolution
    int screenWidth;
    int screenHeight;
    
    // 1:1 replica of original Lua functions
    void executePickup();           // altDrag()
    void pressTab();                // PressAndReleaseKey("Tab")
    void moveMouseTo(int x, int y); // MoveMouseTo(x, y) - Logitech coords 0-65535
    void pressMouseButton();        // PressMouseButton(1)
    void releaseMouseButton();      // ReleaseMouseButton(1)
    void logitechToPixel(int logitechX, int logitechY, int& pixelX, int& pixelY);

public:
    AutoPickup();
    ~AutoPickup();

    // Set input devices (from main program)
    void setInputDevices(
        SerialConnection* arduinoSerial,
        GhubMouse* gHubMouse,
        Kmbox_b_Connection* kmboxSerial,
        KmboxNetConnection* kmboxNetSerial,
        MakcuConnection* makcuSerial
    );
    
    void start();
    void stop();
    bool isActive() const { return isRunning.load(); }
    
    // Configure pickup parameters
    // screen: 1=1920x1080, 2=2560x1080, 3=2560x1440
    // items: number of items to pickup (original: 13)
    // Other params are kept for compatibility but calculated internally
    void configure(int screen, int items, int startX, int startY, 
                   int spacingY, int targetX, int targetY, int delay);
    
    void fastPickup();
    
    // Getters for UI
    int getScreenType() const { return screenType; }
    int getItemsPerLoop() const { return itemsPerLoop; }
    int getDelayMs() const { return delayMs; }
};

#endif // AUTO_PICKUP_H
