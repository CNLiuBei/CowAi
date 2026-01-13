#ifdef USE_CUDA

#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <winsock2.h>
#include <Windows.h>
#include <dxgi.h>

#include "cuda_d3d11_interop.h"
#include <iostream>

CudaD3D11Interop::CudaD3D11Interop()
{
}

CudaD3D11Interop::~CudaD3D11Interop()
{
    Release();
}

bool CudaD3D11Interop::Initialize(ID3D11Device* device)
{
    if (!device)
    {
        std::cerr << "[CudaD3D11Interop] Invalid D3D11 device" << std::endl;
        return false;
    }

    m_d3dDevice = device;

    // Get the DXGI adapter from D3D11 device
    IDXGIDevice* dxgiDevice = nullptr;
    HRESULT hr = device->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);
    if (FAILED(hr))
    {
        std::cerr << "[CudaD3D11Interop] QueryInterface IDXGIDevice failed" << std::endl;
        return false;
    }

    IDXGIAdapter* adapter = nullptr;
    hr = dxgiDevice->GetAdapter(&adapter);
    dxgiDevice->Release();
    if (FAILED(hr))
    {
        std::cerr << "[CudaD3D11Interop] GetAdapter failed" << std::endl;
        return false;
    }

    // Get CUDA device that corresponds to the DXGI adapter
    int cudaDevice = 0;
    cudaError_t err = cudaD3D11GetDevice(&cudaDevice, adapter);
    adapter->Release();
    if (err != cudaSuccess)
    {
        std::cerr << "[CudaD3D11Interop] cudaD3D11GetDevice failed: " 
                  << cudaGetErrorString(err) << std::endl;
        return false;
    }

    // Set the CUDA device
    err = cudaSetDevice(cudaDevice);
    if (err != cudaSuccess)
    {
        std::cerr << "[CudaD3D11Interop] cudaSetDevice failed: " 
                  << cudaGetErrorString(err) << std::endl;
        return false;
    }

    m_initialized = true;
    std::cout << "[CudaD3D11Interop] Initialized with CUDA device " << cudaDevice << std::endl;
    return true;
}

bool CudaD3D11Interop::RegisterTexture(ID3D11Texture2D* texture)
{
    if (!texture)
        return false;

    // If same texture is already registered, skip
    if (m_registeredTexture == texture && m_cudaResource)
        return true;

    // Unregister previous texture if different
    UnregisterTexture();

    // Get texture description
    D3D11_TEXTURE2D_DESC desc;
    texture->GetDesc(&desc);
    m_width = desc.Width;
    m_height = desc.Height;

    // Register texture with CUDA
    cudaError_t err = cudaGraphicsD3D11RegisterResource(
        &m_cudaResource,
        texture,
        cudaGraphicsRegisterFlagsNone
    );

    if (err != cudaSuccess)
    {
        std::cerr << "[CudaD3D11Interop] cudaGraphicsD3D11RegisterResource failed: "
                  << cudaGetErrorString(err) << std::endl;
        m_cudaResource = nullptr;
        return false;
    }

    m_registeredTexture = texture;
    return true;
}

void CudaD3D11Interop::UnregisterTexture()
{
    if (m_mapped)
    {
        Unmap();
    }

    if (m_cudaResource)
    {
        cudaGraphicsUnregisterResource(m_cudaResource);
        m_cudaResource = nullptr;
    }
    m_registeredTexture = nullptr;
}


bool CudaD3D11Interop::MapTextureToGpuMat(ID3D11Texture2D* texture, cv::cuda::GpuMat& gpuMat)
{
    if (!m_initialized)
    {
        std::cerr << "[CudaD3D11Interop] Not initialized" << std::endl;
        return false;
    }

    // Register texture if needed
    if (!RegisterTexture(texture))
    {
        return false;
    }

    // Map the resource
    cudaError_t err = cudaGraphicsMapResources(1, &m_cudaResource, 0);
    if (err != cudaSuccess)
    {
        std::cerr << "[CudaD3D11Interop] cudaGraphicsMapResources failed: "
                  << cudaGetErrorString(err) << std::endl;
        return false;
    }
    m_mapped = true;

    // Get mapped array
    cudaArray_t cudaArray = nullptr;
    err = cudaGraphicsSubResourceGetMappedArray(&cudaArray, m_cudaResource, 0, 0);
    if (err != cudaSuccess)
    {
        std::cerr << "[CudaD3D11Interop] cudaGraphicsSubResourceGetMappedArray failed: "
                  << cudaGetErrorString(err) << std::endl;
        Unmap();
        return false;
    }

    // Create GpuMat and copy from CUDA array
    // Note: D3D11 textures are typically BGRA format
    gpuMat.create(m_height, m_width, CV_8UC4);
    
    err = cudaMemcpy2DFromArray(
        gpuMat.data,
        gpuMat.step,
        cudaArray,
        0, 0,
        m_width * 4,  // 4 bytes per pixel (BGRA)
        m_height,
        cudaMemcpyDeviceToDevice
    );

    if (err != cudaSuccess)
    {
        std::cerr << "[CudaD3D11Interop] cudaMemcpy2DFromArray failed: "
                  << cudaGetErrorString(err) << std::endl;
        Unmap();
        return false;
    }

    return true;
}

void CudaD3D11Interop::Unmap()
{
    if (m_mapped && m_cudaResource)
    {
        cudaGraphicsUnmapResources(1, &m_cudaResource, 0);
        m_mapped = false;
    }
}

void CudaD3D11Interop::Release()
{
    UnregisterTexture();
    m_d3dDevice = nullptr;
    m_initialized = false;
    m_width = 0;
    m_height = 0;
}

#endif // USE_CUDA
