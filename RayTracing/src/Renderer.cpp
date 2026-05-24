#include "Renderer.h"

#include "Walnut/Random.h"

#include <cstring>
#include <execution>
#include <limits>

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

Renderer::~Renderer()
{
	delete[] m_ImageData;
	delete[] m_AccumulationData;
}

void Renderer::OnResize(uint32_t width, uint32_t height)
{
	if (m_FinalImage)
	{
		// No resize necessity
		if (m_FinalImage->GetWidth() == width && m_FinalImage->GetHeight() == height )
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
	delete[] m_AccumulationData;
	m_AccumulationData = new glm::vec4[width * height];
	m_FrameIndex = 1;

	m_ImageHorizontalIterator.resize(width);
	m_ImageVerticalIterator.resize(height);

	for (uint32_t i = 0; i < width; i++)
		m_ImageHorizontalIterator[i] = i;
	for (uint32_t i = 0; i < height; i++)
		m_ImageVerticalIterator[i] = i;
}

void Renderer::Render(const Scene& scene, const Camera& camera)
{
	m_ActiveScene = &scene;
	m_ActiveCamera = &camera;

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
		[this] (uint32_t y)
		{
			std::ranges::for_each(
				m_ImageHorizontalIterator.begin(), m_ImageHorizontalIterator.end(),
				[this, y] (const uint32_t x)
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

			accumulatedColor = glm::clamp(
				accumulatedColor,
				glm::vec4(0.0f), glm::vec4(1.0f)
			);
			m_ImageData[x + y * m_FinalImage->GetWidth()]
			= Utils::ConvertToRGBA(accumulatedColor);
		}
	}
#endif

	m_FinalImage->SetData(m_ImageData);

	if (m_Settings.Accumulate)
		m_FrameIndex++;
	else
		m_FrameIndex = 1;
}

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
		// ray.Direction = glm::reflect(
		// 	 ray.Direction, 
		//	 payload.WorldNormal + material.Roughness * Walnut::Random::Vec3(-0.5f, 0.5f)
		//  );
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

		// (bx^2 + by^2)t^2 + 2(axbx + ayby)t + (ax^2 + ay^2 -r^2) = 0
		const float a = glm::dot(ray.Direction, ray.Direction);
		const float b = 2.0f * glm::dot(origin, ray.Direction);
		const float c = glm::dot(origin, origin) - Radius * Radius;

		// Quadratic formula discriminant: b^2 - 4ac
		const float discriminant = b * b - 4.0f * a * c;
		if (discriminant < 0.0f)
			continue;

		// [ -b +- sqrt(discriminant) ] / 2a

		// float t0 = (-b + glm::sqrt(discriminant)) / (2.0f * a);

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
