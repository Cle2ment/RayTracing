#pragma once

#include "CUDATypes.cuh"

// ──────────────────────────────────────────────
// Device-side utility functions (RNG)
// ──────────────────────────────────────────────

__device__ inline uint32_t PCGHash(uint32_t input)
{
    uint32_t state = input * 747796405u + 2891336453u;
    uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

__device__ inline float RandomFloat(uint32_t& seed)
{
    seed = PCGHash(seed);
    return __uint2float_ru(seed) * 2.3283064365386963e-10f; // 2^-32
}

// Cosine-weighted hemisphere sampling (tangent space, z-up).
// Eliminates rejection sampling → no warp divergence.
// Returns direction in hemisphere around local Z axis (unit length).
__device__ inline float3 RandomCosineWeightedDirection(uint32_t& seed)
{
	float r1 = RandomFloat(seed);
	float r2 = RandomFloat(seed);

	float phi = 2.0f * 3.14159265358979323846f * r1;
	float cosTheta = sqrtf(r2);           // cosine-weighted
	float sinTheta = sqrtf(1.0f - r2);

	return make_float3(
		cosf(phi) * sinTheta,
		sinf(phi) * sinTheta,
		cosTheta
	);
}

// Build orthonormal basis (ONB) from a normal vector (Duff et al. 2017).
// Output: tangent (u), bitangent (v), normal (w == n).
__device__ inline void BuildONB(const float3& n, float3& u, float3& v, float3& w)
{
	w = n;
	float sign = (w.z > 0.0f) ? 1.0f : -1.0f;
	float a = -1.0f / (sign + w.z);
	float b = w.x * w.y * a;
	u = make_float3(1.0f + sign * w.x * w.x * a, sign * b, -sign * w.x);
	v = make_float3(b, sign + w.y * w.y * a, -w.y);
}

// ──────────────────────────────────────────────
// Device-side ray-sphere intersection
// ──────────────────────────────────────────────

__device__ inline GPUHitPayload Miss(const GPURay& ray)
{
    GPUHitPayload payload;
    payload.HitDistance = -1.0f;
    payload.WorldPosition = make_float3(0, 0, 0);
    payload.WorldNormal = make_float3(0, 0, 0);
    payload.ObjectIndex = -1;
    return payload;
}

__device__ inline GPUHitPayload ClosestHit(
    const GPURay& ray,
    float hitDistance,
    int objectIndex,
    const GPUSphere* spheres)
{
    GPUHitPayload payload;
    payload.HitDistance = hitDistance;
    payload.ObjectIndex = objectIndex;

    const GPUSphere& sphere = spheres[objectIndex];

    float3 origin = make_float3(
        ray.Origin.x - sphere.Position.x,
        ray.Origin.y - sphere.Position.y,
        ray.Origin.z - sphere.Position.z
    );

    payload.WorldPosition = make_float3(
        origin.x + ray.Direction.x * hitDistance,
        origin.y + ray.Direction.y * hitDistance,
        origin.z + ray.Direction.z * hitDistance
    );

    float len = sqrtf(
        payload.WorldPosition.x * payload.WorldPosition.x +
        payload.WorldPosition.y * payload.WorldPosition.y +
        payload.WorldPosition.z * payload.WorldPosition.z
    );

    if (len > 0.0f)
    {
        payload.WorldNormal = make_float3(
            payload.WorldPosition.x / len,
            payload.WorldPosition.y / len,
            payload.WorldPosition.z / len
        );
    }

    payload.WorldPosition.x += sphere.Position.x;
    payload.WorldPosition.y += sphere.Position.y;
    payload.WorldPosition.z += sphere.Position.z;

    return payload;
}

__device__ inline GPUHitPayload TraceRay(
    const GPURay& ray,
    const GPUScene& scene)
{
    int closestSphere = -1;
    float hitDistance = 1e30f; // FLT_MAX equivalent

    for (uint32_t i = 0; i < scene.SphereCount; i++)
    {
        const GPUSphere& sphere = scene.Spheres[i];

        float3 origin = make_float3(
            ray.Origin.x - sphere.Position.x,
            ray.Origin.y - sphere.Position.y,
            ray.Origin.z - sphere.Position.z
        );

        // Quadratic: a*t^2 + b*t + c = 0, where a = dot(dir,dir) ≈ 1
        float a = ray.Direction.x * ray.Direction.x +
                  ray.Direction.y * ray.Direction.y +
                  ray.Direction.z * ray.Direction.z;
        float b = 2.0f * (origin.x * ray.Direction.x +
                          origin.y * ray.Direction.y +
                          origin.z * ray.Direction.z);
        float c = origin.x * origin.x +
                  origin.y * origin.y +
                  origin.z * origin.z -
                  sphere.Radius * sphere.Radius;

        float discriminant = b * b - 4.0f * a * c;
        if (discriminant < 0.0f)
            continue;

        float closestT = (-b - sqrtf(discriminant)) / (2.0f * a);
        if (closestT > 0.0f && closestT < hitDistance)
        {
            hitDistance = closestT;
            closestSphere = static_cast<int>(i);
        }
    }

    if (closestSphere < 0)
        return Miss(ray);

    return ClosestHit(ray, hitDistance, closestSphere, scene.Spheres);
}

// ──────────────────────────────────────────────
// Per-pixel path tracing (device function)
// ──────────────────────────────────────────────

__device__ inline float3 PerPixel(
    uint32_t x, uint32_t y,
    const GPUScene& scene,
    const GPUCamera& camera,
    const GPURenderSettings& settings,
    uint32_t frameIndex)
{
    GPURay ray;
    ray.Origin = camera.Position;
    ray.Direction = camera.RayDirections[x + y * camera.ImageWidth];

    float3 light = make_float3(0, 0, 0);
    float3 contribution = make_float3(1, 1, 1);

    uint32_t seed = x + y * camera.ImageWidth;
    seed *= frameIndex;

    for (int i = 0; i < settings.MaxBounces; i++)
    {
        seed += static_cast<uint32_t>(i);

        GPUHitPayload payload = TraceRay(ray, scene);

        if (payload.HitDistance < 0.0f)
        {
            // Sky miss - terminate path
            break;
        }

        const GPUSphere& sphere = scene.Spheres[payload.ObjectIndex];
        const GPUMaterial& material = scene.Materials[sphere.MaterialIndex];

        // Accumulate emission
        float3 emission = make_float3(
            material.EmissionColor.x * material.EmissionPower,
            material.EmissionColor.y * material.EmissionPower,
            material.EmissionColor.z * material.EmissionPower
        );

        light.x += contribution.x * emission.x;
        light.y += contribution.y * emission.y;
        light.z += contribution.z * emission.z;

        // Attenuate by albedo
        contribution.x *= material.Albedo.x;
        contribution.y *= material.Albedo.y;
        contribution.z *= material.Albedo.z;

        // Russian roulette: terminate low-contribution paths after 3 bounces
        if (i > 2)
        {
            float p = fmaxf(contribution.x, fmaxf(contribution.y, contribution.z));
            if (p < 0.001f || (p < 1.0f && RandomFloat(seed) > p))
                break;
            float invP = 1.0f / p;
            contribution.x *= invP;
            contribution.y *= invP;
            contribution.z *= invP;
        }

        // Bounce: offset origin slightly to avoid self-intersection
        ray.Origin = make_float3(
            payload.WorldPosition.x + payload.WorldNormal.x * 0.0001f,
            payload.WorldPosition.y + payload.WorldNormal.y * 0.0001f,
            payload.WorldPosition.z + payload.WorldNormal.z * 0.0001f
        );

        // Cosine-weighted diffuse BRDF: sample direction in hemisphere via ONB
        float3 u, v, w;
        BuildONB(payload.WorldNormal, u, v, w);
        float3 localDir = RandomCosineWeightedDirection(seed);

        ray.Direction = make_float3(
            u.x * localDir.x + v.x * localDir.y + w.x * localDir.z,
            u.y * localDir.x + v.y * localDir.y + w.y * localDir.z,
            u.z * localDir.x + v.z * localDir.y + w.z * localDir.z
        );
    }

    return light;
}

// ──────────────────────────────────────────────
// Render kernel: one thread per pixel, outputs raw sample color
// ──────────────────────────────────────────────

__launch_bounds__(256, 2)
__global__ void RenderKernel(
    float4* __restrict__ sampleBuffer,
    const GPUScene scene,
    const GPUCamera camera,
    GPURenderSettings settings,
    uint32_t frameIndex,
    uint32_t imageWidth,
    uint32_t imageHeight)
{
    uint32_t x = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= imageWidth || y >= imageHeight)
        return;

    float3 color = PerPixel(x, y, scene, camera, settings, frameIndex);
    uint32_t pixelIndex = x + y * imageWidth;
    sampleBuffer[pixelIndex] = make_float4(color.x, color.y, color.z, 0.0f);
}

