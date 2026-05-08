#include "HighManeuverKalmanFilter.h"
#include <cmath>
#include <algorithm>

HighManeuverKalmanFilter::HighManeuverKalmanFilter(MotionModel model)
    : m_model(model), m_initialized(false)
{
    m_config.model = model;
    initializeMatrices();
}

HighManeuverKalmanFilter::HighManeuverKalmanFilter(const Config& config)
    : m_config(config), m_model(config.model), m_initialized(false)
{
    initializeMatrices();
}

void HighManeuverKalmanFilter::initializeMatrices()
{
    // Initialize state to zero
    m_state.setZero();
    m_predictedState.setZero();
    
    // Measurement matrix H: we observe [cx, cy, w, h]
    m_H.setZero();
    m_H(0, 0) = 1.0f;  // cx
    m_H(1, 1) = 1.0f;  // cy
    m_H(2, 2) = 1.0f;  // w
    m_H(3, 3) = 1.0f;  // h
    
    // Measurement noise R (assume moderate noise in detection)
    m_R.setIdentity();
    m_R *= 10.0f;  // Measurement noise variance
    
    // Base process noise Q (will be adapted)
    m_Q.setIdentity();
    m_Q.block<4, 4>(0, 0) *= static_cast<float>(m_config.baseProcessNoise);      // Position
    m_Q.block<2, 2>(4, 4) *= static_cast<float>(m_config.baseProcessNoise * 10); // Velocity
    m_Q.block<2, 2>(6, 6) *= static_cast<float>(m_config.baseAccelNoise);        // Acceleration
    m_Q(8, 8) = 0.01f;  // Aspect ratio (very stable)
    
    // Initial error covariance P (high uncertainty)
    m_P.setIdentity();
    m_P *= 100.0f;
}

void HighManeuverKalmanFilter::init(const Eigen::Vector4f& measurement)
{
    // Initialize state: [cx, cy, w, h, 0, 0, 0, 0, aspect_ratio]
    m_state[0] = measurement[0];  // cx
    m_state[1] = measurement[1];  // cy
    m_state[2] = measurement[2];  // w
    m_state[3] = measurement[3];  // h
    m_state[4] = 0.0f;  // vx
    m_state[5] = 0.0f;  // vy
    m_state[6] = 0.0f;  // ax
    m_state[7] = 0.0f;  // ay
    m_state[8] = measurement[2] / std::max(measurement[3], 1.0f);  // aspect ratio
    
    m_predictedState = m_state;
    m_initialized = true;
    
    // Clear history
    m_velocityHistory.clear();
    m_accelHistory.clear();
}

void HighManeuverKalmanFilter::buildTransitionMatrix(double dt)
{
    m_F.setIdentity();
    
    float dt_f = static_cast<float>(dt);
    float dt2_f = static_cast<float>(dt * dt);
    
    if (m_model == MotionModel::CA)
    {
        // Constant Acceleration model
        // Position: x = x + vx*dt + 0.5*ax*dt²
        m_F(0, 4) = dt_f;           // cx += vx * dt
        m_F(0, 6) = 0.5f * dt2_f;   // cx += 0.5 * ax * dt²
        m_F(1, 5) = dt_f;           // cy += vy * dt
        m_F(1, 7) = 0.5f * dt2_f;   // cy += 0.5 * ay * dt²
        
        // Velocity: v = v + a*dt
        m_F(4, 6) = dt_f;           // vx += ax * dt
        m_F(5, 7) = dt_f;           // vy += ay * dt
        
        // Size (assume constant velocity for w, h)
        m_F(2, 4) = 0.0f;  // w doesn't depend on vx
        m_F(3, 5) = 0.0f;  // h doesn't depend on vy
        
        // Acceleration and aspect ratio remain constant
    }
    else if (m_model == MotionModel::CV)
    {
        // Constant Velocity model (no acceleration)
        m_F(0, 4) = dt_f;  // cx += vx * dt
        m_F(1, 5) = dt_f;  // cy += vy * dt
        
        // Acceleration terms are zero (not used in CV)
    }
    // CTRV model can be added here in the future
}

