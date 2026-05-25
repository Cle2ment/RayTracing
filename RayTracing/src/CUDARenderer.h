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

// Download output image (device → host)
void CUDARenderer_GetOutput(
    CUDARenderState* state,
    void* hostOutput, uint32_t byteSize);

// Settings
void CUDARenderer_SetSettings(
    CUDARenderState* state,
    int maxBounces, int accumulate);

// Debug: fills output with checkerboard test pattern
void CUDARenderer_DebugFill(CUDARenderState* state);

#ifdef __cplusplus
}
#endif

// ──────────────────────────────────────────────
// C++ Wrapper: GPU-compatible packed data types
// These must match CUDATypes.cuh layout EXACTLY
// ──────────────────────────────────────────────

#ifdef __cplusplus

#include <cstdint>

// C++-compatible float3 (matches CUDA float3 layout: 12 bytes, alignment 4)
struct float3
{
    float x, y, z;
};

// Memory layout must match GPUSphere in CUDATypes.cuh
struct GPUPackedSphere
{
    float Position[3];
    float Radius;
    int   MaterialIndex;
};

// Memory layout must match GPUMaterial in CUDATypes.cuh
struct GPUPackedMaterial
{
    float Albedo[3];
    float Roughness;
    float Metallic;
    float EmissionColor[3];
    float EmissionPower;
};

#endif // __cplusplus
