#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <winsock2.h>
#include <Windows.h>

#include <cmath>
#include <algorithm>
#include <chrono>
#include <mutex>
#include <atomic>
#include <vector>
#include <random>

#include "mouse.h"
#include "capture.h"
#include "SerialConnection.h"
#include "sunone_aimbot_cpp.h"
#include "ghub.h"

MouseThread::MouseThread(
    int resolution,
    int fovX,
    int fovY,
    double minSpeedMultiplier,
    double maxSpeedMultiplier,
    double predictionInterval,
    bool auto_shoot,
    float bScope_multiplier,
    SerialConnection* serialConnection,
    GhubMouse* gHubMouse,
    Kmbox_b_Connection* kmboxConnection,
    KmboxNetConnection* Kmbox_Net_Connection,
    MakcuConnection* makcuConnection)
    : screen_width(resolution),
    screen_height(resolution),
    prediction_interval(predictionInterval),
    fov_x(fovX),
    fov_y(fovY),
    max_distance(std::hypot(resolution, resolution) / 2.0),
    min_speed_multiplier(minSpeedMultiplier),
    max_speed_multiplier(maxSpeedMultiplier),
    center_x(resolution / 2.0),
    center_y(resolution / 2.0),
    auto_shoot(auto_shoot),
    bScope_multiplier(bScope_multiplier),
    serial(serialConnection),
    kmbox(kmboxConnection),
    kmbox_net(Kmbox_Net_Connection),
    makcu(makcuConnection),
    gHub(gHubMouse),

    prev_velocity_x(0.0),
    prev_velocity_y(0.0),
    prev_x(0.0),
    prev_y(0.0)
{
    prev_time = std::chrono::steady_clock::time_point();
    last_target_time = std::chrono::steady_clock::now();

    wind_mouse_enabled = config.wind_mouse_enabled;
    wind_G = config.wind_G;
    wind_W = config.wind_W;
    wind_M = config.wind_M;
    wind_D = config.wind_D;

    moveWorker = std::thread(&MouseThread::moveWorkerLoop, this);
}

void MouseThread::updateConfig(
    int resolution,
    int fovX,
    int fovY,
    double minSpeedMultiplier,
    double maxSpeedMultiplier,
    double predictionInterval,
    bool auto_shoot,
    float bScope_multiplier
)
{
    screen_width = screen_height = resolution;
    fov_x = fovX;  fov_y = fovY;
    min_speed_multiplier = minSpeedMultiplier;
    max_speed_multiplier = maxSpeedMultiplier;
    prediction_interval = predictionInterval;
    this->auto_shoot = auto_shoot;
    this->bScope_multiplier = bScope_multiplier;

    center_x = center_y = resolution / 2.0;
    max_distance = std::hypot(resolution, resolution) / 2.0;

    wind_mouse_enabled = config.wind_mouse_enabled;
    wind_G = config.wind_G; wind_W = config.wind_W;
    wind_M = config.wind_M; wind_D = config.wind_D;
}

MouseThread::~MouseThread()
{
    workerStop = true;
    queueCv.notify_all();
    if (moveWorker.joinable()) moveWorker.join();
}

void MouseThread::queueMove(int dx, int dy)
{
    if (dx == 0 && dy == 0)
    {
        return;
    }

    // Track our own mouse movement
    {
        std::lock_guard<std::mutex> lock(mouse_accum_mutex);
        accumulated_mouse_dx += dx;
        accumulated_mouse_dy += dy;
    }

    std::lock_guard lg(queueMtx);
    if (moveQueue.size() >= queueLimit) moveQueue.pop();
    moveQueue.push({ dx,dy });
    queueCv.notify_one();
}

void MouseThread::moveWorkerLoop()
{
    while (!workerStop)
    {
        std::unique_lock ul(queueMtx);
        queueCv.wait(ul, [&] { return workerStop || !moveQueue.empty(); });

        while (!moveQueue.empty())
        {
            Move m = moveQueue.front();
            moveQueue.pop();
            ul.unlock();
            sendMovementToDriver(m.dx, m.dy);
            ul.lock();
        }
    }
}

