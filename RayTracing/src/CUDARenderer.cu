#include "CUDARenderer.cuh"

#include <cstdio>
#include <cstdlib>
// NOTE: std::print / std::println (C++23 <print>) is not available here
// because NVCC cannot forward --std=c++23 to the MSVC host compiler.
// MSVC only supports /std:c++latest (not /std:c++23), so NVCC falls back.

// ─── CUDA Error Checking Macro ───
// Debug:  abort immediately for fast bug detection
// Release: mark error flag in state, let host code gracefully fall back to CPU
#ifdef PN_DEBUG
#define CUDA_CHECK(call)                                                       \
    do {                                                                       \
        cudaError_t _e = (call);                                               \
        if (_e != cudaSuccess) {                                               \
            std::fprintf(stderr, "[CUDA] Error at %s:%d — %s (code %d)\n",     \
                         __FILE__, __LINE__, cudaGetErrorString(_e), static_cast<int>(_e)); \
            std::abort();                                                      \
        }                                                                      \
    } while (0)
#else
#define CUDA_CHECK(call)                                                       \
    do {                                                                       \
        cudaError_t _e = (call);                                               \
        if (_e != cudaSuccess) {                                               \
            std::fprintf(stderr, "[CUDA] Error at %s:%d — %s (code %d)\n",     \
                         __FILE__, __LINE__, cudaGetErrorString(_e), static_cast<int>(_e)); \
            state->cudaError = true;                                           \
            return;                                                            \
        }                                                                      \
    } while (0)
#endif

// ──────────────────────────────────────────────
// Host wrapper functions (C linkage for interop)
// ──────────────────────────────────────────────

