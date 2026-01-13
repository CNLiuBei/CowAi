#ifndef AIMBOTTARGET_H
#define AIMBOTTARGET_H

#include <opencv2/opencv.hpp>
#include <vector>

class AimbotTarget
{
public:
    int x, y, w, h;
    int classId;
    int trackId;
    int associatedBodyTrackId;  // For head targets, stores the associated body's trackId
    
    // Velocity from tracker (pixels per frame)
    double velocityX;
    double velocityY;

    double pivotX;
    double pivotY;

    AimbotTarget(int x, int y, int w, int h, int classId, double pivotX = 0.0, double pivotY = 0.0, 
                 int trackId = -1, int bodyTrackId = -1, double vx = 0.0, double vy = 0.0);
};

AimbotTarget* sortTargets(
    const std::vector<cv::Rect>& boxes,
    const std::vector<int>& classes,
    const std::vector<int>& trackIds,
    int screenWidth,
    int screenHeight,
    bool disableHeadshot,
    int lockedTrackId = -1,
    float lockThreshold = 1.5f,
    int lockedBodyTrackId = -1,
    const std::vector<std::pair<float, float>>* velocities = nullptr
);

#endif // AIMBOTTARGET_H