void MouseThread::windMouseMoveRelative(int dx, int dy)
{
    if (dx == 0 && dy == 0) return;

    // 线程安全的随机数生成器
    static thread_local std::mt19937 rng(std::random_device{}());
    static thread_local std::uniform_real_distribution<double> dist01(0.0, 1.0);

    constexpr double SQRT3 = 1.7320508075688772;
    constexpr double SQRT5 = 2.23606797749979;

    double sx = 0, sy = 0;
    double dxF = static_cast<double>(dx);
    double dyF = static_cast<double>(dy);
    double vx = 0, vy = 0, wX = 0, wY = 0;
    int    cx = 0, cy = 0;

    while (std::hypot(dxF - sx, dyF - sy) >= 1.0)
    {
        double dist = std::hypot(dxF - sx, dyF - sy);
        double wMag = std::min(wind_W, dist);

        if (dist >= wind_D)
        {
            wX = wX / SQRT3 + (dist01(rng) * 2.0 - 1.0) * wMag / SQRT5;
            wY = wY / SQRT3 + (dist01(rng) * 2.0 - 1.0) * wMag / SQRT5;
        }
        else
        {
            wX /= SQRT3;  wY /= SQRT3;
            wind_M = wind_M < 3.0 ? dist01(rng) * 3.0 + 3.0 : wind_M / SQRT5;
        }

        vx += wX + wind_G * (dxF - sx) / dist;
        vy += wY + wind_G * (dyF - sy) / dist;

        double vMag = std::hypot(vx, vy);
        if (vMag > wind_M)
        {
            double vClip = wind_M / 2.0 + dist01(rng) * wind_M / 2.0;
            vx = (vx / vMag) * vClip;
            vy = (vy / vMag) * vClip;
        }

        sx += vx;  sy += vy;
        int rx = static_cast<int>(std::round(sx));
        int ry = static_cast<int>(std::round(sy));
        int step_x = rx - cx;
        int step_y = ry - cy;
        if (step_x || step_y)
        {
            queueMove(step_x, step_y);
            cx = rx; cy = ry;
        }
    }
}

std::pair<double, double> MouseThread::predict_target_position(double target_x, double target_y)
{
    auto current_time = std::chrono::steady_clock::now();

    if (prev_time.time_since_epoch().count() == 0 || !target_detected.load())
    {
        prev_time = current_time;
        prev_x = target_x;
        prev_y = target_y;
        prev_velocity_x = 0.0;
        prev_velocity_y = 0.0;
        return { target_x, target_y };
    }

    double dt = std::chrono::duration<double>(current_time - prev_time).count();
    if (dt < 1e-8) dt = 1e-8;

    double vx = (target_x - prev_x) / dt;
    double vy = (target_y - prev_y) / dt;

    vx = std::clamp(vx, -20000.0, 20000.0);
    vy = std::clamp(vy, -20000.0, 20000.0);

    prev_time = current_time;
    prev_x = target_x;
    prev_y = target_y;
    prev_velocity_x = vx;
    prev_velocity_y = vy;

    double predictedX = target_x + vx * prediction_interval;
    double predictedY = target_y + vy * prediction_interval;

    double detectionDelay = 0.05;
#ifdef USE_CUDA
    detectionDelay = trt_detector.lastInferenceTime.count();
#else
    if (dml_detector) detectionDelay = dml_detector->lastInferenceTimeDML.count();
#endif
    predictedX += vx * detectionDelay;
    predictedY += vy * detectionDelay;

    return { predictedX, predictedY };
}

void MouseThread::sendMovementToDriver(int dx, int dy)
{
    if (dx == 0 && dy == 0)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(input_method_mutex);

    if (kmbox)
    {
        kmbox->move(dx, dy);
    }
    else if (kmbox_net)
    {
        kmbox_net->move(dx, dy);
    }
    else if (makcu)
    {
        makcu->move(dx, dy);
    }
    else if (serial)
    {
        serial->move(dx, dy);
    }
    else if (gHub)
    {
        gHub->mouse_xy(dx, dy);
    }
    else
    {
        INPUT in{ 0 };
        in.type = INPUT_MOUSE;
        in.mi.dx = dx;  in.mi.dy = dy;
        in.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_VIRTUALDESK;
        SendInput(1, &in, sizeof(INPUT));
    }
}

