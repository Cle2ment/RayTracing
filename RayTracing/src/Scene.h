#pragma once

#include <glm/glm.hpp>

#include <vector>

struct Material
{
	glm::vec3 Albedo{1.0f};

	float Roughness = 1.0f;

	float Metallic = 0.0f;

	glm::vec3 EmissionColor{ 0.0f };
	float EmissionPower = 0.0f;

	[[nodiscard]] glm::vec3 GetEmission() const noexcept { return EmissionColor * EmissionPower; }
};

struct Sphere
{
	glm::vec3 Position{0.0f};
	float Radius = 0.5f;

	int MaterialIndex = 0;
};

struct Scene
{
	std::vector<Sphere> Spheres;
	std::vector<Material> Materials;

	uint32_t Version = 0;  // Incremented when any scene property changes; Renderer uses this to skip redundant GPU uploads
};
