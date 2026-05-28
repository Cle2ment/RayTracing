# Copilot Instructions

## General Guidelines
- Full project docs: `AGENTS.md` — read it first
- PascalCase classes/structs, camelCase methods, `m_` prefix for members
- Compile with MSVC + NVCC (premake5 vs2026)

## CRITICAL Rules
1. **NEVER modify Walnut library files** — extend via `ExampleLayer` subclass only
2. **NEVER change `GPUPacked*` struct layout** (`CUDARenderer.h:78-93`) without updating `GPUMaterial`/`GPUSphere` in `CUDATypes.cuh:16-30` — memory layouts must match exactly
3. **NEVER suppress CUDA errors** — always check `cudaDeviceSynchronize()` return
4. **NEVER use `--remote` on `git submodule update`** — vendor deps pinned by Walnut

## Build
```bash
scripts\Setup.bat                       # Generate VS2026 solution
msbuild RayTracing.slnx /p:Config=Release /p:Platform=x64 /m  # Build
```

## GPU/CPU Paths
- `#ifdef WL_CUDA` gates all CUDA code; `#ifndef WL_CUDA` is CPU-only
- Scene→GPU upload: `Renderer::UploadSceneToGPU()` (called every frame)
- Emission order (BOTH paths): `light += contribution * emission` BEFORE `contribution *= albedo`

## Tests
- No test framework exists — verify changes via visual inspection of rendered output