std::pair<double, double> MouseThread::calc_movement(double tx, double ty, bool isShooting)
{
    // Base center position with aim offset always applied
    double effective_center_x = center_x - config.aim_offset_x;
    double effective_center_y = center_y - config.aim_offset_y;
    
    // Apply crosshair offset ONLY when manually shooting (parameter controlled)
    if (isShooting)
    {
        effective_center_x = center_x - config.crosshair_offset_x - config.aim_offset_x;
        effective_center_y = center_y - config.crosshair_offset_y - config.aim_offset_y;
    }
    
    double offx = tx - effective_center_x;
    double offy = ty - effective_center_y;
    double dist = std::hypot(offx, offy);
    double speed = calculate_speed_multiplier(dist);

    double degPerPxX = fov_x / screen_width;
    double degPerPxY = fov_y / screen_height;

    double mmx = offx * degPerPxX;
    double mmy = offy * degPerPxY;

    double corr = 1.0;
    double fps = static_cast<double>(captureFps.load());
    if (fps > 40.0) corr = 40.0 / fps;

    auto counts_pair = config.degToCounts(mmx, mmy, fov_x);
    double move_x = counts_pair.first * speed * corr;
    double move_y = counts_pair.second * speed * corr;

    return { move_x, move_y };
}

double MouseThread::calculate_speed_multiplier(double distance)
{
    if (distance < config.snapRadius)
        return min_speed_multiplier * config.snapBoostFactor;

    if (distance < config.nearRadius)
    {
        double t = distance / config.nearRadius;
        double curve = 1.0 - std::pow(1.0 - t, config.speedCurveExponent);
        return min_speed_multiplier +
            (max_speed_multiplier - min_speed_multiplier) * curve;
    }

    double norm = std::clamp(distance / max_distance, 0.0, 1.0);
    return min_speed_multiplier +
        (max_speed_multiplier - min_speed_multiplier) * norm;
}

bool MouseThread::check_target_in_scope(double target_x, double target_y, double target_w, double target_h, double reduction_factor)
{
    double center_target_x = target_x + target_w / 2.0;
    double center_target_y = target_y + target_h / 2.0;

    // reduction_factor controls hit area size (1.0 = full size, 1.5 = 1.5x size)
    double reduced_w = target_w * reduction_factor / 2.0;
    double reduced_h = target_h * reduction_factor / 2.0;

    double x1 = center_target_x - reduced_w;
    double x2 = center_target_x + reduced_w;
    double y1 = center_target_y - reduced_h;
    double y2 = center_target_y + reduced_h;

    // Apply crosshair offset for scope check (this is used during shooting)
    double effective_center_x = center_x - config.crosshair_offset_x;
    double effective_center_y = center_y - config.crosshair_offset_y;

    return (effective_center_x >= x1 && effective_center_x <= x2 && effective_center_y >= y1 && effective_center_y <= y2);
}

// Check if crosshair is on target using pivot point (more accurate for auto-shoot)
bool MouseThread::check_pivot_in_scope(double pivotX, double pivotY, double targetW, double targetH, double tolerance)
{
    // 自动开枪不使用准星偏移，直接用屏幕中心判断
    double effective_center_x = center_x;
    double effective_center_y = center_y;
    
    // Calculate hit box around pivot point
    // tolerance controls hit area size (1.0 = full target size, 1.5 = 1.5x size)
    double halfW = targetW * tolerance / 2.0;
    double halfH = targetH * tolerance / 2.0;
    
    // Minimum size to avoid too strict detection
    halfW = std::max(halfW, 10.0);
    halfH = std::max(halfH, 10.0);
    
    double x1 = pivotX - halfW;
    double x2 = pivotX + halfW;
    double y1 = pivotY - halfH;
    double y2 = pivotY + halfH;
    
    // Check if effective crosshair position is within the hit box
    return (effective_center_x >= x1 && effective_center_x <= x2 && effective_center_y >= y1 && effective_center_y <= y2);
}

void MouseThread::moveMouse(const AimbotTarget& target)
{
    std::lock_guard lg(input_method_mutex);

    auto predicted = predict_target_position(
        target.x + target.w / 2.0,
        target.y + target.h / 2.0);

    auto mv = calc_movement(predicted.first, predicted.second);
    queueMove(static_cast<int>(mv.first), static_cast<int>(mv.second));
}

