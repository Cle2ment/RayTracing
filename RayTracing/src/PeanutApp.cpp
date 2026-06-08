#include "Peanut/Application.h"
#include "Peanut/EntryPoint.h"

#include "Peanut/Image.h"
#include "Peanut/Timer.h"

#include "Renderer.h"
#include "Camera.h"

#include <glm/gtc/type_ptr.hpp>
#include <limits>

using namespace Peanut;

class ExampleLayer final : public Peanut::Layer
{
public:
	ExampleLayer()
		: m_Camera(45.0f, 0.1f, 100.0f)
	{

		auto& [
			pinkAlbedo, 
			pinkRoughness, 
			pinkMetallic, 
			pinkEmissionColor, 
			pinkEmissionPower
		] = m_Scene.Materials.emplace_back();
		pinkAlbedo = { 1.0f, 0.0f, 1.0f };
		pinkRoughness = 0.2f;

		auto& [
			blueAlbedo, 
			blueRoughness, 
			blueMetallic, 
			blueEmissionColor, 
			blueEmissionPower
		] = m_Scene.Materials.emplace_back();
		blueAlbedo = { 0.2f, 0.3f, 1.0f };
		blueRoughness = 0.1f;

		auto& [
			orangeAlbedo, 
			orangeRoughness, 
			orangeMetallic, 
			orangeEmissionColor, 
			orangeEmissionPower
		] = m_Scene.Materials.emplace_back();
		orangeAlbedo = { 0.8f, 0.5f, 0.2f };
		orangeRoughness = 0.1f;
		orangeEmissionColor = orangeAlbedo;
		orangeEmissionPower = 2.0f;

		{
			Sphere sphere;
			sphere.Position = { 0.0f, 0.0f, 0.0f };
			sphere.Radius = 1.0f;
			sphere.MaterialIndex = 0;
			m_Scene.Spheres.push_back(sphere);
		}
		
		{
			Sphere sphere;
			sphere.Position = { 2.0f, 0.0f, 0.0f };
			sphere.Radius = 1.0f;
			sphere.MaterialIndex = 2;
			m_Scene.Spheres.push_back(sphere);
		}

		{
			Sphere sphere;
			sphere.Position = { 0.0f, -101.0f, 0.0f };
			sphere.Radius = 100.0f;
			sphere.MaterialIndex = 1;
			m_Scene.Spheres.push_back(sphere);
		}

		m_Scene.Version = 1;  // Ensure initial scene state triggers GPU upload
	}

	virtual void OnUpdate(const float ts) override
	{
		if (m_Camera.OnUpdate(ts))
		{
			m_Renderer.ResetFrameIndex();
			m_Renderer.MarkRayDirsDirty();
			m_NeedsRender = true;
		}
	}

