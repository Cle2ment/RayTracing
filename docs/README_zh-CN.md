以下是翻译后的中文版本README.md：

# Ray Tracing — NVIDIA GPU 加速路径追踪器

![Static Badge](https://img.shields.io/badge/Inspired_by-TheCherno-yellow?logo=Github)
![Static Badge](https://img.shields.io/badge/Language-C++20-blue?logo=cplusplus)
![Static Badge](https://img.shields.io/badge/GPU-CUDA-green?logo=nvidia)
![Static Badge](https://img.shields.io/badge/Built_by-Premake-blue?logo=lua)
![Static Badge](https://img.shields.io/badge/License-MIT-green)

## 描述

一个基于 C++20 和 Walnut 应用框架构建的实时交互式路径追踪器。**通过 NVIDIA CUDA 实现 GPU 加速** — 整个路径追踪管线（光线生成、求交、着色、累积）均在 GPU 上运行。当 CUDA 不可用时，自动回退到 CPU 多线程渲染。

### 架构

| 组件 | 多线程 CPU | GPU (CUDA) |
|-----------|---------------------|------------------------|
| 光线生成 | 跨 CPU 线程使用 `std::execution::par` | CUDA 内核 — 每像素一个线程 |
| 光线-球体求交 | 暴力循环（CPU） | `__device__` 函数（GPU） |
| 路径追踪（5 次反弹） | CPU 标量循环 | GPU SIMT 并行 |
| 随机数生成 | PCG 哈希（CPU） | PCG 哈希（GPU `__device__`） |
| 累积缓冲区 | 主机 `glm::vec4[]` | 设备 `float4[]` |
| 显示 | Walnut::Image（Vulkan） | Walnut::Image（Vulkan）通过 D2H 拷贝 |
| 俄罗斯轮盘 | CPU | GPU |

**GPU 内核布局**：16×16 线程块，每个像素分配一个 CUDA 线程。`RenderKernel` 对每个像素执行完整的路径追踪，包括光线-球体求交、俄罗斯轮盘终止、朗伯漫反射 BRDF 采样和渐进累积。

## 要求

- **NVIDIA GPU**（可选）Compute Capability ≥ 7.5（图灵 / 安培 / 艾达 / 布莱克韦尔）
  - sm_75: GTX 16xx, RTX 20xx
  - sm_86: RTX 30xx
  - sm_89: RTX 40xx
  - sm_120: RTX 50xx
- **CUDA Toolkit 12.0+**（可选，推荐 13.x，用于 GPU 加速）
- **Vulkan SDK 1.4+**
- **Visual Studio 2026**（或 2022，向下兼容）支持 C++20

## 如何构建

### 1. 克隆仓库
```bash
git clone --recursive https://github.com/Cle2ment/RayTracing.git
cd RayTracing
```

### 2. 安装依赖
- **[CUDA Toolkit](https://developer.nvidia.com/cuda-downloads)** — GPU 渲染所需。安装程序会自动设置 `CUDA_PATH` 环境变量。
- **[Vulkan SDK](https://vulkan.lunarg.com/)** — 显示所需。安装到默认位置。

### 3. 生成项目文件
```bash
cd scripts
Setup.bat
```
这会运行 Premake5 生成 Visual Studio 2026 解决方案文件。构建系统会自动检测 CUDA 并启用 GPU 加速。

### 4. 构建并运行
在 Visual Studio 2026 中打开 `RayTracing.slnx` 并构建（建议使用 Release 或 Dist 模式以获得最佳性能）。

### 无 CUDA 构建
如果未安装 CUDA Toolkit，项目将构建为仅 CPU 路径追踪器，使用 `std::execution::par`。构建系统仅在检测到 CUDA 时定义 `WL_CUDA`。

## 渲染管线

```
Camera (CPU)                    Scene (CPU)
    │                                │
    ├─ Ray Directions ─┐         ┌─── Spheres + Materials
    │                  │         │
    ▼                  ▼         ▼
┌─────────────────────────────────────────┐
│           CUDARenderer.cu               │
│                                         │
│  RenderKernel <<<grid, block>>>         │
│    └─ PerPixel(x, y)                    │
│         └─ for bounce in 0..5:          │
│              └─ TraceRay()              │
│                   └─ sphere loop (GPU)  │
│              └─ Russian Roulette        │
│              └─ Diffuse BRDF            │
│    └─ Accumulate + Tone Map             │
│    └─ Write RGBA8 output                │
└─────────────────────────────────────────┘
    │
    ▼ (cudaMemcpy D2H)
Walnut::Image (Vulkan) ──► Display
```

## 文件结构

```
RayTracing/
├── src/
│   ├── WalnutApp.cpp          # 入口点，ImGui UI，场景设置
│   ├── Renderer.h/cpp         # 渲染器类（CPU/GPU 调度）
│   ├── Camera.h/cpp           # 相机（CPU），光线方向生成
│   ├── Ray.h                  # 光线结构体（CPU）
│   ├── Scene.h                # 场景数据（CPU 球体 + 材质）
│   ├── CUDATypes.cuh          # GPU 数据结构
│   ├── CUDARenderer.cuh       # GPU 内核 + 设备函数
│   ├── CUDARenderer.cu        # CUDA 主机封装（C 链接）
│   └── CUDARenderer.h         # 主机 C++ 接口 + 打包辅助函数
├── premake5.lua               # 构建配置（+ CUDA 支持）
├── .github/workflows/build.yml # CI/CD 流水线
└── README.md
```

## CI/CD

[![Build (CUDA + Vulkan)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml/badge.svg)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml)

GitHub Actions 在每次推送和拉取请求时构建：
- **Windows Server 2022** 搭配 CUDA 12.8 + Vulkan SDK
- Debug 和 Release 配置
- Release 构建可下载构建产物

## 快捷键

| 键位 | 操作 |
|-----|--------|
| 右键拖动 | 旋转相机 |
| W/A/S/D | 移动相机 |
| Q/E | 向下/向上移动 |
| 渲染按钮 | 触发重新渲染 |
| 累积复选框 | 启用/禁用渐进渲染 |

## 演示

光线追踪项目仍在开发中。

以下是当前项目的演示效果。\
![Ray Tracing Default Example](https://github.com/BoningtonChen/RayTracing/blob/master/Materials/RayTracing-example01.png)
![Ray Tracing Example](https://github.com/BoningtonChen/RayTracing/blob/master/Materials/RayTracing-example02.png)

## 关于 WalnutAppTemplate
- 描述\
这是一个简单的 Walnut 应用模板——与 Walnut 仓库中的示例不同，它将 Walnut 作为外部子模块，更适合实际构建应用程序。更多细节请参阅 Walnut 仓库。
- 快速开始\
克隆完成后，您可以根据需要自定义 `premake5.lua` 和 `WalnutApp/premake5.lua` 文件（例如将名称从 "WalnutApp" 改为其他名称）。修改满意后，运行 `scripts/Setup.bat` 生成 Visual Studio 2022 解决方案/项目文件。您的应用位于 `WalnutApp/` 目录下，其中 `WalnutApp/src/WalnutApp.cpp` 包含一些基本示例代码以帮助您入门。建议修改该 WalnutApp 项目来创建您自己的应用程序，因为所有设置都已准备就绪。

## 常见问题

| 现象 | 原因 | 解决 |
|------|------|------|
| Viewport 全黑 | CUDA 架构不匹配 | 确认 GPU 型号，检查 `premake5.lua` 中 `cudaArchs` 是否包含对应 `sm_XX` |
| `no kernel image is available` | nvcc 未为目标 GPU 编译内核 | 添加对应 `-gencode=arch=compute_XX,code=sm_XX` |
| `CUDA_PATH` 未设置 | 环境变量缺失 | 系统属性 → 环境变量 → 新建 `CUDA_PATH`，指向 CUDA Toolkit 目录 |
| 构建时 `.cu` 文件未编译 | `CUDA_PATH` 未在生成时生效 | 重启终端，确认 `echo %CUDA_PATH%` 非空后重新 `Setup.bat` |
| 链接器报 `CUDARenderer_*` 未定义 | `CUDARenderer.obj` 未被链接 | 检查 `premake5.lua` 中 `linkoptions { "$(IntDir)CUDARenderer.obj" }` |

## 许可证
该项目使用 `MIT 许可证`。

## 版权
© Bonity, 2024