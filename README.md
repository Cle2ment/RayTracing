# Ray Tracing: Accelerated RT Demo

[English](/README.md) | [中文](/docs/README_zh-CN.md) | [Français](/docs/README_fr-FR.md)

![Static Badge](https://img.shields.io/badge/Language-C++23-blue?logo=cplusplus)
![Static Badge](https://img.shields.io/badge/GPU-CUDA-green?logo=nvidia)
![Static Badge](https://img.shields.io/badge/CPU-ISPC-cyan?logo=intel)
<br>
![Static Badge](https://img.shields.io/badge/Build-Xmake-brightgreen?logo=data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA2NCA2NCI+PHBvbHlnb24gcG9pbnRzPSI2NCw0IDY0LDYyIDYyLDY0IDEsNjQgMCw2MyAwLDQwIDYwLDMiIGZpbGw9IiNmZmYiLz48L3N2Zz4=)
![Static Badge](https://img.shields.io/badge/Project-Premake-blue?logo=lua)
[![Build](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml/badge.svg)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml)
![Static Badge](https://img.shields.io/badge/License-MIT-green)

## Overview

A real-time interactive path tracer built with C++23 on the [Walnut](https://github.com/TheCherno/Walnut) application framework. **GPU-accelerated via NVIDIA CUDA** and **CPU-accelerated via Intel ISPC** — the entire path tracing pipeline runs on the GPU when CUDA is available, with CPU SIMD fallback via ISPC (AVX2/AVX-512).

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
| Path Tracing (5 bounces) | Lambertian diffuse BRDF | Lambertian diffuse BRDF |
| Random Number Generation | PCG Hash | PCG Hash (`__device__`) |
| Russian Roulette | After 3 bounces | After 3 bounces |
| Display | Walnut::Image (Vulkan) | Walnut::Image (Vulkan) via D2H copy |

**GPU Kernel Layout**: 16×16 thread blocks, one CUDA thread per pixel.

## Requirements

- **NVIDIA GPU** (optional) with Compute Capability ≥ 7.5
  - sm_75: GTX 16xx, RTX 20xx
  - sm_86: RTX 30xx
  - sm_89: RTX 40xx
  - sm_120: RTX 50xx
- **CUDA Toolkit 12.0+** (optional, 13.x recommended)
- **Vulkan SDK 1.4+**
- **Visual Studio 2026** (or 2022) with C++23 support
- **ISPC** — auto-downloaded by `scripts\Setup.bat`, no manual install needed

## Quick Start

```bash
# Clone with submodules
git clone --recursive https://github.com/Cle2ment/RayTracing.git
cd RayTracing

# One-click setup & build (auto-downloads ISPC)
scripts\Setup.bat

# Or manual build:
xmake f -m release && xmake build
xmake run RayTracing
```

## File Structure

```
RayTracing/
├── src/
│   ├── WalnutApp.cpp          # Entry point, ImGui UI, scene setup
│   ├── Renderer.h/cpp         # Renderer (CPU/GPU/ISPC dispatch)
│   ├── Camera.h/cpp           # FPS camera, ray direction pre-computation
│   ├── Ray.h                  # Ray struct
│   ├── Scene.h                # Material, Sphere, Scene data
│   ├── PathTracer.ispc        # ISPC SIMD path tracing kernel
│   ├── CUDATypes.cuh          # GPU data structures
│   ├── CUDARenderer.cuh       # GPU kernels + device functions
│   ├── CUDARenderer.cu        # CUDA host wrappers (C linkage)
│   └── CUDARenderer.h         # Host C++ interface + packing helpers
├── xmake.lua                   # Build config (CUDA + ISPC detection)
├── scripts/Setup.bat          # One-click project generation
└── .github/workflows/         # CI/CD (CUDA 13.3 + Vulkan)
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
| Viewport is black | CUDA architecture mismatch | Check GPU supports `sm_XX` in `xmake.lua` |
| `no kernel image is available` | nvcc not targeting your GPU | Add `-gencode=arch=compute_XX,code=sm_XX` |
| `CUDA_PATH` not set | Environment variable missing | System → Environment Variables → `CUDA_PATH` → CUDA dir |
| `.cu` files not compiled | `CUDA_PATH` not set at generation time | Restart terminal, `echo %CUDA_PATH%`, re-run `Setup.bat` |
| `CUDARenderer_*` undefined | Object file not linked | Check `linkoptions { "$(IntDir)CUDARenderer.obj" }` |
| `invalid value 'C++23'` | Old premake5 bundled with Walnut | Re-run `scripts\Setup.bat` for premake5 5.0.0-beta8 |

## License

MIT License. See [LICENSE](LICENSE) for details.
