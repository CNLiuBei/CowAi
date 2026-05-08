#include "BoTSORT.h"
#include "STrack.h"
#include <algorithm>
#include <numeric>

BoTSORT::BoTSORT(const Config& config)
    : m_config(config), m_frameId(0), m_trackIdCount(0)
{
    // Initialize ReID extractor if enabled
    if (m_config.useReID)
    {
        ReIDFeatureExtractor::Config reidConfig;
        reidConfig.modelPath = m_config.reidModelPath;
        reidConfig.featureDim = m_config.featureDim;
        m_featureExtractor = std::make_unique<ReIDFeatureExtractor>(reidConfig);
    }
    
    // Initialize GMC if enabled
    if (m_config.useGMC)
    {
        CameraMotionCompensator::Config gmcConfig;
        gmcConfig.method = m_config.gmcMethod;
        m_gmc = std::make_unique<CameraMotionCompensator>(gmcConfig);
    }
    
    m_homography = cv::Mat::eye(3, 3, CV_64F);
}

BoTSORT::~BoTSORT()
{
}

void BoTSORT::reset()
{
    m_trackedTracks.clear();
    m_lostTracks.clear();
    m_longTermLostTracks.clear();
    m_frameId = 0;
    m_trackIdCount = 0;
    m_prevFrame = cv::Mat();
    m_homography = cv::Mat::eye(3, 3, CV_64F);
}

void BoTSORT::setConfig(const Config& config)
{
    m_config = config;
    reset();
}

