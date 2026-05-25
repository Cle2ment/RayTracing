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

#ifdef __cplusplus
}
#endif

// ──────────────────────────────────────────────
// C++ Wrapper (optional, for convenience)
// ──────────────────────────────────────────────

#ifdef __cplusplus

#include <glm/glm.hpp>
#include <vector>

struct CPUMaterial
{
    glm::vec3 Albedo{ 1.0f };
    float     Roughness = 1.0f;
    float     Metallic = 0.0f;
    glm::vec3 EmissionColor{ 0.0f };
    float     EmissionPower = 0.0f;
};

struct CPUSphere
{
    glm::vec3 Position{ 0.0f };
    float     Radius = 0.5f;
    int       MaterialIndex = 0;
};

// GPU-compatible packed versions (must match CUDATypes.cuh layout exactly)
struct GPUPackedSphere
{
    float Position[3];
    float Radius;
    int   MaterialIndex;
};

struct GPUPackedMaterial
{
    float Albedo[3];
    float Roughness;
    float Metallic;
    float EmissionColor[3];
    float EmissionPower;
};

inline void PackSpheres(const std::vector<CPUSphere>& cpuSpheres,
                        std::vector<GPUPackedSphere>& gpuSpheres)
{
    gpuSpheres.resize(cpuSpheres.size());
    for (size_t i = 0; i < cpuSpheres.size(); i++)
    {
        gpuSpheres[i].Position[0] = cpuSpheres[i].Position.x;
        gpuSpheres[i].Position[1] = cpuSpheres[i].Position.y;
        gpuSpheres[i].Position[2] = cpuSpheres[i].Position.z;
        gpuSpheres[i].Radius = cpuSpheres[i].Radius;
        gpuSpheres[i].MaterialIndex = cpuSpheres[i].MaterialIndex;
    }
}

inline void PackMaterials(const std::vector<CPUMaterial>& cpuMaterials,
                          std::vector<GPUPackedMaterial>& gpuMaterials)
{
    gpuMaterials.resize(cpuMaterials.size());
    for (size_t i = 0; i < cpuMaterials.size(); i++)
    {
        gpuMaterials[i].Albedo[0] = cpuMaterials[i].Albedo.x;
        gpuMaterials[i].Albedo[1] = cpuMaterials[i].Albedo.y;
        gpuMaterials[i].Albedo[2] = cpuMaterials[i].Albedo.z;
        gpuMaterials[i].Roughness = cpuMaterials[i].Roughness;
        gpuMaterials[i].Metallic = cpuMaterials[i].Metallic;
        gpuMaterials[i].EmissionColor[0] = cpuMaterials[i].EmissionColor.x;
        gpuMaterials[i].EmissionColor[1] = cpuMaterials[i].EmissionColor.y;
        gpuMaterials[i].EmissionColor[2] = cpuMaterials[i].EmissionColor.z;
        gpuMaterials[i].EmissionPower = cpuMaterials[i].EmissionPower;
    }
}

#endif // __cplusplus
