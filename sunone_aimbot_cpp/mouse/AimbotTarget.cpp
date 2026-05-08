#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <winsock2.h>
#include <Windows.h>

#include <cmath>
#include <limits>
#include <opencv2/opencv.hpp>

#include "sunone_aimbot_cpp.h"
#include "AimbotTarget.h"
#include "config.h"
#include <optional>

AimbotTarget::AimbotTarget(int x_, int y_, int w_, int h_, int cls, double px, double py, int tid, int bodyTid, double vx, double vy)
    : x(x_), y(y_), w(w_), h(h_), classId(cls), pivotX(px), pivotY(py), trackId(tid), associatedBodyTrackId(bodyTid), velocityX(vx), velocityY(vy)
{
}

// Calculate aim point for target
// body_y_offset: 0.0 = top of box, 0.5 = center, 1.0 = bottom
// head_y_offset: same logic for head targets
static cv::Point calculateAimPoint(const cv::Rect& box, int classId, bool disableHeadshot)
{
    int offsetY;
    if (classId == config.class_head && !disableHeadshot)
    {
        // Head: use head_y_offset (0.0 = top, 0.5 = center, 1.0 = bottom)
        float effectiveOffset = config.head_y_offset;
        // Clamp to valid range
        effectiveOffset = std::max(0.0f, std::min(1.0f, effectiveOffset));
        offsetY = static_cast<int>(box.height * effectiveOffset);
    }
    else
    {
        // Body: use body_y_offset (0.0 = top, 0.5 = center, 1.0 = bottom)
        float effectiveOffset = config.body_y_offset;
        // Clamp to valid range
        effectiveOffset = std::max(0.0f, std::min(1.0f, effectiveOffset));
        offsetY = static_cast<int>(box.height * effectiveOffset);
    }
    
    // Clamp to stay within box
    offsetY = std::max(0, std::min(offsetY, std::max(0, box.height - 1)));
    
    return cv::Point(box.x + box.width / 2, box.y + offsetY);
}

// Calculate distance to screen center
static double calculateDistance(const cv::Point& target, const cv::Point& center)
{
    double dx = target.x - center.x;
    double dy = target.y - center.y;
    return std::sqrt(dx * dx + dy * dy);
}

// Calculate IoU between two boxes
static float calculateIoU(const cv::Rect& a, const cv::Rect& b)
{
    int x1 = std::max(a.x, b.x);
    int y1 = std::max(a.y, b.y);
    int x2 = std::min(a.x + a.width, b.x + b.width);
    int y2 = std::min(a.y + a.height, b.y + b.height);
    
    if (x2 <= x1 || y2 <= y1)
        return 0.0f;
    
    float intersection = static_cast<float>((x2 - x1) * (y2 - y1));
    float areaA = static_cast<float>(a.width * a.height);
    float areaB = static_cast<float>(b.width * b.height);
    
    return intersection / (areaA + areaB - intersection);
}

// Calculate distance between two box centers
static double calculateBoxDistance(const cv::Rect& a, const cv::Rect& b)
{
    double ax = a.x + a.width / 2.0;
    double ay = a.y + a.height / 2.0;
    double bx = b.x + b.width / 2.0;
    double by = b.y + b.height / 2.0;
    return std::sqrt((ax - bx) * (ax - bx) + (ay - by) * (ay - by));
}

