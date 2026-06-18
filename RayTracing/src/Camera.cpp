#include "Camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "Peanut/Input/Input.h"

using namespace Peanut;

Camera::Camera(const float verticalFOV, const float nearClip, const float farClip)
	: m_VerticalFOV(verticalFOV), m_NearClip(nearClip), m_FarClip(farClip)
{
	m_ForwardDirection = glm::vec3(0, 0, -1);
	m_Position = glm::vec3(0, 0, 6);
}

bool Camera::OnUpdate(const float ts)
{
	const glm::vec2 mousePos = Input::GetMousePosition();
	const glm::vec2 delta = (mousePos - m_LastMousePosition) * kMouseSensitivity;

	m_LastMousePosition = mousePos;

	if (!Input::IsMouseButtonDown(MouseButton::Right))
	{
		Input::SetCursorMode(CursorMode::Normal);
		return false;
	}

	Input::SetCursorMode(CursorMode::Locked);

	bool moved = false;

	constexpr glm::vec3 upDirection(0.0f, 1.0f, 0.0f);
	const glm::vec3 rightDirection = glm::cross(m_ForwardDirection, upDirection);

	constexpr float speed = kMoveSpeed;

	// Movement
	if (Input::IsKeyDown(KeyCode::W))
	{
		m_Position += m_ForwardDirection * speed * ts;
		moved = true;
	}
	else if (Input::IsKeyDown(KeyCode::S))
	{
		m_Position -= m_ForwardDirection * speed * ts;
		moved = true;
	}
	if (Input::IsKeyDown(KeyCode::A))
	{
		m_Position -= rightDirection * speed * ts;
		moved = true;
	}
	else if (Input::IsKeyDown(KeyCode::D))
	{
		m_Position += rightDirection * speed * ts;
		moved = true;
	}
	if (Input::IsKeyDown(KeyCode::Q))
	{
		m_Position -= upDirection * speed * ts;
		moved = true;
	}
	else if (Input::IsKeyDown(KeyCode::E))
	{
		m_Position += upDirection * speed * ts;
		moved = true;
	}

	// Rotation
	if (delta.x != 0.0f || delta.y != 0.0f)
	{
		const float pitchDelta = delta.y * GetRotationSpeed();
		const float yawDelta = delta.x * GetRotationSpeed();

		const glm::quat q = glm::normalize(
			glm::cross(
				glm::angleAxis(-pitchDelta, rightDirection),
				glm::angleAxis(-yawDelta, glm::vec3(0.f, 1.0f, 0.0f))
			));

		m_ForwardDirection = glm::rotate(q, m_ForwardDirection);

		moved = true;
	}

	if (moved)
	{
		RecalculateView();
		RecalculateRayDirections();
	}

	return moved;
}

void Camera::OnResize(const uint32_t width, const uint32_t height)
{
	if (width == m_ViewportWidth && height == m_ViewportHeight)
		return;

	m_ViewportWidth = width;
	m_ViewportHeight = height;

	RecalculateProjection();
	RecalculateRayDirections();
}

float Camera::GetRotationSpeed() noexcept
{
	return kRotationSpeed;
}

void Camera::RecalculateProjection()
{
	m_Projection = glm::perspectiveFov(
			glm::radians(m_VerticalFOV), 
			static_cast<float>(m_ViewportWidth), 
			static_cast<float>(m_ViewportHeight), 
			m_NearClip, 
			m_FarClip
		);
	m_InverseProjection = glm::inverse(m_Projection);
}

void Camera::RecalculateView()
{
	m_View = glm::lookAt(
		m_Position, 
		m_Position + m_ForwardDirection, 
		upDirection
	);

	m_InverseView = glm::inverse(m_View);
}

void Camera::RecalculateRayDirections()
{
	m_RayDirections.resize(
		static_cast<size_t>(m_ViewportWidth) * static_cast<size_t>(m_ViewportHeight)
	);

	for (uint32_t y = 0; y < m_ViewportHeight; y++)
	{
		for (uint32_t x = 0; x < m_ViewportWidth; x++)
		{
			glm::vec2 coordinate = {
				(static_cast<float>(x) + 0.5f) / static_cast<float>(m_ViewportWidth),
				(static_cast<float>(y) + 0.5f) / static_cast<float>(m_ViewportHeight)
			};
			coordinate = coordinate * 2.0f - 1.0f; // -1 -> 1

			glm::vec4 target = m_InverseProjection * glm::vec4(coordinate.x, coordinate.y, 1, 1);
			const auto rayDirection = glm::vec3(
				m_InverseView * glm::vec4(
					glm::normalize(glm::vec3(target) / target.w), 
					0
				)
			);
			// World space

			m_RayDirections[x + y * m_ViewportWidth] = rayDirection;
		}
	}
}