std::vector<TrackedObject> BoTSORT::update(
    const cv::Mat& frame,
    const std::vector<TrackerDetection>& detections)
{
    m_frameId++;
    
    // Estimate camera motion
    if (m_config.useGMC && m_gmc && !m_prevFrame.empty())
    {
        cv::Mat gray, prevGray;
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        cv::cvtColor(m_prevFrame, prevGray, cv::COLOR_BGR2GRAY);
        m_homography = m_gmc->estimateCameraMotion(prevGray, gray);
    }
    
    // Predict all tracks
    for (auto& track : m_trackedTracks)
    {
        track->predict();
        if (m_config.useGMC && m_gmc && m_gmc->hasSignificantMotion())
            track->applyCameraMotion(m_homography);
    }
    
    for (auto& track : m_lostTracks)
    {
        track->predict();
        if (m_config.useGMC && m_gmc && m_gmc->hasSignificantMotion())
            track->applyCameraMotion(m_homography);
    }
    
    for (auto& track : m_longTermLostTracks)
    {
        track->predict();
        if (m_config.useGMC && m_gmc && m_gmc->hasSignificantMotion())
            track->applyCameraMotion(m_homography);
    }
    
    // Split detections by confidence
    std::vector<TrackerDetection> highDets, lowDets;
    for (const auto& det : detections)
    {
        if (det.score >= m_config.highThresh)
            highDets.push_back(det);
        else if (det.score >= m_config.trackThresh)
            lowDets.push_back(det);
    }
    
    // Extract ReID features for high confidence detections
    std::vector<std::vector<float>> highFeatures;
    if (m_config.useReID && m_featureExtractor && m_featureExtractor->isInitialized())
    {
        std::vector<cv::Rect> bboxes;
        for (const auto& det : highDets)
            bboxes.push_back(det.rect);
        highFeatures = m_featureExtractor->extractFeatures(frame, bboxes);
    }
    else
    {
        highFeatures.resize(highDets.size());
    }
    
    // Stage 1: High confidence detections + Active tracks (IOU + ReID)
    std::vector<std::pair<int, int>> matches1;
    std::vector<int> unmatchedTracks1, unmatchedDets1;
    firstAssociation(highDets, highFeatures, matches1, unmatchedTracks1, unmatchedDets1);
    
    std::vector<std::shared_ptr<BoTTrack>> newTrackedTracks;
    
    // Update matched tracks
    for (const auto& [trackIdx, detIdx] : matches1)
    {
        float quality = calculateFeatureQuality(highDets[detIdx]);
        m_trackedTracks[trackIdx]->update(
            highDets[detIdx].rect,
            highDets[detIdx].score,
            highDets[detIdx].classId,
            highFeatures[detIdx],
            quality,
            m_frameId
        );
        newTrackedTracks.push_back(m_trackedTracks[trackIdx]);
    }
    
    // Stage 2: Low confidence detections + Unmatched tracks (IOU only)
    std::vector<std::shared_ptr<BoTTrack>> remainTracks;
    for (int idx : unmatchedTracks1)
        remainTracks.push_back(m_trackedTracks[idx]);
    
    std::vector<std::pair<int, int>> matches2;
    std::vector<int> unmatchedTracks2, unmatchedDets2;
    secondAssociation(lowDets, remainTracks, matches2, unmatchedTracks2, unmatchedDets2);
    
    for (const auto& [trackIdx, detIdx] : matches2)
    {
        remainTracks[trackIdx]->update(
            lowDets[detIdx].rect,
            lowDets[detIdx].score,
            lowDets[detIdx].classId,
            {},
            0.5f,
            m_frameId
        );
        newTrackedTracks.push_back(remainTracks[trackIdx]);
    }
    
    // Mark unmatched tracks as lost
    for (int idx : unmatchedTracks2)
    {
        remainTracks[idx]->markLost();
        m_lostTracks.push_back(remainTracks[idx]);
    }
    
    // Stage 3: Unmatched high detections + Lost tracks (ReID primary)
    std::vector<TrackerDetection> unmatchedHighDets;
    std::vector<std::vector<float>> unmatchedHighFeatures;
    for (int idx : unmatchedDets1)
    {
        unmatchedHighDets.push_back(highDets[idx]);
        unmatchedHighFeatures.push_back(highFeatures[idx]);
    }
    
    std::vector<std::pair<int, int>> matches3;
    std::vector<int> unmatchedLost3, unmatchedDets3;
    thirdAssociation(unmatchedHighDets, unmatchedHighFeatures, matches3, unmatchedLost3, unmatchedDets3);
    
    for (const auto& [lostIdx, detIdx] : matches3)
    {
        float quality = calculateFeatureQuality(unmatchedHighDets[detIdx]);
        m_lostTracks[lostIdx]->reActivate(
            unmatchedHighDets[detIdx].rect,
            unmatchedHighDets[detIdx].score,
            unmatchedHighDets[detIdx].classId,
            unmatchedHighFeatures[detIdx],
            quality,
            m_frameId
        );
        newTrackedTracks.push_back(m_lostTracks[lostIdx]);
    }
    
    // Stage 4: Long-term lost tracks (ReID only)
    std::vector<TrackerDetection> remainingDets;
    std::vector<std::vector<float>> remainingFeatures;
    for (int idx : unmatchedDets3)
    {
        remainingDets.push_back(unmatchedHighDets[idx]);
        remainingFeatures.push_back(unmatchedHighFeatures[idx]);
    }
    
    std::vector<std::pair<int, int>> matches4;
    std::vector<int> unmatchedLongTerm4, unmatchedDets4;
    fourthAssociation(remainingDets, remainingFeatures, matches4, unmatchedLongTerm4, unmatchedDets4);
    
    for (const auto& [longTermIdx, detIdx] : matches4)
    {
        float quality = calculateFeatureQuality(remainingDets[detIdx]);
        m_longTermLostTracks[longTermIdx]->reActivate(
            remainingDets[detIdx].rect,
            remainingDets[detIdx].score,
            remainingDets[detIdx].classId,
            remainingFeatures[detIdx],
            quality,
            m_frameId
        );
        newTrackedTracks.push_back(m_longTermLostTracks[longTermIdx]);
    }
    
    // Create new tracks for remaining unmatched detections
    for (int idx : unmatchedDets4)
    {
        auto newTrack = std::make_shared<BoTTrack>(
            remainingDets[idx].rect,
            remainingDets[idx].score,
            remainingDets[idx].classId,
            m_config.kalmanModel
        );
        newTrack->activate(m_frameId, ++m_trackIdCount);
        
        if (!remainingFeatures[idx].empty())
        {
            float quality = calculateFeatureQuality(remainingDets[idx]);
            newTrack->update(
                remainingDets[idx].rect,
                remainingDets[idx].score,
                remainingDets[idx].classId,
                remainingFeatures[idx],
                quality,
                m_frameId
            );
        }
        
        newTrackedTracks.push_back(newTrack);
    }
    
    // Update lost tracks
    std::vector<std::shared_ptr<BoTTrack>> newLostTracks;
    for (int idx : unmatchedLost3)
    {
        if (m_frameId - m_lostTracks[idx]->frameId < m_config.maxTimeLost)
        {
            newLostTracks.push_back(m_lostTracks[idx]);
        }
        else if (m_frameId - m_lostTracks[idx]->frameId < m_config.longTermLostFrames)
        {
            m_longTermLostTracks.push_back(m_lostTracks[idx]);
        }
    }
    
    // Update long-term lost tracks
    std::vector<std::shared_ptr<BoTTrack>> newLongTermLostTracks;
    for (int idx : unmatchedLongTerm4)
    {
        if (m_frameId - m_longTermLostTracks[idx]->frameId < m_config.longTermLostFrames)
        {
            newLongTermLostTracks.push_back(m_longTermLostTracks[idx]);
        }
    }
    
    m_trackedTracks = newTrackedTracks;
    m_lostTracks = newLostTracks;
    m_longTermLostTracks = newLongTermLostTracks;
    
    // Build output
    std::vector<TrackedObject> result;
    for (const auto& track : m_trackedTracks)
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
    
    m_prevFrame = frame.clone();
    
    return result;
}

