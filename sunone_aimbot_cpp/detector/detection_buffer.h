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

    void set(const std::vector<cv::Rect>& newBoxes, const std::vector<int>& newClasses, const std::vector<float>& newScores = {})
    {
        std::lock_guard<std::mutex> lock(mutex);
        boxes = newBoxes;
        classes = newClasses;
        scores = newScores;
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
    
    // Clear all detection data
    void clear()
    {
        std::lock_guard<std::mutex> lock(mutex);
        boxes.clear();
        classes.clear();
        scores.clear();
        version = 0;
    }
};