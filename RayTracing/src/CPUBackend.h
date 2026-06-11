#pragma once

#include "IRenderBackend.h"
#include "Camera.h"
#include "Ray.h"
#include "Scene.h"
#include "BVH.h"

#include <vector>
#include <cstdint>

#include <glm/glm.hpp>

class CPUBackend : public IRenderBackend
{
public:
	CPUBackend() = default;
	~CPUBackend() override = default;

	void OnResize(uint32_t width, uint32_t height) override;
	void Render(const Scene& scene, const Camera& camera, uint32_t* outputBuffer, uint32_t frameIndex, int maxBounces) override;

private:
	struct HitPayLoad
	{
		float HitDistance;
		glm::vec3 WorldPosition;
		glm::vec3 WorldNormal;

		int ObjectIndex;
	};

	[[nodiscard]] glm::vec4 PerPixel(uint32_t x, uint32_t y) const;	// RayGen Shader
	[[nodiscard]] HitPayLoad TraceRay(const Ray& ray) const;
	[[nodiscard]] HitPayLoad ClosestHit(
		const Ray& ray,
		float hitDistance,
		int objectIndex
	) const noexcept;
	static HitPayLoad Miss(const Ray& ray) noexcept;

private:
	uint32_t m_Width = 0;
	uint32_t m_Height = 0;

	const Scene* m_ActiveScene = nullptr;
	const Camera* m_ActiveCamera = nullptr;

	int m_MaxBounces = 5;
	uint32_t m_AccumFrameIndex = 1;

	std::vector<uint32_t> m_ImageHorizontalIterator, m_ImageVerticalIterator;
	std::vector<glm::vec4> m_AccumulationData;

	BVH m_BVH;
	uint32_t m_LastBvhSceneVersion = UINT32_MAX;  // Rebuild BVH on scene change

#ifdef PN_ISPC
	uint32_t m_LastISPCSceneVersion = UINT32_MAX;  // Track scene changes to skip SoA repacking
	// ISPC SoA packing buffers (reused across frames to avoid reallocation)
	std::vector<float> m_ISPCRayDirX, m_ISPCRayDirY, m_ISPCRayDirZ;
	std::vector<float> m_ISCPSphPosX, m_ISCPSphPosY, m_ISCPSphPosZ;
	std::vector<float> m_ISCPSphRadius;
	std::vector<int32_t> m_ISCPSphMatIdx;
	std::vector<float> m_ISPCMatAlbedoR, m_ISPCMatAlbedoG, m_ISPCMatAlbedoB;
	std::vector<float> m_ISPCMatRoughness, m_ISPCMatMetallic;
	std::vector<float> m_ISPCMatEmissionR, m_ISPCMatEmissionG, m_ISPCMatEmissionB;
	std::vector<float> m_ISPCMatEmissionPower;
	std::vector<float> m_ISPCOutputR, m_ISPCOutputG, m_ISPCOutputB, m_ISPCOutputA;
#endif
};
