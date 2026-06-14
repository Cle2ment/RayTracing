# Ray Tracing: Accelerated RT Demo

[English](/README.md) | [中文](/docs/README_zh-CN.md) | [Français](/docs/README_fr-FR.md)

![Static Badge](https://img.shields.io/badge/License-MIT-green)
<br/>
![Static Badge](https://img.shields.io/badge/Language-C++23-00599C?logo=cplusplus)
![Static Badge](https://img.shields.io/badge/GPU-CUDA-76B900?logo=nvidia)
![Static Badge](https://img.shields.io/badge/CPU_Acceleration-ISPC-0071C5?logo=intel)
![Static Badge](https://img.shields.io/badge/Project-Xmake-brightgreen?logo=data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA2NCA2NCI+PHBvbHlnb24gcG9pbnRzPSI2NCw0IDY0LDYyIDYyLDY0IDEsNjQgMCw2MyAwLDQwIDYwLDMiIGZpbGw9IiNmZmYiLz48L3N2Zz4=)
<br/>
![Static Badge](https://img.shields.io/badge/Auto_Translation-Deepseek-5786FE?logo=deepseek)
<br/>
[![Build](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml/badge.svg)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml)

## Overview

A real-time interactive path tracer built with C++23 on the [Peanut](https://github.com/Cle2ment/Peanut) application framework. **GPU-accelerated via NVIDIA CUDA** and **CPU-accelerated via Intel ISPC** — the entire path tracing pipeline runs on the GPU when CUDA is available, with CPU SIMD fallback via ISPC (AVX2/AVX-512).

### Rendering Backends

| Backend | Acceleration | When Used |
|---------|-------------|-----------|
| **CUDA GPU** | NVIDIA GPU (SM 7.5+) | `CUDA_PATH` detected |
| **ISPC CPU** | AVX2 + AVX-512 SIMD | `vendor/ispc/bin/ispc.exe` detected |
| **C++ CPU** | `std::execution::par` multi-threaded | Fallback |

### Architecture

| Component | CPU | GPU (CUDA) |
|-----------|-----|------------|
| Ray Generation | ISPC `foreach` or `std::execution::par` | CUDA kernel — one thread per pixel |
| Ray-Sphere Intersection | Brute-force loop | `__device__` function |
| Path Tracing (5 bounces) | GGX Microfacet BRDF | GGX Microfacet BRDF |
| Random Number Generation | PCG Hash | PCG Hash (`__device__`) |
| Russian Roulette | After 3 bounces | After 3 bounces |
| Display | Peanut::Image (Vulkan) | Peanut::Image (Vulkan) via D2H copy |

**GPU Kernel Layout**: 16×16 thread blocks, one CUDA thread per pixel.

## Quick Start

```bash
# Clone with submodules (RECOMMENDED)
git clone --recursive https://github.com/Cle2ment/RayTracing.git
cd RayTracing

# If you cloned without --recursive, initialize submodules manually:
git submodule update --init --recursive

# One-click setup, build, and VS solution generation
scripts\Setup.bat

# Run the path tracer
xmake run RayTracing
```

## Build & Run

### Prerequisites

| Dependency | Required | Notes |
|-----------|----------|-------|
| [Visual Studio 2026](https://visualstudio.microsoft.com/) (or 2022) | ✅ | C++ desktop workload, MSVC v143+ |
| [Vulkan SDK 1.4+](https://vulkan.lunarg.com/) | ✅ | Set `VULKAN_SDK` environment variable |
| [CUDA Toolkit 12.0+](https://developer.nvidia.com/cuda-downloads) | Optional | Set `CUDA_PATH`; auto-detected by xmake |
| [ISPC](https://ispc.github.io/) | Optional | Auto-downloaded by `Setup.bat` → `vendor/ispc/` |
| [.NET SDK](https://dotnet.microsoft.com/) | Optional | Required only for `.sln` → `.slnx` migration |
| [xmake](https://xmake.io/) | Auto | `Setup.bat` uses xmake; install globally for CLI usage |

### Method 1: One-Click (Setup.bat)

```bash
scripts\Setup.bat
```

This single command handles everything:

1. Checks Peanut submodule — auto-initializes if missing
2. Configures xmake (`xmake f -m release`)
3. Builds all targets (`xmake build`) — Peanut.lib + RayTracing.exe
4. Generates Visual Studio solution (`xmake project -k vsxmake`)
5. Converts `.sln` → `.slnx` via `dotnet sln migrate`

**Output**:

- `build/windows/x64/release/RayTracing.exe`
- `vsxmake2026/RayTracing.slnx` — open this in Visual Studio

### Method 2: CLI Build

```bash
# Configure and build (Release)
xmake f -m release
xmake build

# Run
xmake run RayTracing

# Debug build
xmake f -m debug
xmake build
```

### Method 3: Visual Studio

```bash
# Generate VS solution (after initial build)
xmake project -k vsxmake -y -m release

# Convert to .slnx
dotnet sln vsxmake2026\RayTracing.sln migrate
```

Open `vsxmake2026\RayTracing.slnx` in Visual Studio, set RayTracing as startup project, and press F5.

> **Note**: After editing `xmake.lua`, re-run `xmake project -k vsxmake -y -m release && dotnet sln vsxmake2026\RayTracing.sln migrate` to refresh the VS project files.

### Build Matrix

| Command | CUDA | ISPC | Output |
|---------|------|------|--------|
| `xmake f -m release && xmake build` | Auto-detect | Auto-detect | `build/windows/x64/release/RayTracing.exe` |
| `xmake f -m debug && xmake build` | Auto-detect | Auto-detect | `build/windows/x64/debug/RayTracing.exe` |
| `xmake run RayTracing` | — | — | Runs the built executable |

## File Structure

```
RayTracing/
├── RayTracing/src/             # Application source
│   ├── PeanutApp.cpp           # Entry point, ImGui UI, scene setup
│   ├── Renderer.h/cpp          # Renderer (CPU/GPU/ISPC dispatch)
│   ├── Camera.h/cpp            # FPS camera, ray direction pre-computation
│   ├── Ray.h                   # Ray struct
│   ├── Scene.h                 # Material, Sphere, Scene data
│   ├── PathTracer.ispc         # ISPC SIMD path tracing kernel
│   ├── CUDATypes.cuh           # GPU data structures
│   ├── CUDARenderer.cuh        # GPU kernels + device functions
│   ├── CUDARenderer.cu         # CUDA host wrappers (C linkage)
│   ├── CUDARenderer.h          # Host C++ interface + packing helpers
│   ├── VkCUDAInterop.h/cpp     # Vulkan-CUDA zero-copy memory sharing
│   └── OptiXDenoiser.h/cpp     # OptiX AI denoiser integration
├── xmake.lua                   # Build config (CUDA + ISPC detection)
├── scripts/
│   └── Setup.bat               # One-click build + solution generation
├── Peanut/                     # Git submodule (independent fork, modifiable for general-purpose improvements)
│   ├── Peanut/src/             # Peanut framework
│   ├── vendor/GLFW/            # GLFW windowing
│   ├── vendor/imgui/           # ImGui UI library
│   └── vendor/glm/             # GLM math library
├── Directory.Build.props       # VS IntelliSense config (auto-discovered by MSBuild)
└── .github/workflows/          # CI/CD (build, release, translation)
```

## Key Bindings

| Key | Action |
|-----|--------|
| Right Mouse + Drag | Rotate camera |
| W/A/S/D | Move camera |
| Q/E | Move down/up |
| Render button | Trigger re-render |
| Accumulate | Toggle progressive rendering |
| Reset | Clear accumulation buffer |

## Demonstration

![Random Sampling](/screenshots/example-random.png)
![Accumulated](/screenshots/example-accumulate.png)

## Troubleshooting

| Symptom | Cause | Solution |
|---------|-------|----------|
| `Peanut\Peanut\src\...` not found | Submodule not initialized | `git submodule update --init --recursive` |
| `.vcxproj` not found in VS | `vsxmake2026\RayTracing.slnx` is stale | Re-run `scripts\Setup.bat` or `xmake project -k vsxmake` then `dotnet sln vsxmake2026\RayTracing.sln migrate` |
| Viewport is black | CUDA architecture mismatch | Check GPU supports `sm_XX` in `xmake.lua` |
| `no kernel image is available` | nvcc not targeting your GPU | Add matching `add_cugencodes("compute_XX", "sm_XX")` in `xmake.lua` |
| `CUDA_PATH` not set / `.cu` not compiled | Environment variable missing | Set `CUDA_PATH` in System Environment Variables, restart terminal |
| `cannot match add_files("Peanut\Peanut\src\**.cpp")` | `git submodule update --init` not run | See first row above |
| ISPC not found (no SIMD) | ISPC not in `vendor/ispc/` | `Setup.bat` auto-downloads it; re-run if needed |
| `dotnet sln migrate` fails | .NET SDK not installed | Install [.NET SDK](https://dotnet.microsoft.com/) or open `vsxmake2026\RayTracing.sln` directly |

## License

MIT License. See [LICENSE](LICENSE) for details.

## Copyright

Copyright (c) 2026 Cle2ment
