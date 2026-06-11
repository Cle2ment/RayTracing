#pragma once

#include <cuda_runtime.h>

// ──────────────────────────────────────────────
// GPU-compatible data structures (mirror Scene.h)
// Must be trivially copyable for cudaMemcpy
// ──────────────────────────────────────────────

struct GPURay
{
    float3 Origin;
    float3 Direction;
};

struct GPUSphere
{
    float3 Position;
    float  Radius;
    int    MaterialIndex;
};

// Compile-time memory layout contract with host-side GPUPackedSphere (CUDARenderer.h)
static_assert(sizeof(GPUSphere) == 20, "GPUSphere must be 20 bytes — must match GPUPackedSphere");
static_assert(alignof(GPUSphere) == 4,  "GPUSphere alignment must match GPUPackedSphere");

struct GPUBVHNode
{
    float3 BoundsMin;  // AABB minimum corner
    float3 BoundsMax;  // AABB maximum corner
    int    LeftFirst;  // <0 = leaf (encoded sphere index), >=0 = internal node index
    int    Count;      // leaf: sphere count, internal: right child index
};

struct GPUMaterial
{
    float3 Albedo;
    float  Roughness;
    float  Metallic;
    float3 EmissionColor;
    float  EmissionPower;
};

// Compile-time memory layout contract with host-side GPUPackedMaterial (CUDARenderer.h)
static_assert(sizeof(GPUMaterial) == 36, "GPUMaterial must be 36 bytes — must match GPUPackedMaterial");
static_assert(alignof(GPUMaterial) == 4,  "GPUMaterial alignment must match GPUPackedMaterial");

struct GPUHitPayload
{
    float  HitDistance;
    float3 WorldPosition;
    float3 WorldNormal;
    int    ObjectIndex;
};

// ──────────────────────────────────────────────
// Scene descriptor (pointers to device memory)
// ──────────────────────────────────────────────

struct GPUScene
{
    GPUSphere*   Spheres;
    GPUMaterial* Materials;
    GPUBVHNode*  BVHNodes;       // Device BVH node array
    int*         SphereIndices;  // Sorted sphere index array (for leaf resolution)
    uint32_t     SphereCount;
    uint32_t     MaterialCount;
    uint32_t     BVHNodeCount;   // Number of BVH nodes (0 = no BVH)
};

// ──────────────────────────────────────────────
// Camera data for ray generation on GPU
// ──────────────────────────────────────────────

struct GPUCamera
{
    float3 Position;
    // Pre-computed ray directions (width * height)
    float3* RayDirections;
    uint32_t ImageWidth;
    uint32_t ImageHeight;
};

// ──────────────────────────────────────────────
// Render settings
// ──────────────────────────────────────────────

struct GPURenderSettings
{
    int  MaxBounces = 5;
};
