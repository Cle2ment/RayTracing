#include "CPUBackend.h"

#include "PathTracerCore.h"
#include "Constants.h"
#include "Peanut/Random.h"

#include <cstring>
#include <execution>
#include <limits>
#include <ranges>
#include <numeric>

#ifdef PN_ISPC
#include "PathTracer_ispc.h"
#endif

// ──────────────────────────────────────────────
// OnResize
// ──────────────────────────────────────────────

void CPUBackend::OnResize(uint32_t width, uint32_t height)
{
	m_Width = width;
	m_Height = height;

	m_AccumulationData.resize(static_cast<size_t>(width) * static_cast<size_t>(height));

	m_ImageHorizontalIterator.resize(width);
	m_ImageVerticalIterator.resize(height);

	std::iota(m_ImageHorizontalIterator.begin(), m_ImageHorizontalIterator.end(), 0u);
	std::iota(m_ImageVerticalIterator.begin(), m_ImageVerticalIterator.end(), 0u);
}

// ──────────────────────────────────────────────
// Render
// ──────────────────────────────────────────────

void CPUBackend::Render(
	const Scene& scene,
	const Camera& camera,
	uint32_t* outputBuffer,
	uint32_t frameIndex,
	int maxBounces
)
{
	m_ActiveScene = &scene;
	m_ActiveCamera = &camera;
	m_MaxBounces = maxBounces;
	m_AccumFrameIndex = frameIndex;
	if (scene.Version != m_LastBvhSceneVersion)
	{
		m_BVH.Build(scene);
		m_LastBvhSceneVersion = scene.Version;
	}

	if (frameIndex == 1)
		memset(
			m_AccumulationData.data(),
			0,
			static_cast<size_t>(m_Width) * static_cast<size_t>(m_Height) * sizeof(glm::vec4)
		);

#ifdef PN_ISPC
	RenderISPC(camera, scene, outputBuffer, frameIndex);
#else
	RenderCPUFallback(outputBuffer);
#endif
}

#ifdef PN_ISPC
void CPUBackend::RenderISPC(const Camera& camera, const Scene& scene, uint32_t* outputBuffer, uint32_t frameIndex)
{
	const uint32_t width = m_Width;
	const uint32_t height = m_Height;
	const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);

	const auto& rayDirs = camera.GetRayDirections();
	const glm::vec3& camPos = camera.GetPosition();

	// ── Pack ray directions into SoA flat arrays ──
	m_ISPCRayDirX.resize(pixelCount);
	m_ISPCRayDirY.resize(pixelCount);
	m_ISPCRayDirZ.resize(pixelCount);
	for (size_t i = 0; i < pixelCount; i++) {
		m_ISPCRayDirX[i] = rayDirs[i].x;
		m_ISPCRayDirY[i] = rayDirs[i].y;
		m_ISPCRayDirZ[i] = rayDirs[i].z;
	}

	// ── Pack scene spheres (SoA layout) ──
	const uint32_t sphereCount = static_cast<uint32_t>(scene.Spheres.size());
	if (scene.Version != m_LastISPCSceneVersion)
	{
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
		m_LastISPCSceneVersion = scene.Version;

		// ── Pack BVH (SoA layout) ──
		{
			const auto& bvhNodes = m_BVH.Nodes();
			const auto& bvhSpIndices = m_BVH.SphereIndices();
			const size_t bvhCount = bvhNodes.size();

			m_ISPCBvhMinX.resize(bvhCount);
			m_ISPCBvhMinY.resize(bvhCount);
			m_ISPCBvhMinZ.resize(bvhCount);
			m_ISPCBvhMaxX.resize(bvhCount);
			m_ISPCBvhMaxY.resize(bvhCount);
			m_ISPCBvhMaxZ.resize(bvhCount);
			m_ISPCBvhLeftFirst.resize(bvhCount);
			m_ISPCBvhCount.resize(bvhCount);
			for (size_t i = 0; i < bvhCount; i++) {
				const auto& node = bvhNodes[i];
				m_ISPCBvhMinX[i] = node.Bounds.Min.x;
				m_ISPCBvhMinY[i] = node.Bounds.Min.y;
				m_ISPCBvhMinZ[i] = node.Bounds.Min.z;
				m_ISPCBvhMaxX[i] = node.Bounds.Max.x;
				m_ISPCBvhMaxY[i] = node.Bounds.Max.y;
				m_ISPCBvhMaxZ[i] = node.Bounds.Max.z;
				m_ISPCBvhLeftFirst[i] = node.LeftFirst;
				m_ISPCBvhCount[i] = node.Count;
			}

			m_ISPCBvhSphereIndices.assign(bvhSpIndices.begin(), bvhSpIndices.end());
		}
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
		// BVH data
		m_ISPCBvhMinX.data(), m_ISPCBvhMinY.data(), m_ISPCBvhMinZ.data(),
		m_ISPCBvhMaxX.data(), m_ISPCBvhMaxY.data(), m_ISPCBvhMaxZ.data(),
		m_ISPCBvhLeftFirst.data(), m_ISPCBvhCount.data(),
		m_ISPCBvhSphereIndices.data(),
		static_cast<int32_t>(m_ISPCBvhLeftFirst.size()),
		static_cast<int32_t>(frameIndex), m_MaxBounces
	);

	// ── Unpack: accumulate + tone map + RGBA convert ──
	for (size_t i = 0; i < pixelCount; i++) {
		glm::vec4 color(m_ISPCOutputR[i], m_ISPCOutputG[i], m_ISPCOutputB[i], m_ISPCOutputA[i]);
		m_AccumulationData[i] += color;

		glm::vec4 accumulated = m_AccumulationData[i];
		accumulated /= static_cast<float>(frameIndex);
		accumulated = glm::clamp(accumulated, glm::vec4(0.0f), glm::vec4(1.0f));
		outputBuffer[i] = PathTracerCore::ConvertToRGBA(accumulated);
	}
}
#endif

