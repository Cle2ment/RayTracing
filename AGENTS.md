# RayTracing — Project Knowledge Base

**Generated:** 2026-05-28
**Commit:** `8c8944b`
**Branch:** `master`

## OVERVIEW

GPU-accelerated real-time path tracer — C++20 + CUDA + Vulkan via Walnut (ImGui) framework. Single light source: emissive materials. No sky/env light.

## STRUCTURE

```
RayTracing/
├── RayTracing/src/        # All application code (11 files, ~1600 loc)
│   ├── WalnutApp.cpp      # Entry: scene setup, ImGui UI, render dispatch
│   ├── Renderer.cpp/h     # Renderer: CPU path tracer + GPU dispatch + scene packing
│   ├── Camera.cpp/h       # Camera: ray direction pre-computation, FPS controls
│   ├── CUDARenderer.cuh/.cu/.h  # GPU: kernels, device code, host wrappers
│   ├── CUDATypes.cuh      # GPU struct definitions (must match CUDARenderer.h)
│   ├── Scene.h            # Material, Sphere, Scene data structures
│   └── Ray.h              # Ray struct (origin + direction)
├── premake5.lua           # Root workspace: includes Walnut + RayTracing
├── RayTracing/premake5.lua # CUDA detection, NVCC prebuild, arch targets
├── scripts/Setup.bat      # premake5 vs2026 generator
├── .github/workflows/     # CI: CUDA 12.8 + Vulkan SDK + MSVC build
└── Walnut/                # Submodule (TheCherno/Walnut) — do NOT modify vendor/
```

## WHERE TO LOOK

| Task | Location | Notes |
|------|----------|-------|
| ImGui scene controls | `WalnutApp.cpp:88-183` | ColorEdit3, DragFloat for materials; DragFloat3 for spheres |
| CPU path tracing | `Renderer.cpp:217-274` | `PerPixel()` — `#ifndef WL_CUDA` block |
| GPU path tracing | `CUDARenderer.cuh:154-245` | `PerPixel()` device function |
| GPU kernel launch | `CUDARenderer.cu:175-216` | `RenderKernel` 16×16 blocks |
| Scene→GPU upload | `Renderer.cpp:351-388` | Packing from glm→GPU structs, then `cudaMemcpy` |
| Material definition | `Scene.h:7-19` | Albedo, Roughness, Metallic, EmissionColor, EmissionPower |
| Camera ray generation | `Camera.cpp:137-166` | Pre-computed ray directions per pixel |
| CUDA architecture targets | `RayTracing/premake5.lua:22-27` | sm_75, sm_86, sm_89, sm_120 |
| Build config | `RayTracing/premake5.lua:29-146` | WL_CUDA define, NVCC prebuild commands |

## CODE MAP

| Symbol | Type | File:Lines | Role |
|--------|------|------------|------|
| `Renderer::PerPixel()` | method | `Renderer.cpp:217` | CPU path trace per pixel |
| `Renderer::RenderGPU()` | method | `Renderer.cpp:390` | GPU render dispatch |
| `::PerPixel()` | device fn | `CUDARenderer.cuh:154` | GPU path trace per pixel |
| `::RenderKernel()` | kernel | `CUDARenderer.cuh:251` | Main CUDA kernel |
| `Renderer::TraceRay()` | method | `Renderer.cpp:276` | CPU ray-sphere intersection |
| `::TraceRay()` | device fn | `CUDARenderer.cuh:103` | GPU ray-sphere intersection |
| `Material` | struct | `Scene.h:7` | Albedo + emission properties |
| `Sphere` | struct | `Scene.h:21` | Position, radius, material index |
| `GPUPackedMaterial` | struct | `CUDARenderer.h:86` | Host-side GPU material (must match GPUMaterial) |
| `GPUMaterial` | struct | `CUDATypes.cuh:23` | Device-side material |
| `Camera::OnUpdate()` | method | `Camera.cpp:21` | FPS camera controls |
| `Camera::RecalculateRayDirections()` | method | `Camera.cpp:137` | Pre-compute ray grid |