void HighManeuverKalmanFilter::predict(double dt)
{
    if (!m_initialized)
        return;
    
    // Build transition matrix for this time step
    buildTransitionMatrix(dt);
    
    // Predict state: x' = F * x
    m_predictedState = m_F * m_state;
    
    // Predict error covariance: P' = F * P * F^T + Q
    m_P = m_F * m_P * m_F.transpose() + m_Q;
    
    // Update motion history for adaptive noise
    if (m_config.adaptiveNoise)
    {
        Eigen::Vector2f velocity(m_state[4], m_state[5]);
        Eigen::Vector2f accel(m_state[6], m_state[7]);
        
        m_velocityHistory.push_back(velocity);
        m_accelHistory.push_back(accel);
        
        // Keep only recent history
        if (m_velocityHistory.size() > static_cast<size_t>(m_config.noiseAdaptationFrames))
            m_velocityHistory.pop_front();
        if (m_accelHistory.size() > static_cast<size_t>(m_config.noiseAdaptationFrames))
            m_accelHistory.pop_front();
        
        // Adapt process noise
        adaptProcessNoise();
    }
}

void HighManeuverKalmanFilter::update(const Eigen::Vector4f& measurement)
{
    if (!m_initialized)
    {
        init(measurement);
        return;
    }
    
    // Innovation: y = z - H * x'
    Eigen::Vector4f innovation = measurement - m_H * m_predictedState;
    
    // Innovation covariance: S = H * P' * H^T + R
    Eigen::Matrix4f S = m_H * m_P * m_H.transpose() + m_R;
    
    // Kalman gain: K = P' * H^T * S^-1
    Eigen::Matrix<float, 9, 4> K = m_P * m_H.transpose() * S.inverse();
    
    // Update state: x = x' + K * y
    m_state = m_predictedState + K * innovation;
    
    // Update error covariance: P = (I - K * H) * P'
    Eigen::Matrix<float, 9, 9> I = Eigen::Matrix<float, 9, 9>::Identity();
    m_P = (I - K * m_H) * m_P;
    
    // Update aspect ratio from measurement
    if (measurement[3] > 1.0f)
        m_state[8] = measurement[2] / measurement[3];
}

Eigen::Vector4f HighManeuverKalmanFilter::getState() const
{
    return m_state.head<4>();
}

Eigen::Vector4f HighManeuverKalmanFilter::getPredictedState() const
{
    return m_predictedState.head<4>();
}

Eigen::Vector2f HighManeuverKalmanFilter::getVelocity() const
{
    return Eigen::Vector2f(m_state[4], m_state[5]);
}

Eigen::Vector2f HighManeuverKalmanFilter::getAcceleration() const
{
    return Eigen::Vector2f(m_state[6], m_state[7]);
}

double HighManeuverKalmanFilter::calculateMotionVariance()
{
    if (m_velocityHistory.size() < 2)
        return 0.0;
    
    // Calculate variance of velocity changes (approximates acceleration variance)
    double sumSqDiff = 0.0;
    for (size_t i = 1; i < m_velocityHistory.size(); ++i)
    {
        Eigen::Vector2f velChange = m_velocityHistory[i] - m_velocityHistory[i - 1];
        sumSqDiff += velChange.squaredNorm();
    }
    
    return sumSqDiff / (m_velocityHistory.size() - 1);
}

void HighManeuverKalmanFilter::adaptProcessNoise()
{
    double motionVar = calculateMotionVariance();
    
    // Determine noise multiplier based on motion variance
    float noiseMultiplier = 1.0f;
    
    if (motionVar > m_config.motionVarianceThreshold)
    {
        // High motion: increase process noise
        noiseMultiplier = static_cast<float>(m_config.highMotionMultiplier);
    }
    else if (motionVar < m_config.motionVarianceThreshold * 0.2)
    {
        // Low motion: use base noise (already at 1.0)
        noiseMultiplier = 1.0f;
    }
    else
    {
        // Medium motion: interpolate
        float ratio = static_cast<float>(motionVar / m_config.motionVarianceThreshold);
        noiseMultiplier = 1.0f + (static_cast<float>(m_config.highMotionMultiplier) - 1.0f) * ratio;
    }
    
    // Smooth the transition over multiple frames
    static float smoothedMultiplier = 1.0f;
    float alpha = 0.3f;  // Smoothing factor
    smoothedMultiplier = alpha * noiseMultiplier + (1.0f - alpha) * smoothedMultiplier;
    
    // Update process noise Q
    m_Q.setIdentity();
    m_Q.block<4, 4>(0, 0) *= static_cast<float>(m_config.baseProcessNoise) * smoothedMultiplier;
    m_Q.block<2, 2>(4, 4) *= static_cast<float>(m_config.baseProcessNoise * 10) * smoothedMultiplier;
    m_Q.block<2, 2>(6, 6) *= static_cast<float>(m_config.baseAccelNoise) * smoothedMultiplier;
    m_Q(8, 8) = 0.01f;  // Aspect ratio remains stable
}
