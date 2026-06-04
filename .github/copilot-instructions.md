# Copilot Instructions

## General Guidelines
- Full project docs: `AGENTS.md` — read it first
- PascalCase classes/structs, camelCase methods, `m_` prefix for members
- Compile with MSVC + NVCC via xmake

## CRITICAL Rules
1. **NEVER modify Walnut library files** — extend via `ExampleLayer` subclass only
2. **NEVER change `GPUPacked*` struct layout** (`CUDARenderer.h:97-117`) without updating `GPUMaterial`/`GPUSphere` in `CUDATypes.cuh:16-38` — memory layouts must match exactly
3. **NEVER suppress CUDA errors** — always check `cudaMalloc`/`cudaMemcpy`/`cudaStreamCreate` return values
4. **NEVER use `--remote` on `git submodule update`** — vendor deps pinned by Walnut
5. **NEVER create/destroy VkCUDAInterop during CUDA kernel execution** — set up before render, destroy after sync
6. **NEVER assume scene data is synced to GPU** — increment `Scene::Version` on any scene change

## Build
```bash
scripts\Setup.bat                      # Generate VS2026 solution + initial build
xmake f -m release && xmake build      # Build (Release)
xmake run RayTracing                   # Run
```

## GPU/CPU Paths
- `#ifdef WL_CUDA` gates all CUDA code; `#ifndef WL_CUDA` is CPU-only
- Scene→GPU upload: `Renderer::UploadSceneToGPU()` (version-tracked, skipped when unchanged)
- Output: interop path → `vkCmdCopyBufferToImage` (zero-copy); legacy → D2H + SetData
- GGX BRDF replaces Lambertian on all three paths (CUDA, CPU, ISPC)

## Tests
- No test framework exists — verify changes via visual inspection of rendered output