## CONVENTIONS

### Conditional Compilation
- `#ifdef WL_CUDA` — gates ALL CUDA code. Defined in premake when `CUDA_PATH` is set.
- `#ifdef WL_DEBUG` / `WL_RELEASE` / `WL_DIST` — per-configuration defines.
- `#ifndef WL_CUDA` — CPU-only path (`Renderer.cpp`).

### Memory Layout Contracts (CRITICAL)
- `GPUPackedMaterial` (host, `CUDARenderer.h:86`) ↔ `GPUMaterial` (device, `CUDATypes.cuh:23`) — 36 bytes, identical layout
- `GPUPackedSphere` (host, `CUDARenderer.h:78`) ↔ `GPUSphere` (device, `CUDATypes.cuh:16`) — 20 bytes, identical layout
- `float3` (host, `CUDARenderer.h:72`) ↔ CUDA `float3` — both 12 bytes, 4-byte alignment
- **ANY layout change must update BOTH sides simultaneously.**

### Scene Data Flow
- ImGui modifies `Scene` directly via structured bindings (`glm::value_ptr`)
- CPU path reads `m_ActiveScene` pointer → always gets latest data
- GPU path: `UploadSceneToGPU()` called every frame → `cudaMemcpy` host→device
- GPU realloc only when sphere/material counts change (`CUDARenderer.cu:137-167`)

### Emission Integration Order
- Both CPU and GPU: `light += contribution * emission` **BEFORE** `contribution *= albedo`
- Matches path tracing integral: Le · ∏(previous BSDFs)
- `Renderer.cpp:259-260` (CPU), `CUDARenderer.cuh:193-200` (GPU)

### Naming
- PascalCase for classes/structs, camelCase for methods/members
- `m_` prefix for member variables (`m_Camera`, `m_FrameIndex`)
- GPU types prefixed with `GPU` (`GPUSphere`, `GPUMaterial`)
- Host-side GPU packing structs prefixed with `GPUPacked`

## ANTI-PATTERNS

- **NEVER modify Walnut library files** — extend via `ExampleLayer` subclass
- **NEVER suppress CUDA errors** — always check `cudaDeviceSynchronize()` return
- **NEVER assume scene data is synced to GPU** — `UploadSceneToGPU()` must be called
- **NEVER change `GPUPacked*` struct layout without updating `GPUMaterial`/`GPUSphere` in `CUDATypes.cuh`**
- **NEVER use `--remote` on `git submodule update`** — vendor deps are pinned by Walnut

## UNIQUE STYLES

- Structured bindings for sphere/material field access: `auto& [Position, Radius, MaterialIndex] = spheres[i]`
- Box-drawing comment separators: `// ─── Section Title ───`
- No sky/environment light — all illumination from emissive materials only
- `m_NeedsRender` flag for conditional re-render (ImGui change detection)
- `m_SceneDirty` removed (May 2026) — ImGui modifies Scene directly, dirty tracking unreliable

## COMMANDS

```bash
# Generate VS2026 solution
scripts\Setup.bat

# Build (Release)
msbuild RayTracing.slnx /p:Configuration=Release /p:Platform=x64 /m

# Build (Debug, with CUDA debug symbols)
msbuild RayTracing.slnx /p:Configuration=Debug /p:Platform=x64 /m
```

## NOTES

- **No tests exist** — verification is manual (visual inspection of rendered output)
- **Single light source** — only emissive materials illuminate the scene; no env/sky light
- **GPU accumulation** — old samples persist in buffer; use Reset button or disable Accumulate for clean re-render after ImGui changes
- **CUDA arch fallback** — if nvcc fails, check GPU compute capability matches `sm_XX` in `premake5.lua:22-27`
- **Walnut submodule** — upstream: `https://github.com/TheCherno/Walnut`; vendor deps pinned to tested commits