extern "C"
{

struct CUDARenderState
{
    float4*    d_SampleBuffer;        // Device per-pixel sample buffer (raw path trace output)
    float4*    d_DenoiseBuffer;       // Device per-pixel denoise buffer (averaged HDR, for OptiX)
    float4*    d_AccumulationBuffer;  // Device accumulation buffer
    uint32_t*  d_OutputImage;         // Device output RGBA buffer (legacy, null if interop)
    void*       d_InteropBuffer;       // External device buffer (Vulkan-CUDA interop, nullptr if disabled)

    GPUSphere*   d_Spheres;           // Device scene spheres
    GPUMaterial* d_Materials;         // Device scene materials
    float3*      d_RayDirections;     // Device ray directions
    GPUBVHNode*  d_BVHNodes;          // Device BVH node array
    int*         d_SphereIndices;     // Device sorted sphere indices for leaf resolution

    GPUScene   gpuScene;             // Device scene descriptor
    GPUCamera  gpuCamera;            // Device camera descriptor
    GPURenderSettings gpuSettings;   // Device settings

    uint32_t imageWidth;
    uint32_t imageHeight;
    uint32_t pixelCount;

    uint32_t allocatedSphereCount;   // Track current GPU allocation size
    uint32_t allocatedMaterialCount;
    uint32_t allocatedBVHNodeCount;   // Track BVH node allocation size
    uint32_t allocatedSphereIndexCount; // Track sphere index allocation size

    cudaStream_t uploadStream;          // Async stream for H2D transfers
    cudaStream_t computeStream;         // Async stream for kernel launches
    cudaEvent_t  uploadCompleteEvent;   // Signals when pending uploads finish
    bool streamsCreated;
    bool cudaError;                    // Set on CUDA error (Release: skip GPU ops; Debug: abort)

    bool initialized;
};

CUDARenderState* CUDARenderer_Create()
{
    CUDARenderState* state = new CUDARenderState();
    state->cudaError = false;
    state->initialized = false;

    state->d_SampleBuffer = nullptr;
    state->d_DenoiseBuffer = nullptr;
    state->d_AccumulationBuffer = nullptr;
    state->d_OutputImage = nullptr;
    state->d_InteropBuffer = nullptr;
    state->d_Spheres = nullptr;
    state->d_Materials = nullptr;
    state->d_RayDirections = nullptr;
    state->d_BVHNodes = nullptr;
    state->d_SphereIndices = nullptr;

    state->imageWidth = 0;
    state->imageHeight = 0;
    state->pixelCount = 0;

    state->allocatedSphereCount = 0;
    state->allocatedMaterialCount = 0;
    state->allocatedBVHNodeCount = 0;
    state->allocatedSphereIndexCount = 0;

    return state;
}

void CUDARenderer_Destroy(CUDARenderState* state)
{
    if (!state) return;

    if (state->d_SampleBuffer)       cudaFree(state->d_SampleBuffer);
    if (state->d_DenoiseBuffer)      cudaFree(state->d_DenoiseBuffer);
    if (state->d_AccumulationBuffer) cudaFree(state->d_AccumulationBuffer);
    if (state->d_OutputImage)        cudaFree(state->d_OutputImage);
    if (state->d_Spheres)            cudaFree(state->d_Spheres);
    if (state->d_Materials)          cudaFree(state->d_Materials);
    if (state->d_RayDirections)      cudaFree(state->d_RayDirections);
    if (state->d_BVHNodes)           cudaFree(state->d_BVHNodes);
    if (state->d_SphereIndices)      cudaFree(state->d_SphereIndices);

    if (state->streamsCreated)
    {
        cudaEventDestroy(state->uploadCompleteEvent);
        cudaStreamDestroy(state->uploadStream);
        cudaStreamDestroy(state->computeStream);
    }

    delete state;
}

int CUDARenderer_Init(CUDARenderState* state)
{
    // Check CUDA device availability
    int deviceCount;
    cudaError_t err = cudaGetDeviceCount(&deviceCount);
    if (err != cudaSuccess || deviceCount == 0)
    {
		std::fprintf(stderr, "[CUDA] No CUDA-capable device found (error: %s)\n",
			cudaGetErrorString(err));
        return 0;
    }

    // Select device 0
    err = cudaSetDevice(0);
    if (err != cudaSuccess)
    {
		std::fprintf(stderr, "[CUDA] Failed to set device 0: %s\n",
			cudaGetErrorString(err));
        return 0;
    }

    cudaDeviceProp prop;
    err = cudaGetDeviceProperties(&prop, 0);
    if (err != cudaSuccess)
    {
		std::fprintf(stderr, "[CUDA] Failed to get device properties: %s\n",
			cudaGetErrorString(err));
        return 0;
    }
	std::printf("[CUDA] Using device: %s (Compute %d.%d, %zu MB)\n",
	   prop.name, prop.major, prop.minor,
	   prop.totalGlobalMem / (1024 * 1024));

    state->initialized = true;
    err = cudaStreamCreate(&state->uploadStream);
    if (err != cudaSuccess)
    {
		std::fprintf(stderr, "[CUDA] Failed to create upload stream: %s\n",
			cudaGetErrorString(err));
        return 0;
    }
    err = cudaStreamCreate(&state->computeStream);
    if (err != cudaSuccess)
    {
		std::fprintf(stderr, "[CUDA] Failed to create compute stream: %s\n",
			cudaGetErrorString(err));
        return 0;
    }
    state->streamsCreated = true;
    cudaEventCreate(&state->uploadCompleteEvent);
    return 1;
}

int CUDARenderer_IsReady(const CUDARenderState* state)
{
    if (!state) return 0;
    return state->initialized && !state->cudaError;
}


void CUDARenderer_OnResize(CUDARenderState* state, uint32_t width, uint32_t height)
{
    if (!state || !state->initialized) return;
    if (state->cudaError) return;

    if (state->imageWidth == width && state->imageHeight == height)
        return;

    state->imageWidth = width;
    state->imageHeight = height;
    state->pixelCount = width * height;

    // Free old buffers
    if (state->d_SampleBuffer)       cudaFree(state->d_SampleBuffer);
    if (state->d_DenoiseBuffer)      cudaFree(state->d_DenoiseBuffer);
    if (state->d_AccumulationBuffer) cudaFree(state->d_AccumulationBuffer);
    if (state->d_OutputImage)        cudaFree(state->d_OutputImage);
    if (state->d_RayDirections)      cudaFree(state->d_RayDirections);

    // Allocate new buffers
    CUDA_CHECK(cudaMalloc(&state->d_SampleBuffer,       state->pixelCount * sizeof(float4)));
    CUDA_CHECK(cudaMalloc(&state->d_DenoiseBuffer,      state->pixelCount * sizeof(float4)));
    CUDA_CHECK(cudaMalloc(&state->d_AccumulationBuffer, state->pixelCount * sizeof(float4)));
    CUDA_CHECK(cudaMalloc(&state->d_OutputImage, state->pixelCount * sizeof(uint32_t)));
    CUDA_CHECK(cudaMalloc(&state->d_RayDirections, state->pixelCount * sizeof(float3)));

    state->gpuCamera.ImageWidth = width;
    state->gpuCamera.ImageHeight = height;
    state->gpuCamera.RayDirections = state->d_RayDirections;

	std::printf("[CUDA] Resized to %ux%u (%.2f MB device memory)\n",
	   width, height,
	   static_cast<float>(state->pixelCount * (2 * sizeof(float4) + sizeof(uint32_t) + sizeof(float3))) / (1024.0f * 1024.0f));
}

void CUDARenderer_UploadScene(
    CUDARenderState* state,
    const void* spheres, uint32_t sphereCount,
    const void* materials, uint32_t materialCount)
{
    if (!state || !state->initialized) return;

    // Reallocate sphere buffer only when count changes
    if (sphereCount != state->allocatedSphereCount)
    {
        if (state->d_Spheres) cudaFree(state->d_Spheres);
        state->d_Spheres = nullptr;

        if (sphereCount > 0)
        {
            CUDA_CHECK(cudaMalloc(&state->d_Spheres, sphereCount * sizeof(GPUSphere)));
            state->allocatedSphereCount = sphereCount;
        }
        else
        {
            state->allocatedSphereCount = 0;
        }
    }

    // Reallocate material buffer only when count changes
    if (materialCount != state->allocatedMaterialCount)
    {
        if (state->d_Materials) cudaFree(state->d_Materials);
        state->d_Materials = nullptr;

        if (materialCount > 0)
        {
            CUDA_CHECK(cudaMalloc(&state->d_Materials, materialCount * sizeof(GPUMaterial)));
            state->allocatedMaterialCount = materialCount;
        }
        else
        {
            state->allocatedMaterialCount = 0;
        }
    }

    // Copy updated data to GPU asynchronously (every frame when data changed)
    if (sphereCount > 0 && state->d_Spheres)
    {
        CUDA_CHECK(cudaMemcpyAsync(state->d_Spheres, spheres,
                   sphereCount * sizeof(GPUSphere), cudaMemcpyHostToDevice,
                   state->uploadStream));
    }

    if (materialCount > 0 && state->d_Materials)
    {
        CUDA_CHECK(cudaMemcpyAsync(state->d_Materials, materials,
                   materialCount * sizeof(GPUMaterial), cudaMemcpyHostToDevice,
                   state->uploadStream));
    }

    // Update scene descriptor
    state->gpuScene.Spheres = state->d_Spheres;
    state->gpuScene.Materials = state->d_Materials;
    state->gpuScene.SphereCount = sphereCount;
    state->gpuScene.MaterialCount = materialCount;
    state->gpuScene.BVHNodes = state->d_BVHNodes;
    state->gpuScene.BVHNodeCount = state->allocatedBVHNodeCount;
    state->gpuScene.SphereIndices = state->d_SphereIndices;
}

void CUDARenderer_UploadBVH(
    CUDARenderState* state,
    const void* nodes, uint32_t nodeCount,
    const void* sphereIndices, uint32_t indexCount)
{
    if (!state || !state->initialized) return;

    // Reallocate BVH nodes only when count changes
    if (nodeCount != state->allocatedBVHNodeCount)
    {
        if (state->d_BVHNodes) cudaFree(state->d_BVHNodes);
        state->d_BVHNodes = nullptr;
        state->allocatedBVHNodeCount = 0;

        if (nodeCount > 0)
        {
            CUDA_CHECK(cudaMalloc(&state->d_BVHNodes, nodeCount * sizeof(GPUBVHNode)));
            state->allocatedBVHNodeCount = nodeCount;
        }
    }

    // Reallocate sphere indices only when count changes
    if (indexCount != state->allocatedSphereIndexCount)
    {
        if (state->d_SphereIndices) cudaFree(state->d_SphereIndices);
        state->d_SphereIndices = nullptr;
        state->allocatedSphereIndexCount = 0;

        if (indexCount > 0)
        {
            CUDA_CHECK(cudaMalloc(&state->d_SphereIndices, indexCount * sizeof(int)));
            state->allocatedSphereIndexCount = indexCount;
        }
    }

    if (nodeCount > 0 && state->d_BVHNodes)
    {
        CUDA_CHECK(cudaMemcpyAsync(state->d_BVHNodes, nodes,
                   nodeCount * sizeof(GPUBVHNode), cudaMemcpyHostToDevice,
                   state->uploadStream));
    }

    if (indexCount > 0 && state->d_SphereIndices)
    {
        CUDA_CHECK(cudaMemcpyAsync(state->d_SphereIndices, sphereIndices,
                   indexCount * sizeof(int), cudaMemcpyHostToDevice,
                   state->uploadStream));
    }

    state->gpuScene.BVHNodes = state->d_BVHNodes;
    state->gpuScene.BVHNodeCount = nodeCount;
    state->gpuScene.SphereIndices = state->d_SphereIndices;
}

void CUDARenderer_UploadRayDirections(
    CUDARenderState* state,
    const void* rayDirections, uint32_t count)
{
    if (!state || !state->initialized || !state->d_RayDirections) return;

    CUDA_CHECK(cudaMemcpyAsync(state->d_RayDirections, rayDirections,
               count * sizeof(float3), cudaMemcpyHostToDevice,
               state->uploadStream));
}

void CUDARenderer_SetCameraPosition(
    CUDARenderState* state, float x, float y, float z)
{
    if (!state) return;
    state->gpuCamera.Position = make_float3(x, y, z);
}

void CUDARenderer_Render(
    CUDARenderState* state,
    uint32_t frameIndex)
{
    if (!state || !state->initialized) return;
    if (state->pixelCount == 0) return;

    // Signal compute stream to wait for pending uploads (non-blocking)
    cudaEventRecord(state->uploadCompleteEvent, state->uploadStream);
    cudaStreamWaitEvent(state->computeStream, state->uploadCompleteEvent, 0);

    // Clear accumulation buffer on first frame
    if (frameIndex == 1)
    {
        int threadsPerBlock = 256;
        int blocks = (state->pixelCount + threadsPerBlock - 1) / threadsPerBlock;
        ClearAccumulationKernel<<<blocks, threadsPerBlock, 0, state->computeStream>>>(
            state->d_AccumulationBuffer, state->pixelCount);
        // No sync needed: subsequent kernels on same computeStream execute in order
    }

    // ── Pass 1: Path trace raw samples ──
    dim3 blockDim(16, 16);
    dim3 gridDim(
        (state->imageWidth + blockDim.x - 1) / blockDim.x,
        (state->imageHeight + blockDim.y - 1) / blockDim.y
    );

    RenderKernel<<<gridDim, blockDim, 0, state->computeStream>>>(
        state->d_SampleBuffer,
        state->gpuScene,
        state->gpuCamera,
        state->gpuSettings,
        frameIndex,
        state->imageWidth,
        state->imageHeight
    );

    // ── Pass 2: Accumulate, average, clamp, RGBA convert ──
    int ppThreads = 256;
    int ppBlocks = (state->pixelCount + ppThreads - 1) / ppThreads;
    PostProcessKernel<<<ppBlocks, ppThreads, 0, state->computeStream>>>(
        state->d_SampleBuffer,
        state->d_AccumulationBuffer,
        state->d_DenoiseBuffer,
        state->d_InteropBuffer ? reinterpret_cast<uint32_t*>(state->d_InteropBuffer) : state->d_OutputImage,
        frameIndex,
        state->pixelCount
    );

    // Check for async launch errors (non-blocking)
    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess)
    {
        std::fprintf(stderr, "[CUDA] Kernel launch error: %s\n", cudaGetErrorString(err));
        state->cudaError = true;
    }
}

