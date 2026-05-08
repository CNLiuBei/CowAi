#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <winsock2.h>
#include <Windows.h>

#include "d3d11_staging_manager.h"
#include <iostream>

D3D11StagingManager::D3D11StagingManager()
{
}

D3D11StagingManager::~D3D11StagingManager()
{
    Release();
}

bool D3D11StagingManager::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, int width, int height)
{
    if (!device || !context)
    {
        std::cerr << "[D3D11StagingManager] Invalid device or context" << std::endl;
        return false;
    }

    m_device = device;
    m_context = context;
    m_width = width;
    m_height = height;

    if (!CreateStagingTexture())
    {
        return false;
    }

    m_initialized = true;
    std::cout << "[D3D11StagingManager] Initialized " << width << "x" << height << std::endl;
    return true;
}

bool D3D11StagingManager::CreateStagingTexture()
{
    if (m_stagingTexture)
    {
        m_stagingTexture->Release();
        m_stagingTexture = nullptr;
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = m_width;
    desc.Height = m_height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;

    HRESULT hr = m_device->CreateTexture2D(&desc, nullptr, &m_stagingTexture);
    if (FAILED(hr))
    {
        std::cerr << "[D3D11StagingManager] CreateTexture2D failed hr=0x" 
                  << std::hex << hr << std::endl;
        return false;
    }

    return true;
}

bool D3D11StagingManager::CopyAndMap(ID3D11Texture2D* srcTexture, void** data, UINT* rowPitch)
{
    if (!m_initialized || !srcTexture || !data || !rowPitch)
    {
        return false;
    }

    if (m_mapped)
    {
        Unmap();
    }

    // Copy source texture to staging texture
    m_context->CopyResource(m_stagingTexture, srcTexture);

    // Map staging texture for CPU read
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT hr = m_context->Map(m_stagingTexture, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr))
    {
        std::cerr << "[D3D11StagingManager] Map failed hr=0x" 
                  << std::hex << hr << std::endl;
        return false;
    }

    *data = mapped.pData;
    *rowPitch = mapped.RowPitch;
    m_mapped = true;

    return true;
}

void D3D11StagingManager::Unmap()
{
    if (m_mapped && m_context && m_stagingTexture)
    {
        m_context->Unmap(m_stagingTexture, 0);
        m_mapped = false;
    }
}

bool D3D11StagingManager::CopyToMat(ID3D11Texture2D* srcTexture, cv::Mat& outMat)
{
    void* data = nullptr;
    UINT rowPitch = 0;

    if (!CopyAndMap(srcTexture, &data, &rowPitch))
    {
        return false;
    }

    // Create Mat from mapped data (BGRA format)
    // Note: rowPitch may be larger than width*4 due to alignment
    if (rowPitch == static_cast<UINT>(m_width * 4))
    {
        // No padding, direct copy
        outMat = cv::Mat(m_height, m_width, CV_8UC4, data).clone();
    }
    else
    {
        // Has padding, need row-by-row copy
        outMat.create(m_height, m_width, CV_8UC4);
        const uint8_t* srcPtr = static_cast<const uint8_t*>(data);
        for (int y = 0; y < m_height; ++y)
        {
            memcpy(outMat.ptr(y), srcPtr + y * rowPitch, m_width * 4);
        }
    }

    Unmap();
    return true;
}

void D3D11StagingManager::Release()
{
    Unmap();

    if (m_stagingTexture)
    {
        m_stagingTexture->Release();
        m_stagingTexture = nullptr;
    }

    m_device = nullptr;
    m_context = nullptr;
    m_width = 0;
    m_height = 0;
    m_initialized = false;
}
