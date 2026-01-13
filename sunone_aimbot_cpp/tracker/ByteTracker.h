#ifndef BYTE_TRACKER_H
#define BYTE_TRACKER_H

#include <vector>
#include <memory>
#include <opencv2/opencv.hpp>
#include "STrack.h"

struct TrackerDetection
{
    cv::Rect rect;
    float score;
    int classId;
};

struct TrackedObject
{
    int trackId;
    cv::Rect rect;
    cv::Rect predictedRect;
    float score;
    int classId;
    bool isNew;
    float velocityX;  // Velocity in pixels per frame
    float velocityY;
};

class ByteTracker
{
public:
    ByteTracker(float trackThresh = 0.5f, float highThresh = 0.6f, 
                float matchThresh = 0.8f, int maxTimeLost = 30);
    
    std::vector<TrackedObject> update(const std::vector<TrackerDetection>& detections);
    void reset();
    
private:
    std::vector<std::vector<float>> calcIoUMatrix(
        const std::vector<std::shared_ptr<STrack>>& tracks,
        const std::vector<TrackerDetection>& detections);
    
    void linearAssignment(const std::vector<std::vector<float>>& costMatrix,
                          float thresh,
                          std::vector<std::pair<int, int>>& matches,
                          std::vector<int>& unmatchedTracks,
                          std::vector<int>& unmatchedDets);
    
    float trackThresh;
    float highThresh;
    float matchThresh;
    int maxTimeLost;
    int frameId;
    int trackIdCount;
    
    std::vector<std::shared_ptr<STrack>> trackedTracks;
    std::vector<std::shared_ptr<STrack>> lostTracks;
};

#endif