void MouseThread::moveMousePivot(double pivotX, double pivotY, double targetVelX, double targetVelY, bool isShooting)
{
    std::lock_guard lg(input_method_mutex);

    auto current_time = std::chrono::steady_clock::now();
    
    // Get and reset accumulated mouse movement (mouse counts sent to driver)
    double mouseCounts_dx = 0.0, mouseCounts_dy = 0.0;
    {
        std::lock_guard<std::mutex> lock(mouse_accum_mutex);
        mouseCounts_dx = accumulated_mouse_dx;
        mouseCounts_dy = accumulated_mouse_dy;
        accumulated_mouse_dx = 0.0;
        accumulated_mouse_dy = 0.0;
    }

    double dt = std::chrono::duration<double>(current_time - prev_time).count();
    
    // Check if this is first frame or after a long gap
    bool isFirstFrame = (prev_time.time_since_epoch().count() == 0);
    bool isAfterGap = (dt > 0.5);  // More than 500ms gap
    
    if (isFirstFrame || isAfterGap)
    {
        prev_time = current_time;
        prev_x = pivotX; 
        prev_y = pivotY;
        prev_velocity_x = 0.0;
        prev_velocity_y = 0.0;
        smoothed_pivot_x = pivotX;
        smoothed_pivot_y = pivotY;

        // First frame: just move to target without prediction
        auto m0 = calc_movement(pivotX, pivotY, isShooting);
        queueMove(static_cast<int>(m0.first), static_cast<int>(m0.second));
        return;
    }

    prev_time = current_time;
    dt = std::max(dt, 1e-8);

    // ================================================================
    // SMOOTH PIVOT POSITION (for head-body switch)
    // ================================================================
    
    // Observed position change on screen
    double observed_dx = pivotX - prev_x;
    double observed_dy = pivotY - prev_y;
    double jump_dist = std::sqrt(observed_dx * observed_dx + observed_dy * observed_dy);
    
    // Detect head-body switch: medium jump (15-80 pixels), mostly vertical
    // Head-body switch typically has larger vertical component
    bool isHeadBodySwitch = false;
    if (jump_dist > 15.0 && jump_dist < 80.0)
    {
        double verticalRatio = std::abs(observed_dy) / (jump_dist + 0.001);
        // Head-body switch is mostly vertical (ratio > 0.7)
        isHeadBodySwitch = (verticalRatio > 0.6);
    }
    
    // Large jump = target switch (different person)
    bool isTargetSwitch = (jump_dist > 80.0);
    
    // Apply smoothing to pivot position
    double effectivePivotX = pivotX;
    double effectivePivotY = pivotY;
    
    if (isHeadBodySwitch)
    {
        // Head-body switch: use strong smoothing for gradual transition
        const double smoothAlpha = 0.3;  // Slower transition
        smoothed_pivot_x = smoothed_pivot_x * (1.0 - smoothAlpha) + pivotX * smoothAlpha;
        smoothed_pivot_y = smoothed_pivot_y * (1.0 - smoothAlpha) + pivotY * smoothAlpha;
        effectivePivotX = smoothed_pivot_x;
        effectivePivotY = smoothed_pivot_y;
    }
    else if (isTargetSwitch)
    {
        // Target switch: reset smoothing, move directly to new target
        smoothed_pivot_x = pivotX;
        smoothed_pivot_y = pivotY;
        effectivePivotX = pivotX;
        effectivePivotY = pivotY;
    }
    else
    {
        // Normal tracking: minimal smoothing for fast response
        const double smoothAlpha = 0.85;  // Very fast response
        smoothed_pivot_x = smoothed_pivot_x * (1.0 - smoothAlpha) + pivotX * smoothAlpha;
        smoothed_pivot_y = smoothed_pivot_y * (1.0 - smoothAlpha) + pivotY * smoothAlpha;
        effectivePivotX = smoothed_pivot_x;
        effectivePivotY = smoothed_pivot_y;
    }

    // ================================================================
    // VELOCITY CALCULATION
    // ================================================================
    
    // Convert our mouse counts to screen pixel effect
    auto screenEffect = config.countsToScreenPixels(mouseCounts_dx, mouseCounts_dy, fov_x, screen_width, screen_height);
    double mouseEffect_px_x = screenEffect.first;
    double mouseEffect_px_y = screenEffect.second;
    
    double target_vel_x = 0.0;
    double target_vel_y = 0.0;
    
    if (isTargetSwitch || isHeadBodySwitch)
    {
        // Large jump or head-body switch: don't use velocity to avoid jitter
        target_vel_x = 0.0;
        target_vel_y = 0.0;
    }
    else
    {
        // Normal case: calculate velocity
        // Target's TRUE movement = observed + mouse_effect
        double target_true_dx = observed_dx + mouseEffect_px_x;
        double target_true_dy = observed_dy + mouseEffect_px_y;
        
        // Convert to velocity (pixels per second)
        target_vel_x = target_true_dx / dt;
        target_vel_y = target_true_dy / dt;
        
        // Use tracker velocity if available (Kalman filtered, more stable)
        bool hasTrackerVel = (std::abs(targetVelX) > 0.1 || std::abs(targetVelY) > 0.1);
        if (hasTrackerVel)
        {
            double fps = static_cast<double>(captureFps.load());
            if (fps < 1.0) fps = 30.0;
            
            // Tracker velocity is pixels per frame -> convert to pixels per second
            double tracker_vel_x = targetVelX * fps;
            double tracker_vel_y = targetVelY * fps;
            
            // Tracker also observes combined effect, compensate
            double mouse_vel_x = mouseEffect_px_x / dt;
            double mouse_vel_y = mouseEffect_px_y / dt;
            
            double tracker_true_vel_x = tracker_vel_x + mouse_vel_x;
            double tracker_true_vel_y = tracker_vel_y + mouse_vel_y;
            
            // Blend: 60% tracker (more stable), 40% calculated (more responsive)
            target_vel_x = target_vel_x * 0.4 + tracker_true_vel_x * 0.6;
            target_vel_y = target_vel_y * 0.4 + tracker_true_vel_y * 0.6;
        }
    }
    
    // Smooth velocity (EMA filter) - faster response for tracking
    const double alpha = 0.6;  // Higher = more responsive
    target_vel_x = prev_velocity_x * (1.0 - alpha) + target_vel_x * alpha;
    target_vel_y = prev_velocity_y * (1.0 - alpha) + target_vel_y * alpha;
    
    // Clamp to reasonable range
    target_vel_x = std::clamp(target_vel_x, -6000.0, 6000.0);
    target_vel_y = std::clamp(target_vel_y, -6000.0, 6000.0);
    
    // Store for next frame
    prev_x = pivotX;
    prev_y = pivotY;
    prev_velocity_x = target_vel_x;
    prev_velocity_y = target_vel_y;

    // ================================================================
    // PREDICTION AND MOVEMENT
    // ================================================================
    
    // Note: crosshair offset is applied inside calc_movement(), not here
    // to avoid double application
    
    double target_speed = std::sqrt(target_vel_x * target_vel_x + target_vel_y * target_vel_y);
    
    // Current offset from crosshair to target (use smoothed position)
    // Use base center here, offset will be applied in calc_movement
    double offset_x = effectivePivotX - center_x;
    double offset_y = effectivePivotY - center_y;
    double offset_dist = std::sqrt(offset_x * offset_x + offset_y * offset_y);
    
    // Calculate mouse speed from accumulated movement
    double mouse_speed = std::sqrt(mouseEffect_px_x * mouseEffect_px_x + mouseEffect_px_y * mouseEffect_px_y) / dt;
    
    // Scenario classification
    const double SPEED_THRESHOLD = 50.0;  // pixels per second
    bool target_moving = (target_speed > SPEED_THRESHOLD);
    bool mouse_moving = (mouse_speed > SPEED_THRESHOLD);
    
    // ================================================================
    // STEP 3: Calculate prediction time based on scenario
    // ================================================================
    
    double prediction_time = prediction_interval;
    
    if (target_moving && !mouse_moving)
    {
        // Case 1: Only target moving - need more prediction
        double speed_factor = std::min(target_speed / 500.0, 1.0);
        prediction_time = prediction_interval + speed_factor * 0.05;  // Up to +50ms
    }
    else if (!target_moving && mouse_moving)
    {
        // Case 2: Only mouse moving (user tracking) - minimal prediction
        prediction_time = prediction_interval * 0.3;
    }
    else if (target_moving && mouse_moving)
    {
        // Case 3: Both moving - moderate prediction
        // Check if mouse is following target (same direction)
        double dot = (target_vel_x * mouseEffect_px_x + target_vel_y * mouseEffect_px_y);
        bool following = (dot > 0);
        
        if (following)
        {
            // Mouse following target - less prediction needed
            prediction_time = prediction_interval * 0.5;
        }
        else
        {
            // Mouse moving opposite to target - more prediction
            prediction_time = prediction_interval + 0.03;
        }
    }
    else
    {
        // Case 4: Both stationary - minimal prediction
        prediction_time = prediction_interval * 0.2;
    }
    
    // Cap prediction time
    prediction_time = std::clamp(prediction_time, 0.005, 0.12);
    
    // ================================================================
    // STEP 4: Calculate required mouse movement
    // ================================================================
    
    // Predicted target position (use smoothed position)
    double pred_x = effectivePivotX + target_vel_x * prediction_time;
    double pred_y = effectivePivotY + target_vel_y * prediction_time;
    
    // For moving targets, we need to be more aggressive to catch up
    // The speed multiplier system is designed for static aiming, not tracking
    // So we add a tracking boost when target is moving
    double tracking_boost = 1.0;
    if (target_moving)
    {
        // Base boost for any moving target
        double base_boost = 1.1;
        
        // Additional boost based on how fast target is moving
        double speed_boost = std::min(target_speed / 500.0, 0.5) * 0.3;  // Up to +0.15 for fast targets
        
        // Additional boost if we're behind the target (offset in same direction as velocity)
        double offset_in_vel_direction = (offset_x * target_vel_x + offset_y * target_vel_y) / (target_speed + 1.0);
        double catchup_boost = 0.0;
        if (offset_in_vel_direction > 0)
        {
            // We're behind the target - need to catch up
            catchup_boost = std::min(offset_in_vel_direction / 50.0, 0.5);  // Up to +0.5
        }
        
        tracking_boost = base_boost + speed_boost + catchup_boost;
        tracking_boost = std::min(tracking_boost, 1.8);  // Cap at 1.8x to avoid overshooting
    }
    
    // Required movement = predicted offset from center
    // This accounts for both current offset AND target movement
    auto mv = calc_movement(pred_x, pred_y, isShooting);
    
    // Apply tracking boost
    int mx = static_cast<int>(mv.first * tracking_boost);
    int my = static_cast<int>(mv.second * tracking_boost);

    if (mx == 0 && my == 0)
    {
        return;
    }

    if (wind_mouse_enabled)
    {
        windMouseMoveRelative(mx, my);
    }
    else
    {
        queueMove(mx, my);
    }
}

