#ifndef D3D11_STAGING_MANAGER_H
#define D3D11_STAGING_MANAGER_H

#include <d3d11.h>
#include <opencv2/opencv.hpp>

// D3D11 Staging Manager for efficient GPU to CPU texture transfer
// Used by DirectML path for zero-copy optimization
class D3D11StagingManager
{
public:
    D3D11StagingManager();
    ~D3D11StagingManager();

    // Initialize with D3D11 device and context
    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context, int width, int height);

    // Copy texture to staging and map for CPU access
    // Returns pointer to mapped data, nullptr on failure
    bool CopyAndMap(ID3D11Texture2D* srcTexture, void** data, UINT* rowPitch);

    // Unmap the staging texture
    void Unmap();

    // Copy texture to staging and get as cv::Mat (BGRA format)
    bool CopyToMat(ID3D11Texture2D* srcTexture, cv::Mat& outMat);

    // Release all resources
    void Release();

    // Check if initialized
    bool IsInitialized() const { return m_initialized; }

    // Get dimensions
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

private:
    bool CreateStagingTexture();

    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;
    ID3D11Texture2D* m_stagingTexture = nullptr;
    
    int m_width = 0;
    int m_height = 0;
    bool m_initialized = false;
    bool m_mapped = false;
};

#endif // D3D11_STAGING_MANAGER_H
