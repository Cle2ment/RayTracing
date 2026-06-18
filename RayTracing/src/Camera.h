#pragma once

#include <glm/glm.hpp>
#include <vector>

class Camera
{
public:
	Camera(float verticalFOV, float nearClip, float farClip);

	bool OnUpdate(float ts);
	void OnResize(uint32_t width, uint32_t height);

	[[nodiscard]] const glm::mat4& GetProjection() const noexcept
	{ return m_Projection; }
	[[nodiscard]] const glm::mat4& GetInverseProjection() const noexcept
	{ return m_InverseProjection; }
	[[nodiscard]] const glm::mat4& GetView() const noexcept
	{ return m_View; }
	[[nodiscard]] const glm::mat4& GetInverseView() const noexcept
	{ return m_InverseView; }

	[[nodiscard]] const glm::vec3& GetPosition() const noexcept
	{ return m_Position; }
	[[nodiscard]] const glm::vec3& GetDirection() const noexcept
	{ return m_ForwardDirection; }

	[[nodiscard]] const std::vector<glm::vec3>& GetRayDirections() const noexcept
	{ return m_RayDirections; }

	static constexpr float kRotationSpeed   = 0.3f;
	static constexpr float kMouseSensitivity = 0.002f;
	static constexpr float kMoveSpeed        = 5.0f;
	static constexpr glm::vec3 kUpDirection  = {0.0f, 1.0f, 0.0f};

	static float GetRotationSpeed() noexcept;
private:
	void RecalculateProjection();
	void RecalculateView();
	void RecalculateRayDirections();

private:
	glm::mat4 m_Projection{ 1.0f };
	glm::mat4 m_View{ 1.0f };
	glm::mat4 m_InverseProjection{ 1.0f };
	glm::mat4 m_InverseView{ 1.0f };

	float m_VerticalFOV = 45.0f;
	float m_NearClip = 0.1f;
	float m_FarClip = 100.0f;

	glm::vec3 m_Position{ 0.0f, 0.0f, 0.0f };
	glm::vec3 m_ForwardDirection{ 0.0f, 0.0f, 0.0f };

	// Cached ray directions
	std::vector<glm::vec3> m_RayDirections;

	glm::vec2 m_LastMousePosition{ 0.0f, 0.0f };

	uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;
};
