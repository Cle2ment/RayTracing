#pragma once

#include "Peanut/Image.h"

#include "Camera.h"
#include "Ray.h"
#include "Scene.h"
#include "IRenderBackend.h"

#include <memory>
#include <vector>
#include <cstdint>

#include <glm/glm.hpp>

class Renderer
{
public:
	struct Settings
	{
		bool Accumulate = true;
		bool SlowRandom = false;
		int  MaxBounces = 5;
#ifdef PN_OPTIX
		bool EnableDenoising = true;
#endif
#ifdef PN_CUDA
		bool EnableInterop = true;
#endif
	};

	Renderer();
	~Renderer() noexcept;

	Renderer(const Renderer&) = delete;
	Renderer& operator=(const Renderer&) = delete;
	Renderer(Renderer&&) = delete;
	Renderer& operator=(Renderer&&) = delete;

	void OnResize(uint32_t width, uint32_t height);
	void Render(const Scene& scene, const Camera& camera);

	[[nodiscard]] std::shared_ptr<Peanut::Image> GetFinalImage() const { return m_FinalImage; }

	void ResetFrameIndex() { m_FrameIndex = 1; }
	void MarkRayDirsDirty();

	Settings& GetSettings() { return m_Settings; }

private:
	std::shared_ptr<Peanut::Image> m_FinalImage;

	Settings m_Settings;

	std::unique_ptr<IRenderBackend> m_Backend;

	const Scene* m_ActiveScene = nullptr;
	const Camera* m_ActiveCamera = nullptr;

	std::vector<uint32_t>  m_ImageData;

	uint32_t m_FrameIndex = 1;
};
