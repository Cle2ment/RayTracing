#pragma once

#include <glm/glm.hpp>
#include <glm/geometric.hpp>
#include <glm/common.hpp>

#include <cstdint>
#include <limits>

#include "Constants.h"

// ──────────────────────────────────────────────
// CPU Path Tracer Core Functions
// Transparently usable by Renderer (CPU path)
// and test harness (without CUDA dependency).
// ──────────────────────────────────────────────

namespace PathTracerCore
{
	// ── Color Conversion ──

	inline uint32_t ConvertToRGBA(const glm::vec4& color)
	{
		const uint8_t r = static_cast<uint8_t>(color.r * 255.0f);
		const uint8_t g = static_cast<uint8_t>(color.g * 255.0f);
		const uint8_t b = static_cast<uint8_t>(color.b * 255.0f);
		const uint8_t a = static_cast<uint8_t>(color.a * 255.0f);

		const uint32_t result = (a << 24) | (b << 16) | (g << 8) | r;

		return result;
	}

	// ── Random Number Generation (PCG) ──

	inline uint32_t PCG_Hash(const uint32_t input)
	{
		const uint32_t state = input * 747796405u + 2891336453u;
		const uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;

		return (word >> 22u) ^ word;
	}

	inline float RandomFloat(uint32_t& seed)
	{
		seed = PCG_Hash(seed);

		return static_cast<float>(seed) / static_cast<float>(std::numeric_limits<uint32_t>::max());
	}

	inline glm::vec3 InUnitSphere(uint32_t& seed)
	{
		return glm::normalize(glm::vec3(
			RandomFloat(seed) * 2.0f - 1.0f,
			RandomFloat(seed) * 2.0f - 1.0f,
			RandomFloat(seed) * 2.0f - 1.0f
		));
	}

	// ── GGX Microfacet BRDF ──

	inline void BuildONB(const glm::vec3& n, glm::vec3& u, glm::vec3& v, glm::vec3& w)
	{
		w = n;
		float sign = (w.z > 0.0f) ? 1.0f : -1.0f;
		float a = -1.0f / (sign + w.z);
		float b = w.x * w.y * a;
		u = glm::vec3(1.0f + sign * w.x * w.x * a, sign * b, -sign * w.x);
		v = glm::vec3(b, sign + w.y * w.y * a, -w.y);
	}

	inline glm::vec3 FresnelSchlick(float cosTheta, const glm::vec3& F0)
	{
		float t = glm::max(1.0f - cosTheta, 0.0f);
		float t5 = t * t * t * t * t;
		return F0 + (glm::vec3(1.0f) - F0) * t5;
	}

	inline float GGX_D(float NdotH, float a)
	{
		float a2 = a * a;
		float d = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
		return a2 / (kPi * d * d);
	}

	inline float GGX_G1(float NdotV, float a)
	{
		float a2 = a * a;
		float denom = NdotV + glm::sqrt(NdotV * NdotV * (1.0f - a2) + a2);
		return 2.0f * NdotV / glm::max(denom, kDenominatorEpsilon);
	}

	inline float GGX_G(float NdotL, float NdotV, float a)
	{
		return GGX_G1(NdotL, a) * GGX_G1(NdotV, a);
	}

	inline glm::vec3 SampleGGX_VNDF(const glm::vec3& V, float a, float r1, float r2)
	{
		glm::vec3 Vh = glm::normalize(glm::vec3(a * V.x, a * V.y, V.z));
		glm::vec3 up(0.0f, 0.0f, 1.0f);
		glm::vec3 T1 = (Vh.z < 0.9999f) ? glm::normalize(glm::cross(up, Vh)) : glm::vec3(1.0f, 0.0f, 0.0f);
		glm::vec3 T2 = glm::cross(Vh, T1);

		float r = glm::sqrt(r1);
		float phi = 2.0f * kPi * r2;
		float t1 = r * glm::cos(phi);
		float t2 = r * glm::sin(phi);
		float s = 0.5f * (1.0f + Vh.z);
		t2 = (1.0f - s) * glm::sqrt(glm::max(1.0f - t1 * t1, 0.0f)) + s * t2;

		glm::vec3 Nh = t1 * T1 + t2 * T2 + glm::sqrt(glm::max(1.0f - t1*t1 - t2*t2, 0.0f)) * Vh;
		return glm::normalize(glm::vec3(a * Nh.x, a * Nh.y, glm::max(0.0f, Nh.z)));
	}
}
