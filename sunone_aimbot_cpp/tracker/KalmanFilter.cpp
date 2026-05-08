#include "KalmanFilter.h"

KalmanFilter::KalmanFilter() : initialized(false)
{
    // State transition matrix (constant velocity model)
    F = Eigen::Matrix<float, 8, 8>::Identity();
    F(0, 4) = 1.0f;  // cx += vx
    F(1, 5) = 1.0f;  // cy += vy
    F(2, 6) = 1.0f;  // w += vw
    F(3, 7) = 1.0f;  // h += vh
    
    // Measurement matrix
    H = Eigen::Matrix<float, 4, 8>::Zero();
    H(0, 0) = 1.0f;
    H(1, 1) = 1.0f;
    H(2, 2) = 1.0f;
    H(3, 3) = 1.0f;
    
    // Process noise
    Q = Eigen::Matrix<float, 8, 8>::Identity();
    Q.block<4, 4>(0, 0) *= 1.0f;
    Q.block<4, 4>(4, 4) *= 0.01f;
    
    // Measurement noise
    R = Eigen::Matrix<float, 4, 4>::Identity() * 1.0f;
    
    // Initial error covariance
    P = Eigen::Matrix<float, 8, 8>::Identity() * 10.0f;
    P.block<4, 4>(4, 4) *= 1000.0f;
    
    x = Eigen::Matrix<float, 8, 1>::Zero();
}

void KalmanFilter::init(const Eigen::Vector4f& measurement)
{
    x.head<4>() = measurement;
    x.tail<4>().setZero();
    initialized = true;
}

void KalmanFilter::predict()
{
    if (!initialized) return;
    
    x = F * x;
    P = F * P * F.transpose() + Q;
}

void KalmanFilter::update(const Eigen::Vector4f& measurement)
{
    if (!initialized)
    {
        init(measurement);
        return;
    }
    
    Eigen::Matrix<float, 4, 1> y = measurement - H * x;
    Eigen::Matrix<float, 4, 4> S = H * P * H.transpose() + R;
    Eigen::Matrix<float, 8, 4> K = P * H.transpose() * S.inverse();
    
    x = x + K * y;
    P = (Eigen::Matrix<float, 8, 8>::Identity() - K * H) * P;
}

Eigen::Vector4f KalmanFilter::getState() const
{
    return x.head<4>();
}

Eigen::Vector4f KalmanFilter::getPredictedState() const
{
    Eigen::Matrix<float, 8, 1> predicted = F * x;
    return predicted.head<4>();
}


Eigen::Vector2f KalmanFilter::getVelocity() const
{
    return x.segment<2>(4);  // vx, vy are at indices 4, 5
}
