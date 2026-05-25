#include "Renderer.h"

#include "Walnut/Random.h"

#include <cstring>
#include <execution>
#include <limits>

#ifndef WL_CUDA

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
}

#endif // !WL_CUDA

// ──────────────────────────────────────────────
// Shared: Constructor / Destructor
// ──────────────────────────────────────────────

Renderer::Renderer()
{
#ifdef WL_CUDA
	m_CUDAState = CUDARenderer_Create();
	fprintf(stdout, "[CUDA] Renderer created, state=%p\n", static_cast<void*>(m_CUDAState));
#endif
}

Renderer::~Renderer()
{
#ifdef WL_CUDA
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
		m_FinalImage = std::make_shared<Walnut::Image>(
			width,
			height,
			Walnut::ImageFormat::RGBA
		);
	}

	delete[] m_ImageData;
	m_ImageData = new uint32_t[width * height];

#ifndef WL_CUDA
	delete[] m_AccumulationData;
	m_AccumulationData = new glm::vec4[width * height];

	m_ImageHorizontalIterator.resize(width);
	m_ImageVerticalIterator.resize(height);

	for (uint32_t i = 0; i < width; i++)
		m_ImageHorizontalIterator[i] = i;
	for (uint32_t i = 0; i < height; i++)
		m_ImageVerticalIterator[i] = i;
#endif

#ifdef WL_CUDA
	// Initialize CUDA on first resize
	static bool cudaInitialized = false;
	if (!cudaInitialized && m_CUDAState)
	{
		if (CUDARenderer_Init(m_CUDAState))
		{
			cudaInitialized = true;
			CUDARenderer_SetSettings(m_CUDAState, 5, static_cast<int>(m_Settings.Accumulate));
			fprintf(stdout, "[CUDA] Init SUCCESS\n");
		}
		else
		{
			fprintf(stderr, "[CUDA] Init FAILED - falling back to no-op GPU path\n");
		}
	}

	if (cudaInitialized)
	{
		CUDARenderer_OnResize(m_CUDAState, width, height);
		fprintf(stdout, "[CUDA] OnResize: %ux%u\n", width, height);
	}
#endif

	m_FrameIndex = 1;
}

// ──────────────────────────────────────────────
// Render dispatch
// ──────────────────────────────────────────────

void Renderer::Render(const Scene& scene, const Camera& camera)
{
	m_ActiveScene = &scene;
	m_ActiveCamera = &camera;

#ifdef WL_CUDA
	RenderGPU(scene, camera);
#else
	// ── CPU Rendering Path ──
	if (m_FrameIndex == 1)
		memset(
			m_AccumulationData,
			0,
			m_FinalImage->GetWidth() * m_FinalImage->GetHeight() * sizeof(glm::vec4)
		);

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
#endif // !WL_CUDA

	m_FinalImage->SetData(m_ImageData);

	if (m_Settings.Accumulate)
		m_FrameIndex++;
	else
		m_FrameIndex = 1;
}

// ──────────────────────────────────────────────
// CPU-Only: PerPixel / TraceRay / ClosestHit / Miss
// ──────────────────────────────────────────────

#ifndef WL_CUDA

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
			[[maybe_unused]] auto skyColor = glm::vec3(0.6f, 0.7f, 0.9f);
			// light += skyColor * contribution;
			break;
		}

		const auto& [
			Position,
			Radius,
			MaterialIndex
		] = m_ActiveScene->Spheres[ObjectIndex];
		const Material& material = m_ActiveScene->Materials[MaterialIndex];


		contribution *= material.Albedo;
		light += material.GetEmission();

		// Russian roulette: probabilistically terminate low-contribution paths (after 3 guaranteed bounces)
		if (i > 2)
		{
			const float p = glm::max(contribution.r, glm::max(contribution.g, contribution.b));
			if (p < 0.001f || (p < 1.0f && Utils::RandomFloat(seed) > p))
				break;
			contribution /= p;
		}

		ray.Origin = WorldPosition + WorldNormal * 0.0001f;
		if (m_Settings.SlowRandom)
			ray.Direction = glm::normalize(WorldNormal + Walnut::Random::InUnitSphere());
		else
			ray.Direction = glm::normalize(WorldNormal + Utils::InUnitSphere(seed));
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

#endif // !WL_CUDA

// ──────────────────────────────────────────────
// CUDA GPU Rendering Path
// ──────────────────────────────────────────────

#ifdef WL_CUDA

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

	// Upload scene data (only when dirty to avoid redundant transfers)
	if (m_SceneDirty)
	{
		UploadSceneToGPU(scene);
		m_SceneDirty = false;
	}

	// Upload camera position
	const glm::vec3& camPos = camera.GetPosition();
	CUDARenderer_SetCameraPosition(
		m_CUDAState, camPos.x, camPos.y, camPos.z
	);

	// Upload ray directions
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

	// Update settings
	CUDARenderer_SetSettings(
		m_CUDAState,
		5, // MaxBounces
		static_cast<int>(m_Settings.Accumulate)
	);

	// Launch CUDA render kernel
	fprintf(stdout, "[CUDA] Render frame %u (%ux%u)\n", m_FrameIndex, width, height);
	CUDARenderer_Render(m_CUDAState, m_FrameIndex);

	// Download output image from GPU
	CUDARenderer_GetOutput(
		m_CUDAState,
		m_ImageData,
		width * height * sizeof(uint32_t)
	);

	// Diagnostic: sample center pixel
	if (width > 0 && height > 0)
	{
		uint32_t centerIdx = (height/2) * width + (width/2);
		fprintf(stdout, "[CUDA] Output pixel[%u,%u] = 0x%08X\n",
			width/2, height/2, m_ImageData[centerIdx]);
	}
}

#endif // WL_CUDA