void CUDARenderer_GetOutput(
    CUDARenderState* state,
    void* hostOutput, uint32_t byteSize)
{
    if (!state || !state->initialized || !state->d_OutputImage)
        return;

    // Ensure compute is finished before reading output
    CUDA_CHECK(cudaStreamSynchronize(state->computeStream));

    CUDA_CHECK(cudaMemcpy(hostOutput, state->d_OutputImage,
               byteSize, cudaMemcpyDeviceToHost));
}

void* CUDARenderer_GetDenoiseBuffer(CUDARenderState* state)
{
    if (!state || !state->initialized) return nullptr;
    return state->d_DenoiseBuffer;
}

void CUDARenderer_ConvertDenoisedToRGBA(CUDARenderState* state, cudaStream_t stream)
{
    if (!state || !state->initialized || state->pixelCount == 0) return;

    int threads = 256;
    int blocks = (state->pixelCount + threads - 1) / threads;
    ConvertToRGBAKernel<<<blocks, threads, 0, stream>>>(
        reinterpret_cast<const float4*>(state->d_DenoiseBuffer),
        state->d_InteropBuffer ? reinterpret_cast<uint32_t*>(state->d_InteropBuffer) : state->d_OutputImage,
        state->pixelCount
    );
}

void* CUDARenderer_GetComputeStream(CUDARenderState* state)
{
    if (!state) return nullptr;
    return state->computeStream;
}

