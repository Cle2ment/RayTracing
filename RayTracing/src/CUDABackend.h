#pragma once

#ifdef PN_CUDA

#include "IRenderBackend.h"

#include "VkCUDAInterop.h"  // Must precede CUDARenderer.h — defines CUDART_VERSION so float3 guard works
#include "CUDARenderer.h"
#include "BVH.h"

#ifdef PN_OPTIX
#include "OptiXDenoiser.h"
#endif

#include <memory>
#include <vector>
#include <cstdint>

#include <glm/glm.hpp>

struct Scene;
class Camera;

/// GPU (CUDA) rendering backend. Implements IRenderBackend.
/// Owns all device-side resources: CUDA state, interop buffers, OptiX denoiser.
/// Extracted from Renderer to support IRenderBackend abstraction (MOD-02b CPU fallback).
class CUDABackend : public IRenderBackend
{
public:
	// ── Settings (public for ImGui binding — mirror Renderer::Settings) ──
#ifdef PN_OPTIX
	bool EnableDenoising = true;
#endif
	bool EnableInterop = true;

	CUDABackend();
	~CUDABackend() noexcept override = default;

	[[nodiscard]] bool IsValid() const noexcept { return m_CUDAState != nullptr && CUDARenderer_IsReady(m_CUDAState.get()); }

	CUDABackend(const CUDABackend&) = delete;
	CUDABackend& operator=(const CUDABackend&) = delete;
	CUDABackend(CUDABackend&&) = delete;
	CUDABackend& operator=(CUDABackend&&) = delete;

	// ── IRenderBackend interface ──
	void OnResize(uint32_t width, uint32_t height) override;
	void Render(
		const Scene& scene,
		const Camera& camera,
		uint32_t* outputBuffer,
		uint32_t frameIndex,
		int maxBounces
	) override;

	[[nodiscard]] bool OutputDelivered() const override { return m_InteropEnabled; }

	void InvalidateRayDirs() override { m_RayDirsDirty = true; }

	/// Set destination VkImage for Vulkan-CUDA interop copy.
	/// Must be called after OnResize when interop is active.
	void SetDestinationImage(VkImage image) noexcept { m_DestinationImage = image; }

private:
	// ── Scene upload (host → GPU packing + cudaMemcpy) ──
	void UploadSceneToGPU(const Scene& scene);

	// ── CUDA state ──
	std::unique_ptr<CUDARenderState, CUDARenderStateDeleter> m_CUDAState;
	std::unique_ptr<VkCUDAInterop> m_Interop;
	bool m_InteropEnabled = false;  // Actual interop state (may differ from EnableInterop on creation failure)

	// ── Host-side GPU packing buffers ──
	std::vector<GPUPackedSphere>   m_GPUSpheres;
	std::vector<GPUPackedMaterial> m_GPUMaterials;
	std::vector<float3>            m_GPURayDirs;
	std::vector<GPUPackedBVHNode>  m_GPUBVHNodes;    // Packed BVH nodes for cudaMemcpy
	std::vector<int>               m_GPUSphereIndices; // Sorted sphere indices for leaf resolution

	// ── Dirty tracking ──
	uint32_t m_LastSceneVersion = UINT32_MAX;  // Force first upload
	bool     m_RayDirsDirty     = true;

	// ── BVH ──
	BVH m_BVH;

	// ── Viewport ──
	uint32_t m_ImageWidth  = 0;
	uint32_t m_ImageHeight = 0;

	// ── Interop destination (VkImage from Peanut::Image) ──
	VkImage m_DestinationImage = VK_NULL_HANDLE;

#ifdef PN_OPTIX
	OptiXDenoiser m_Denoiser;
#endif
};

#endif // PN_CUDA
