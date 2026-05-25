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

struct GPUMaterial
{
    float3 Albedo;
    float  Roughness;
    float  Metallic;
    float3 EmissionColor;
    float  EmissionPower;
};

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
    uint32_t     SphereCount;
    uint32_t     MaterialCount;
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
    int  MaxBounces      = 5;
    bool Accumulate      = true;
    bool UseFastRandom   = false; // true = Walnut random, false = PCG hash
};
