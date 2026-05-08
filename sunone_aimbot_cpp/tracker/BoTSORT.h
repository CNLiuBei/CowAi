#ifndef BOTSORT_H
#define BOTSORT_H

#include <vector>
#include <memory>
#include <opencv2/opencv.hpp>
#include "BoTTrack.h"
#include "ReIDFeatureExtractor.h"
#include "CameraMotionCompensator.h"
#include "ByteTracker.h"

/**
 * @brief BoT-SORT tracker with ReID and camera motion compensation
 */
class BoTSORT
{
public:
    struct Config
    {
        // ByteTrack parameters
        float trackThresh = 0.5f;
        float highThresh = 0.6f;
        float matchThresh = 0.8f;
        int maxTimeLost = 30;
        int longTermLostFrames = 300;
        
        // ReID parameters
        bool useReID = true;
        std::string reidModelPath = "models/osnet_x0_25.onnx";
        int featureDim = 128;
        float lambdaIOU = 0.8f;
        float lambdaReID = 0.2f;
        float lambdaIOULow = 0.3f;
        float lambdaReIDLow = 0.7f;
        float reidMatchThresh = 0.7f;
        
        // Kalman parameters
        HighManeuverKalmanFilter::MotionModel kalmanModel = 
            HighManeuverKalmanFilter::MotionModel::CA;
        bool adaptiveNoise = true;
        
        // GMC parameters
        bool useGMC = true;
        CameraMotionCompensator::Method gmcMethod = 
            CameraMotionCompensator::Method::SPARSE_OPTICAL_FLOW;
    };
    
    BoTSORT(const Config& config = Config());
    ~BoTSORT();
    
    std::vector<TrackedObject> update(
        const cv::Mat& frame,
        const std::vector<TrackerDetection>& detections
    );
    
    void reset();
    
    const Config& getConfig() const { return m_config; }
    void setConfig(const Config& config);
    
private:
    Config m_config;
    
    std::unique_ptr<ReIDFeatureExtractor> m_featureExtractor;
    std::unique_ptr<CameraMotionCompensator> m_gmc;
    
    int m_frameId;
    int m_trackIdCount;
    std::vector<std::shared_ptr<BoTTrack>> m_trackedTracks;
    std::vector<std::shared_ptr<BoTTrack>> m_lostTracks;
    std::vector<std::shared_ptr<BoTTrack>> m_longTermLostTracks;
    
    cv::Mat m_prevFrame;
    cv::Mat m_homography;
    
    void firstAssociation(
        const std::vector<TrackerDetection>& highDets,
        const std::vector<std::vector<float>>& features,
        std::vector<std::pair<int, int>>& matches,
        std::vector<int>& unmatchedTracks,
        std::vector<int>& unmatchedDets
    );
    
    void secondAssociation(
        const std::vector<TrackerDetection>& lowDets,
        const std::vector<std::shared_ptr<BoTTrack>>& remainTracks,
        std::vector<std::pair<int, int>>& matches,
        std::vector<int>& unmatchedTracks,
        std::vector<int>& unmatchedDets
    );
    
    void thirdAssociation(
        const std::vector<TrackerDetection>& unmatchedHighDets,
        const std::vector<std::vector<float>>& features,
        std::vector<std::pair<int, int>>& matches,
        std::vector<int>& unmatchedLost,
        std::vector<int>& unmatchedDets
    );
    
    void fourthAssociation(
        const std::vector<TrackerDetection>& remainingDets,
        const std::vector<std::vector<float>>& features,
        std::vector<std::pair<int, int>>& matches,
        std::vector<int>& unmatchedLongTerm,
        std::vector<int>& unmatchedDets
    );
    
    std::vector<std::vector<float>> calcHybridCostMatrix(
        const std::vector<std::shared_ptr<BoTTrack>>& tracks,
        const std::vector<TrackerDetection>& detections,
        const std::vector<std::vector<float>>& features,
        float lambdaIOU,
        float lambdaReID
    );
    
    void linearAssignment(
        const std::vector<std::vector<float>>& costMatrix,
        float thresh,
        std::vector<std::pair<int, int>>& matches,
        std::vector<int>& unmatchedA,
        std::vector<int>& unmatchedB
    );
    
    float calculateFeatureQuality(const TrackerDetection& det);
};

#endif // BOTSORT_H
