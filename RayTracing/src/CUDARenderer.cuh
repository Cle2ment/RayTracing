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
// GGX Microfacet BRDF
// ──────────────────────────────────────────────

__device__ inline float3 FresnelSchlick(float cosTheta, float3 F0)
{
    float t = fmaxf(1.0f - cosTheta, 0.0f);
    float t5 = t * t * t * t * t;
    return make_float3(
        F0.x + (1.0f - F0.x) * t5,
        F0.y + (1.0f - F0.y) * t5,
        F0.z + (1.0f - F0.z) * t5
    );
}

__device__ inline float GGX_D(float NdotH, float a)
{
    float a2 = a * a;
    float d = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
    return a2 / (3.14159265358979323846f * d * d);
}

__device__ inline float GGX_G1(float NdotV, float a)
{
    float a2 = a * a;
    float denom = NdotV + sqrtf(NdotV * NdotV * (1.0f - a2) + a2);
    return 2.0f * NdotV / fmaxf(denom, 0.0001f);
}

__device__ inline float GGX_G(float NdotL, float NdotV, float a)
{
    return GGX_G1(NdotL, a) * GGX_G1(NdotV, a);
}

// VNDF sampling: GGX visible normal distribution
__device__ inline float3 SampleGGX_VNDF(float3 V, float a, float r1, float r2)
{
    float3 Vh = make_float3(a * V.x, a * V.y, V.z);
    float vhLen = sqrtf(Vh.x*Vh.x + Vh.y*Vh.y + Vh.z*Vh.z);
    Vh = make_float3(Vh.x/vhLen, Vh.y/vhLen, Vh.z/vhLen);

    float3 up = make_float3(0.0f, 0.0f, 1.0f);
    float3 T1;
    if (Vh.z < 0.9999f) {
        T1 = make_float3(-Vh.y, Vh.x, 0.0f);
        float t1Len = sqrtf(T1.x*T1.x + T1.y*T1.y);
        T1 = make_float3(T1.x/t1Len, T1.y/t1Len, 0.0f);
    } else {
        T1 = make_float3(1.0f, 0.0f, 0.0f);
    }
    float3 T2 = make_float3(
        T1.y*Vh.z - T1.z*Vh.y,
        T1.z*Vh.x - T1.x*Vh.z,
        T1.x*Vh.y - T1.y*Vh.x
    );

    float r = sqrtf(r1);
    float phi = 2.0f * 3.14159265358979323846f * r2;
    float t1 = r * cosf(phi);
    float t2 = r * sinf(phi);
    float s = 0.5f * (1.0f + Vh.z);
    t2 = (1.0f - s) * sqrtf(fmaxf(1.0f - t1 * t1, 0.0f)) + s * t2;

    float3 Nh = make_float3(
        t1 * T1.x + t2 * T2.x + sqrtf(fmaxf(1.0f - t1 * t1 - t2 * t2, 0.0f)) * Vh.x,
        t1 * T1.y + t2 * T2.y + sqrtf(fmaxf(1.0f - t1 * t1 - t2 * t2, 0.0f)) * Vh.y,
        t1 * T1.z + t2 * T2.z + sqrtf(fmaxf(1.0f - t1 * t1 - t2 * t2, 0.0f)) * Vh.z
    );

    float3 unstretched = make_float3(a * Nh.x, a * Nh.y, fmaxf(0.0f, Nh.z));
    float len = sqrtf(unstretched.x*unstretched.x + unstretched.y*unstretched.y + unstretched.z*unstretched.z);
    return make_float3(unstretched.x/len, unstretched.y/len, unstretched.z/len);
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

        // ── GGX Microfacet BRDF ──
        float rough = fmaxf(material.Roughness, 0.001f);

        // DIAGNOSTIC: roughness >= 0.999 → pure diffuse (old Lambertian behavior)
        if (rough >= 0.999f)
        {
            float3 u_d, v_d, w_d;
            BuildONB(payload.WorldNormal, u_d, v_d, w_d);
            float3 localDir = RandomCosineWeightedDirection(seed);
            ray.Direction = make_float3(
                u_d.x*localDir.x + v_d.x*localDir.y + w_d.x*localDir.z,
                u_d.y*localDir.x + v_d.y*localDir.y + w_d.y*localDir.z,
                u_d.z*localDir.x + v_d.z*localDir.y + w_d.z*localDir.z
            );
            contribution.x *= material.Albedo.x;
            contribution.y *= material.Albedo.y;
            contribution.z *= material.Albedo.z;
            continue;
        }

        float3 w_o = make_float3(-ray.Direction.x, -ray.Direction.y, -ray.Direction.z);
        float3 N   = payload.WorldNormal;
        float NdotV = fmaxf(N.x*w_o.x + N.y*w_o.y + N.z*w_o.z, 0.001f);

        // Fresnel at normal incidence
        float3 F0 = make_float3(
            0.04f + (material.Albedo.x - 0.04f) * material.Metallic,
            0.04f + (material.Albedo.y - 0.04f) * material.Metallic,
            0.04f + (material.Albedo.z - 0.04f) * material.Metallic
        );
        float  a = rough * rough;

        // ── Randomly choose specular or diffuse ──
        // Use Fresnel at NdotV as selection probability (clamped)
        float3 F_NdotV = FresnelSchlick(NdotV, F0);
        float specProb = (F_NdotV.x + F_NdotV.y + F_NdotV.z) / 3.0f;
        specProb = fminf(fmaxf(specProb, 0.1f), 0.9f);

        if (RandomFloat(seed) < specProb)
        {
            // ── Specular: VNDF importance sampling ──
            float3 u, v, w_onb;
            BuildONB(N, u, v, w_onb);
            float3 localWo = make_float3(
                u.x*w_o.x + u.y*w_o.y + u.z*w_o.z,
                v.x*w_o.x + v.y*w_o.y + v.z*w_o.z,
                w_onb.x*w_o.x + w_onb.y*w_o.y + w_onb.z*w_o.z
            );

            float  r1 = RandomFloat(seed), r2 = RandomFloat(seed);
            float3 localH = SampleGGX_VNDF(localWo, a, r1, r2);
            float  WoDotH = localWo.x*localH.x + localWo.y*localH.y + localWo.z*localH.z;
            float  NdotH   = fmaxf(localH.z, 0.001f);

            float3 localWi = make_float3(
                2.0f * WoDotH * localH.x - localWo.x,
                2.0f * WoDotH * localH.y - localWo.y,
                2.0f * WoDotH * localH.z - localWo.z
            );
            float NdotL = fmaxf(localWi.z, 0.001f);

            float3 wi = make_float3(
                u.x * localWi.x + v.x * localWi.y + w_onb.x * localWi.z,
                u.y * localWi.x + v.y * localWi.y + w_onb.y * localWi.z,
                u.z * localWi.x + v.z * localWi.y + w_onb.z * localWi.z
            );

            float D = GGX_D(NdotH, a);
            float G = GGX_G(NdotL, NdotV, a);
            float3 F = FresnelSchlick(fabsf(WoDotH), F0);

            float3 f = make_float3(
                D * G * F.x * NdotL / (4.0f * NdotL * NdotV + 0.001f),
                D * G * F.y * NdotL / (4.0f * NdotL * NdotV + 0.001f),
                D * G * F.z * NdotL / (4.0f * NdotL * NdotV + 0.001f)
            );

            float pdf = fmaxf(D * NdotH / (4.0f * fabsf(WoDotH) + 0.001f), 0.001f) * specProb;

            contribution.x *= f.x / pdf;
            contribution.y *= f.y / pdf;
            contribution.z *= f.z / pdf;

            float wiLen = sqrtf(wi.x*wi.x + wi.y*wi.y + wi.z*wi.z);
            ray.Direction = make_float3(wi.x/wiLen, wi.y/wiLen, wi.z/wiLen);
        }
        else
        {
            // ── Diffuse: cosine-weighted hemisphere sampling ──
            float3 u, v, w_onb;
            BuildONB(N, u, v, w_onb);
            float3 localDir = RandomCosineWeightedDirection(seed);
            float NdotL = localDir.z;

            float3 wi = make_float3(
                u.x*localDir.x + v.x*localDir.y + w_onb.x*localDir.z,
                u.y*localDir.x + v.y*localDir.y + w_onb.y*localDir.z,
                u.z*localDir.x + v.z*localDir.y + w_onb.z*localDir.z
            );

            float3 kD = make_float3(
                (1.0f - FresnelSchlick(NdotL, F0).x) * (1.0f - material.Metallic),
                (1.0f - FresnelSchlick(NdotL, F0).y) * (1.0f - material.Metallic),
                (1.0f - FresnelSchlick(NdotL, F0).z) * (1.0f - material.Metallic)
            );

            float3 f = make_float3(
                kD.x * material.Albedo.x * NdotL / 3.14159265358979323846f,
                kD.y * material.Albedo.y * NdotL / 3.14159265358979323846f,
                kD.z * material.Albedo.z * NdotL / 3.14159265358979323846f
            );

            float pdf = (NdotL / 3.14159265358979323846f) * (1.0f - specProb);

            contribution.x *= f.x / pdf;
            contribution.y *= f.y / pdf;
            contribution.z *= f.z / pdf;

            ray.Direction = wi;
        }
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
    float4* __restrict__ denoiseBuffer,
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
    accumulated.w += 1.0f;
    accumulationBuffer[idx] = accumulated;

    // Average
    float invFrames = 1.0f / static_cast<float>(frameIndex);
    float4 averaged = make_float4(
        accumulated.x * invFrames,
        accumulated.y * invFrames,
        accumulated.z * invFrames,
        accumulated.w * invFrames
    );

    // Write averaged HDR for denoising (before clamp/RGBA conversion)
    denoiseBuffer[idx] = make_float4(averaged.x, averaged.y, averaged.z, 0.0f);

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
// Convert denoised float4 → RGBA8 (used after OptiX denoise pass)
// ──────────────────────────────────────────────

__launch_bounds__(256, 4)
__global__ void ConvertToRGBAKernel(
    const float4* __restrict__ inputBuffer,
    uint32_t* __restrict__ outputImage,
    uint32_t pixelCount)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= pixelCount)
        return;

    float4 color = inputBuffer[idx];
    color.x = fminf(fmaxf(color.x, 0.0f), 1.0f);
    color.y = fminf(fmaxf(color.y, 0.0f), 1.0f);
    color.z = fminf(fmaxf(color.z, 0.0f), 1.0f);

    uint8_t r = static_cast<uint8_t>(color.x * 255.0f);
    uint8_t g = static_cast<uint8_t>(color.y * 255.0f);
    uint8_t b = static_cast<uint8_t>(color.z * 255.0f);
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