void BoTSORT::firstAssociation(
    const std::vector<TrackerDetection>& highDets,
    const std::vector<std::vector<float>>& features,
    std::vector<std::pair<int, int>>& matches,
    std::vector<int>& unmatchedTracks,
    std::vector<int>& unmatchedDets)
{
    auto costMatrix = calcHybridCostMatrix(
        m_trackedTracks, highDets, features,
        m_config.lambdaIOU, m_config.lambdaReID
    );
    
    linearAssignment(costMatrix, m_config.matchThresh, matches, unmatchedTracks, unmatchedDets);
}

void BoTSORT::secondAssociation(
    const std::vector<TrackerDetection>& lowDets,
    const std::vector<std::shared_ptr<BoTTrack>>& remainTracks,
    std::vector<std::pair<int, int>>& matches,
    std::vector<int>& unmatchedTracks,
    std::vector<int>& unmatchedDets)
{
    // IOU only for low confidence detections
    std::vector<std::vector<float>> iouMatrix(remainTracks.size(),
        std::vector<float>(lowDets.size(), 0.0f));
    
    for (size_t i = 0; i < remainTracks.size(); i++)
    {
        cv::Rect trackRect = remainTracks[i]->getPredictedRect();
        for (size_t j = 0; j < lowDets.size(); j++)
        {
            iouMatrix[i][j] = STrack::calcIoU(trackRect, lowDets[j].rect);
        }
    }
    
    linearAssignment(iouMatrix, 0.5f, matches, unmatchedTracks, unmatchedDets);
}

void BoTSORT::thirdAssociation(
    const std::vector<TrackerDetection>& unmatchedHighDets,
    const std::vector<std::vector<float>>& features,
    std::vector<std::pair<int, int>>& matches,
    std::vector<int>& unmatchedLost,
    std::vector<int>& unmatchedDets)
{
    auto costMatrix = calcHybridCostMatrix(
        m_lostTracks, unmatchedHighDets, features,
        m_config.lambdaIOULow, m_config.lambdaReIDLow
    );
    
    linearAssignment(costMatrix, m_config.matchThresh, matches, unmatchedLost, unmatchedDets);
}

