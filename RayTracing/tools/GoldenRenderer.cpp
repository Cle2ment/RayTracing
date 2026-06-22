// GoldenRenderer — headless CPU path tracer for CI golden image generation
// Builds the default scene, renders N frames via CPUBackend, saves PNG.

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "CPUBackend.h"
#include "Camera.h"
#include "Scene.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static Scene CreateDefaultScene()
{
	Scene scene;

	// Material 0: Pink diffuse sphere
	{
		auto& mat = scene.Materials.emplace_back();
		mat.Albedo = { 1.0f, 0.0f, 1.0f };
		mat.Roughness = 0.2f;
		mat.Metallic = 0.0f;
	}

	// Material 1: Blue metallic ground
	{
		auto& mat = scene.Materials.emplace_back();
		mat.Albedo = { 0.2f, 0.3f, 1.0f };
		mat.Roughness = 0.1f;
		mat.Metallic = 0.0f;
	}

	// Material 2: Orange emissive sphere
	{
		auto& mat = scene.Materials.emplace_back();
		mat.Albedo = { 0.8f, 0.5f, 0.2f };
		mat.Roughness = 0.1f;
		mat.Metallic = 0.0f;
		mat.EmissionColor = mat.Albedo;
		mat.EmissionPower = 2.0f;
	}

	// Sphere 0: Pink center
	scene.Spheres.push_back({ { 0.0f, 0.0f, 0.0f }, 1.0f, 0 });

	// Sphere 1: Orange emissive right
	scene.Spheres.push_back({ { 2.0f, 0.0f, 0.0f }, 1.0f, 2 });

	// Sphere 2: Blue ground plane
	scene.Spheres.push_back({ { 0.0f, -101.0f, 0.0f }, 100.0f, 1 });

	scene.Version = 1;
	return scene;
}

static void PrintUsage(const char* prog)
{
	fprintf(stderr, "Usage: %s --output <path.png> [--width W] [--height H] [--frames N] [--bounces B]\n", prog);
	fprintf(stderr, "  --output   Output PNG path (required)\n");
	fprintf(stderr, "  --width    Image width  (default: 800)\n");
	fprintf(stderr, "  --height   Image height (default: 600)\n");
	fprintf(stderr, "  --frames   Accumulation frames (default: 100)\n");
	fprintf(stderr, "  --bounces  Max path bounces (default: 5)\n");
}

int main(int argc, char* argv[])
{
	const char* outputPath = nullptr;
	uint32_t width = 800;
	uint32_t height = 600;
	int totalFrames = 100;
	int maxBounces = 5;

	// Parse CLI arguments
	for (int i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "--output") == 0 && i + 1 < argc)
			outputPath = argv[++i];
		else if (strcmp(argv[i], "--width") == 0 && i + 1 < argc)
			width = static_cast<uint32_t>(atoi(argv[++i]));
		else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc)
			height = static_cast<uint32_t>(atoi(argv[++i]));
		else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc)
			totalFrames = atoi(argv[++i]);
		else if (strcmp(argv[i], "--bounces") == 0 && i + 1 < argc)
			maxBounces = atoi(argv[++i]);
		else
		{
			PrintUsage(argv[0]);
			return 1;
		}
	}

	if (!outputPath)
	{
		fprintf(stderr, "Error: --output is required\n");
		PrintUsage(argv[0]);
		return 1;
	}

	if (width == 0 || height == 0 || totalFrames <= 0 || maxBounces <= 0)
	{
		fprintf(stderr, "Error: width/height/frames/bounces must be positive\n");
		return 1;
	}

	// Setup
	Scene scene = CreateDefaultScene();
	Camera camera(45.0f, 0.1f, 100.0f);

	CPUBackend backend;
	backend.OnResize(width, height);
	camera.OnResize(width, height);

	std::vector<uint32_t> outputBuffer(static_cast<size_t>(width) * static_cast<size_t>(height));

	// Render accumulated frames
	fprintf(stderr, "Rendering %dx%d, %d frames, %d bounces...\n", width, height, totalFrames, maxBounces);
	for (int frame = 1; frame <= totalFrames; ++frame)
	{
		backend.Render(scene, camera, outputBuffer.data(), static_cast<uint32_t>(frame), maxBounces);
		if (frame % 25 == 0 || frame == totalFrames)
			fprintf(stderr, "  Frame %d/%d done\n", frame, totalFrames);
	}

	// Save PNG (RGBA8, flip vertically for stb which expects top-left origin)
	stbi_flip_vertically_on_write(1);
	if (!stbi_write_png(outputPath,
		static_cast<int>(width),
		static_cast<int>(height),
		4, // RGBA
		outputBuffer.data(),
		static_cast<int>(width * sizeof(uint32_t))))
	{
		fprintf(stderr, "Error: Failed to write PNG to %s\n", outputPath);
		return 1;
	}

	fprintf(stderr, "Golden image saved to %s\n", outputPath);
	return 0;
}
