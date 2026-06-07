#include "Renderer.h"

#include "Peanut/Random.h"

#ifdef PN_CUDA
#include "Peanut/Application.h"
#endif

#include <cstring>
#include <execution>
#include <limits>
#include <ranges>

#ifdef PN_ISPC
#include "PathTracer_ispc.h"
#endif

#ifndef PN_CUDA

// ──────────────────────────────────────────────
// CPU-Only Rendering Path
// ──────────────────────────────────────────────

namespace Utils
{
	static uint32_t ConvertToRGBA(const glm::vec4& color)
	{
		const uint8_t r = static_cast<uint8_t>(color.r * 255.0f);
		const uint8_t g = static_cast<uint8_t>(color.g * 255.0f);
		const uint8_t b = static_cast<uint8_t>(color.b * 255.0f);
		const uint8_t a = static_cast<uint8_t>(color.a * 255.0f);

		const uint32_t result = (a << 24) | (b << 16) | (g << 8) | r;

		return result;
	}

	static uint32_t PCG_Hash(const uint32_t input)
	{
		const uint32_t state = input * 747796405u + 2891336453u;
		const uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;

		return (word >> 22u) ^ word;
	}

	static float RandomFloat(uint32_t& seed)
	{
		seed = PCG_Hash(seed);

		return static_cast<float>(seed) / static_cast<float>(std::numeric_limits<uint32_t>::max());
	}

	static glm::vec3 InUnitSphere(uint32_t& seed)
	{
		return glm::normalize(glm::vec3(
			RandomFloat(seed) * 2.0f - 1.0f,
			RandomFloat(seed) * 2.0f - 1.0f,
			RandomFloat(seed) * 2.0f - 1.0f
		));
	}

	// ── GGX Microfacet ──
	static glm::vec3 FresnelSchlick(float cosTheta, const glm::vec3& F0)
	{
		float t = glm::max(1.0f - cosTheta, 0.0f);
		float t5 = t * t * t * t * t;
		return F0 + (glm::vec3(1.0f) - F0) * t5;
	}

	static float GGX_D(float NdotH, float a)
	{
		float a2 = a * a;
		float d = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
		return a2 / (glm::pi<float>() * d * d);
	}

	static float GGX_G1(float NdotV, float a)
	{
		float a2 = a * a;
		float denom = NdotV + glm::sqrt(NdotV * NdotV * (1.0f - a2) + a2);
		return 2.0f * NdotV / glm::max(denom, 0.0001f);
	}

	static float GGX_G(float NdotL, float NdotV, float a)
	{
		return GGX_G1(NdotL, a) * GGX_G1(NdotV, a);
	}

	static glm::vec3 SampleGGX_VNDF(const glm::vec3& V, float a, float r1, float r2)
	{
		glm::vec3 Vh = glm::normalize(glm::vec3(a * V.x, a * V.y, V.z));
		glm::vec3 up(0.0f, 0.0f, 1.0f);
		glm::vec3 T1 = (Vh.z < 0.9999f) ? glm::normalize(glm::cross(up, Vh)) : glm::vec3(1.0f, 0.0f, 0.0f);
		glm::vec3 T2 = glm::cross(Vh, T1);

		float r = glm::sqrt(r1);
		float phi = 2.0f * glm::pi<float>() * r2;
		float t1 = r * glm::cos(phi);
		float t2 = r * glm::sin(phi);
		float s = 0.5f * (1.0f + Vh.z);
		t2 = (1.0f - s) * glm::sqrt(glm::max(1.0f - t1 * t1, 0.0f)) + s * t2;

		glm::vec3 Nh = t1 * T1 + t2 * T2 + glm::sqrt(glm::max(1.0f - t1*t1 - t2*t2, 0.0f)) * Vh;
		return glm::normalize(glm::vec3(a * Nh.x, a * Nh.y, glm::max(0.0f, Nh.z)));
	}
}

#endif // !PN_CUDA

// ──────────────────────────────────────────────
// Shared: Constructor / Destructor
// ──────────────────────────────────────────────

Renderer::Renderer()
{
#ifdef PN_CUDA
	m_CUDAState = CUDARenderer_Create();
#endif
}

