// auto_pickup.cpp - EXACT 1:1 replica of the original Lua ghost hand pickup (altDrag)
#include "auto_pickup.h"
#include <iostream>

AutoPickup::AutoPickup()
    : arduino(nullptr)
    , gHub(nullptr)
    , kmbox(nullptr)
    , kmboxNet(nullptr)
    , makcu(nullptr)
    , screenType(1)      // 1=1920x1080, 2=2560x1080, 3=2560x1440
    , itemsPerLoop(13)   // Original: until(i>13)
    , delayMs(0)
    , screenWidth(GetSystemMetrics(SM_CXSCREEN))
    , screenHeight(GetSystemMetrics(SM_CYSCREEN))
{
}

AutoPickup::~AutoPickup()
{
    stop();
}

void AutoPickup::setInputDevices(
    SerialConnection* arduinoSerial,
    GhubMouse* gHubMouse,
    Kmbox_b_Connection* kmboxSerial,
    KmboxNetConnection* kmboxNetSerial,
    MakcuConnection* makcuSerial)
{
    arduino = arduinoSerial;
    gHub = gHubMouse;
    kmbox = kmboxSerial;
    kmboxNet = kmboxNetSerial;
    makcu = makcuSerial;
}

void AutoPickup::configure(int screen, int items, int sX, int sY, 
                           int spacingY, int tX, int tY, int delay)
{
    screenType = screen;
    itemsPerLoop = items;
    delayMs = delay;
}

// Original: PressAndReleaseKey("Tab")
void AutoPickup::pressTab()
{
    keybd_event(VK_TAB, 0, 0, 0);
    Sleep(30);
    keybd_event(VK_TAB, 0, KEYEVENTF_KEYUP, 0);
}

// Original: MoveMouseTo(x, y) - Logitech absolute coordinates (0-65535)
void AutoPickup::moveMouseTo(int logitechX, int logitechY)
{
    // Logitech coordinates are 0-65535, same as Windows MOUSEEVENTF_ABSOLUTE
    // Direct mapping - no conversion needed!
    mouse_event(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE, logitechX, logitechY, 0, 0);
}

// Original: PressMouseButton(1)
void AutoPickup::pressMouseButton()
{
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
}

// Original: ReleaseMouseButton(1)
void AutoPickup::releaseMouseButton()
{
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
}

// EXACT 1:1 replica of altDrag() function from original Lua script
// Original code (lines 4016-4070):
//
// function altDrag()
// PressAndReleaseKey("Tab")
// to=50000
// cury=55000
// i=0
// Sleep(70)
// DeltaY = 3755
// bdown=true
// repeat
//   if PUBG.GuiShou_Ping == "1" then 
//     MoveMouseTo(7000,cury)
//     PressMouseButton(1)
//     MoveMouseTo(7000+2000,cury)
//     PressMouseButton(1)
//     MoveMouseTo(7000,cury)
//   elseif PUBG.GuiShou_Ping == "2" then
//     MoveMouseTo(10000,cury)
//     PressMouseButton(1)
//     MoveMouseTo(10000+2000,cury)
//     PressMouseButton(1)
//     MoveMouseTo(10000,cury)
//   elseif PUBG.GuiShou_Ping == "3" then
//     MoveMouseTo(10000,cury)
//     PressMouseButton(1)
//     MoveMouseTo(10000+2000,cury)
//     PressMouseButton(1)
//     MoveMouseTo(10000,cury)
//   end
//   PressMouseButton(1)
//   MoveMouseTo(to,55000)
//   ReleaseMouseButton(1)
//   MoveMouseTo(to+2000,55000)
//   ReleaseMouseButton(1)
//   MoveMouseTo(to,55000)
//   ReleaseMouseButton(1)
//   cury=cury-DeltaY
//   i=i+1
// until(i>13)
// PressAndReleaseKey("Tab")
// end

void AutoPickup::executePickup()
{
    std::cout << "[Pickup] Starting ghost hand pickup (screen type: " << screenType << ")..." << std::endl;
    std::cout << "[Pickup] Screen resolution: " << screenWidth << "x" << screenHeight << std::endl;
    
    // ========== EXACT COPY OF ORIGINAL LUA altDrag() ==========
    
    // PressAndReleaseKey("Tab") - 打开背包
    pressTab();
    
    // to=50000, cury=55000, i=0
    int to = 50000;
    int cury = 55000;
    int i = 0;
    
    // Sleep(70)
    Sleep(70);
    
    // DeltaY = 3755
    int DeltaY = 3755;
    
    // For 2K screen (2560x1440), adjust DeltaY
    if (screenType == 3) {
        DeltaY = 2816;  // 3755 * 1080 / 1440 ≈ 2816
    }
    
    // Determine startX based on screen type
    int startX;
    if (screenType == 1) {
        startX = 7000;   // 1920x1080
    } else {
        startX = 10000;  // 2560x1080 or 2560x1440
    }
    
    int maxItems = (itemsPerLoop > 0) ? itemsPerLoop : 13;
    
    std::cout << "[Pickup] startX=" << startX << ", DeltaY=" << DeltaY << ", maxItems=" << maxItems << std::endl;
    
    // repeat ... until(i>13)
    do
    {
        if (!isRunning.load()) break;
        
        // ========== 物品位置的"抖动"点击 ==========
        // MoveMouseTo(startX, cury)
        moveMouseTo(startX, cury);
        
        // PressMouseButton(1)
        pressMouseButton();
        
        // MoveMouseTo(startX+2000, cury)
        moveMouseTo(startX + 2000, cury);
        
        // PressMouseButton(1)
        pressMouseButton();
        
        // MoveMouseTo(startX, cury)
        moveMouseTo(startX, cury);
        
        // ========== 拖拽到背包 ==========
        // PressMouseButton(1)
        pressMouseButton();
        
        // MoveMouseTo(to, 55000)
        moveMouseTo(to, 55000);
        
        // ReleaseMouseButton(1)
        releaseMouseButton();
        
        // MoveMouseTo(to+2000, 55000)
        moveMouseTo(to + 2000, 55000);
        
        // ReleaseMouseButton(1)
        releaseMouseButton();
        
        // MoveMouseTo(to, 55000)
        moveMouseTo(to, 55000);
        
        // ReleaseMouseButton(1)
        releaseMouseButton();
        
        // cury=cury-DeltaY (move UP to next item)
        cury = cury - DeltaY;
        
        // i=i+1
        i = i + 1;
        
        // Optional extra delay
        if (delayMs > 0)
            Sleep(delayMs);
            
    } while (i <= maxItems && isRunning.load());  // until(i>13)
    
    // PressAndReleaseKey("Tab") - 关闭背包
    pressTab();
    
    std::cout << "[Pickup] Ghost hand pickup completed (" << i << " items)" << std::endl;
    isRunning.store(false);
}

void AutoPickup::start()
{
    if (isRunning.load())
    {
        std::cout << "[Pickup] Pickup already in progress" << std::endl;
        return;
    }
    
    isRunning.store(true);
    
    if (pickupThread.joinable())
    {
        pickupThread.join();
    }
    
    pickupThread = std::thread([this]() {
        executePickup();
    });
}

void AutoPickup::stop()
{
    if (isRunning.load())
    {
        isRunning.store(false);
        if (pickupThread.joinable())
        {
            pickupThread.join();
        }
        std::cout << "[Pickup] Pickup stopped" << std::endl;
    }
}

void AutoPickup::fastPickup()
{
    start();
}