void CPUBackend::RenderCPUFallback(uint32_t* outputBuffer)
{
	static constexpr bool kMultithreaded = true;
	if constexpr (kMultithreaded) {
		std::for_each(
			std::execution::par,
			m_ImageVerticalIterator.begin(), m_ImageVerticalIterator.end(),
			[this, outputBuffer](uint32_t y)
			{
				std::ranges::for_each(
					m_ImageHorizontalIterator.begin(), m_ImageHorizontalIterator.end(),
					[this, y, outputBuffer, width = m_Width](const uint32_t x)
					{
						const glm::vec4 color = PerPixel(x, y);
						const size_t idx = static_cast<size_t>(x) + static_cast<size_t>(y) * static_cast<size_t>(width);

						m_AccumulationData[idx] += color;

						glm::vec4 accumulatedColor = m_AccumulationData[idx];
						accumulatedColor /= static_cast<float>(m_AccumFrameIndex);

						accumulatedColor = glm::clamp(accumulatedColor, glm::vec4(0.0f), glm::vec4(1.0f));
						outputBuffer[idx] = PathTracerCore::ConvertToRGBA(accumulatedColor);
					});
			});
	} else {
		for (uint32_t y = 0; y < m_Height; y++)
		{
			for (uint32_t x = 0; x < m_Width; x++)
			{
				const size_t idx = static_cast<size_t>(y) * static_cast<size_t>(m_Width) + static_cast<size_t>(x);
				glm::vec4 color = PerPixel(x, y);
				m_AccumulationData[idx] += color;
				glm::vec4 accumulatedColor = m_AccumulationData[idx];
				accumulatedColor /= static_cast<float>(m_AccumFrameIndex);
				accumulatedColor = glm::clamp(accumulatedColor, glm::vec4(0.0f), glm::vec4(1.0f));
				outputBuffer[idx] = PathTracerCore::ConvertToRGBA(accumulatedColor);
			}
		}
	}
}

// ──────────────────────────────────────────────
// PerPixel / TraceRay / ClosestHit / Miss
// ──────────────────────────────────────────────

