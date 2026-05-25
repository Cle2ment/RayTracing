#include "CUDARenderer.cuh"

#include <cstdio>

// ──────────────────────────────────────────────
// Host wrapper functions (C linkage for interop)
// ──────────────────────────────────────────────

extern "C"
{

struct CUDARenderState
{
    float4*    d_AccumulationBuffer;  // Device accumulation buffer
    uint32_t*  d_OutputImage;         // Device output RGBA buffer

    GPUSphere*   d_Spheres;           // Device scene spheres
    GPUMaterial* d_Materials;         // Device scene materials
    float3*      d_RayDirections;     // Device ray directions

    GPUScene   gpuScene;             // Device scene descriptor
    GPUCamera  gpuCamera;            // Device camera descriptor
    GPURenderSettings gpuSettings;   // Device settings

    uint32_t imageWidth;
    uint32_t imageHeight;
    uint32_t pixelCount;

    bool initialized;
};

CUDARenderState* CUDARenderer_Create()
{
    CUDARenderState* state = new CUDARenderState();
    state->initialized = false;

    state->d_AccumulationBuffer = nullptr;
    state->d_OutputImage = nullptr;
    state->d_Spheres = nullptr;
    state->d_Materials = nullptr;
    state->d_RayDirections = nullptr;

    state->imageWidth = 0;
    state->imageHeight = 0;
    state->pixelCount = 0;

    return state;
}

void CUDARenderer_Destroy(CUDARenderState* state)
{
    if (!state) return;

    if (state->d_AccumulationBuffer) cudaFree(state->d_AccumulationBuffer);
    if (state->d_OutputImage)        cudaFree(state->d_OutputImage);
    if (state->d_Spheres)            cudaFree(state->d_Spheres);
    if (state->d_Materials)          cudaFree(state->d_Materials);
    if (state->d_RayDirections)      cudaFree(state->d_RayDirections);

    delete state;
}

int CUDARenderer_Init(CUDARenderState* state)
{
    // Check CUDA device availability
    int deviceCount;
    cudaError_t err = cudaGetDeviceCount(&deviceCount);
    if (err != cudaSuccess || deviceCount == 0)
    {
        fprintf(stderr, "[CUDA] No CUDA-capable device found (error: %s)\n",
                cudaGetErrorString(err));
        return 0;
    }

    // Select device 0
    err = cudaSetDevice(0);
    if (err != cudaSuccess)
    {
        fprintf(stderr, "[CUDA] Failed to set device 0: %s\n",
                cudaGetErrorString(err));
        return 0;
    }

    cudaDeviceProp prop;
    cudaGetDeviceProperties(&prop, 0);
    printf("[CUDA] Using device: %s (Compute %d.%d, %zu MB)\n",
           prop.name, prop.major, prop.minor,
           prop.totalGlobalMem / (1024 * 1024));

    state->initialized = true;
    return 1;
}

void CUDARenderer_OnResize(CUDARenderState* state, uint32_t width, uint32_t height)
{
    if (!state || !state->initialized) return;

    if (state->imageWidth == width && state->imageHeight == height)
        return;

    state->imageWidth = width;
    state->imageHeight = height;
    state->pixelCount = width * height;

    // Free old buffers
    if (state->d_AccumulationBuffer) cudaFree(state->d_AccumulationBuffer);
    if (state->d_OutputImage)        cudaFree(state->d_OutputImage);
    if (state->d_RayDirections)      cudaFree(state->d_RayDirections);

    // Allocate new buffers
    cudaMalloc(&state->d_AccumulationBuffer, state->pixelCount * sizeof(float4));
    cudaMalloc(&state->d_OutputImage, state->pixelCount * sizeof(uint32_t));
    cudaMalloc(&state->d_RayDirections, state->pixelCount * sizeof(float3));

    state->gpuCamera.ImageWidth = width;
    state->gpuCamera.ImageHeight = height;
    state->gpuCamera.RayDirections = state->d_RayDirections;

    printf("[CUDA] Resized to %ux%u (%.2f MB device memory)\n",
           width, height,
           static_cast<float>(state->pixelCount * (sizeof(float4) + sizeof(uint32_t) + sizeof(float3))) / (1024.0f * 1024.0f));
}

void CUDARenderer_UploadScene(
    CUDARenderState* state,
    const void* spheres, uint32_t sphereCount,
    const void* materials, uint32_t materialCount)
{
    if (!state || !state->initialized) return;

    // Free old scene data
    if (state->d_Spheres)   cudaFree(state->d_Spheres);
    if (state->d_Materials) cudaFree(state->d_Materials);

    // Allocate and copy spheres
    if (sphereCount > 0)
    {
        cudaMalloc(&state->d_Spheres, sphereCount * sizeof(GPUSphere));
        cudaMemcpy(state->d_Spheres, spheres,
                   sphereCount * sizeof(GPUSphere), cudaMemcpyHostToDevice);
    }

    // Allocate and copy materials
    if (materialCount > 0)
    {
        cudaMalloc(&state->d_Materials, materialCount * sizeof(GPUMaterial));
        cudaMemcpy(state->d_Materials, materials,
                   materialCount * sizeof(GPUMaterial), cudaMemcpyHostToDevice);
    }

    // Update scene descriptor
    state->gpuScene.Spheres = state->d_Spheres;
    state->gpuScene.Materials = state->d_Materials;
    state->gpuScene.SphereCount = sphereCount;
    state->gpuScene.MaterialCount = materialCount;
}

void CUDARenderer_UploadRayDirections(
    CUDARenderState* state,
    const void* rayDirections, uint32_t count)
{
    if (!state || !state->initialized || !state->d_RayDirections) return;

    cudaMemcpy(state->d_RayDirections, rayDirections,
               count * sizeof(float3), cudaMemcpyHostToDevice);
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

    // Clear accumulation buffer on first frame
    if (frameIndex == 1)
    {
        int threadsPerBlock = 256;
        int blocks = (state->pixelCount + threadsPerBlock - 1) / threadsPerBlock;
        ClearAccumulationKernel<<<blocks, threadsPerBlock>>>(
            state->d_AccumulationBuffer, state->pixelCount);
        cudaDeviceSynchronize();
    }

    // Launch render kernel (16x16 thread blocks for good occupancy)
    dim3 blockDim(16, 16);
    dim3 gridDim(
        (state->imageWidth + blockDim.x - 1) / blockDim.x,
        (state->imageHeight + blockDim.y - 1) / blockDim.y
    );

    RenderKernel<<<gridDim, blockDim>>>(
        state->d_AccumulationBuffer,
        state->d_OutputImage,
        state->gpuScene,
        state->gpuCamera,
        state->gpuSettings,
        frameIndex,
        state->imageWidth,
        state->imageHeight
    );

    // Synchronize to catch kernel errors
    cudaError_t err = cudaDeviceSynchronize();
    if (err != cudaSuccess)
    {
        fprintf(stderr, "[CUDA] Kernel error: %s\n", cudaGetErrorString(err));
    }
}

void CUDARenderer_GetOutput(
    CUDARenderState* state,
    void* hostOutput, uint32_t byteSize)
{
    if (!state || !state->initialized || !state->d_OutputImage) return;

    cudaMemcpy(hostOutput, state->d_OutputImage,
               byteSize, cudaMemcpyDeviceToHost);
}

void CUDARenderer_SetSettings(
    CUDARenderState* state,
    int maxBounces, int accumulate)
{
    if (!state) return;
    state->gpuSettings.MaxBounces = maxBounces;
    state->gpuSettings.Accumulate = (accumulate != 0);
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
        state->d_OutputImage,
        state->imageWidth,
        state->imageHeight
    );

    cudaDeviceSynchronize();
}

void CUDARenderer_CheckError(const char* file, int line)
{
    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess)
    {
        fprintf(stderr, "[CUDA] Error at %s:%d - %s\n",
                file, line, cudaGetErrorString(err));
    }
}

} // extern "C"
