#ifdef PN_OPTIX

#include "OptiXDenoiser.h"

// Define the global OptiX function table (required by optix_stubs.h)
// Must be defined in exactly one translation unit.
OptixFunctionTable g_optixFunctionTable_118 = {};

#include <cstdio>

// ── Error-checking helper ──

#define OPTIX_CHECK(call) \
    do { \
        OptixResult res = (call); \
        if (res != OPTIX_SUCCESS) { \
            std::fprintf(stderr, \
                "[OptiX] Error at %s:%d - %s (code: %d)\n", \
                __FILE__, __LINE__, optixGetErrorString(res), \
                static_cast<int>(res)); \
        } \
    } while (0)

#define CUDA_CHECK(call)                                                       \
    do {                                                                       \
        cudaError_t _e = (call);                                               \
        if (_e != cudaSuccess) {                                               \
            std::fprintf(stderr, "[CUDA] Error at %s:%d — %s (code %d)\n",     \
                         __FILE__, __LINE__, cudaGetErrorString(_e), static_cast<int>(_e)); \
        }                                                                      \
    } while (0)

// ── RAII Lifecycle ──

OptiXDenoiser::OptiXDenoiser()
{
    OptixResult result = optixInit();
    if (result != OPTIX_SUCCESS)
    {
        std::fprintf(stderr, "[OptiX] optixInit failed: %s\n",
                     optixGetErrorString(result));
        return;
    }

    // Retrieve current CUDA context (must already exist)
    CUcontext cuCtx = nullptr;
    CUresult cuRes = cuCtxGetCurrent(&cuCtx);
    if (cuRes != CUDA_SUCCESS || cuCtx == nullptr)
    {
        CUdevice cuDevice;
        cuCtxGetDevice(&cuDevice);
        cuRes = cuDevicePrimaryCtxRetain(&cuCtx, cuDevice);
        if (cuRes != CUDA_SUCCESS)
        {
            std::fprintf(stderr, "[OptiX] Failed to get CUDA context\n");
            return;
        }
    }

    OptixDeviceContextOptions ctxOpts = {};
    result = optixDeviceContextCreate(cuCtx, &ctxOpts, &m_optixContext);
    if (result != OPTIX_SUCCESS)
    {
        std::fprintf(stderr, "[OptiX] optixDeviceContextCreate failed: %s\n",
                     optixGetErrorString(result));
        return;
    }

    std::printf("[OptiX] Context created successfully\n");
}

OptiXDenoiser::~OptiXDenoiser() noexcept
{
    Cleanup();
    if (m_optixContext)
        optixDeviceContextDestroy(m_optixContext);
}

// ── Initialize / Resize ──

bool OptiXDenoiser::Initialize(uint32_t width, uint32_t height, cudaStream_t stream)
{
    if (!m_optixContext || width == 0 || height == 0)
        return false;

    Cleanup();  // Free previous denoiser resources

    m_width  = width;
    m_height = height;

    OptixDenoiserOptions opts = {};
    opts.guideAlbedo   = 0;
    opts.guideNormal   = 0;
    opts.denoiseAlpha  = OPTIX_DENOISER_ALPHA_MODE_COPY;

    OPTIX_CHECK(optixDenoiserCreate(
        m_optixContext, OPTIX_DENOISER_MODEL_KIND_HDR,
        &opts, &m_denoiser));

    if (!m_denoiser) return false;

    OPTIX_CHECK(optixDenoiserComputeMemoryResources(
        m_denoiser, width, height, &m_sizes));

    CUDA_CHECK(cudaMalloc(&m_dStateBuffer,  m_sizes.stateSizeInBytes));
    CUDA_CHECK(cudaMalloc(&m_dScratchBuffer, m_sizes.withoutOverlapScratchSizeInBytes));
    CUDA_CHECK(cudaMalloc(&m_dHdrIntensity,  sizeof(float)));

    OPTIX_CHECK(optixDenoiserSetup(
        m_denoiser, reinterpret_cast<CUstream>(stream),
        width, height,
        reinterpret_cast<CUdeviceptr>(m_dStateBuffer),  m_sizes.stateSizeInBytes,
        reinterpret_cast<CUdeviceptr>(m_dScratchBuffer), m_sizes.withoutOverlapScratchSizeInBytes));

    m_valid = true;

    std::printf("[OptiX] Denoiser ready (%ux%u, %.1f MB scratch)\n",
                width, height,
                static_cast<float>(m_sizes.withoutOverlapScratchSizeInBytes
                                   + m_sizes.stateSizeInBytes
                                   + sizeof(float)) / (1024.0f * 1024.0f));
    return true;
}

void OptiXDenoiser::Resize(uint32_t width, uint32_t height, cudaStream_t stream)
{
    if (width == m_width && height == m_height && m_valid)
        return;
    Initialize(width, height, stream);
}

// ── Per-Frame Denoise ──

void OptiXDenoiser::Denoise(float4* d_input, float4* d_output,
                             uint32_t width, uint32_t height,
                             cudaStream_t stream)
{
    if (!m_valid || !m_denoiser || width != m_width || height != m_height)
        return;

    OptixImage2D inputLayer = {};
    inputLayer.data               = reinterpret_cast<CUdeviceptr>(d_input);
    inputLayer.width              = width;
    inputLayer.height             = height;
    inputLayer.rowStrideInBytes   = width * sizeof(float4);
    inputLayer.pixelStrideInBytes = sizeof(float4);
    inputLayer.format             = OPTIX_PIXEL_FORMAT_FLOAT4;

    OptixImage2D outputLayer = inputLayer;
    outputLayer.data = reinterpret_cast<CUdeviceptr>(d_output);

    // Compute HDR intensity for brightness normalization
    OPTIX_CHECK(optixDenoiserComputeIntensity(
        m_denoiser, reinterpret_cast<CUstream>(stream),
        &inputLayer,
        reinterpret_cast<CUdeviceptr>(m_dHdrIntensity),
        reinterpret_cast<CUdeviceptr>(m_dScratchBuffer),
        m_sizes.withoutOverlapScratchSizeInBytes));

    OptixDenoiserParams params = {};
    params.blendFactor   = 0.0f;
    params.hdrIntensity  = reinterpret_cast<CUdeviceptr>(m_dHdrIntensity);

    OptixDenoiserGuideLayer guideLayer = {};
    OptixDenoiserLayer layer = {};
    layer.input  = inputLayer;
    layer.output = outputLayer;

    OPTIX_CHECK(optixDenoiserInvoke(
        m_denoiser, reinterpret_cast<CUstream>(stream),
        &params,
        reinterpret_cast<CUdeviceptr>(m_dStateBuffer), m_sizes.stateSizeInBytes,
        &guideLayer, &layer, 1,
        0, 0,
        reinterpret_cast<CUdeviceptr>(m_dScratchBuffer),
        m_sizes.withoutOverlapScratchSizeInBytes));
}

// ── Cleanup ──

void OptiXDenoiser::Cleanup()
{
    if (m_dHdrIntensity)  { cudaFree(m_dHdrIntensity);  m_dHdrIntensity  = nullptr; }
    if (m_dScratchBuffer) { cudaFree(m_dScratchBuffer); m_dScratchBuffer = nullptr; }
    if (m_dStateBuffer)   { cudaFree(m_dStateBuffer);   m_dStateBuffer   = nullptr; }
    if (m_denoiser)       { optixDenoiserDestroy(m_denoiser); m_denoiser = nullptr; }
    m_valid = false;
}

#endif // PN_OPTIX
