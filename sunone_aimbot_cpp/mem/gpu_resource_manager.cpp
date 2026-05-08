#include "gpu_resource_manager.h"

bool GPUResourceManager::reserveGPUMemory(size_t reservedMemoryMB)
{
#ifdef USE_CUDA
    size_t totalMemory, freeMemory;
    cudaMemGetInfo(&freeMemory, &totalMemory);

    reservedSize = reservedMemoryMB * 1024 * 1024;

    if (freeMemory < reservedSize) {
        return false;
    }

    cudaMalloc(&reservedBuffer, reservedSize);
    cudaMemset(reservedBuffer, 0, reservedSize);

    return true;
#else
    return true;
#endif
}

bool GPUResourceManager::setGPUExclusiveMode()
{
#ifdef USE_CUDA
    cudaDeviceSetCacheConfig(cudaFuncCachePreferL1);
#endif
    return true;
}
