#pragma once
#include <vector>
#include <mutex>
#include <condition_variable>
#include <opencv2/opencv.hpp>

struct DetectionBuffer
{
    std::mutex mutex;
    std::condition_variable cv;
    int version = 0;
    std::vector<cv::Rect> boxes;
    std::vector<int> classes;
    std::vector<float> scores;
    cv::Mat sourceFrame;  // Frame that was used for detection (for dataset sync)

    void set(const std::vector<cv::Rect>& newBoxes, const std::vector<int>& newClasses, const std::vector<float>& newScores = {})
    {
        std::lock_guard<std::mutex> lock(mutex);
        boxes = newBoxes;
        classes = newClasses;
        scores = newScores;
        ++version;
        cv.notify_all();
    }
    
    void setWithFrame(const std::vector<cv::Rect>& newBoxes, const std::vector<int>& newClasses, 
                      const std::vector<float>& newScores, const cv::Mat& frame)
    {
        std::lock_guard<std::mutex> lock(mutex);
        boxes = newBoxes;
        classes = newClasses;
        scores = newScores;
        if (!frame.empty())
            sourceFrame = frame.clone();
        ++version;
        cv.notify_all();
    }

    void get(std::vector<cv::Rect>& outBoxes, std::vector<int>& outClasses, int& outVersion)
    {
        std::lock_guard<std::mutex> lock(mutex);
        outBoxes = boxes;
        outClasses = classes;
        outVersion = version;
    }
    
    // Clear source frame to free memory
    void clearSourceFrame()
    {
        std::lock_guard<std::mutex> lock(mutex);
        sourceFrame.release();
    }
};