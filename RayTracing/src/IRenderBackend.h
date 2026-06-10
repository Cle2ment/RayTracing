#pragma once

#include <cstdint>
#include <glm/glm.hpp>

struct Scene;
class Camera;

/// Abstract rendering backend — GPU (CUDA) or CPU (C++/ISPC).
/// The orchestrator (Renderer) owns the output image and frame counter;
/// the backend owns its own device-specific resources and path tracing logic.
class IRenderBackend
{
public:
	virtual ~IRenderBackend() = default;

	/// Initialize or resize backend resources. Called before first Render(),
	/// and on every viewport resize. Must be idempotent (same width/height = no-op).
	virtual void OnResize(uint32_t width, uint32_t height) = 0;

	/// Render a single frame. Writes RGBA8 pixels to outputBuffer.
	/// @param scene       Current scene (spheres + materials)
	/// @param camera      Current camera (position + ray directions)
	/// @param outputBuffer Host-side RGBA8 buffer (width * height * 4 bytes)
	/// @param frameIndex  1-based accumulation counter
	/// @param maxBounces  Path depth limit
	virtual void Render(
		const Scene& scene,
		const Camera& camera,
		uint32_t* outputBuffer,
		uint32_t frameIndex,
		int maxBounces
	) = 0;

	/// Returns true if the backend already delivered output to the image
	/// (e.g. via zero-copy interop). When false, the orchestrator calls
	/// SetData on the final image using outputBuffer from Render().
	[[nodiscard]] virtual bool OutputDelivered() const { return false; }
};
