#include "ByteTracker.h"
#include <algorithm>
#include <numeric>
#include <cmath>

ByteTracker::ByteTracker(float trackThresh, float highThresh, 
                         float matchThresh, int maxTimeLost)
    : trackThresh(trackThresh), highThresh(highThresh),
      matchThresh(matchThresh), maxTimeLost(maxTimeLost),
      frameId(0), trackIdCount(0)
{
}

void ByteTracker::reset()
{
    trackedTracks.clear();
    lostTracks.clear();
    frameId = 0;
    trackIdCount = 0;
}

std::vector<std::vector<float>> ByteTracker::calcIoUMatrix(
    const std::vector<std::shared_ptr<STrack>>& tracks,
    const std::vector<TrackerDetection>& detections)
{
    std::vector<std::vector<float>> iouMatrix(tracks.size(), 
        std::vector<float>(detections.size(), 0.0f));
    
    for (size_t i = 0; i < tracks.size(); i++)
    {
        cv::Rect trackRect = tracks[i]->getPredictedRect();
        for (size_t j = 0; j < detections.size(); j++)
        {
            iouMatrix[i][j] = STrack::calcIoU(trackRect, detections[j].rect);
        }
    }
    
    return iouMatrix;
}

void ByteTracker::linearAssignment(const std::vector<std::vector<float>>& costMatrix,
                                   float thresh,
                                   std::vector<std::pair<int, int>>& matches,
                                   std::vector<int>& unmatchedTracks,
                                   std::vector<int>& unmatchedDets)
{
    matches.clear();
    unmatchedTracks.clear();
    unmatchedDets.clear();
    
    if (costMatrix.empty() || costMatrix[0].empty())
    {
        for (size_t i = 0; i < costMatrix.size(); i++)
            unmatchedTracks.push_back((int)i);
        return;
    }
    
    int numTracks = (int)costMatrix.size();
    int numDets = (int)costMatrix[0].size();
    
    std::vector<bool> trackMatched(numTracks, false);
    std::vector<bool> detMatched(numDets, false);
    
    // Greedy matching (simplified Hungarian)
    std::vector<std::tuple<float, int, int>> candidates;
    for (int i = 0; i < numTracks; i++)
    {
        for (int j = 0; j < numDets; j++)
        {
            if (costMatrix[i][j] >= thresh)
            {
                candidates.push_back({costMatrix[i][j], i, j});
            }
        }
    }
    
    std::sort(candidates.begin(), candidates.end(), 
        [](const auto& a, const auto& b) { return std::get<0>(a) > std::get<0>(b); });
    
    for (const auto& [iou, trackIdx, detIdx] : candidates)
    {
        if (!trackMatched[trackIdx] && !detMatched[detIdx])
        {
            matches.push_back({trackIdx, detIdx});
            trackMatched[trackIdx] = true;
            detMatched[detIdx] = true;
        }
    }
    
    for (int i = 0; i < numTracks; i++)
        if (!trackMatched[i]) unmatchedTracks.push_back(i);
    
    for (int j = 0; j < numDets; j++)
        if (!detMatched[j]) unmatchedDets.push_back(j);
}

