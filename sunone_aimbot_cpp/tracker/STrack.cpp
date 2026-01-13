#include "STrack.h"
#include <algorithm>

int STrack::nextId = 0;

STrack::STrack(const cv::Rect& rect, float score, int classId)
    : rect(rect), score(score), classId(classId),
      trackId(-1), state(TrackState::New), frameId(0),
      startFrame(0), trackletLen(0), isActivated(false)
{
    Eigen::Vector4f measurement;
    measurement << rect.x + rect.width / 2.0f,
                   rect.y + rect.height / 2.0f,
                   (float)rect.width,
                   (float)rect.height;
    kf.init(measurement);
}

void STrack::predict()
{
    kf.predict();
}

void STrack::update(const cv::Rect& newRect, float newScore, int newClassId, int newFrameId)
{
    rect = newRect;
    score = newScore;
    classId = newClassId;
    frameId = newFrameId;
    trackletLen++;
    state = TrackState::Tracked;
    isActivated = true;
    
    Eigen::Vector4f measurement;
    measurement << newRect.x + newRect.width / 2.0f,
                   newRect.y + newRect.height / 2.0f,
                   (float)newRect.width,
                   (float)newRect.height;
    kf.update(measurement);
}

void STrack::markLost()
{
    state = TrackState::Lost;
}

void STrack::markRemoved()
{
    state = TrackState::Removed;
}

void STrack::activate(int newFrameId, int newTrackId)
{
    trackId = newTrackId;
    frameId = newFrameId;
    startFrame = newFrameId;
    trackletLen = 1;
    state = TrackState::Tracked;
    isActivated = true;
}

void STrack::reActivate(const cv::Rect& newRect, float newScore, int newClassId, int newFrameId)
{
    rect = newRect;
    score = newScore;
    classId = newClassId;
    frameId = newFrameId;
    trackletLen++;
    state = TrackState::Tracked;
    isActivated = true;
    
    Eigen::Vector4f measurement;
    measurement << newRect.x + newRect.width / 2.0f,
                   newRect.y + newRect.height / 2.0f,
                   (float)newRect.width,
                   (float)newRect.height;
    kf.update(measurement);
}

cv::Rect STrack::getRect() const
{
    return rect;
}

cv::Rect STrack::getPredictedRect() const
{
    Eigen::Vector4f state = kf.getPredictedState();
    int cx = (int)state(0);
    int cy = (int)state(1);
    int w = (int)state(2);
    int h = (int)state(3);
    return cv::Rect(cx - w / 2, cy - h / 2, w, h);
}

std::pair<float, float> STrack::getVelocity() const
{
    Eigen::Vector2f vel = kf.getVelocity();
    return std::make_pair(vel(0), vel(1));
}

float STrack::calcIoU(const cv::Rect& a, const cv::Rect& b)
{
    int x1 = std::max(a.x, b.x);
    int y1 = std::max(a.y, b.y);
    int x2 = std::min(a.x + a.width, b.x + b.width);
    int y2 = std::min(a.y + a.height, b.y + b.height);
    
    if (x2 <= x1 || y2 <= y1) return 0.0f;
    
    float intersection = (float)(x2 - x1) * (y2 - y1);
    float areaA = (float)a.width * a.height;
    float areaB = (float)b.width * b.height;
    
    return intersection / (areaA + areaB - intersection);
}