glm::vec4 CPUBackend::PerPixel(uint32_t x, uint32_t y) const
{
	Ray ray;
	const size_t idx = static_cast<size_t>(y) * static_cast<size_t>(m_Width) + static_cast<size_t>(x);
	ray.Origin = m_ActiveCamera->GetPosition();
	ray.Direction = m_ActiveCamera->GetRayDirections()[idx];

	glm::vec3 light(0.0f);
	glm::vec3 contribution(1.0f);

	uint32_t seed = static_cast<uint32_t>(idx);
	seed *= m_AccumFrameIndex;

	int bounces = m_MaxBounces;
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
		const float p = kLuminanceR * contribution.r + kLuminanceG * contribution.g + kLuminanceB * contribution.b;
			if (p < kRussianRouletteThreshold || (p < 1.0f && PathTracerCore::RandomFloat(seed) > p))
				break;
			contribution /= p;
		}

		ray.Origin = WorldPosition + WorldNormal * kSelfIntersectionEpsilon;

		// ── GGX Microfacet BRDF ──
		const glm::vec3 w_o = -ray.Direction;
		const glm::vec3 F0 = glm::mix(glm::vec3(0.04f), material.Albedo, material.Metallic);
		const float rough = glm::max(material.Roughness, kRoughnessMin);
		const float a = rough * rough;

		// Build ONB from surface normal (Duff et al. 2017)
		glm::vec3 u, v, w;
		PathTracerCore::BuildONB(WorldNormal, u, v, w);

		// Transform wo to local frame where n = (0,0,1)
		float localWoX = glm::dot(u, w_o);
		float localWoY = glm::dot(v, w_o);
		float localWoZ = glm::dot(w, w_o);

		const float r1 = PathTracerCore::RandomFloat(seed), r2 = PathTracerCore::RandomFloat(seed);
		const glm::vec3 localH = PathTracerCore::SampleGGX_VNDF(glm::vec3(localWoX, localWoY, localWoZ), a, r1, r2);
		const float NdotH = glm::max(localH.z, kNdotMin);
		float WoDotH = localWoX*localH.x + localWoY*localH.y + localWoZ*localH.z;

		// Reflect in local frame: wi = 2*(wo·H)*H - wo
		const glm::vec3 localWi(
			2.0f * WoDotH * localH.x - localWoX,
			2.0f * WoDotH * localH.y - localWoY,
			2.0f * WoDotH * localH.z - localWoZ
		);
		const float NdotL = glm::max(localWi.z, kNdotMin);
		const float NdotV = glm::max(localWoZ, kNdotMin);

		// Transform wi back to world
		const glm::vec3 wi = u*localWi.x + v*localWi.y + w*localWi.z;

		const float  D = PathTracerCore::GGX_D(NdotH, a);
		const float  G1_v = PathTracerCore::GGX_G1(NdotV, a);
		const float  G = G1_v * PathTracerCore::GGX_G1(NdotL, a);
		const glm::vec3 F = PathTracerCore::FresnelSchlick(WoDotH, F0);

		const glm::vec3 specBRDF = D * G * F / (4.0f * NdotL * NdotV + kSpecDenominatorEps);
		const glm::vec3 kD = (glm::vec3(1.0f) - F) * (1.0f - material.Metallic);
		const glm::vec3 diffBRDF = kD * material.Albedo / kPi;

		// VNDF-correct PDF includes G1 for zero-variance at grazing angles
		const float pdf = glm::max(G1_v * D / (4.0f * NdotV + kSpecDenominatorEps), kNdotMin);

		const glm::vec3 bsdf = (specBRDF + diffBRDF) * NdotL;
		contribution *= bsdf / pdf;

		ray.Direction = glm::normalize(wi);
	}
	return { light, 1.0f };
}

CPUBackend::HitPayLoad CPUBackend::TraceRay(const Ray& ray) const
{
	if (m_BVH.IsEmpty())
		return Miss(ray);

	const auto [hitDistance, closestSphere] = m_BVH.Trace(ray,
		[this](const Ray& r, int sphereIndex) -> std::pair<float, int>
		{
			const auto& [Position, Radius, MaterialIndex]
				= m_ActiveScene->Spheres[sphereIndex];
			(void)MaterialIndex;

			const glm::vec3 origin = r.Origin - Position;

			const float a = glm::dot(r.Direction, r.Direction);
			const float b = 2.0f * glm::dot(origin, r.Direction);
			const float c = glm::dot(origin, origin) - Radius * Radius;

			const float discriminant = b * b - 4.0f * a * c;
			if (discriminant < 0.0f)
				return { -1.0f, -1 };

			const float closestT = (-b - glm::sqrt(discriminant)) / (2.0f * a);
			if (closestT > 0.0f)
				return { closestT, sphereIndex };
			return { -1.0f, -1 };
		});

	if (closestSphere < 0)
		return Miss(ray);

	return ClosestHit(ray, hitDistance, closestSphere);
}

CPUBackend::HitPayLoad CPUBackend::ClosestHit(
	const Ray& ray,
	const float hitDistance,
	const int objectIndex
) const noexcept
{
	CPUBackend::HitPayLoad payload;
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

CPUBackend::HitPayLoad CPUBackend::Miss(const Ray& ray) noexcept
{
	CPUBackend::HitPayLoad payload;
	payload.HitDistance = -1.0f;
	return payload;
}
