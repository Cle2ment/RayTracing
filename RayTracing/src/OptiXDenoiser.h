#pragma once

#include <cstdint>

// ──────────────────────────────────────────────
// OptiX AI Denoiser — RAII Wrapper
// Requires OptiX SDK 7.0+ and PN_OPTIX define.
// When PN_OPTIX is not defined, stub methods are no-ops.
// ──────────────────────────────────────────────

#ifdef PN_OPTIX

#include <cuda_runtime.h>
#include <optix.h>
#include <optix_stubs.h>

class OptiXDenoiser
{
public:
    OptiXDenoiser();
    ~OptiXDenoiser();

    OptiXDenoiser(const OptiXDenoiser&) = delete;
    OptiXDenoiser& operator=(const OptiXDenoiser&) = delete;

    // Initialize OptiX context + create denoiser for given resolution.
    // Returns true on success.
    bool Initialize(uint32_t width, uint32_t height, cudaStream_t stream);

    // Reconfigure for new resolution (reallocates scratch buffers).
    // No-op if dimensions unchanged.
    void Resize(uint32_t width, uint32_t height, cudaStream_t stream);

    // Denoise input → output (in-place allowed).
    // d_input / d_output: device-side float4* HDR color buffers.
    // Call after path tracing, on the compute stream.
    void Denoise(float4* d_input, float4* d_output,
                 uint32_t width, uint32_t height,
                 cudaStream_t stream);

    bool IsValid() const { return m_valid; }

private:
    void Cleanup();

    OptixDeviceContext m_optixContext = nullptr;
    OptixDenoiser     m_denoiser     = nullptr;

    // GPU scratch buffers (allocated via cudaMalloc)
    void*   m_dStateBuffer   = nullptr;
    void*   m_dScratchBuffer = nullptr;
    float*  m_dHdrIntensity  = nullptr;

    OptixDenoiserSizes m_sizes   = {};
    uint32_t           m_width  = 0;
    uint32_t           m_height = 0;
    bool               m_valid  = false;
};

#else  // !PN_OPTIX — Stub (types resolved via void* to avoid CUDA dependency)

class OptiXDenoiser
{
public:
    bool Initialize(uint32_t, uint32_t, void*) { return false; }
    void Resize(uint32_t, uint32_t, void*) {}
    void Denoise(void*, void*, uint32_t, uint32_t, void*) {}
    bool IsValid() const { return false; }
};

#endif // PN_OPTIX
