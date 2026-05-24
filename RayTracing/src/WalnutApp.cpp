#include "Walnut/Application.h"
#include "Walnut/EntryPoint.h"

#include "Walnut/Image.h"
#include "Walnut/Timer.h"

#include "Renderer.h"
#include "Camera.h"

#include <glm/gtc/type_ptr.hpp>
#include <limits>

using namespace Walnut;

class ExampleLayer final : public Walnut::Layer
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
	}

	virtual void OnUpdate(const float ts) override
	{
		if (m_Camera.OnUpdate(ts))
			m_Renderer.ResetFrameIndex();
	}

	virtual void OnUIRender() override
	{
		ImGui::Begin("Settings");

		ImGui::Text("Last render: %.3fms", m_LastRenderTime);
		if (ImGui::Button("Render"))
			Render();

		ImGui::Checkbox("Accumulate", &m_Renderer.GetSettings().Accumulate);
		ImGui::Checkbox("Slow Random", &m_Renderer.GetSettings().SlowRandom);

		if (ImGui::Button("Reset"))
			m_Renderer.ResetFrameIndex();

		ImGui::End();

		ImGui::Begin("Scene");

		for (size_t i = 0; i < m_Scene.Spheres.size(); i++)
		{
			ImGui::PushID(static_cast<int>(i));

			auto& [Position, Radius, MaterialIndex] = m_Scene.Spheres[i];

			ImGui::DragFloat3(
				"Position", 
				glm::value_ptr(Position), 
				0.1f
			);
			ImGui::DragFloat(
				"Radius", 
				&Radius, 
				0.1f
			);
			ImGui::DragInt(
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

			ImGui::ColorEdit3(
				"Albedo", 
				glm::value_ptr(Albedo)
			);
			ImGui::DragFloat(
				"Roughness", 
				&Roughness, 
				0.05f, 0.0f, 1.0f
			);
			ImGui::DragFloat(
				"Metallic",
				&Metallic,
				0.05f, 0.0f, 1.0f
			);
			ImGui::ColorEdit3(
				"Emission Color", 
				glm::value_ptr(EmissionColor)
			);
			ImGui::DragFloat(
				"Emission Power",
				&EmissionPower,
				0.05f, 0.0f, std::numeric_limits<float>::max()
			);

			ImGui::Separator();

			ImGui::PopID();
		}

		ImGui::End();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

		ImGui::Begin("Viewport");

		m_ViewportWidth = static_cast<uint32_t>(ImGui::GetContentRegionAvail().x);
		m_ViewportHeight = static_cast<uint32_t>(ImGui::GetContentRegionAvail().y);

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

		Render();
	}

	void Render()
	{
		Timer timer;

		m_Renderer.OnResize(m_ViewportWidth, m_ViewportHeight);
		m_Camera.OnResize(m_ViewportWidth, m_ViewportHeight);
		m_Renderer.Render(m_Scene, m_Camera);

		m_LastRenderTime = timer.ElapsedMillis();
	}

private:
	Renderer m_Renderer;
	Camera m_Camera;
	Scene m_Scene;

	uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;

	float m_LastRenderTime = 0.0f;
};

Walnut::Application* Walnut::CreateApplication(int argc, char** argv)
{
	Walnut::ApplicationSpecification spec;
	spec.Name = "Ray Tracing";

	auto app = new Walnut::Application(spec);
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
