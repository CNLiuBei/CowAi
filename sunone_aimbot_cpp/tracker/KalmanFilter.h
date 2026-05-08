#ifndef KALMAN_FILTER_H
#define KALMAN_FILTER_H

#include <Eigen/Dense>

class KalmanFilter
{
public:
    KalmanFilter();
    
    void init(const Eigen::Vector4f& measurement);
    void predict();
    void update(const Eigen::Vector4f& measurement);
    
    Eigen::Vector4f getState() const;
    Eigen::Vector4f getPredictedState() const;
    Eigen::Vector2f getVelocity() const;  // Get velocity (vx, vy)
    
private:
    Eigen::Matrix<float, 8, 8> F;  // State transition matrix
    Eigen::Matrix<float, 4, 8> H;  // Measurement matrix
    Eigen::Matrix<float, 8, 8> Q;  // Process noise
    Eigen::Matrix<float, 4, 4> R;  // Measurement noise
    Eigen::Matrix<float, 8, 8> P;  // Error covariance
    Eigen::Matrix<float, 8, 1> x;  // State [cx, cy, w, h, vx, vy, vw, vh]
    
    bool initialized;
};

#endif
