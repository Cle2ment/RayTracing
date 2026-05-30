# 光线追踪 — NVIDIA GPU 加速路径追踪器

[English](/README.md) | [中文](/docs/README_zh-CN.md) | [Français](/docs/README_fr-FR.md)

![Static Badge](https://img.shields.io/badge/Inspired_by-TheCherno-yellow?logo=Github)
![Static Badge](https://img.shields.io/badge/Language-C++23-blue?logo=cplusplus)
![Static Badge](https://img.shields.io/badge/GPU-CUDA-green?logo=nvidia)
![Static Badge](https://img.shields.io/badge/Built_by-Premake-blue?logo=lua)
![Static Badge](https://img.shields.io/badge/License-MIT-green)

## 描述

一个基于 C++23 和 Walnut 应用框架构建的实时交互式路径追踪器。**通过 NVIDIA CUDA 进行 GPU 加速** — 整个路径追踪管线（光线生成、相交检测、着色、累积）都在 GPU 上运行。当 CUDA 不可用时，回退到 CPU 多线程渲染。

### 架构

| 组件 | 多线程 CPU | GPU (CUDA) |
|-----------|---------------------|------------------------|
| 光线生成 | 跨 CPU 线程的 `std::execution::par` | CUDA 内核 — 每像素一个线程 |
| 光线-球体相交 | 暴力循环（CPU） | `__device__` 函数（GPU） |
| 路径追踪（5次弹射） | CPU 标量循环 | GPU SIMT 并行 |
| 随机数生成 | PCG 哈希（CPU） | PCG 哈希（GPU `__device__`） |
| 累积缓冲区 | 主机 `glm::vec4[]` | 设备 `float4[]` |
| 显示 | Walnut::Image (Vulkan) | 通过 D2H 拷贝的 Walnut::Image (Vulkan) |
| 俄罗斯轮盘赌 | CPU | GPU |

**GPU 内核布局**：16×16 线程块，每个像素对应一个 CUDA 线程。`RenderKernel` 对每个像素执行完整的路径追踪，包括光线-球体相交、俄罗斯轮盘赌终止、朗伯漫反射 BRDF 采样和渐进式累积。

## 需求

- **NVIDIA GPU**（可选）计算能力 ≥ 7.5（图灵 / 安培 / 艾达 / 布莱克韦尔）
  - sm_75: GTX 16xx, RTX 20xx
  - sm_86: RTX 30xx
  - sm_89: RTX 40xx
  - sm_120: RTX 50xx
- **CUDA Toolkit 12.0+**（可选，推荐 13.x，用于 GPU 加速）
- **Vulkan SDK 1.4+**
- **Visual Studio 2026**（或 2022，向下兼容）支持 C++23

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
这将运行 **Premake5 5.0.0-beta8** 生成 Visual Studio 2026 解决方案文件。该脚本会自动下载 premake5（如果不存在，Walnut 自带的版本不支持 `cppdialect "C++23"`）。构建系统会自动检测 CUDA 并启用 GPU 加速。

### 4. 构建并运行
在 Visual Studio 2026 中打开 `RayTracing.slnx`，然后构建（建议使用 Release 或 Dist 模式以获得最佳性能）。

### 无 CUDA 构建
如果未安装 CUDA Toolkit，项目将构建为仅 CPU 的路径追踪器，使用 `std::execution::par`。只有当检测到 CUDA 时，构建系统才会定义 `WL_CUDA`。

### ISPC 加速（可选）
[ISPC](https://github.com/ispc/ispc)（Intel SPMD 程序编译器）为 CPU 路径追踪器提供 SIMD 加速。`Setup.bat` 脚本会自动下载 ISPC v1.30.0 到 `vendor/ispc/`。当检测到时，构建系统会定义 `WL_ISPC` 并启用 AVX2+AVX-512 向量化路径追踪。

手动安装：从 [ISPC 发布页面](https://github.com/ispc/ispc/releases/tag/v1.30.0) 下载 `ispc-v1.30.0-windows.zip`，将其内容解压到 `vendor/ispc/`。

## 渲染管线

```
Camera (CPU)                    Scene (CPU)
    │                                │
    ├─ 光线方向 ─┐         ┌─── 球体 + 材质
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
│              └─ 俄罗斯轮盘赌            │
│              └─ 漫反射 BRDF             │
│    └─ 累积 + 色调映射                   │
│    └─ 写入 RGBA8 输出                   │
└─────────────────────────────────────────┘
    │
    ▼ (cudaMemcpy D2H)
Walnut::Image (Vulkan) ──► 显示
```

## 文件结构

```
RayTracing/
├── src/
│   ├── WalnutApp.cpp          # 入口点、ImGui 界面、场景设置
│   ├── Renderer.h/cpp         # 渲染器类（CPU/GPU 调度）
│   ├── Camera.h/cpp           # 相机（CPU），光线方向生成
│   ├── Ray.h                  # 光线结构体（CPU）
│   ├── Scene.h                # 场景数据（CPU 球体 + 材质）
│   ├── CUDATypes.cuh          # GPU 数据结构
│   ├── CUDARenderer.cuh       # GPU 内核 + 设备函数
│   ├── CUDARenderer.cu        # CUDA 主机封装（C 链接）
│   └── CUDARenderer.h         # 主机 C++ 接口 + 打包帮助器
├── premake5.lua               # 构建配置（+ CUDA 支持）
├── .github/workflows/build.yml # CI/CD 流水线
└── README.md
```

## CI/CD

[![Build (CUDA + Vulkan)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml/badge.svg)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml)

GitHub Actions 在每次推送和拉取请求时构建：
- **Windows Server 2025** 附带 CUDA 13.3 + Vulkan SDK
- Debug 和 Release 配置
- Release 构建提供可下载的构建产物

## 快捷键

| 键 | 操作 |
|-----|--------|
| 鼠标右键 + 拖动 | 旋转相机 |
| W/A/S/D | 移动相机 |
| Q/E | 向下/向上移动 |
| 渲染按钮 | 触发重新渲染 |
| 累积复选框 | 启用/禁用渐进式渲染 |

## 演示

光线追踪项目仍在开发中。

以下是当前项目的演示。\
![光线追踪随机](screenshots/example-random.png)
![光线追踪累积](screenshots/example-accumulate.png)

## 关于 WalnutAppTemplate
- 描述\
这是一个简单的 Walnut 应用模板——与 Walnut 仓库中的示例不同，它将 Walnut 作为外部子模块保留，更适合实际构建应用程序。详情请参阅 Walnut 仓库。
- 开始使用\
克隆后，您可以根据需要定制 `premake5.lua` 和 `WalnutApp/premake5.lua` 文件（例如将名称从 "WalnutApp" 改为其他）。满意后，运行 `scripts/Setup.bat` 生成 Visual Studio 2022 解决方案/项目文件。您的应用位于 `WalnutApp/` 目录中，其中包含一些基本示例代码，您可以在 `WalnutApp/src/WalnutApp.cpp` 中找到。我建议修改那个 WalnutApp 项目来创建您自己的应用程序，因为所有内容都已经设置好，可以直接使用。

## 故障排除

| 现象 | 原因 | 解决 |
|------|------|------|
| Viewport 全黑 | CUDA 架构不匹配 | 确认 GPU 型号，检查 `premake5.lua` 中 `cudaArchs` 是否包含对应 `sm_XX` |
| `no kernel image is available` | nvcc 未为目标 GPU 编译内核 | 添加对应 `-gencode=arch=compute_XX,code=sm_XX` |
| `CUDA_PATH` 未设置 | 环境变量缺失 | 系统属性 → 环境变量 → 新建 `CUDA_PATH`，指向 CUDA Toolkit 目录 |
| 构建时 `.cu` 文件未编译 | `CUDA_PATH` 未在生成时生效 | 重启终端，确认 `echo %CUDA_PATH%` 非空后重新 `Setup.bat` |
| 链接器报 `CUDARenderer_*` 未定义 | `CUDARenderer.obj` 未被链接 | 检查 `premake5.lua` 中 `linkoptions { "$(IntDir)CUDARenderer.obj" }` |
| `invalid value 'C++23' for cppdialect` | 使用了 Walnut 自带的旧版 premake5 | 运行 `scripts\Setup.bat`（它会自动下载新版）或手动下载 premake5 5.0.0-beta8 |

## 许可证
该项目使用 `MIT 许可证`。

## 版权
© Bonity, 2024