// Find the nearest body to a head (for same-person association)
static int findNearestBodyToHead(int headIdx, const std::vector<cv::Rect>& boxes, 
                                  const std::vector<int>& classes)
{
    if (headIdx < 0 || headIdx >= (int)boxes.size())
        return -1;
    
    const cv::Rect& headBox = boxes[headIdx];
    int bestBodyIdx = -1;
    double bestScore = -1.0;
    
    double headCenterX = headBox.x + headBox.width / 2.0;
    double headBottom = headBox.y + headBox.height;
    
    for (size_t i = 0; i < boxes.size(); i++)
    {
        if (classes[i] != config.class_player)
            continue;
        
        const cv::Rect& bodyBox = boxes[i];
        double bodyCenterX = bodyBox.x + bodyBox.width / 2.0;
        double bodyTop = bodyBox.y;
        
        // Calculate distance between centers
        double dist = calculateBoxDistance(headBox, bodyBox);
        
        // Body should be reasonably close to head
        double maxDist = bodyBox.height * 2.0;
        if (dist > maxDist)
            continue;
        
        // 1. IoU score (overlap)
        float iou = calculateIoU(headBox, bodyBox);
        
        // 2. Horizontal alignment score
        double horizontalOffset = std::abs(headCenterX - bodyCenterX);
        double maxHorizontalOffset = bodyBox.width * 0.8;
        double horizontalScore = 1.0 - std::min(1.0, horizontalOffset / maxHorizontalOffset);
        
        // 3. Vertical position score (body should be below head)
        double verticalGap = bodyTop - headBottom;
        double verticalScore;
        if (verticalGap < 0)
        {
            // Head overlaps body - good
            verticalScore = 1.0;
        }
        else
        {
            // Body below head - penalize based on gap
            double maxVerticalGap = bodyBox.height * 0.3;
            verticalScore = 1.0 - std::min(1.0, verticalGap / maxVerticalGap);
        }
        verticalScore = std::max(0.0, verticalScore);
        
        // 4. Distance score
        double distScore = 1.0 - (dist / maxDist);
        
        // Final score
        double score = iou * 0.35 + horizontalScore * 0.30 + verticalScore * 0.20 + distScore * 0.15;
        
        if (score > bestScore)
        {
            bestScore = score;
            bestBodyIdx = static_cast<int>(i);
        }
    }
    
    // Only return if score is above threshold
    if (bestScore < 0.25)
        return -1;
    
    return bestBodyIdx;
}

// Find the nearest head to a body (for same-person association)
// Uses combination of distance, IoU overlap, vertical position, and horizontal alignment
// Handles multiple heads at same height by prioritizing IoU and horizontal alignment
static int findNearestHeadToBody(int bodyIdx, const std::vector<cv::Rect>& boxes, 
                                  const std::vector<int>& classes)
{
    if (bodyIdx < 0 || bodyIdx >= (int)boxes.size())
        return -1;
    
    const cv::Rect& bodyBox = boxes[bodyIdx];
    int bestHeadIdx = -1;
    double bestScore = -1.0;
    
    // Body top edge - head should be near or above this
    double bodyTop = bodyBox.y;
    double bodyCenterX = bodyBox.x + bodyBox.width / 2.0;
    
    for (size_t i = 0; i < boxes.size(); i++)
    {
        if (classes[i] != config.class_head)
            continue;
        
        const cv::Rect& headBox = boxes[i];
        double headCenterX = headBox.x + headBox.width / 2.0;
        double headBottom = headBox.y + headBox.height;
        
        // Calculate distance between centers
        double dist = calculateBoxDistance(headBox, bodyBox);
        
        // Head should be reasonably close to body (within 2x body height)
        double maxDist = bodyBox.height * 2.0;
        if (dist > maxDist)
            continue;
        
        // 1. IoU score (overlap) - most reliable indicator
        float iou = calculateIoU(headBox, bodyBox);
        
        // 2. Horizontal alignment score (head center should be near body center)
        double horizontalOffset = std::abs(headCenterX - bodyCenterX);
        double maxHorizontalOffset = bodyBox.width * 0.8;  // Allow some offset for rotation
        double horizontalScore = 1.0 - std::min(1.0, horizontalOffset / maxHorizontalOffset);
        
        // 3. Vertical position score (head should be near body top)
        // Head bottom should be near or overlapping body top
        double verticalGap = bodyTop - headBottom;  // Negative if overlapping
        double maxVerticalGap = bodyBox.height * 0.5;
        double verticalScore;
        if (verticalGap < 0)
        {
            // Head overlaps body - good, but penalize if too deep inside
            double overlapDepth = -verticalGap;
            double maxOverlap = bodyBox.height * 0.4;
            verticalScore = 1.0 - std::min(1.0, overlapDepth / maxOverlap) * 0.3;
        }
        else
        {
            // Head above body - penalize based on gap
            verticalScore = 1.0 - std::min(1.0, verticalGap / maxVerticalGap);
        }
        verticalScore = std::max(0.0, verticalScore);
        
        // 4. Distance score (closer is better)
        double distScore = 1.0 - (dist / maxDist);
        
        // 5. Size ratio score (head should be smaller than body)
        double headArea = headBox.width * headBox.height;
        double bodyArea = bodyBox.width * bodyBox.height;
        double sizeRatio = headArea / (bodyArea + 1.0);
        double sizeScore = (sizeRatio > 0.05 && sizeRatio < 0.5) ? 1.0 : 0.5;
        
        // Final score: weighted combination
        // IoU and horizontal alignment are most important for multi-head scenarios
        double score = iou * 0.35 + horizontalScore * 0.30 + verticalScore * 0.15 + 
                       distScore * 0.15 + sizeScore * 0.05;
        
        if (score > bestScore)
        {
            bestScore = score;
            bestHeadIdx = static_cast<int>(i);
        }
    }
    
    // Only return if score is above threshold (avoid false associations)
    if (bestScore < 0.3)
        return -1;
    
    return bestHeadIdx;
}

