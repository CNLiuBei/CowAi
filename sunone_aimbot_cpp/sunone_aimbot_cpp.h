#ifndef SUNONE_AIMBOT_CPP_H
#define SUNONE_AIMBOT_CPP_H

#include "config.h"
#ifdef USE_CUDA
#include "trt_detector.h"
#else
#include "dml_detector.h"
#endif
#include "mouse.h"
#include "SerialConnection.h"
#include "detection_buffer.h"
#include "Kmbox_b.h"
#include "KmboxNetConnection.h"
#include "Makcu.h"
#include "weapon/weapon_recognizer.h"
#include "weapon/recoil_controller.h"

extern Config config;
#ifdef USE_CUDA
extern TrtDetector trt_detector;
#else
extern DirectMLDetector* dml_detector;
#endif
extern DetectionBuffer detectionBuffer;
extern MouseThread* globalMouseThread;
extern SerialConnection* arduinoSerial;
extern Kmbox_b_Connection* kmboxSerial;
extern KmboxNetConnection* kmboxNetSerial;
extern MakcuConnection* makcuSerial;
extern std::atomic<bool> input_method_changed;
extern std::atomic<bool> aiming;
extern std::atomic<bool> shooting;
extern std::atomic<bool> zooming;

// Weapon slot tracking (主副武器槽位)
extern std::atomic<int> currentWeaponSlot;  // 0=未知, 1=主武器, 2=副武器

// Weapon recognition (武器识别)
extern WeaponRecognizer* globalWeaponRecognizer;

// Recoil controller (弹道压枪)
extern RecoilController* globalRecoilController;

#endif // SUNONE_AIMBOT_CPP_H