#pragma once

// ──────────────────────────────────────────────
// CUDA Renderer Host Interface
// This header can be included from plain C++ code.
// All CUDA implementation details are in CUDARenderer.cu
// ──────────────────────────────────────────────

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque render state handle
typedef struct CUDARenderState CUDARenderState;

// Lifecycle
CUDARenderState* CUDARenderer_Create(void);
void             CUDARenderer_Destroy(CUDARenderState* state);
int              CUDARenderer_Init(CUDARenderState* state);
void             CUDARenderer_CheckError(const char* file, int line);
int              CUDARenderer_IsReady(const CUDARenderState* state);  // initialized && !cudaError

// Resize (allocates device buffers for given viewport size)
void CUDARenderer_OnResize(CUDARenderState* state, uint32_t width, uint32_t height);

// Scene data upload (host → device)
void CUDARenderer_UploadScene(
    CUDARenderState* state,
    const void* spheres, uint32_t sphereCount,
    const void* materials, uint32_t materialCount);

// Ray direction upload (from Camera pre-computation)
void CUDARenderer_UploadRayDirections(
    CUDARenderState* state,
    const void* rayDirections, uint32_t count);

// Camera position (updated per-frame)
void CUDARenderer_SetCameraPosition(
    CUDARenderState* state, float x, float y, float z);

// Render one frame (launches CUDA kernel)
void CUDARenderer_Render(CUDARenderState* state, uint32_t frameIndex);

// Download output image (device → host) — legacy, skipped when interop active
void CUDARenderer_GetOutput(
    CUDARenderState* state,
    void* hostOutput, uint32_t byteSize);

// Vulkan-CUDA interop: set externally-allocated output buffer (null = use d_OutputImage)
void CUDARenderer_SetOutputBuffer(CUDARenderState* state, void* devPtr);

// Synchronize CUDA compute stream (replaces blocking GetOutput when interop active)
void CUDARenderer_Sync(CUDARenderState* state);

// Get denoise buffer pointer (float4* HDR, for OptiX denoiser)
void* CUDARenderer_GetDenoiseBuffer(CUDARenderState* state);

// Convert denoised float4 buffer → RGBA8 output (GPU-side)
void CUDARenderer_ConvertDenoisedToRGBA(CUDARenderState* state, void* stream);

// Get compute stream for OptiX denoiser
void* CUDARenderer_GetComputeStream(CUDARenderState* state);

// Settings
void CUDARenderer_SetSettings(
    CUDARenderState* state,
    int maxBounces);

// Debug: fills output with checkerboard test pattern
void CUDARenderer_DebugFill(CUDARenderState* state);

// BVH upload (host → device)
void CUDARenderer_UploadBVH(
    CUDARenderState* state,
    const void* nodes, uint32_t nodeCount,
    const void* sphereIndices, uint32_t indexCount);

#ifdef __cplusplus
}
#endif

// ──────────────────────────────────────────────
// C++ Wrapper: GPU-compatible packed data types
// These must match CUDATypes.cuh layout EXACTLY
// ──────────────────────────────────────────────

#ifdef __cplusplus

#include <memory>
#include <cstdint>

// RAII deleter for CUDARenderState
struct CUDARenderStateDeleter
{
    void operator()(CUDARenderState* state) const noexcept
    {
        CUDARenderer_Destroy(state);
    }
};

// C++-compatible float3 (matches CUDA float3 layout: 12 bytes, alignment 4)
// Only define when CUDA's float3 from cuda_runtime.h is not already available
#if !defined(CUDART_VERSION)
struct float3
{
    float x, y, z;
};
#endif

// Memory layout must match GPUSphere in CUDATypes.cuh
// Compile-time enforcement via static_assert in CUDATypes.cuh
struct GPUPackedSphere
{
    float Position[3];
    float Radius;
    int   MaterialIndex;
};
static_assert(sizeof(GPUPackedSphere) == 20, "GPUPackedSphere must be 20 bytes — must match GPUSphere");
static_assert(alignof(GPUPackedSphere) == 4,  "GPUPackedSphere alignment must match GPUSphere");

// Memory layout must match GPUMaterial in CUDATypes.cuh
// Compile-time enforcement via static_assert in CUDATypes.cuh
struct GPUPackedMaterial
{
    float Albedo[3];
    float Roughness;
    float Metallic;
    float EmissionColor[3];
    float EmissionPower;
};
static_assert(sizeof(GPUPackedMaterial) == 36, "GPUPackedMaterial must be 36 bytes — must match GPUMaterial");
static_assert(alignof(GPUPackedMaterial) == 4,  "GPUPackedMaterial alignment must match GPUMaterial");

// Memory layout must match GPUBVHNode in CUDATypes.cuh
struct GPUPackedBVHNode
{
    float BoundsMin[3];  // AABB minimum corner
    float BoundsMax[3];  // AABB maximum corner
    int   LeftFirst;     // <0 = leaf (~sphere index), >=0 = internal node index
    int   Count;         // leaf: sphere count, internal: right child index
};
static_assert(sizeof(GPUPackedBVHNode) == 32, "GPUPackedBVHNode must be 32 bytes");
static_assert(alignof(GPUPackedBVHNode) == 4, "GPUPackedBVHNode alignment must match GPUBVHNode");

#endif // __cplusplus