void MouseThread::pressMouse(const AimbotTarget& target)
{
    std::lock_guard<std::mutex> lock(input_method_mutex);

    bool bScope = check_target_in_scope(target.x, target.y, target.w, target.h, bScope_multiplier);
    if (bScope && !mouse_pressed)
    {
        if (kmbox)
        {
            kmbox->press(0);
        }
        else if (kmbox_net)
        {
            kmbox_net->leftDown();
        }
        else if (makcu)
        {
            makcu->press(0);
        }
        else if (serial)
        {
            serial->press();
        }
        else if (gHub)
        {
            gHub->mouse_down();
        }
        else
        {
            INPUT input = { 0 };
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            SendInput(1, &input, sizeof(INPUT));
        }
        mouse_pressed = true;
    }
    else if (!bScope && mouse_pressed)
    {
        if (kmbox)
        {
            kmbox->release(0);
        }
        else if (kmbox_net)
        {
            kmbox_net->leftUp();
        }
        else if (makcu)
        {
            makcu->release(0);
        }
        else if (serial)
        {
            serial->release();
        }
        else if (gHub)
        {
            gHub->mouse_up();
        }
        else
        {
            INPUT input = { 0 };
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
            SendInput(1, &input, sizeof(INPUT));
        }
        mouse_pressed = false;
    }
}

