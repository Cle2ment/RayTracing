#include "CUDABackend.h"

#ifdef PN_CUDA

#include "Scene.h"
#include "Camera.h"
#include "Constants.h"
#include "BVH.h"

#include "Peanut/Application.h"

#include <cstdio>

// ──────────────────────────────────────────────
// Constructor / Destructor
// ──────────────────────────────────────────────

CUDABackend::CUDABackend()
{
	m_CUDAState.reset(CUDARenderer_Create());
}

// ──────────────────────────────────────────────
// OnResize — allocate device buffers + interop
// ──────────────────────────────────────────────

void CUDABackend::OnResize(uint32_t width, uint32_t height)
{
	m_ImageWidth = width;
	m_ImageHeight = height;

	// Initialize CUDA on first resize
	static bool cudaInitialized = false;
	if (!cudaInitialized && m_CUDAState)
	{
		if (CUDARenderer_Init(m_CUDAState.get()))
		{
			cudaInitialized = true;
			CUDARenderer_SetSettings(m_CUDAState.get(), 5);
		}
	}

	if (cudaInitialized)
	{
		CUDARenderer_OnResize(m_CUDAState.get(), width, height);

		// Recreate interop buffer on resize when enabled
		if (m_InteropEnabled)
		{
			try { m_Interop = std::make_unique<VkCUDAInterop>(width, height); }
			catch (...) { std::fprintf(stderr, "[Interop] Failed to create VkCUDAInterop on resize\n"); m_Interop.reset(); m_InteropEnabled = false; }
			if (m_Interop)
				CUDARenderer_SetOutputBuffer(m_CUDAState.get(), m_Interop->GetCUDADevicePtr());
		}
	}

	// Resize OptiX denoiser to match new viewport (releases old GPU scratch buffers)
#ifdef PN_OPTIX
	if (cudaInitialized && EnableDenoising)
	{
		cudaStream_t stream = (cudaStream_t)CUDARenderer_GetComputeStream(m_CUDAState.get());
		if (stream)
			m_Denoiser.Resize(width, height, stream);
	}
#endif

	m_RayDirsDirty = true;  // Force re-upload on resize
}

// ──────────────────────────────────────────────
// UploadSceneToGPU — pack + cudaMemcpy host→device
// ──────────────────────────────────────────────

void CUDABackend::UploadSceneToGPU(const Scene& scene)
{
	// Pack Sphere data (glm::vec3 → float[3] for GPU compatibility)
	m_GPUSpheres.resize(scene.Spheres.size());
	for (size_t i = 0; i < scene.Spheres.size(); i++)
	{
		const auto& s = scene.Spheres[i];
		m_GPUSpheres[i].Position[0] = s.Position.x;
		m_GPUSpheres[i].Position[1] = s.Position.y;
		m_GPUSpheres[i].Position[2] = s.Position.z;
		m_GPUSpheres[i].Radius = s.Radius;
		m_GPUSpheres[i].MaterialIndex = s.MaterialIndex;
	}

	// Pack Material data
	m_GPUMaterials.resize(scene.Materials.size());
	for (size_t i = 0; i < scene.Materials.size(); i++)
	{
		const auto& m = scene.Materials[i];
		m_GPUMaterials[i].Albedo[0] = m.Albedo.x;
		m_GPUMaterials[i].Albedo[1] = m.Albedo.y;
		m_GPUMaterials[i].Albedo[2] = m.Albedo.z;
		m_GPUMaterials[i].Roughness = m.Roughness;
		m_GPUMaterials[i].Metallic = m.Metallic;
		m_GPUMaterials[i].EmissionColor[0] = m.EmissionColor.x;
		m_GPUMaterials[i].EmissionColor[1] = m.EmissionColor.y;
		m_GPUMaterials[i].EmissionColor[2] = m.EmissionColor.z;
		m_GPUMaterials[i].EmissionPower = m.EmissionPower;
	}

	CUDARenderer_UploadScene(
		m_CUDAState.get(),
		m_GPUSpheres.data(),
		static_cast<uint32_t>(m_GPUSpheres.size()),
		m_GPUMaterials.data(),
		static_cast<uint32_t>(m_GPUMaterials.size())
	);
}

// ──────────────────────────────────────────────
// Render — GPU path tracing + OptiX denoise + output
// ──────────────────────────────────────────────

