#ifndef DUPLICATION_API_CAPTURE_H
#define DUPLICATION_API_CAPTURE_H

#include <d3d11.h>
#include <dxgi1_2.h>
#include <opencv2/opencv.hpp>
#include <memory>

#include "capture.h"

class DDAManager;

class DuplicationAPIScreenCapture : public IScreenCapture
{
public:
    DuplicationAPIScreenCapture(int desiredWidth, int desiredHeight);
    ~DuplicationAPIScreenCapture();

    cv::Mat GetNextFrameCpu() override;
    
    // GPU 零拷贝捕获接口
    GpuFrame GetNextFrameGpu() override;
    void ReleaseGpuFrame() override;
    bool SupportsGpuCapture() const override { return true; }
    
    // 获取 D3D11 设备（供 CUDA 互操作使用）
    ID3D11Device* GetD3D11Device() const { return d3dDevice; }
    ID3D11DeviceContext* GetD3D11Context() const { return d3dContext; }

private:
    std::unique_ptr<DDAManager> m_ddaManager;

    ID3D11Device* d3dDevice = nullptr;
    ID3D11DeviceContext* d3dContext = nullptr;
    IDXGIOutputDuplication* deskDupl = nullptr;
    IDXGIOutput1* output1 = nullptr;

    ID3D11Texture2D* sharedTexture = nullptr;

    ID3D11Texture2D* stagingTextureCPU = nullptr;
    
    // GPU 零拷贝相关成员
    ID3D11Texture2D* croppedTexture = nullptr;  // 裁剪后的纹理
    bool gpuFrameAcquired = false;               // 是否已获取 GPU 帧

    int screenWidth = 0;
    int screenHeight = 0;
    int regionWidth = 0;
    int regionHeight = 0;

    bool createStagingTextureCPU();
    bool createCroppedTexture();  // 创建裁剪纹理
};

#endif // DUPLICATION_API_CAPTURE_H