Renderer::~Renderer()
{
#ifdef PN_CUDA
	if (m_CUDAState)
	{
		CUDARenderer_Destroy(m_CUDAState);
		m_CUDAState = nullptr;
	}
#endif
	delete[] m_ImageData;
	delete[] m_AccumulationData;
}

// ──────────────────────────────────────────────
// OnResize (shared between CPU and GPU paths)
// ──────────────────────────────────────────────

void Renderer::OnResize(uint32_t width, uint32_t height)
{
	if (m_FinalImage)
	{
		// No resize necessity
		if (m_FinalImage->GetWidth() == width && m_FinalImage->GetHeight() == height)
			return;

		m_FinalImage->Resize(width, height);
	}
	else
	{
		m_FinalImage = std::make_shared<Peanut::Image>(
			width,
			height,
			Peanut::ImageFormat::RGBA
		);
	}

	delete[] m_ImageData;
	m_ImageData = new uint32_t[width * height];

#ifndef PN_CUDA
	delete[] m_AccumulationData;
	m_AccumulationData = new glm::vec4[width * height];

	m_ImageHorizontalIterator.resize(width);
	m_ImageVerticalIterator.resize(height);

	for (uint32_t i = 0; i < width; i++)
		m_ImageHorizontalIterator[i] = i;
	for (uint32_t i = 0; i < height; i++)
		m_ImageVerticalIterator[i] = i;
#endif

#ifdef PN_CUDA
	// Initialize CUDA on first resize
	static bool cudaInitialized = false;
	if (!cudaInitialized && m_CUDAState)
	{
		if (CUDARenderer_Init(m_CUDAState))
		{
			cudaInitialized = true;
			CUDARenderer_SetSettings(m_CUDAState, 5);
		}
	}

	if (cudaInitialized)
	{
		CUDARenderer_OnResize(m_CUDAState, width, height);

		// Recreate interop buffer on resize when enabled
		if (m_InteropEnabled)
		{
			try { m_Interop = std::make_unique<VkCUDAInterop>(width, height); }
		catch (...) { std::fprintf(stderr, "[Interop] Failed to create VkCUDAInterop on resize\n"); m_Interop.reset(); m_InteropEnabled = false; }
			if (m_Interop)
				CUDARenderer_SetOutputBuffer(m_CUDAState, m_Interop->GetCUDADevicePtr());
		}
	}
#endif

	m_RayDirsDirty = true;  // Force re-upload on resize
	m_FrameIndex = 1;
}

// ──────────────────────────────────────────────
// Render dispatch
// ──────────────────────────────────────────────

