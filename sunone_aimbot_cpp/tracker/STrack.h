#ifndef STRACK_H
#define STRACK_H

#include <opencv2/opencv.hpp>
#include "KalmanFilter.h"

enum class TrackState
{
    New,
    Tracked,
    Lost,
    Removed
};

class STrack
{
public:
    STrack(const cv::Rect& rect, float score, int classId);
    
    void predict();
    void update(const cv::Rect& rect, float score, int classId, int frameId);
    void markLost();
    void markRemoved();
    void activate(int frameId, int trackId);
    void reActivate(const cv::Rect& rect, float score, int classId, int frameId);
    
    cv::Rect getRect() const;
    cv::Rect getPredictedRect() const;
    std::pair<float, float> getVelocity() const;  // Get velocity (vx, vy) in pixels per frame
    
    static float calcIoU(const cv::Rect& a, const cv::Rect& b);
    
    int trackId;
    int classId;
    float score;
    TrackState state;
    int frameId;
    int startFrame;
    int trackletLen;
    bool isActivated;
    
private:
    KalmanFilter kf;
    cv::Rect rect;
    
    static int nextId;
};

#endif
