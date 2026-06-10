#include "Renderer.h"

#ifdef PN_CUDA
#include "CUDABackend.h"
#include "Peanut/Application.h"
#endif
#ifdef PN_ISPC
#include "CPUBackend.h"  // already included below when !PN_CUDA, but for ISPC path
#endif

#include "CPUBackend.h"

#include <memory>

// ──────────────────────────────────────────────
// Factory: create the appropriate backend.
// MOD-02b: CPU fallback when GPU backend becomes invalid.
// ──────────────────────────────────────────────
static std::unique_ptr<IRenderBackend> CreateBackend()
{
#ifdef PN_CUDA
	auto gpuBackend = std::make_unique<CUDABackend>();
	// TODO (MOD-02b): check init success, fallback to CPUBackend on failure
	return gpuBackend;
#else
	return std::make_unique<CPUBackend>();
#endif
}

// ──────────────────────────────────────────────
// Constructor / Destructor
// ──────────────────────────────────────────────

Renderer::Renderer()
	: m_Backend(CreateBackend())
{
}

Renderer::~Renderer() noexcept = default;

// ──────────────────────────────────────────────
// OnResize
// ──────────────────────────────────────────────

void Renderer::OnResize(uint32_t width, uint32_t height)
{
	if (!m_FinalImage || m_FinalImage->GetWidth() != width || m_FinalImage->GetHeight() != height)
	{
		m_FinalImage = std::make_shared<Peanut::Image>(width, height, Peanut::ImageFormat::RGBA);
	}

	m_ImageData.resize(static_cast<size_t>(width) * static_cast<size_t>(height));

	m_Backend->OnResize(width, height);

#ifdef PN_CUDA
	// Set interop destination image (for zero-copy Vulkan output)
	auto* cudaBackend = dynamic_cast<CUDABackend*>(m_Backend.get());
	if (cudaBackend)
		cudaBackend->SetDestinationImage(m_FinalImage->GetImage());
#endif
}

// ──────────────────────────────────────────────
// Render — dispatches to backend, handles output
// ──────────────────────────────────────────────

void Renderer::Render(const Scene& scene, const Camera& camera)
{
	m_ActiveScene = &scene;
	m_ActiveCamera = &camera;

	if (m_ImageData.empty())
		return;

	m_Backend->Render(scene, camera, m_ImageData.data(), m_FrameIndex, m_Settings.MaxBounces);

	// Output delivery: if the backend didn't handle it (e.g. zero-copy interop),
	// copy the RGBA buffer to the final image.
	if (!m_Backend->OutputDelivered())
		m_FinalImage->SetData(m_ImageData.data());

	if (m_Settings.Accumulate)
		m_FrameIndex++;
	else
		m_FrameIndex = 1;
}

// ──────────────────────────────────────────────
// MarkRayDirsDirty — delegated to backend
// ──────────────────────────────────────────────

void Renderer::MarkRayDirsDirty()
{
	m_Backend->InvalidateRayDirs();
}
