#ifndef CAPTURE_H
#define CAPTURE_H

#include <opencv2/opencv.hpp>
#include <atomic>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <deque>

// 前向声明 D3D11 类型
struct ID3D11Texture2D;

extern std::atomic<bool> detection_resolution_changed;
extern std::atomic<bool> capture_method_changed;
extern std::atomic<bool> capture_cursor_changed;
extern std::atomic<bool> capture_borders_changed;
extern std::atomic<bool> capture_fps_changed;
extern std::deque<cv::Mat> frameQueue;

void captureThread(int CAPTURE_WIDTH, int CAPTURE_HEIGHT);
extern int screenWidth;
extern int screenHeight;

extern std::atomic<int> captureFrameCount;
extern std::atomic<int> captureFps;
extern std::chrono::time_point<std::chrono::high_resolution_clock> captureFpsStartTime;

extern cv::Mat latestFrame;

extern std::mutex frameMutex;
extern std::condition_variable frameCV;
extern std::atomic<bool> shouldExit;
extern std::atomic<bool> show_window_changed;

// Zero-copy capture status (for UI display)
extern std::atomic<bool> zeroCopyActiveStatus;

// GPU 帧结构体，用于零拷贝捕获
struct GpuFrame {
    ID3D11Texture2D* texture = nullptr;  // D3D11 纹理指针
    int width = 0;                        // 纹理宽度
    int height = 0;                       // 纹理高度
    bool valid = false;                   // 是否有效
};

class IScreenCapture
{
public:
    virtual ~IScreenCapture() {}
    virtual cv::Mat GetNextFrameCpu() = 0;
    
    // GPU 零拷贝捕获接口（默认实现返回无效帧）
    virtual GpuFrame GetNextFrameGpu() { return GpuFrame{}; }
    virtual void ReleaseGpuFrame() {}
    virtual bool SupportsGpuCapture() const { return false; }
};

#endif // CAPTURE_H
