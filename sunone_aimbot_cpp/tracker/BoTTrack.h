#ifndef BOT_TRACK_H
#define BOT_TRACK_H

#include <opencv2/opencv.hpp>
#include "HighManeuverKalmanFilter.h"
#include "FeatureBank.h"

enum class TrackState;

/**
 * @brief Enhanced track with ReID features and high-maneuver Kalman filter
 */
class BoTTrack
{
public:
    BoTTrack(const cv::Rect& rect, float score, int classId,
             HighManeuverKalmanFilter::MotionModel model = HighManeuverKalmanFilter::MotionModel::CA);
    
    void predict(double dt = 1.0);
    void update(const cv::Rect& rect, float score, int classId,
                const std::vector<float>& feature, float featureQuality, int frameId);
    void markLost();
    void markRemoved();
    void activate(int frameId, int trackId);
    void reActivate(const cv::Rect& rect, float score, int classId,
                    const std::vector<float>& feature, float featureQuality, int frameId);
    
    void applyCameraMotion(const cv::Mat& homography);
    
    cv::Rect getRect() const;
    cv::Rect getPredictedRect() const;
    std::pair<float, float> getVelocity() const;
    std::pair<float, float> getAcceleration() const;
    std::vector<float> getFeature() const;
    float getFeatureQuality() const;
    
    int trackId;
    int classId;
    float score;
    TrackState state;
    int frameId;
    int startFrame;
    int trackletLen;
    bool isActivated;
    
private:
    HighManeuverKalmanFilter m_kf;
    FeatureBank m_featureBank;
    cv::Rect m_rect;
};

#endif // BOT_TRACK_H