// ──────────────────────────────────────────────
// Post-process kernel: accumulate, average, clamp, RGBA convert
// ──────────────────────────────────────────────

__launch_bounds__(256, 4)
__global__ void PostProcessKernel(
    const float4* __restrict__ sampleBuffer,
    float4* __restrict__ accumulationBuffer,
    uint32_t* __restrict__ outputImage,
    uint32_t frameIndex,
    uint32_t pixelCount)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= pixelCount)
        return;

    float4 sample = sampleBuffer[idx];

    // Accumulate
    float4 accumulated = accumulationBuffer[idx];
    accumulated.x += sample.x;
    accumulated.y += sample.y;
    accumulated.z += sample.z;
    accumulated.w += 1.0f;  // sample count
    accumulationBuffer[idx] = accumulated;

    // Average
    float invFrames = 1.0f / static_cast<float>(frameIndex);
    float4 averaged = make_float4(
        accumulated.x * invFrames,
        accumulated.y * invFrames,
        accumulated.z * invFrames,
        accumulated.w * invFrames
    );

    // Clamp to [0, 1]
    averaged.x = fminf(fmaxf(averaged.x, 0.0f), 1.0f);
    averaged.y = fminf(fmaxf(averaged.y, 0.0f), 1.0f);
    averaged.z = fminf(fmaxf(averaged.z, 0.0f), 1.0f);

    // Convert to RGBA8
    uint8_t r = static_cast<uint8_t>(averaged.x * 255.0f);
    uint8_t g = static_cast<uint8_t>(averaged.y * 255.0f);
    uint8_t b = static_cast<uint8_t>(averaged.z * 255.0f);
    uint8_t a = 255;

    outputImage[idx] = (a << 24) | (b << 16) | (g << 8) | r;
}

// ──────────────────────────────────────────────
// Clear accumulation buffer to zero
// ──────────────────────────────────────────────

__launch_bounds__(256, 4)
__global__ void ClearAccumulationKernel(
    float4* __restrict__ accumulationBuffer,
    uint32_t pixelCount)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= pixelCount)
        return;

    accumulationBuffer[idx] = make_float4(0, 0, 0, 0);
}

// ──────────────────────────────────────────────
// Diagnostic kernel: fills output with test gradient
// Red = x/width, Green = y/height, checkerboard if (x/16 + y/16) % 2
// ──────────────────────────────────────────────

__global__ void DebugFillKernel(
    uint32_t* __restrict__ outputImage,
    uint32_t imageWidth,
    uint32_t imageHeight)
{
    uint32_t x = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= imageWidth || y >= imageHeight)
        return;

    uint32_t pixelIndex = x + y * imageWidth;

    // Checkerboard pattern to verify output pipeline
    bool checker = ((x / 16) + (y / 16)) % 2 == 0;

    uint8_t r = checker ? 255 : 0;
    uint8_t g = checker ? 0 : 255;
    uint8_t b = 128;
    uint8_t a = 255;

    outputImage[pixelIndex] = (a << 24) | (b << 16) | (g << 8) | r;
}