void CUDABackend::Render(
	const Scene& scene,
	const Camera& camera,
	uint32_t* outputBuffer,
	uint32_t frameIndex,
	int maxBounces)
{
	if (!m_CUDAState)
		return;

	const uint32_t width = m_ImageWidth;
	const uint32_t height = m_ImageHeight;
	if (width == 0 || height == 0) return;

	// Sync interop toggle from settings (requires re-creation on enable)
	if (EnableInterop != m_InteropEnabled)
	{
		m_InteropEnabled = EnableInterop;
		if (m_InteropEnabled)
		{
			try
			{
				m_Interop = std::make_unique<VkCUDAInterop>(width, height);
				CUDARenderer_SetOutputBuffer(m_CUDAState.get(), m_Interop->GetCUDADevicePtr());
			}
			catch (...)
			{
				std::fprintf(stderr, "[Interop] Failed to create VkCUDAInterop on toggle\n");
				m_Interop.reset();
				m_InteropEnabled = false;
				EnableInterop = false;
			}
		}
		else
		{
			m_Interop.reset();
			CUDARenderer_SetOutputBuffer(m_CUDAState.get(), nullptr);
		}
	}

	// Upload scene data only when changed (tracked by scene version)
	if (scene.Version != m_LastSceneVersion)
	{
		UploadSceneToGPU(scene);
		m_LastSceneVersion = scene.Version;

		// Rebuild BVH and upload to GPU
		m_BVH.Build(scene);
		const auto& bvhNodes = m_BVH.Nodes();

		m_GPUBVHNodes.resize(bvhNodes.size());
		for (size_t i = 0; i < bvhNodes.size(); i++)
		{
			m_GPUBVHNodes[i].BoundsMin[0] = bvhNodes[i].Bounds.Min.x;
			m_GPUBVHNodes[i].BoundsMin[1] = bvhNodes[i].Bounds.Min.y;
			m_GPUBVHNodes[i].BoundsMin[2] = bvhNodes[i].Bounds.Min.z;
			m_GPUBVHNodes[i].BoundsMax[0] = bvhNodes[i].Bounds.Max.x;
			m_GPUBVHNodes[i].BoundsMax[1] = bvhNodes[i].Bounds.Max.y;
			m_GPUBVHNodes[i].BoundsMax[2] = bvhNodes[i].Bounds.Max.z;
			m_GPUBVHNodes[i].LeftFirst = bvhNodes[i].LeftFirst;
			m_GPUBVHNodes[i].Count     = bvhNodes[i].Count;
		}

		// Copy sorted sphere indices for GPU leaf resolution
		const auto& sphereIndices = m_BVH.SphereIndices();
		m_GPUSphereIndices = sphereIndices;

		CUDARenderer_UploadBVH(
			m_CUDAState.get(),
			m_GPUBVHNodes.data(),
			static_cast<uint32_t>(m_GPUBVHNodes.size()),
			m_GPUSphereIndices.data(),
			static_cast<uint32_t>(m_GPUSphereIndices.size())
		);
	}

	// Upload camera position
	const glm::vec3& camPos = camera.GetPosition();
	CUDARenderer_SetCameraPosition(
		m_CUDAState.get(), camPos.x, camPos.y, camPos.z
	);

	// Upload ray directions — only when camera moved or viewport resized
	if (m_RayDirsDirty)
	{
		const auto& rayDirs = camera.GetRayDirections();
		m_GPURayDirs.resize(rayDirs.size());
		for (size_t i = 0; i < rayDirs.size(); i++)
		{
			m_GPURayDirs[i].x = rayDirs[i].x;
			m_GPURayDirs[i].y = rayDirs[i].y;
			m_GPURayDirs[i].z = rayDirs[i].z;
		}
		CUDARenderer_UploadRayDirections(
			m_CUDAState.get(),
			m_GPURayDirs.data(),
			static_cast<uint32_t>(m_GPURayDirs.size())
		);
		m_RayDirsDirty = false;
	}

	// Update settings
	CUDARenderer_SetSettings(
		m_CUDAState.get(),
		maxBounces
	);

	// Launch CUDA render kernel
	CUDARenderer_Render(m_CUDAState.get(), frameIndex);

#ifdef PN_OPTIX
	// Denoise pass: run OptiX on the averaged HDR buffer, then re-convert to RGBA
	if (EnableDenoising)
	{
		cudaStream_t stream = reinterpret_cast<cudaStream_t>(CUDARenderer_GetComputeStream(m_CUDAState.get()));
		if (!m_Denoiser.IsValid())
			m_Denoiser.Initialize(width, height, stream);

		float4* d_denoiseBuf = reinterpret_cast<float4*>(CUDARenderer_GetDenoiseBuffer(m_CUDAState.get()));
		if (d_denoiseBuf && m_Denoiser.IsValid())
		{
			m_Denoiser.Denoise(d_denoiseBuf, d_denoiseBuf, width, height, stream);
			CUDARenderer_ConvertDenoisedToRGBA(m_CUDAState.get(), stream);
		}
	}
#endif

	// Download output image from GPU (skip D2H when interop writes directly to Vulkan)
	if (m_InteropEnabled && m_Interop)
	{
		m_Interop->SyncCUDAComplete(reinterpret_cast<cudaStream_t>(CUDARenderer_GetComputeStream(m_CUDAState.get())));

		// ── Interop path: CUDA wrote to Vulkan buffer, copy to Peanut's VkImage ──
		VkCommandBuffer cmd = Peanut::Application::GetCommandBuffer(true);
		const VkImage dstImage = m_DestinationImage;

		// Buffer barrier: external (CUDA) write → Vulkan transfer read
		VkBufferMemoryBarrier bufBarrier = {};
		bufBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		bufBarrier.srcAccessMask = 0;
		bufBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		bufBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		bufBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		bufBarrier.buffer = m_Interop->GetVulkanBuffer();
		bufBarrier.size = VK_WHOLE_SIZE;
		vkCmdPipelineBarrier(cmd,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0, 0, nullptr, 1, &bufBarrier, 0, nullptr);

		VkImageMemoryBarrier preBarrier = {};
		preBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		preBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		preBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		preBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		preBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		preBarrier.image = dstImage;
		preBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		preBarrier.srcAccessMask = 0;
		preBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &preBarrier);

		VkBufferImageCopy region = {};
		region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		region.imageExtent = { m_Interop->GetWidth(), m_Interop->GetHeight(), 1 };
		vkCmdCopyBufferToImage(cmd, m_Interop->GetVulkanBuffer(), dstImage,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		VkImageMemoryBarrier postBarrier = {};
		postBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		postBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		postBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		postBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		postBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		postBarrier.image = dstImage;
		postBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		postBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		postBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &postBarrier);

		Peanut::Application::FlushCommandBuffer(cmd);
	}
	else
	{
		CUDARenderer_GetOutput(
			m_CUDAState.get(),
			outputBuffer,
			width * height * sizeof(uint32_t)
		);
	}
}

#endif // PN_CUDA
