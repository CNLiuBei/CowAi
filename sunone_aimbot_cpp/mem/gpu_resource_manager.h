#pragma once

#ifdef USE_CUDA
#include <cuda_runtime.h>
#endif

class GPUResourceManager {
public:
    bool reserveGPUMemory(size_t reservedMemoryMB);
    bool setGPUExclusiveMode();
    void releaseReservation();

private:
    void* reservedBuffer = nullptr;
    size_t reservedSize = 0;
};