std::vector<TrackedObject> ByteTracker::update(const std::vector<TrackerDetection>& detections)
{
    frameId++;
    
    // Predict all tracks
    for (auto& track : trackedTracks)
        track->predict();
    for (auto& track : lostTracks)
        track->predict();
    
    // Split detections into high and low score
    std::vector<TrackerDetection> highDets, lowDets;
    for (const auto& det : detections)
    {
        if (det.score >= highThresh)
            highDets.push_back(det);
        else if (det.score >= trackThresh)
            lowDets.push_back(det);
    }
    
    // First association: high score detections with tracked tracks
    std::vector<std::pair<int, int>> matches;
    std::vector<int> unmatchedTracks, unmatchedDets;
    
    auto iouMatrix = calcIoUMatrix(trackedTracks, highDets);
    linearAssignment(iouMatrix, matchThresh, matches, unmatchedTracks, unmatchedDets);
    
    std::vector<std::shared_ptr<STrack>> newTrackedTracks;
    
    for (const auto& [trackIdx, detIdx] : matches)
    {
        trackedTracks[trackIdx]->update(
            highDets[detIdx].rect,
            highDets[detIdx].score,
            highDets[detIdx].classId,
            frameId);
        newTrackedTracks.push_back(trackedTracks[trackIdx]);
    }
    
    // Second association: low score detections with remaining tracks
    std::vector<std::shared_ptr<STrack>> remainTracks;
    for (int idx : unmatchedTracks)
        remainTracks.push_back(trackedTracks[idx]);
    
    if (!remainTracks.empty() && !lowDets.empty())
    {
        auto iouMatrix2 = calcIoUMatrix(remainTracks, lowDets);
        std::vector<std::pair<int, int>> matches2;
        std::vector<int> unmatchedTracks2, unmatchedDets2;
        linearAssignment(iouMatrix2, 0.5f, matches2, unmatchedTracks2, unmatchedDets2);
        
        for (const auto& [trackIdx, detIdx] : matches2)
        {
            remainTracks[trackIdx]->update(
                lowDets[detIdx].rect,
                lowDets[detIdx].score,
                lowDets[detIdx].classId,
                frameId);
            newTrackedTracks.push_back(remainTracks[trackIdx]);
        }
        
        // Mark unmatched as lost
        for (int idx : unmatchedTracks2)
        {
            remainTracks[idx]->markLost();
            lostTracks.push_back(remainTracks[idx]);
        }
    }
    else
    {
        for (auto& track : remainTracks)
        {
            track->markLost();
            lostTracks.push_back(track);
        }
    }
    
    // Third association: unmatched high detections with lost tracks
    std::vector<TrackerDetection> unmatchedHighDets;
    for (int idx : unmatchedDets)
        unmatchedHighDets.push_back(highDets[idx]);
    
    if (!lostTracks.empty() && !unmatchedHighDets.empty())
    {
        auto iouMatrix3 = calcIoUMatrix(lostTracks, unmatchedHighDets);
        std::vector<std::pair<int, int>> matches3;
        std::vector<int> unmatchedLost, unmatchedNew;
        linearAssignment(iouMatrix3, matchThresh, matches3, unmatchedLost, unmatchedNew);
        
        for (const auto& [trackIdx, detIdx] : matches3)
        {
            lostTracks[trackIdx]->reActivate(
                unmatchedHighDets[detIdx].rect,
                unmatchedHighDets[detIdx].score,
                unmatchedHighDets[detIdx].classId,
                frameId);
            newTrackedTracks.push_back(lostTracks[trackIdx]);
        }
        
        // Create new tracks for unmatched detections
        for (int idx : unmatchedNew)
        {
            auto newTrack = std::make_shared<STrack>(
                unmatchedHighDets[idx].rect,
                unmatchedHighDets[idx].score,
                unmatchedHighDets[idx].classId);
            newTrack->activate(frameId, ++trackIdCount);
            newTrackedTracks.push_back(newTrack);
        }
        
        // Keep lost tracks that weren't matched
        std::vector<std::shared_ptr<STrack>> newLostTracks;
        for (int idx : unmatchedLost)
        {
            if (frameId - lostTracks[idx]->frameId < maxTimeLost)
                newLostTracks.push_back(lostTracks[idx]);
        }
        lostTracks = newLostTracks;
    }
    else
    {
        // Create new tracks for all unmatched high detections
        for (const auto& det : unmatchedHighDets)
        {
            auto newTrack = std::make_shared<STrack>(det.rect, det.score, det.classId);
            newTrack->activate(frameId, ++trackIdCount);
            newTrackedTracks.push_back(newTrack);
        }
        
        // Remove old lost tracks
        std::vector<std::shared_ptr<STrack>> newLostTracks;
        for (auto& track : lostTracks)
        {
            if (frameId - track->frameId < maxTimeLost)
                newLostTracks.push_back(track);
        }
        lostTracks = newLostTracks;
    }
    
    trackedTracks = newTrackedTracks;
    
    // Build output
    std::vector<TrackedObject> result;
    for (const auto& track : trackedTracks)
    {
        if (track->isActivated)
        {
            TrackedObject obj;
            obj.trackId = track->trackId;
            obj.rect = track->getRect();
            obj.predictedRect = track->getPredictedRect();
            obj.score = track->score;
            obj.classId = track->classId;
            obj.isNew = (track->trackletLen == 1);
            auto vel = track->getVelocity();
            obj.velocityX = vel.first;
            obj.velocityY = vel.second;
            result.push_back(obj);
        }
    }
    
    return result;
}