void MouseThread::pressMouseDirect()
{
    std::lock_guard<std::mutex> lock(input_method_mutex);

    if (!mouse_pressed)
    {
        if (kmbox)
        {
            kmbox->press(0);
        }
        else if (kmbox_net)
        {
            kmbox_net->leftDown();
        }
        else if (makcu)
        {
            makcu->press(0);
        }
        else if (serial)
        {
            serial->press();
        }
        else if (gHub)
        {
            gHub->mouse_down();
        }
        else
        {
            INPUT input = { 0 };
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            SendInput(1, &input, sizeof(INPUT));
        }
        mouse_pressed = true;
    }
}

void MouseThread::releaseMouse()
{
    std::lock_guard<std::mutex> lock(input_method_mutex);

    if (mouse_pressed)
    {
        if (kmbox)
        {
            kmbox->release(0);
        }
        else if (kmbox_net)
        {
            kmbox_net->leftUp();
        }
        else if (makcu)
        {
            makcu->release(0);
        }
        else if (serial)
        {
            serial->release();
        }
        else if (gHub)
        {
            gHub->mouse_up();
        }
        else
        {
            INPUT input = { 0 };
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
            SendInput(1, &input, sizeof(INPUT));
        }
        mouse_pressed = false;
    }
}

void MouseThread::forceClick()
{
    std::lock_guard<std::mutex> lock(input_method_mutex);

    // 强制连点：屏蔽物理左键 → 松开 → 按下 → 取消屏蔽
    if (kmbox_net)
    {
        kmbox_net->maskMouseLeft(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        kmbox_net->leftUp();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        kmbox_net->leftDown();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        kmbox_net->maskMouseLeft(false);
    }
    else if (kmbox)
    {
        kmbox->release(0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        kmbox->press(0);
    }
    else if (makcu)
    {
        makcu->release(0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        makcu->press(0);
    }
    else if (serial)
    {
        serial->release();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        serial->press();
    }
    else if (gHub)
    {
        gHub->mouse_up();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        gHub->mouse_down();
    }
    else
    {
        INPUT inputs[2] = {};
        inputs[0].type = INPUT_MOUSE;
        inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput(1, &inputs[0], sizeof(INPUT));
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        inputs[1].type = INPUT_MOUSE;
        inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        SendInput(1, &inputs[1], sizeof(INPUT));
    }
}

void MouseThread::resetPrediction()
{
    prev_time = std::chrono::steady_clock::time_point();
    prev_x = 0;
    prev_y = 0;
    prev_velocity_x = 0;
    prev_velocity_y = 0;
    smoothed_pivot_x = 0;
    smoothed_pivot_y = 0;
    target_detected.store(false);
}

void MouseThread::checkAndResetPredictions()
{
    auto current_time = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(current_time - last_target_time).count();

    if (elapsed > 0.5 && target_detected.load())
    {
        resetPrediction();
    }
}

std::vector<std::pair<double, double>> MouseThread::predictFuturePositions(double pivotX, double pivotY, int frames)
{
    std::vector<std::pair<double, double>> result;
    result.reserve(frames);

    const double fixedFps = 30.0;
    double frame_time = 1.0 / fixedFps;

    auto current_time = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(current_time - prev_time).count();

    if (prev_time.time_since_epoch().count() == 0 || dt > 0.5)
    {
        return result;
    }

    double vx = prev_velocity_x;
    double vy = prev_velocity_y;
    
    for (int i = 1; i <= frames; i++)
    {
        double t = frame_time * i;

        double px = pivotX + vx * t;
        double py = pivotY + vy * t;

        result.push_back({ px, py });
    }

    return result;
}

void MouseThread::storeFuturePositions(const std::vector<std::pair<double, double>>& positions)
{
    std::lock_guard<std::mutex> lock(futurePositionsMutex);
    futurePositions = positions;
}

void MouseThread::clearFuturePositions()
{
    std::lock_guard<std::mutex> lock(futurePositionsMutex);
    futurePositions.clear();
}

std::vector<std::pair<double, double>> MouseThread::getFuturePositions()
{
    std::lock_guard<std::mutex> lock(futurePositionsMutex);
    return futurePositions;
}

void MouseThread::setSerialConnection(SerialConnection* newSerial)
{
    std::lock_guard<std::mutex> lock(input_method_mutex);
    serial = newSerial;
}

void MouseThread::setKmboxConnection(Kmbox_b_Connection* newKmbox)
{
    std::lock_guard<std::mutex> lock(input_method_mutex);
    kmbox = newKmbox;
}

void MouseThread::setKmboxNetConnection(KmboxNetConnection* newKmbox_net)
{
    std::lock_guard<std::mutex> lock(input_method_mutex);
    kmbox_net = newKmbox_net;
}

void MouseThread::setMakcuConnection(MakcuConnection* newMakcu)
{
    std::lock_guard<std::mutex> lock(input_method_mutex);
    makcu = newMakcu;
}

void MouseThread::setGHubMouse(GhubMouse* newGHub)
{
    std::lock_guard<std::mutex> lock(input_method_mutex);
    gHub = newGHub;
}