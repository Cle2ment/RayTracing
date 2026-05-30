# 光线追踪 — NVIDIA GPU 加速路径追踪器

[English](/README.md) | [中文](/docs/README_zh-CN.md) | [Français](/docs/README_fr-FR.md)

![Static Badge](https://img.shields.io/badge/Inspired_by-TheCherno-yellow?logo=Github)
![Static Badge](https://img.shields.io/badge/Language-C++23-blue?logo=cplusplus)
![Static Badge](https://img.shields.io/badge/GPU-CUDA-green?logo=nvidia)
![Static Badge](https://img.shields.io/badge/Built_by-Premake-blue?logo=lua)
![Static Badge](https://img.shields.io/badge/License-MIT-green)

## 描述

一个使用 C++23 和 Walnut 应用程序框架构建的实时交互式路径追踪器。**通过 NVIDIA CUDA 实现 GPU 加速** —— 整个路径追踪管线（光线生成、求交、着色、累积）在 GPU 上运行。当 CUDA 不可用时，回退到 CPU 多线程渲染。

### 架构

| 组件 | 多线程 CPU | GPU (CUDA) |
|-----------|---------------------|------------------------|
| 光线生成 | 跨 CPU 线程的 `std::execution::par` | CUDA 内核 — 每个像素一个线程 |
| 光线-球体求交 | 暴力循环（CPU） | `__device__` 函数（GPU） |
| 路径追踪（5 次反弹） | CPU 标量循环 | GPU SIMT 并行 |
| 随机数生成 | PCG 哈希（CPU） | PCG 哈希（GPU `__device__`） |
| 累积缓冲区 | 主机端 `glm::vec4[]` | 设备端 `float4[]` |
| 显示 | Walnut::Image (Vulkan) | Walnut::Image (Vulkan) 通过 D2H 拷贝 |
| 俄罗斯轮盘赌 | CPU | GPU |

**GPU 内核布局**：16×16 线程块，每个像素一个 CUDA 线程。`RenderKernel` 对每个像素执行完整的路径追踪，包括光线-球体求交、俄罗斯轮盘赌终止、朗伯漫反射 BRDF 采样以及渐进式累积。

## 系统要求

- **NVIDIA GPU**（可选），计算能力 ≥ 7.5（图灵/安培/阿达/布莱克威尔）
  - sm_75：GTX 16xx，RTX 20xx
  - sm_86：RTX 30xx
  - sm_89：RTX 40xx
  - sm_120：RTX 50xx
- **CUDA Toolkit 12.0+**（可选，推荐 13.x 以获得 GPU 加速）
- **Vulkan SDK 1.4+**
- **Visual Studio 2026**（或 2022，向后兼容）且支持 C++23

## 构建方法

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
这会运行 **Premake5 5.0.0-beta8** 生成 Visual Studio 2026 解决方案文件。脚本会自动下载 premake5（如果不存在，Walnut 绑定的版本不支持 `cppdialect "C++23"`）。构建系统会自动检测 CUDA 并启用 GPU 加速。

### 4. 构建并运行
在 Visual Studio 2026 中打开 `RayTracing.slnx` 并构建（建议使用 Release 或 Dist 模式以获得性能）。

### 无 CUDA 构建
如果未安装 CUDA Toolkit，项目将构建为仅 CPU 的路径追踪器，使用 `std::execution::par`。只有当检测到 CUDA 时，构建系统才会定义 `WL_CUDA`。

### ISPC 加速（可选）
[ISPC](https://github.com/ispc/ispc)（Intel SPMD 程序编译器）为 CPU 路径追踪器提供 SIMD 加速。`Setup.bat` 脚本会自动下载 ISPC v1.30.0 到 `vendor/ispc/`。当检测到时，构建系统会定义 `WL_ISPC` 并启用 AVX2+AVX-512 向量化路径追踪。

手动安装：从 [ISPC 发布页面](https://github.com/ispc/ispc/releases/tag/v1.30.0) 下载 `ispc-v1.30.0-windows.zip` 并将其内容解压到 `vendor/ispc/`。

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
│   ├── CUDARenderer.cu        # CUDA 主机包装器（C 链接）
│   └── CUDARenderer.h         # 主机 C++ 接口 + 打包辅助函数
├── premake5.lua               # 构建配置（+ CUDA 支持）
├── .github/workflows/build.yml # CI/CD 管线
└── README.md
```

## CI/CD

[![Build (CUDA + Vulkan)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml/badge.svg)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml)

GitHub Actions 在每次推送和拉取请求时构建：
- **Windows Server 2025**，带 CUDA 13.3 + Vulkan SDK
- Debug 和 Release 配置
- Release 构建可下载构建产物

## 按键绑定

| 键 | 操作 |
|-----|--------|
| 鼠标右键 + 拖动 | 旋转相机 |
| W/A/S/D | 移动相机 |
| Q/E | 向下/向上移动 |
| Render 按钮 | 触发重新渲染 |
| Accumulate 复选框 | 启用/禁用渐进式渲染 |

## 演示

光线追踪项目仍在开发中。

以下是当前项目的演示。\
![Ray Tracing Random](/screenshots/example-random.png)
![Ray Tracing Accumulate](/screenshots/example-accumulate.png)

## 关于 WalnutAppTemplate
- 描述\
这是一个简单的 Walnut 应用程序模板——与 Walnut 仓库中的示例不同，它将 Walnut 作为外部子模块，更适合实际构建应用程序。更多详情请参阅 Walnut 仓库。
- 入门指南\
克隆后，您可以自定义 `premake5.lua` 和 `WalnutApp/premake5.lua` 文件（例如将名称从 "WalnutApp" 改为其他名称）。满意后，运行 `scripts/Setup.bat` 生成 Visual Studio 2022 解决方案/项目文件。您的应用程序位于 `WalnutApp/` 目录下，其中包含一些基础示例代码，位于 `WalnutApp/src/WalnutApp.cpp` 中。建议修改该 WalnutApp 项目以创建您自己的应用程序，因为所有内容都已设置好并准备就绪。

## 故障排除

| 症状 | 原因 | 解决方案 |
|---------|-------|---------|
| 视口为黑色 | CUDA 架构不匹配 | 验证 GPU 型号，检查 `premake5.lua` 中的 `cudaArchs` 是否包含正确的 `sm_XX` |
| `没有可用的内核映像` | nvcc 未为目标 GPU 编译 | 添加对应的 `-gencode=arch=compute_XX,code=sm_XX` |
| `CUDA_PATH` 未设置 | 环境变量缺失 | 系统属性 → 环境变量 → 新建 `CUDA_PATH`，指向 CUDA Toolkit 目录 |
| 构建过程中 `.cu` 文件未被编译 | 生成时 `CUDA_PATH` 未生效 | 重启终端，确认 `echo %CUDA_PATH%` 输出非空，然后重新运行 `Setup.bat` |
| 链接器报告 `CUDARenderer_*` 未定义 | 未链接 `CUDARenderer.obj` | 检查 `premake5.lua` 中的 `linkoptions { "$(IntDir)CUDARenderer.obj" }` |
| `cppdialect 的无效值 'C++23'` | 使用了 Walnut 绑定的旧版 premake5 | 运行 `scripts\Setup.bat`（自动下载更新版本）或手动下载 premake5 5.0.0-beta8 |

## 许可证
本项目使用 `MIT 许可证`。

## 版权
© Bonity, 2024