void Renderer::Render(const Scene& scene, const Camera& camera)
{
	m_ActiveScene = &scene;
	m_ActiveCamera = &camera;

#ifdef PN_CUDA
	RenderGPU(scene, camera);
#else
	// ── CPU Rendering Path ──
	if (m_FrameIndex == 1)
		memset(
			m_AccumulationData,
			0,
			m_FinalImage->GetWidth() * m_FinalImage->GetHeight() * sizeof(glm::vec4)
		);

#ifdef PN_ISPC
	// ═══════════════════════════════════════════════
	// ISPC-Accelerated Path (SIMD vectorized)
	// Replaces the inner pixel loop with ISPC foreach
	// ═══════════════════════════════════════════════
	{
		const uint32_t width = m_FinalImage->GetWidth();
		const uint32_t height = m_FinalImage->GetHeight();
		const uint32_t pixelCount = width * height;

		const auto& rayDirs = camera.GetRayDirections();
		const glm::vec3& camPos = camera.GetPosition();

		// ── Pack ray directions into SoA flat arrays ──
		m_ISPCRayDirX.resize(pixelCount);
		m_ISPCRayDirY.resize(pixelCount);
		m_ISPCRayDirZ.resize(pixelCount);
		for (uint32_t i = 0; i < pixelCount; i++) {
			m_ISPCRayDirX[i] = rayDirs[i].x;
			m_ISPCRayDirY[i] = rayDirs[i].y;
			m_ISPCRayDirZ[i] = rayDirs[i].z;
		}

		// ── Pack scene spheres (SoA layout) ──
		const uint32_t sphereCount = static_cast<uint32_t>(scene.Spheres.size());
		m_ISCPSphPosX.resize(sphereCount);
		m_ISCPSphPosY.resize(sphereCount);
		m_ISCPSphPosZ.resize(sphereCount);
		m_ISCPSphRadius.resize(sphereCount);
		m_ISCPSphMatIdx.resize(sphereCount);
		for (uint32_t i = 0; i < sphereCount; i++) {
			const auto& s = scene.Spheres[i];
			m_ISCPSphPosX[i]   = s.Position.x;
			m_ISCPSphPosY[i]   = s.Position.y;
			m_ISCPSphPosZ[i]   = s.Position.z;
			m_ISCPSphRadius[i] = s.Radius;
			m_ISCPSphMatIdx[i] = s.MaterialIndex;
		}

		// ── Pack materials (SoA layout) ──
		const uint32_t matCount = static_cast<uint32_t>(scene.Materials.size());
		m_ISPCMatAlbedoR.resize(matCount);
		m_ISPCMatAlbedoG.resize(matCount);
		m_ISPCMatAlbedoB.resize(matCount);
		m_ISPCMatRoughness.resize(matCount);
		m_ISPCMatMetallic.resize(matCount);
		m_ISPCMatEmissionR.resize(matCount);
		m_ISPCMatEmissionG.resize(matCount);
		m_ISPCMatEmissionB.resize(matCount);
		m_ISPCMatEmissionPower.resize(matCount);
		for (uint32_t i = 0; i < matCount; i++) {
			const auto& m = scene.Materials[i];
			m_ISPCMatAlbedoR[i]      = m.Albedo.x;
			m_ISPCMatAlbedoG[i]      = m.Albedo.y;
			m_ISPCMatAlbedoB[i]      = m.Albedo.z;
			m_ISPCMatRoughness[i]    = m.Roughness;
			m_ISPCMatMetallic[i]     = m.Metallic;
			m_ISPCMatEmissionR[i]    = m.EmissionColor.x;
			m_ISPCMatEmissionG[i]    = m.EmissionColor.y;
			m_ISPCMatEmissionB[i]    = m.EmissionColor.z;
			m_ISPCMatEmissionPower[i] = m.EmissionPower;
		}

		// ── Output buffers ──
		m_ISPCOutputR.resize(pixelCount);
		m_ISPCOutputG.resize(pixelCount);
		m_ISPCOutputB.resize(pixelCount);
		m_ISPCOutputA.resize(pixelCount);

		// ── Call ISPC kernel ──
		ispc::ISPCRenderPixels(
			camPos.x, camPos.y, camPos.z,
			m_ISPCRayDirX.data(), m_ISPCRayDirY.data(), m_ISPCRayDirZ.data(),
			m_ISCPSphPosX.data(), m_ISCPSphPosY.data(), m_ISCPSphPosZ.data(),
			m_ISCPSphRadius.data(), m_ISCPSphMatIdx.data(),
			m_ISPCMatAlbedoR.data(), m_ISPCMatAlbedoG.data(), m_ISPCMatAlbedoB.data(),
			m_ISPCMatRoughness.data(), m_ISPCMatMetallic.data(),
			m_ISPCMatEmissionR.data(), m_ISPCMatEmissionG.data(), m_ISPCMatEmissionB.data(),
			m_ISPCMatEmissionPower.data(),
			m_ISPCOutputR.data(), m_ISPCOutputG.data(), m_ISPCOutputB.data(), m_ISPCOutputA.data(),
			static_cast<int32_t>(pixelCount), static_cast<int32_t>(sphereCount),
			static_cast<int32_t>(m_FrameIndex), 5  // maxBounces
		);

		// ── Unpack: accumulate + tone map + RGBA convert ──
		for (uint32_t i = 0; i < pixelCount; i++) {
			glm::vec4 color(m_ISPCOutputR[i], m_ISPCOutputG[i], m_ISPCOutputB[i], m_ISPCOutputA[i]);
			m_AccumulationData[i] += color;

			glm::vec4 accumulated = m_AccumulationData[i];
			accumulated /= static_cast<float>(m_FrameIndex);
			accumulated = glm::clamp(accumulated, glm::vec4(0.0f), glm::vec4(1.0f));
			m_ImageData[i] = Utils::ConvertToRGBA(accumulated);
		}
	}
#else
	// ── C++ Scalar / std::execution::par Fallback ──
#define MT 1
#if MT
	std::for_each(
		std::execution::par,
		m_ImageVerticalIterator.begin(), m_ImageVerticalIterator.end(),
		[this](uint32_t y)
		{
			std::ranges::for_each(
				m_ImageHorizontalIterator.begin(), m_ImageHorizontalIterator.end(),
				[this, y](const uint32_t x)
				{
					const glm::vec4 color = PerPixel(x, y);

					m_AccumulationData[x + y * m_FinalImage->GetWidth()] += color;

					glm::vec4 accumulatedColor
						= m_AccumulationData[x + y * m_FinalImage->GetWidth()];
					accumulatedColor /= static_cast<float>(m_FrameIndex);

					accumulatedColor = glm::clamp(
						accumulatedColor,
						glm::vec4(0.0f), glm::vec4(1.0f)
					);
					m_ImageData[x + y * m_FinalImage->GetWidth()]
						= Utils::ConvertToRGBA(accumulatedColor);
				});
		});
#else
	for (uint32_t y = 0; y < m_FinalImage->GetHeight(); y++)
	{
		for (uint32_t x = 0; x < m_FinalImage->GetWidth(); x++)
		{
			glm::vec4 color = PerPixel(x, y);
			m_AccumulationData[x + y * m_FinalImage->GetWidth()] += color;
			glm::vec4 accumulatedColor = m_AccumulationData[x + y * m_FinalImage->GetWidth()];
			accumulatedColor /= static_cast<float>(m_FrameIndex);
			accumulatedColor = glm::clamp(accumulatedColor, glm::vec4(0.0f), glm::vec4(1.0f));
			m_ImageData[x + y * m_FinalImage->GetWidth()] = Utils::ConvertToRGBA(accumulatedColor);
		}
	}
#endif
#endif // PN_ISPC
#endif // !PN_CUDA

#ifdef PN_CUDA
	if (m_InteropEnabled && m_Interop)
	{
		// Interop path: CUDA wrote to Vulkan buffer, copy to Peanut's VkImage
		VkCommandBuffer cmd = Peanut::Application::GetCommandBuffer(true);
		VkImage dstImage = m_FinalImage->GetImage();

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
#endif
	{
		m_FinalImage->SetData(m_ImageData);
	}

	if (m_Settings.Accumulate)
		m_FrameIndex++;
	else
		m_FrameIndex = 1;
}

// ──────────────────────────────────────────────
// CPU-Only: PerPixel / TraceRay / ClosestHit / Miss
// ──────────────────────────────────────────────

#ifndef PN_CUDA

glm::vec4 Renderer::PerPixel(uint32_t x, uint32_t y) const
{
	Ray ray;
	ray.Origin = m_ActiveCamera->GetPosition();
	ray.Direction =
		m_ActiveCamera->GetRayDirections()[x + y * m_FinalImage->GetWidth()];

	glm::vec3 light(0.0f);
	glm::vec3 contribution(1.0f);

	uint32_t seed = x + y * m_FinalImage->GetWidth();
	seed *= m_FrameIndex;

	int bounces = 5;
	for (int i = 0; i < bounces; i++)
	{
		seed += i;

		auto [
			HitDistance,
			WorldPosition,
			WorldNormal, ObjectIndex
		] = TraceRay(ray);
		if (HitDistance < 0.0f)
		{
			// Miss — terminate path (no sky/environment light)
			break;
		}

		const auto& [
			Position,
			Radius,
			MaterialIndex
		] = m_ActiveScene->Spheres[ObjectIndex];
		const Material& material = m_ActiveScene->Materials[MaterialIndex];


		// Emission is attenuated by throughput-so-far (all previous surface BRDFs),
		// but NOT by this surface's albedo (emission is a separate material property).
		// This order matches the GPU path (CUDARenderer.cuh:193-200) and the
		// path tracing integral: Le * ∏(previous BSDFs)
		light += contribution * material.GetEmission();

		// Russian roulette: probabilistically terminate low-contribution paths (after 3 guaranteed bounces)
		if (i > 2)
		{
			// Use luminance (BT.709) for survival probability: fairer than max(channel)
		const float p = 0.2126f * contribution.r + 0.7152f * contribution.g + 0.0722f * contribution.b;
			if (p < 0.001f || (p < 1.0f && Utils::RandomFloat(seed) > p))
				break;
			contribution /= p;
		}

		ray.Origin = WorldPosition + WorldNormal * 0.0001f;

		// ── GGX Microfacet BRDF ──
		const glm::vec3 w_o = -ray.Direction;
		const glm::vec3 F0 = glm::mix(glm::vec3(0.04f), material.Albedo, material.Metallic);
		const float rough = glm::max(material.Roughness, 0.001f);
		const float a = rough * rough;

		const float r1 = Utils::RandomFloat(seed), r2 = Utils::RandomFloat(seed);
		const glm::vec3 H = Utils::SampleGGX_VNDF(w_o, a, r1, r2);
		const float NdotH = glm::max(glm::dot(WorldNormal, H), 0.001f);
		const glm::vec3 wi = glm::reflect(-w_o, H);
		const float NdotL = glm::max(glm::dot(WorldNormal, wi), 0.001f);
		const float NdotV = glm::max(glm::dot(WorldNormal, w_o), 0.001f);
		const float WoDotH = glm::abs(glm::dot(w_o, H));

		const float  D = Utils::GGX_D(NdotH, a);
		const float  G = Utils::GGX_G(NdotL, NdotV, a);
		const glm::vec3 F = Utils::FresnelSchlick(WoDotH, F0);

		const glm::vec3 specBRDF = D * G * F / (4.0f * NdotL * NdotV + 0.001f);
		const glm::vec3 kD = (glm::vec3(1.0f) - F) * (1.0f - material.Metallic);
		const glm::vec3 diffBRDF = kD * material.Albedo / glm::pi<float>();

		const float pdf = glm::max(D * NdotH / (4.0f * WoDotH + 0.001f), 0.001f);

		const glm::vec3 bsdf = (specBRDF + diffBRDF) * NdotL;
		contribution *= bsdf / pdf;

		ray.Direction = glm::normalize(wi);
	}
	return { light, 1.0f };
}

Renderer::HitPayLoad Renderer::TraceRay(const Ray& ray) const
{
	int closestSphere = -1;
	float hitDistance = std::numeric_limits<float>::max();

	for (size_t i = 0; i < m_ActiveScene->Spheres.size(); i++)
	{
		const auto& [
			Position,
			Radius,
			MaterialIndex
		] = m_ActiveScene->Spheres[i];
		glm::vec3 origin = ray.Origin - Position;

		const float a = glm::dot(ray.Direction, ray.Direction);
		const float b = 2.0f * glm::dot(origin, ray.Direction);
		const float c = glm::dot(origin, origin) - Radius * Radius;

		const float discriminant = b * b - 4.0f * a * c;
		if (discriminant < 0.0f)
			continue;

		if (
			const float closestT = (-b - glm::sqrt(discriminant)) / (2.0f * a);
			closestT > 0.0f && closestT < hitDistance
			)
		{
			hitDistance = closestT;
			closestSphere = static_cast<int>(i);
		}
	}

	if (closestSphere < 0)
		return Miss(ray);

	return ClosestHit(ray, hitDistance, closestSphere);
}

Renderer::HitPayLoad Renderer::ClosestHit(
	const Ray& ray,
	const float hitDistance,
	const int objectIndex
) const
{
	Renderer::HitPayLoad payload;
	payload.HitDistance = hitDistance;
	payload.ObjectIndex = objectIndex;

	const auto& [Position, Radius, MaterialIndex]
		= m_ActiveScene->Spheres[objectIndex];

	const glm::vec3 origin = ray.Origin - Position;
	payload.WorldPosition = origin + ray.Direction * hitDistance;
	payload.WorldNormal = glm::normalize(payload.WorldPosition);

	payload.WorldPosition += Position;

	return payload;
}

Renderer::HitPayLoad Renderer::Miss(const Ray& ray)
{
	Renderer::HitPayLoad payload;
	payload.HitDistance = -1.0f;
	return payload;
}

#endif // !PN_CUDA

// ──────────────────────────────────────────────
// CUDA GPU Rendering Path
// ──────────────────────────────────────────────

#ifdef PN_CUDA

void Renderer::UploadSceneToGPU(const Scene& scene)
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
		m_CUDAState,
		m_GPUSpheres.data(),
		static_cast<uint32_t>(m_GPUSpheres.size()),
		m_GPUMaterials.data(),
		static_cast<uint32_t>(m_GPUMaterials.size())
	);
}

