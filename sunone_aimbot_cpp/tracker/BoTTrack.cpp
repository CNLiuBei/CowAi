#include "BoTTrack.h"
#include "STrack.h"
#include <Eigen/Dense>

BoTTrack::BoTTrack(const cv::Rect& rect, float score, int classId,
                   HighManeuverKalmanFilter::MotionModel model)
    : trackId(-1), classId(classId), score(score),
      state(TrackState::New), frameId(0), startFrame(0),
      trackletLen(0), isActivated(false),
      m_kf(model), m_rect(rect)
{
    // Initialize Kalman filter
    Eigen::Vector4f measurement(
        rect.x + rect.width / 2.0f,
        rect.y + rect.height / 2.0f,
        static_cast<float>(rect.width),
        static_cast<float>(rect.height)
    );
    m_kf.init(measurement);
}

void BoTTrack::predict(double dt)
{
    m_kf.predict(dt);
}

void BoTTrack::update(const cv::Rect& rect, float score, int classId,
                      const std::vector<float>& feature, float featureQuality, int frameId)
{
    this->frameId = frameId;
    this->score = score;
    this->classId = classId;
    this->trackletLen++;
    
    // Update Kalman filter
    Eigen::Vector4f measurement(
        rect.x + rect.width / 2.0f,
        rect.y + rect.height / 2.0f,
        static_cast<float>(rect.width),
        static_cast<float>(rect.height)
    );
    m_kf.update(measurement);
    
    // Update feature bank
    if (!feature.empty())
    {
        m_featureBank.addFeature(feature, featureQuality);
    }
    
    // Update rect from Kalman state
    auto state = m_kf.getState();
    m_rect = cv::Rect(
        static_cast<int>(state[0] - state[2] / 2.0f),
        static_cast<int>(state[1] - state[3] / 2.0f),
        static_cast<int>(state[2]),
        static_cast<int>(state[3])
    );
    
    this->state = TrackState::Tracked;
}

void BoTTrack::markLost()
{
    state = TrackState::Lost;
}

void BoTTrack::markRemoved()
{
    state = TrackState::Removed;
}

void BoTTrack::activate(int frameId, int trackId)
{
    this->trackId = trackId;
    this->frameId = frameId;
    this->startFrame = frameId;
    this->trackletLen = 0;
    this->state = TrackState::Tracked;
    this->isActivated = true;
}

void BoTTrack::reActivate(const cv::Rect& rect, float score, int classId,
                          const std::vector<float>& feature, float featureQuality, int frameId)
{
    // Re-initialize Kalman filter
    Eigen::Vector4f measurement(
        rect.x + rect.width / 2.0f,
        rect.y + rect.height / 2.0f,
        static_cast<float>(rect.width),
        static_cast<float>(rect.height)
    );
    m_kf.update(measurement);
    
    // Update feature
    if (!feature.empty())
    {
        m_featureBank.addFeature(feature, featureQuality);
    }
    
    this->frameId = frameId;
    this->score = score;
    this->classId = classId;
    this->trackletLen = 0;
    this->state = TrackState::Tracked;
    this->isActivated = true;
    
    // Update rect
    auto state = m_kf.getState();
    m_rect = cv::Rect(
        static_cast<int>(state[0] - state[2] / 2.0f),
        static_cast<int>(state[1] - state[3] / 2.0f),
        static_cast<int>(state[2]),
        static_cast<int>(state[3])
    );
}

void BoTTrack::applyCameraMotion(const cv::Mat& homography)
{
    if (homography.empty())
        return;
    
    // Get current predicted position
    auto state = m_kf.getPredictedState();
    cv::Point2f center(state[0], state[1]);
    
    // Apply homography transformation
    std::vector<cv::Point2f> points = {center};
    std::vector<cv::Point2f> transformed;
    cv::perspectiveTransform(points, transformed, homography);
    
    // Update Kalman state with compensated position
    Eigen::Vector4f compensated(
        transformed[0].x,
        transformed[0].y,
        state[2],
        state[3]
    );
    
    // Note: This is a simplified compensation
    // In full implementation, we'd adjust the state directly
}

cv::Rect BoTTrack::getRect() const
{
    return m_rect;
}

cv::Rect BoTTrack::getPredictedRect() const
{
    auto state = m_kf.getPredictedState();
    return cv::Rect(
        static_cast<int>(state[0] - state[2] / 2.0f),
        static_cast<int>(state[1] - state[3] / 2.0f),
        static_cast<int>(state[2]),
        static_cast<int>(state[3])
    );
}

std::pair<float, float> BoTTrack::getVelocity() const
{
    auto vel = m_kf.getVelocity();
    return {vel[0], vel[1]};
}

std::pair<float, float> BoTTrack::getAcceleration() const
{
    auto accel = m_kf.getAcceleration();
    return {accel[0], accel[1]};
}

std::vector<float> BoTTrack::getFeature() const
{
    return m_featureBank.getRepresentativeFeature();
}

float BoTTrack::getFeatureQuality() const
{
    return m_featureBank.getAverageQuality();
}