void CUDARenderer_SetSettings(
    CUDARenderState* state,
    int maxBounces)
{
    if (!state) return;
    state->gpuSettings.MaxBounces = maxBounces;
}

void CUDARenderer_DebugFill(CUDARenderState* state)
{
    if (!state || !state->initialized || state->pixelCount == 0) return;

    dim3 blockDim(16, 16);
    dim3 gridDim(
        (state->imageWidth + blockDim.x - 1) / blockDim.x,
        (state->imageHeight + blockDim.y - 1) / blockDim.y
    );

    DebugFillKernel<<<gridDim, blockDim>>>(
        state->d_InteropBuffer ? reinterpret_cast<uint32_t*>(state->d_InteropBuffer) : state->d_OutputImage,
        state->imageWidth,
        state->imageHeight
    );

    CUDA_CHECK(cudaDeviceSynchronize());
}

// ─── Vulkan-CUDA Interop API ───

void CUDARenderer_SetOutputBuffer(CUDARenderState* state, void* devPtr)
{
    if (!state) return;
    state->d_InteropBuffer = devPtr;
}

void CUDARenderer_Sync(CUDARenderState* state)
{
    if (!state || !state->initialized) return;
    CUDA_CHECK(cudaStreamSynchronize(state->computeStream));
}

void CUDARenderer_CheckError(const char* file, int line)
{
    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess)
    {
		std::fprintf(stderr, "[CUDA] Error at %s:%d - %s\n",
			file, line, cudaGetErrorString(err));
    }
}

} // extern "C"