void Renderer::RenderGPU(const Scene& scene, const Camera& camera)
{
	if (!m_CUDAState || !m_FinalImage)
		return;

	const uint32_t width = m_FinalImage->GetWidth();
	const uint32_t height = m_FinalImage->GetHeight();
	if (width == 0 || height == 0) return;

	// Sync interop toggle from settings (requires re-creation on enable)
	if (m_Settings.EnableInterop != m_InteropEnabled)
	{
		m_InteropEnabled = m_Settings.EnableInterop;
		if (m_InteropEnabled)
		{
			try
			{
				m_Interop = std::make_unique<VkCUDAInterop>(width, height);
				CUDARenderer_SetOutputBuffer(m_CUDAState, m_Interop->GetCUDADevicePtr());
			}
			catch (...)
			{
				std::fprintf(stderr, "[Interop] Failed to create VkCUDAInterop on toggle\n");
				m_Interop.reset();
				m_InteropEnabled = false;
				m_Settings.EnableInterop = false;
			}
		}
		else
		{
			m_Interop.reset();
			CUDARenderer_SetOutputBuffer(m_CUDAState, nullptr);
		}
	}

	// Upload scene data only when changed (tracked by scene version)
	if (scene.Version != m_LastSceneVersion)
	{
		UploadSceneToGPU(scene);
		m_LastSceneVersion = scene.Version;
	}

	// Upload camera position
	const glm::vec3& camPos = camera.GetPosition();
	CUDARenderer_SetCameraPosition(
		m_CUDAState, camPos.x, camPos.y, camPos.z
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
			m_CUDAState,
			m_GPURayDirs.data(),
			static_cast<uint32_t>(m_GPURayDirs.size())
		);
		m_RayDirsDirty = false;
	}

	// Update settings
	CUDARenderer_SetSettings(
		m_CUDAState,
		5  // MaxBounces
	);

	// Launch CUDA render kernel
	CUDARenderer_Render(m_CUDAState, m_FrameIndex);

#ifdef PN_OPTIX
	// Denoise pass: run OptiX on the averaged HDR buffer, then re-convert to RGBA
	if (m_Settings.EnableDenoising)
	{
		cudaStream_t stream = (cudaStream_t)CUDARenderer_GetComputeStream(m_CUDAState);
		if (!m_Denoiser.IsValid())
			m_Denoiser.Initialize(width, height, stream);

		float4* d_denoiseBuf = (float4*)CUDARenderer_GetDenoiseBuffer(m_CUDAState);
		if (d_denoiseBuf && m_Denoiser.IsValid())
		{
			m_Denoiser.Denoise(d_denoiseBuf, d_denoiseBuf, width, height, stream);
			CUDARenderer_ConvertDenoisedToRGBA(m_CUDAState, stream);
		}
	}
#endif

	// Download output image from GPU (skip D2H when interop writes directly to Vulkan)
	if (m_InteropEnabled && m_Interop)
	{
		m_Interop->SyncCUDAComplete((cudaStream_t)CUDARenderer_GetComputeStream(m_CUDAState));
	}
	else
	{
		CUDARenderer_GetOutput(
			m_CUDAState,
			m_ImageData,
			width * height * sizeof(uint32_t)
		);
	}
}

#endif // PN_CUDA