void BoTSORT::fourthAssociation(
    const std::vector<TrackerDetection>& remainingDets,
    const std::vector<std::vector<float>>& features,
    std::vector<std::pair<int, int>>& matches,
    std::vector<int>& unmatchedLongTerm,
    std::vector<int>& unmatchedDets)
{
    if (!m_config.useReID || features.empty())
    {
        unmatchedLongTerm.resize(m_longTermLostTracks.size());
        std::iota(unmatchedLongTerm.begin(), unmatchedLongTerm.end(), 0);
        unmatchedDets.resize(remainingDets.size());
        std::iota(unmatchedDets.begin(), unmatchedDets.end(), 0);
        return;
    }
    
    // ReID only for long-term lost
    auto costMatrix = calcHybridCostMatrix(
        m_longTermLostTracks, remainingDets, features,
        0.0f, 1.0f  // Pure ReID
    );
    
    linearAssignment(costMatrix, m_config.reidMatchThresh, matches, unmatchedLongTerm, unmatchedDets);
}

std::vector<std::vector<float>> BoTSORT::calcHybridCostMatrix(
    const std::vector<std::shared_ptr<BoTTrack>>& tracks,
    const std::vector<TrackerDetection>& detections,
    const std::vector<std::vector<float>>& features,
    float lambdaIOU,
    float lambdaReID)
{
    std::vector<std::vector<float>> costMatrix(tracks.size(),
        std::vector<float>(detections.size(), 1.0f));
    
    for (size_t i = 0; i < tracks.size(); i++)
    {
        cv::Rect trackRect = tracks[i]->getPredictedRect();
        auto trackFeature = tracks[i]->getFeature();
        
        for (size_t j = 0; j < detections.size(); j++)
        {
            // Calculate IOU
            float iou = STrack::calcIoU(trackRect, detections[j].rect);
            float iouCost = 1.0f - iou;
            
            // Calculate ReID similarity
            float reidCost = 1.0f;
            if (m_config.useReID && !trackFeature.empty() && 
                j < features.size() && !features[j].empty())
            {
                float similarity = ReIDFeatureExtractor::cosineSimilarity(
                    trackFeature, features[j]
                );
                reidCost = 1.0f - similarity;
            }
            
            // Hybrid cost
            costMatrix[i][j] = lambdaIOU * iouCost + lambdaReID * reidCost;
        }
    }
    
    return costMatrix;
}

void BoTSORT::linearAssignment(
    const std::vector<std::vector<float>>& costMatrix,
    float thresh,
    std::vector<std::pair<int, int>>& matches,
    std::vector<int>& unmatchedA,
    std::vector<int>& unmatchedB)
{
    matches.clear();
    unmatchedA.clear();
    unmatchedB.clear();
    
    if (costMatrix.empty() || costMatrix[0].empty())
    {
        for (size_t i = 0; i < costMatrix.size(); i++)
            unmatchedA.push_back(i);
        return;
    }
    
    int numA = costMatrix.size();
    int numB = costMatrix[0].size();
    
    std::vector<bool> matchedA(numA, false);
    std::vector<bool> matchedB(numB, false);
    
    // Greedy matching
    std::vector<std::tuple<float, int, int>> candidates;
    for (int i = 0; i < numA; i++)
    {
        for (int j = 0; j < numB; j++)
        {
            if (costMatrix[i][j] <= (1.0f - thresh))
            {
                candidates.push_back({costMatrix[i][j], i, j});
            }
        }
    }
    
    std::sort(candidates.begin(), candidates.end(),
        [](const auto& a, const auto& b) { return std::get<0>(a) < std::get<0>(b); });
    
    for (const auto& [cost, idxA, idxB] : candidates)
    {
        if (!matchedA[idxA] && !matchedB[idxB])
        {
            matches.push_back({idxA, idxB});
            matchedA[idxA] = true;
            matchedB[idxB] = true;
        }
    }
    
    for (int i = 0; i < numA; i++)
        if (!matchedA[i]) unmatchedA.push_back(i);
    
    for (int j = 0; j < numB; j++)
        if (!matchedB[j]) unmatchedB.push_back(j);
}

float BoTSORT::calculateFeatureQuality(const TrackerDetection& det)
{
    float quality = det.score;  // Base on detection confidence
    
    // Adjust based on bbox size
    float area = det.rect.width * det.rect.height;
    if (area < 400)  // Small detection
        quality *= 0.7f;
    else if (area > 10000)  // Large detection
        quality *= 0.9f;
    
    return std::min(1.0f, std::max(0.0f, quality));
}
