#ifndef CUDA_D3D11_INTEROP_H
#define CUDA_D3D11_INTEROP_H

#ifdef USE_CUDA

#include <d3d11.h>
#include <opencv2/opencv.hpp>
#include <opencv2/core/cuda.hpp>
#include <cuda_runtime.h>
#include <cuda_d3d11_interop.h>

// CUDA-D3D11 interop class for zero-copy texture transfer
class CudaD3D11Interop
{
public:
    CudaD3D11Interop();
    ~CudaD3D11Interop();

    // Initialize with D3D11 device
    bool Initialize(ID3D11Device* device);

    // Map D3D11 texture to CUDA GpuMat (BGRA format)
    // Returns true if successful, output is in gpuMat
    bool MapTextureToGpuMat(ID3D11Texture2D* texture, cv::cuda::GpuMat& gpuMat);

    // Unmap the currently mapped texture
    void Unmap();

    // Release all resources
    void Release();

    // Check if initialized
    bool IsInitialized() const { return m_initialized; }

private:
    bool RegisterTexture(ID3D11Texture2D* texture);
    void UnregisterTexture();

    ID3D11Device* m_d3dDevice = nullptr;
    cudaGraphicsResource_t m_cudaResource = nullptr;
    ID3D11Texture2D* m_registeredTexture = nullptr;
    bool m_initialized = false;
    bool m_mapped = false;
    
    // Cached texture dimensions
    int m_width = 0;
    int m_height = 0;
};

#endif // USE_CUDA

#endif // CUDA_D3D11_INTEROP_H
