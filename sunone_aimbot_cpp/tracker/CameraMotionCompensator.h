#ifndef CAMERA_MOTION_COMPENSATOR_H
#define CAMERA_MOTION_COMPENSATOR_H

#include <opencv2/opencv.hpp>

/**
 * @brief Camera motion compensation using optical flow or feature matching
 */
class CameraMotionCompensator
{
public:
    enum class Method
    {
        NONE,                   // No compensation
        OPTICAL_FLOW,           // Dense optical flow
        ORB_MATCHING,           // ORB feature matching
        SPARSE_OPTICAL_FLOW     // Sparse optical flow (faster)
    };
    
    struct Config
    {
        Method method = Method::SPARSE_OPTICAL_FLOW;
        int maxFeatures = 200;              // Max feature points for sparse flow
        float ransacThreshold = 3.0f;       // RANSAC threshold (pixels)
        float minMotionThreshold = 5.0f;    // Minimum motion to consider (pixels)
    };
    
    CameraMotionCompensator(const Config& config = Config());
    
    /**
     * @brief Estimate camera motion between frames
     * @param prevFrame Previous frame (grayscale)
     * @param currFrame Current frame (grayscale)
     * @return Homography matrix H (3x3), identity if estimation fails
     */
    cv::Mat estimateCameraMotion(const cv::Mat& prevFrame, const cv::Mat& currFrame);
    
    /**
     * @brief Apply compensation to bounding box
     * @param bbox Original bbox
     * @param H Homography matrix
     * @return Compensated bbox
     */
    static cv::Rect applyCompensation(const cv::Rect& bbox, const cv::Mat& H);
    
    /**
     * @brief Apply compensation to point
     * @param point Original point
     * @param H Homography matrix
     * @return Compensated point
     */
    static cv::Point2f applyCompensation(const cv::Point2f& point, const cv::Mat& H);
    
    /**
     * @brief Get camera motion magnitude
     * @return Motion magnitude in pixels
     */
    float getMotionMagnitude() const { return m_motionMagnitude; }
    
    /**
     * @brief Check if significant motion detected
     */
    bool hasSignificantMotion() const;
    
private:
    Config m_config;
    cv::Mat m_prevFrame;
    cv::Mat m_homography;
    float m_motionMagnitude;
    
    cv::Ptr<cv::ORB> m_orbDetector;
    
    cv::Mat estimateWithORB(const cv::Mat& prev, const cv::Mat& curr);
    cv::Mat estimateWithSparseFlow(const cv::Mat& prev, const cv::Mat& curr);
};

#endif // CAMERA_MOTION_COMPENSATOR_H
