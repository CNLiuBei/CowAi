#include "CameraMotionCompensator.h"
#include <algorithm>

CameraMotionCompensator::CameraMotionCompensator(const Config& config)
    : m_config(config), m_motionMagnitude(0.0f)
{
    m_homography = cv::Mat::eye(3, 3, CV_64F);
    
    if (m_config.method == Method::ORB_MATCHING)
    {
        m_orbDetector = cv::ORB::create(m_config.maxFeatures);
    }
}

cv::Mat CameraMotionCompensator::estimateCameraMotion(
    const cv::Mat& prevFrame,
    const cv::Mat& currFrame)
{
    if (prevFrame.empty() || currFrame.empty())
    {
        m_motionMagnitude = 0.0f;
        return cv::Mat::eye(3, 3, CV_64F);
    }
    
    cv::Mat H;
    
    switch (m_config.method)
    {
        case Method::SPARSE_OPTICAL_FLOW:
            H = estimateWithSparseFlow(prevFrame, currFrame);
            break;
            
        case Method::ORB_MATCHING:
            H = estimateWithORB(prevFrame, currFrame);
            break;
            
        case Method::NONE:
        default:
            H = cv::Mat::eye(3, 3, CV_64F);
            m_motionMagnitude = 0.0f;
            break;
    }
    
    if (H.empty())
    {
        H = cv::Mat::eye(3, 3, CV_64F);
        m_motionMagnitude = 0.0f;
    }
    else
    {
        // Calculate motion magnitude from translation component
        double tx = H.at<double>(0, 2);
        double ty = H.at<double>(1, 2);
        m_motionMagnitude = static_cast<float>(std::sqrt(tx * tx + ty * ty));
    }
    
    m_homography = H;
    m_prevFrame = currFrame.clone();
    
    return H;
}

cv::Mat CameraMotionCompensator::estimateWithSparseFlow(
    const cv::Mat& prev,
    const cv::Mat& curr)
{
    try
    {
        // Detect good features to track
        std::vector<cv::Point2f> prevPoints;
        cv::goodFeaturesToTrack(prev, prevPoints, m_config.maxFeatures, 0.01, 10);
        
        if (prevPoints.size() < 4)
            return cv::Mat();
        
        // Calculate optical flow
        std::vector<cv::Point2f> currPoints;
        std::vector<uchar> status;
        std::vector<float> err;
        
        cv::calcOpticalFlowPyrLK(
            prev, curr,
            prevPoints, currPoints,
            status, err,
            cv::Size(21, 21), 3
        );
        
        // Filter good matches
        std::vector<cv::Point2f> goodPrev, goodCurr;
        for (size_t i = 0; i < status.size(); ++i)
        {
            if (status[i])
            {
                goodPrev.push_back(prevPoints[i]);
                goodCurr.push_back(currPoints[i]);
            }
        }
        
        if (goodPrev.size() < 4)
            return cv::Mat();
        
        // Estimate homography with RANSAC
        cv::Mat H = cv::findHomography(
            goodPrev, goodCurr,
            cv::RANSAC,
            m_config.ransacThreshold
        );
        
        return H;
    }
    catch (const cv::Exception& e)
    {
        return cv::Mat();
    }
}

cv::Mat CameraMotionCompensator::estimateWithORB(
    const cv::Mat& prev,
    const cv::Mat& curr)
{
    try
    {
        // Detect ORB features
        std::vector<cv::KeyPoint> prevKeypoints, currKeypoints;
        cv::Mat prevDescriptors, currDescriptors;
        
        m_orbDetector->detectAndCompute(prev, cv::noArray(), prevKeypoints, prevDescriptors);
        m_orbDetector->detectAndCompute(curr, cv::noArray(), currKeypoints, currDescriptors);
        
        if (prevKeypoints.size() < 4 || currKeypoints.size() < 4)
            return cv::Mat();
        
        // Match features
        cv::BFMatcher matcher(cv::NORM_HAMMING);
        std::vector<cv::DMatch> matches;
        matcher.match(prevDescriptors, currDescriptors, matches);
        
        // Filter good matches (top 50%)
        std::sort(matches.begin(), matches.end(),
            [](const cv::DMatch& a, const cv::DMatch& b) { return a.distance < b.distance; });
        
        size_t numGoodMatches = std::min(matches.size(), matches.size() / 2);
        matches.resize(numGoodMatches);
        
        if (matches.size() < 4)
            return cv::Mat();
        
        // Extract matched points
        std::vector<cv::Point2f> prevPoints, currPoints;
        for (const auto& match : matches)
        {
            prevPoints.push_back(prevKeypoints[match.queryIdx].pt);
            currPoints.push_back(currKeypoints[match.trainIdx].pt);
        }
        
        // Estimate homography
        cv::Mat H = cv::findHomography(
            prevPoints, currPoints,
            cv::RANSAC,
            m_config.ransacThreshold
        );
        
        return H;
    }
    catch (const cv::Exception& e)
    {
        return cv::Mat();
    }
}

cv::Rect CameraMotionCompensator::applyCompensation(
    const cv::Rect& bbox,
    const cv::Mat& H)
{
    if (H.empty() || H.rows != 3 || H.cols != 3)
        return bbox;
    
    // Transform bbox corners
    std::vector<cv::Point2f> corners = {
        cv::Point2f(bbox.x, bbox.y),
        cv::Point2f(bbox.x + bbox.width, bbox.y),
        cv::Point2f(bbox.x + bbox.width, bbox.y + bbox.height),
        cv::Point2f(bbox.x, bbox.y + bbox.height)
    };
    
    std::vector<cv::Point2f> transformedCorners;
    cv::perspectiveTransform(corners, transformedCorners, H);
    
    // Find bounding box of transformed corners
    float minX = transformedCorners[0].x;
    float maxX = transformedCorners[0].x;
    float minY = transformedCorners[0].y;
    float maxY = transformedCorners[0].y;
    
    for (const auto& pt : transformedCorners)
    {
        minX = std::min(minX, pt.x);
        maxX = std::max(maxX, pt.x);
        minY = std::min(minY, pt.y);
        maxY = std::max(maxY, pt.y);
    }
    
    return cv::Rect(
        static_cast<int>(minX),
        static_cast<int>(minY),
        static_cast<int>(maxX - minX),
        static_cast<int>(maxY - minY)
    );
}

cv::Point2f CameraMotionCompensator::applyCompensation(
    const cv::Point2f& point,
    const cv::Mat& H)
{
    if (H.empty() || H.rows != 3 || H.cols != 3)
        return point;
    
    std::vector<cv::Point2f> points = {point};
    std::vector<cv::Point2f> transformed;
    
    cv::perspectiveTransform(points, transformed, H);
    
    return transformed[0];
}

bool CameraMotionCompensator::hasSignificantMotion() const
{
    return m_motionMagnitude > m_config.minMotionThreshold;
}
