#ifndef HIGH_MANEUVER_KALMAN_FILTER_H
#define HIGH_MANEUVER_KALMAN_FILTER_H

#include <Eigen/Dense>
#include <deque>

/**
 * @brief High-Maneuver Kalman Filter with acceleration modeling
 * 
 * Supports Constant Acceleration (CA) and CTRV motion models.
 * Features adaptive process noise based on motion variance.
 */
class HighManeuverKalmanFilter
{
public:
    enum class MotionModel
    {
        CV,    // Constant Velocity (original, 8D state)
        CA,    // Constant Acceleration (9D state with ax, ay)
        CTRV   // Constant Turn Rate and Velocity (future)
    };
    
    struct Config
    {
        MotionModel model = MotionModel::CA;
        bool adaptiveNoise = true;
        double baseProcessNoise = 1.0;
        double baseAccelNoise = 0.01;
        double highMotionMultiplier = 5.0;
        double motionVarianceThreshold = 50.0;  // px/s²
        int noiseAdaptationFrames = 5;
    };
    
    HighManeuverKalmanFilter(MotionModel model = MotionModel::CA);
    HighManeuverKalmanFilter(const Config& config);
    
    /**
     * @brief Initialize filter with first measurement
     * @param measurement [cx, cy, w, h]
     */
    void init(const Eigen::Vector4f& measurement);
    
    /**
     * @brief Predict next state using second-order motion model
     * @param dt Time interval in seconds (default 1.0 for frame-based)
     */
    void predict(double dt = 1.0);
    
    /**
     * @brief Update state with new measurement
     * @param measurement [cx, cy, w, h]
     */
    void update(const Eigen::Vector4f& measurement);
    
    /**
     * @brief Get current state [cx, cy, w, h]
     */
    Eigen::Vector4f getState() const;
    
    /**
     * @brief Get predicted state [cx, cy, w, h]
     */
    Eigen::Vector4f getPredictedState() const;
    
    /**
     * @brief Get velocity [vx, vy] in pixels per frame
     */
    Eigen::Vector2f getVelocity() const;
    
    /**
     * @brief Get acceleration [ax, ay] in pixels per frame²
     */
    Eigen::Vector2f getAcceleration() const;
    
    /**
     * @brief Adapt process noise based on motion variance
     */
    void adaptProcessNoise();
    
    /**
     * @brief Check if filter is initialized
     */
    bool isInitialized() const { return m_initialized; }
    
private:
    Config m_config;
    MotionModel m_model;
    
    // State vector: [cx, cy, w, h, vx, vy, ax, ay, aspect_ratio]
    Eigen::Matrix<float, 9, 1> m_state;
    
    // Predicted state (before update)
    Eigen::Matrix<float, 9, 1> m_predictedState;
    
    // State transition matrix (depends on motion model and dt)
    Eigen::Matrix<float, 9, 9> m_F;
    
    // Measurement matrix H: maps state to measurement
    Eigen::Matrix<float, 4, 9> m_H;
    
    // Process noise covariance Q
    Eigen::Matrix<float, 9, 9> m_Q;
    
    // Measurement noise covariance R
    Eigen::Matrix<float, 4, 4> m_R;
    
    // Error covariance P
    Eigen::Matrix<float, 9, 9> m_P;
    
    // Motion history for adaptive noise
    std::deque<Eigen::Vector2f> m_velocityHistory;
    std::deque<Eigen::Vector2f> m_accelHistory;
    
    bool m_initialized;
    
    /**
     * @brief Build state transition matrix F based on motion model and dt
     */
    void buildTransitionMatrix(double dt);
    
    /**
     * @brief Calculate motion variance from velocity/acceleration history
     * @return Motion variance in px/s²
     */
    double calculateMotionVariance();
    
    /**
     * @brief Initialize matrices (H, R, base Q, P)
     */
    void initializeMatrices();
};

#endif // HIGH_MANEUVER_KALMAN_FILTER_H