	virtual void OnUIRender() override
	{
		ImGui::Begin("Settings");

		ImGui::Text("Last render: %.3fms", m_LastRenderTime);
		if (ImGui::Button("Render"))
			m_NeedsRender = true;

		if (ImGui::Checkbox("Accumulate", &m_Renderer.GetSettings().Accumulate))
			m_NeedsRender = true;
		if (ImGui::Checkbox("Slow Random", &m_Renderer.GetSettings().SlowRandom))
			m_NeedsRender = true;
		if (ImGui::SliderInt("Max Bounces", &m_Renderer.GetSettings().MaxBounces, 1, 20))
			m_NeedsRender = true;
#ifdef PN_OPTIX
		if (ImGui::Checkbox("Denoise", &m_Renderer.GetSettings().EnableDenoising))
			m_NeedsRender = true;
#endif
#ifdef PN_CUDA
		if (ImGui::Checkbox("Vulkan-CUDA Interop", &m_Renderer.GetSettings().EnableInterop))
			m_NeedsRender = true;
#endif

		if (ImGui::Button("Reset"))
		{
			m_Renderer.ResetFrameIndex();
			m_NeedsRender = true;
		}

		ImGui::End();

		ImGui::Begin("Scene");

		bool changed = false;

		for (size_t i = 0; i < m_Scene.Spheres.size(); i++)
		{
			ImGui::PushID(static_cast<int>(i));

			auto& [Position, Radius, MaterialIndex] = m_Scene.Spheres[i];

			changed |= ImGui::DragFloat3(
				"Position", 
				glm::value_ptr(Position), 
				0.1f
			);
			changed |= ImGui::DragFloat(
				"Radius", 
				&Radius, 
				0.1f
			);
			changed |= ImGui::DragInt(
				"Material", 
				&MaterialIndex, 
				1.0f, 0, static_cast<int>(m_Scene.Materials.size()) - 1
				);

			ImGui::Separator();

			ImGui::PopID();
		}

		for (size_t i = 0; i < m_Scene.Materials.size(); i++)
		{
			ImGui::PushID(static_cast<int>(i));

			auto& [
				Albedo, 
				Roughness, 
				Metallic, 
				EmissionColor, 
				EmissionPower
			] = m_Scene.Materials[i];

			changed |= ImGui::ColorEdit3(
				"Albedo", 
				glm::value_ptr(Albedo)
			);
			changed |= ImGui::DragFloat(
				"Roughness", 
				&Roughness, 
				0.05f, 0.0f, 1.0f
			);
			changed |= ImGui::DragFloat(
				"Metallic",
				&Metallic,
				0.05f, 0.0f, 1.0f
			);
			changed |= ImGui::ColorEdit3(
				"Emission Color", 
				glm::value_ptr(EmissionColor)
			);
			changed |= ImGui::DragFloat(
				"Emission Power",
				&EmissionPower,
				0.05f, 0.0f, std::numeric_limits<float>::max()
			);

			ImGui::Separator();

			ImGui::PopID();
		}

		if (changed)
		{
			m_Scene.Version++;
			m_Renderer.ResetFrameIndex(); // Avoid old samples polluting the accumulation buffer
			m_NeedsRender = true;
		}

		ImGui::End();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

		ImGui::Begin("Viewport");

		uint32_t newWidth = static_cast<uint32_t>(ImGui::GetContentRegionAvail().x);
		uint32_t newHeight = static_cast<uint32_t>(ImGui::GetContentRegionAvail().y);
		if (newWidth != m_ViewportWidth || newHeight != m_ViewportHeight)
			m_NeedsRender = true;
		m_ViewportWidth = newWidth;
		m_ViewportHeight = newHeight;

		if (const auto image = m_Renderer.GetFinalImage())
			ImGui::Image(
				image->GetDescriptorSet(),
			{
				static_cast<float>(image->GetWidth()),
				static_cast<float>(image->GetHeight())
			},
				ImVec2(0, 1), ImVec2(1, 0)
			);

		ImGui::End();

		ImGui::PopStyleVar();

		if (m_NeedsRender)
			Render();
	}

	void Render()
	{
		Timer timer;

		m_Renderer.OnResize(m_ViewportWidth, m_ViewportHeight);
		m_Camera.OnResize(m_ViewportWidth, m_ViewportHeight);
		m_Renderer.Render(m_Scene, m_Camera);

		m_LastRenderTime = timer.ElapsedMillis();

		if (!m_Renderer.GetSettings().Accumulate)
			m_NeedsRender = false;
	}

private:
	Renderer m_Renderer;
	Camera m_Camera;
	Scene m_Scene;

	uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;

	float m_LastRenderTime = 0.0f;
	bool m_NeedsRender = true;
};

Peanut::Application* Peanut::CreateApplication(int argc, char** argv)
{
	Peanut::ApplicationSpecification spec;
	spec.Name = "Ray Tracing";

	auto app = new Peanut::Application(spec);
	app->PushLayer<ExampleLayer>();
			app->SetMenubarCallback([app] ()
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Exit"))
			{
				app->Close();
			}
			ImGui::EndMenu();
		}
	});
	return app;
}
