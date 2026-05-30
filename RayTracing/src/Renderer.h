#pragma once

#include "Walnut/Image.h"

#include "Camera.h"
#include "Ray.h"
#include "Scene.h"

#ifdef WL_CUDA
#include "CUDARenderer.h"
#endif

#include <memory>

#include <glm/glm.hpp>

class Renderer
{
public:
	struct Settings
	{
		bool Accumulate = true;
		bool SlowRandom = true;
	};

	Renderer();
	~Renderer();

	Renderer(const Renderer&) = delete;
	Renderer& operator=(const Renderer&) = delete;
	Renderer(Renderer&&) = delete;
	Renderer& operator=(Renderer&&) = delete;

	void OnResize(uint32_t width, uint32_t height);
	void Render(const Scene& scene, const Camera& camera);

	[[nodiscard]] std::shared_ptr<Walnut::Image> GetFinalImage() const { return m_FinalImage; }

	void ResetFrameIndex() { m_FrameIndex = 1; }

	Settings& GetSettings() { return m_Settings; }

private:
	struct HitPayLoad
	{
		float HitDistance;
		glm::vec3 WorldPosition;
		glm::vec3 WorldNormal;

		int ObjectIndex;
	};

#ifdef WL_CUDA
	// GPU rendering path
	void RenderGPU(const Scene& scene, const Camera& camera);
	void UploadSceneToGPU(const Scene& scene);
	// Scene data is uploaded every frame (ImGui modifies Scene directly)
#else
	// CPU rendering path
	[[nodiscard]] glm::vec4 PerPixel(uint32_t x, uint32_t y) const;	// RayGen Shader
	[[nodiscard]] HitPayLoad TraceRay(const Ray& ray) const;
	[[nodiscard]] HitPayLoad ClosestHit(
		const Ray& ray,
		float hitDistance,
		int objectIndex
	) const;
	static HitPayLoad Miss(const Ray& ray);
#endif

private:
	std::shared_ptr<Walnut::Image> m_FinalImage;

	Settings m_Settings;

	std::vector<uint32_t> m_ImageHorizontalIterator, m_ImageVerticalIterator;

	const Scene* m_ActiveScene = nullptr;
	const Camera* m_ActiveCamera = nullptr;

	uint32_t* m_ImageData = nullptr;
	glm::vec4* m_AccumulationData = nullptr;

	uint32_t m_FrameIndex = 1;

#ifdef WL_CUDA
	CUDARenderState* m_CUDAState = nullptr;
	std::vector<GPUPackedSphere>   m_GPUSpheres;
	std::vector<GPUPackedMaterial> m_GPUMaterials;
	std::vector<float3>            m_GPURayDirs;
#endif

#ifdef WL_ISPC
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