std::optional<AimbotTarget> sortTargets(
    const std::vector<cv::Rect>& boxes,
    const std::vector<int>& classes,
    const std::vector<int>& trackIds,
    int screenWidth,
    int screenHeight,
    bool disableHeadshot,
    int lockedTrackId,
    float lockThreshold,
    int lockedBodyTrackId,
    const std::vector<std::pair<float, float>>* velocities)
{
    if (boxes.empty() || classes.empty())
    {
        return std::nullopt;
    }

    cv::Point center(screenWidth / 2, screenHeight / 2);
    
    // Calculate FOV range limit (in pixels from center)
    // fovX and fovY define the aiming range
    double fovRangeX = config.fovX * screenWidth / 100.0;  // Convert percentage to pixels
    double fovRangeY = config.fovY * screenHeight / 100.0;

    // 1. Find locked target's current position and info
    int lockedIdx = -1;
    double lockedDistance = std::numeric_limits<double>::max();
    int lockedClass = -1;
    
    if (lockedTrackId >= 0 && !trackIds.empty())
    {
        for (size_t i = 0; i < trackIds.size(); i++)
        {
            if (trackIds[i] == lockedTrackId)
            {
                lockedIdx = static_cast<int>(i);
                lockedClass = classes[i];
                cv::Point aimPoint = calculateAimPoint(boxes[i], classes[i], disableHeadshot);
                lockedDistance = calculateDistance(aimPoint, center);
                break;
            }
        }
    }
    
    // 1b. If locked target not found but we have associated body, try to find it
    int fallbackBodyIdx = -1;
    if (lockedIdx < 0 && lockedBodyTrackId >= 0 && !trackIds.empty())
    {
        for (size_t i = 0; i < trackIds.size(); i++)
        {
            if (trackIds[i] == lockedBodyTrackId && classes[i] == config.class_player)
            {
                fallbackBodyIdx = static_cast<int>(i);
                break;
            }
        }
    }
    
    // 1c. If locked head not found and no associated body trackId, 
    // try to find the nearest body to screen center as fallback
    // This handles the case where head was locked but body association wasn't established
    if (lockedIdx < 0 && lockedTrackId >= 0 && fallbackBodyIdx < 0 && lockedBodyTrackId < 0)
    {
        // Find nearest body to screen center as emergency fallback
        double nearestBodyDist = std::numeric_limits<double>::max();
        for (size_t i = 0; i < boxes.size(); i++)
        {
            if (classes[i] == config.class_player)
            {
                cv::Point bodyCenter(boxes[i].x + boxes[i].width / 2, boxes[i].y + boxes[i].height / 2);
                double dist = calculateDistance(bodyCenter, center);
                if (dist < nearestBodyDist)
                {
                    nearestBodyDist = dist;
                    fallbackBodyIdx = static_cast<int>(i);
                }
            }
        }
    }

    // Helper lambda to check if point is within FOV range
    auto isInFovRange = [&](const cv::Point& pt) -> bool {
        double dx = std::abs(pt.x - center.x);
        double dy = std::abs(pt.y - center.y);
        return (dx <= fovRangeX && dy <= fovRangeY);
    };

    // 2. Find nearest Head (within FOV range)
    int nearestHeadIdx = -1;
    double nearestHeadDistance = std::numeric_limits<double>::max();
    
    if (!disableHeadshot)
    {
        for (size_t i = 0; i < boxes.size(); i++)
        {
            if (classes[i] == config.class_head)
            {
                cv::Point aimPoint = calculateAimPoint(boxes[i], classes[i], disableHeadshot);
                
                // Check FOV range
                if (!isInFovRange(aimPoint))
                    continue;
                
                double distance = calculateDistance(aimPoint, center);
                if (distance < nearestHeadDistance)
                {
                    nearestHeadDistance = distance;
                    nearestHeadIdx = static_cast<int>(i);
                }
            }
        }
    }

    // 3. Find nearest Body (within FOV range)
    int nearestBodyIdx = -1;
    double nearestBodyDistance = std::numeric_limits<double>::max();
    
    for (size_t i = 0; i < boxes.size(); i++)
    {
        if (classes[i] == config.class_player)
        {
            cv::Point aimPoint = calculateAimPoint(boxes[i], classes[i], disableHeadshot);
            
            // Check FOV range
            if (!isInFovRange(aimPoint))
                continue;
            
            double distance = calculateDistance(aimPoint, center);
            if (distance < nearestBodyDistance)
            {
                nearestBodyDistance = distance;
                nearestBodyIdx = static_cast<int>(i);
            }
        }
    }

    // 4. Determine best target (Head priority)
    int bestIdx = -1;
    double bestDistance = std::numeric_limits<double>::max();
    
    if (!disableHeadshot && nearestHeadIdx >= 0)
    {
        bestIdx = nearestHeadIdx;
        bestDistance = nearestHeadDistance;
    }
    else if (nearestBodyIdx >= 0)
    {
        bestIdx = nearestBodyIdx;
        bestDistance = nearestBodyDistance;
    }

    if (bestIdx == -1)
    {
        return std::nullopt;
    }

    // 5. Apply hysteresis with person association
    // Allow switching when:
    // 1. New target is significantly closer (user moved crosshair to new target)
    // 2. Body->Head switch for same person
    int finalIdx = bestIdx;
    int associatedBodyIdx = -1;  // Track which body this head is associated with
    
    // Threshold for "user intentionally moved to new target"
    // If new target is closer than 50% of locked target distance, switch
    const double switchThreshold = 0.5;
    
    if (lockedIdx >= 0 && lockedTrackId >= 0)
    {
        bool shouldKeepLocked = false;
        bool userMovedToNewTarget = (bestDistance < lockedDistance * switchThreshold);
        
        if (userMovedToNewTarget)
        {
            // User clearly moved crosshair to a new target, allow switch
            shouldKeepLocked = false;
        }
        else if (lockedClass == config.class_head && !disableHeadshot)
        {
            // Locked is Head
            if (bestIdx == nearestHeadIdx)
            {
                // New target is also Head, apply hysteresis
                if (bestDistance * lockThreshold >= lockedDistance)
                {
                    shouldKeepLocked = true;
                }
            }
            else
            {
                // New target is Body (no Head available), keep locked Head
                shouldKeepLocked = true;
            }
        }
        else if (lockedClass == config.class_player)
        {
            // Locked is Body
            if (!disableHeadshot && nearestHeadIdx >= 0)
            {
                // Head appeared - find the nearest head to locked body
                int nearestHeadToLocked = findNearestHeadToBody(lockedIdx, boxes, classes);
                
                if (nearestHeadToLocked >= 0)
                {
                    // Found a head near locked body - switch to it (same person likely)
                    finalIdx = nearestHeadToLocked;
                    associatedBodyIdx = lockedIdx;  // Remember the body
                    shouldKeepLocked = false;
                }
                else
                {
                    // No head near locked body, check if screen-nearest head is worth switching
                    if (nearestHeadDistance * lockThreshold >= lockedDistance)
                    {
                        // Keep locked body (new head not close enough to screen center)
                        shouldKeepLocked = true;
                    }
                    else
                    {
                        // Switch to new head (significantly closer to screen center)
                        shouldKeepLocked = false;
                    }
                }
            }
            else
            {
                // Only Body available, apply hysteresis
                if (bestDistance * lockThreshold >= lockedDistance)
                {
                    shouldKeepLocked = true;
                }
            }
        }
        
        if (shouldKeepLocked)
        {
            finalIdx = lockedIdx;
        }
    }
    else if (lockedTrackId >= 0 && lockedIdx < 0)
    {
        // Locked target (head) disappeared from detection
        // Try to use fallback body if available
        if (fallbackBodyIdx >= 0)
        {
            // Found the associated body, try to find a head near it
            if (!disableHeadshot)
            {
                int headNearFallback = findNearestHeadToBody(fallbackBodyIdx, boxes, classes);
                if (headNearFallback >= 0)
                {
                    finalIdx = headNearFallback;
                    associatedBodyIdx = fallbackBodyIdx;
                }
                else
                {
                    // No head found near body, use the body itself
                    finalIdx = fallbackBodyIdx;
                }
            }
            else
            {
                finalIdx = fallbackBodyIdx;
            }
        }
        else
        {
            // No fallback body available - locked head disappeared completely
            // Return nullopt to stop aiming (don't switch to another person)
            // Don't switch to another person's head!
            return std::nullopt;
        }
    }

    // 6. Calculate final aim point
    cv::Point finalAimPoint = calculateAimPoint(boxes[finalIdx], classes[finalIdx], disableHeadshot);
    
    int finalX = boxes[finalIdx].x;
    int finalY = boxes[finalIdx].y;  // Fixed: use box.y directly
    int finalW = boxes[finalIdx].width;
    int finalH = boxes[finalIdx].height;
    int finalClass = classes[finalIdx];
    int finalTrackId = (!trackIds.empty() && finalIdx < (int)trackIds.size()) ? trackIds[finalIdx] : -1;
    
    // Get velocity from tracker if available
    double finalVelX = 0.0, finalVelY = 0.0;
    if (velocities && finalIdx < (int)velocities->size())
    {
        finalVelX = (*velocities)[finalIdx].first;
        finalVelY = (*velocities)[finalIdx].second;
    }
    
    // For head targets, store the associated body's trackId
    // For body targets, store its own trackId as associatedBodyId for future reference
    int associatedBodyId = -1;
    if (finalClass == config.class_head)
    {
        if (associatedBodyIdx >= 0)
        {
            // We have a known associated body from previous logic
            associatedBodyId = (!trackIds.empty() && associatedBodyIdx < (int)trackIds.size()) ? trackIds[associatedBodyIdx] : -1;
        }
        else if (lockedIdx >= 0 && lockedClass == config.class_player)
        {
            // We switched from body to head, remember the body's trackId
            associatedBodyId = (!trackIds.empty() && lockedIdx < (int)trackIds.size()) ? trackIds[lockedIdx] : -1;
        }
        else if (lockedBodyTrackId >= 0)
        {
            // Preserve existing body association
            associatedBodyId = lockedBodyTrackId;
        }
        else
        {
            // No known body association - try to find the body for this head
            int bodyForHead = findNearestBodyToHead(finalIdx, boxes, classes);
            if (bodyForHead >= 0)
            {
                associatedBodyId = (!trackIds.empty() && bodyForHead < (int)trackIds.size()) ? trackIds[bodyForHead] : -1;
            }
        }
    }
    else if (finalClass == config.class_player)
    {
        // Body target: store its own trackId as associatedBodyId
        // This ensures lockedBodyTrackId gets updated in main loop
        associatedBodyId = finalTrackId;
    }

    double pivotX = finalAimPoint.x;
    double pivotY = finalAimPoint.y;

    return AimbotTarget(finalX, finalY, finalW, finalH, finalClass, pivotX, pivotY, finalTrackId, associatedBodyId, finalVelX, finalVelY);
}
