# 光线追踪：加速 RT 演示

[English](/README.md) | [中文](/docs/README_zh-CN.md) | [Français](/docs/README_fr-FR.md)

[![License: MIT](https://img.shields.io/badge/License-MIT-green)](LICENSE)

[![Release](https://img.shields.io/github/v/release/Cle2ment/RayTracing?label=Release&color=blue)](https://github.com/Cle2ment/RayTracing/releases)
[![Build](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml/badge.svg)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml)

[![C++23](https://img.shields.io/badge/C%2B%2B-23-00599C?logo=c%2B%2B&logoColor=white)](https://en.cppreference.com/w/)
[![CUDA](https://img.shields.io/badge/GPU-CUDA-76B900?logo=nvidia&logoColor=white)](https://developer.nvidia.com/cuda-toolkit)
[![ISPC](https://img.shields.io/badge/CPU_Accel-ISPC-0071C5?logo=intel&logoColor=white)](https://ispc.github.io/)
[![Python](https://img.shields.io/badge/Test-Python_3-3776AB?logo=python&logoColor=white)](https://python.org/)

[![uv](https://img.shields.io/badge/Package-uv-7248B9?logo=uv&logoColor=white)](https://docs.astral.sh/uv/)
[![Xmake](https://img.shields.io/badge/Build-Xmake-brightgreen)](https://xmake.io/)

[![DeepSeek](https://img.shields.io/badge/Translate-DeepSeek-5786FE?logo=deepseek)](https://deepseek.com/)

## 概述

基于 [Peanut](https://github.com/Cle2ment/Peanut) 应用框架，使用 C++23 构建的实时交互式路径追踪器。**通过 NVIDIA CUDA 进行 GPU 加速**，**通过 Intel ISPC 进行 CPU 加速**——当 CUDA 可用时，整个路径追踪管线在 GPU 上运行；否则通过 ISPC（AVX2/AVX-512）进行 CPU SIMD 回退。

### 渲染后端

| 后端 | 加速方式 | 使用条件 |
|---------|-------------|-----------|
| **CUDA GPU** | NVIDIA GPU (SM 7.5+) | 检测到 `CUDA_PATH` |
| **ISPC CPU** | AVX2 + AVX-512 SIMD | 检测到 `vendor/ispc/bin/ispc.exe` |
| **C++ CPU** | `std::execution::par` 多线程 | 回退方案 |

### 架构

| 组件 | CPU | GPU (CUDA) |
|-----------|-----|------------|
| 光线生成 | ISPC `foreach` 或 `std::execution::par` | CUDA 内核——每个像素一个线程 |
| 光线-球体相交 | 暴力循环 | `__device__` 函数 |
| 路径追踪（5 次反弹） | GGX 微表面 BRDF | GGX 微表面 BRDF |
| 随机数生成 | PCG Hash | PCG Hash (`__device__`) |
| 俄罗斯轮盘赌 | 3 次反弹后 | 3 次反弹后 |
| 显示 | Peanut::Image (Vulkan) | Peanut::Image (Vulkan)，通过 D2H 拷贝 |

**GPU 内核布局**：16×16 线程块，每个像素对应一个 CUDA 线程。

## 快速开始

```bash
# 克隆并初始化子模块（推荐）
git clone --recursive https://github.com/Cle2ment/RayTracing.git
cd RayTracing

# 如果克隆时没有使用 --recursive，则手动初始化子模块：
git submodule update --init --recursive

# 一键设置、构建并生成 VS 解决方案
scripts\Setup.bat

# 运行路径追踪器
xmake run RayTracing
```

## 构建与运行

### 前提条件

| 依赖项 | 必需 | 说明 |
|-----------|----------|-------|
| [Visual Studio 2026](https://visualstudio.microsoft.com/)（或 2022） | ✅ | C++ 桌面工作负载，MSVC v143+ |
| [Vulkan SDK 1.4+](https://vulkan.lunarg.com/) | ✅ | 设置 `VULKAN_SDK` 环境变量 |
| [CUDA Toolkit 12.0+](https://developer.nvidia.com/cuda-downloads) | 可选 | 设置 `CUDA_PATH`，由 xmake 自动检测 |
| [ISPC](https://ispc.github.io/) | 可选 | 由 `Setup.bat` 自动下载至 `vendor/ispc/` |
| [.NET SDK](https://dotnet.microsoft.com/) | 可选 | 仅用于 `.sln` 到 `.slnx` 的迁移 |
| [xmake](https://xmake.io/) | 自动安装 | `Setup.bat` 会使用 xmake；如需命令行使用请全局安装 |

### 方法 1：一键脚本（Setup.bat）

```bash
scripts\Setup.bat
```

该命令将自动处理所有步骤：

1. 检查 Peanut 子模块——若缺失则自动初始化
2. 配置 xmake（`xmake f -m release`）
3. 构建所有目标（`xmake build`）—— Peanut.lib + RayTracing.exe
4. 生成 Visual Studio 解决方案（`xmake project -k vsxmake`）
5. 通过 `dotnet sln migrate` 将 `.sln` 转换为 `.slnx`

**输出**：

- `build/windows/x64/release/RayTracing.exe`
- `vsxmake2026/RayTracing.slnx`——在 Visual Studio 中打开此文件

### 方法 2：CLI 构建

```bash
# 配置并构建（Release）
xmake f -m release
xmake build

# 运行
xmake run RayTracing

# Debug 构建
xmake f -m debug
xmake build
```

### 方法 3：Visual Studio

```bash
# 生成 VS 解决方案（在首次构建后）
xmake project -k vsxmake -y -m release

# 转换为 .slnx
dotnet sln vsxmake2026\RayTracing.sln migrate
```

在 Visual Studio 中打开 `vsxmake2026\RayTracing.slnx`，将 RayTracing 设为启动项目，然后按 F5。

> **注意**：修改 `xmake.lua` 后，请重新运行 `xmake project -k vsxmake -y -m release && dotnet sln vsxmake2026\RayTracing.sln migrate` 以刷新 VS 项目文件。

### 构建矩阵

| 命令 | CUDA | ISPC | 输出 |
|---------|------|------|--------|
| `xmake f -m release && xmake build` | 自动检测 | 自动检测 | `build/windows/x64/release/RayTracing.exe` |
| `xmake f -m debug && xmake build` | 自动检测 | 自动检测 | `build/windows/x64/debug/RayTracing.exe` |
| `xmake run RayTracing` | — | — | 运行已构建的可执行文件 |

## 文件结构

```
RayTracing/
├── RayTracing/src/             # 应用程序源代码
│   ├── PeanutApp.cpp           # 入口点、ImGui 界面、场景设置
│   ├── Renderer.h/cpp          # 渲染器（CPU/GPU/ISPC 调度）
│   ├── Camera.h/cpp            # FPS 相机，光线方向预计算
│   ├── Ray.h                   # Ray 结构体
│   ├── Scene.h                 # 材质、球体、场景数据
│   ├── PathTracer.ispc         # ISPC SIMD 路径追踪内核
│   ├── CUDATypes.cuh           # GPU 数据结构
│   ├── CUDARenderer.cuh        # GPU 内核 + 设备函数
│   ├── CUDARenderer.cu         # CUDA 宿主包装器（C 链接）
│   ├── CUDARenderer.h          # 宿主 C++ 接口 + 打包辅助函数
│   ├── VkCUDAInterop.h/cpp     # Vulkan-CUDA 零拷贝内存共享
│   └── OptiXDenoiser.h/cpp     # OptiX AI 去噪器集成
├── xmake.lua                   # 构建配置（CUDA + ISPC + OptiX 检测）
├── scripts/
│   ├── Setup.bat               # 一键构建 + 解决方案生成
│   └── golden/                  # Python 黄金图像测试（由 uv 管理）
│       ├── pyproject.toml
│       ├── test_golden.py        # SSIM/MSE 回归测试
│       └── uv.lock
├── test/golden/                 # CPU 参考图像
├── RayTracing/tools/            # 无头渲染器
│   └── GoldenRenderer.cpp       # 用于 CI 的命令行 CPU 路径追踪器
├── Peanut/                     # Git 子模块（独立分支，可修改以进行通用改进）
│   ├── Peanut/src/             # Peanut 框架
│   ├── vendor/GLFW/            # GLFW 窗口管理
│   ├── vendor/imgui/           # ImGui 界面库
│   └── vendor/glm/             # GLM 数学库
├── Directory.Build.props       # VS IntelliSense 配置（MSBuild 自动发现）
└── .github/workflows/          # CI/CD（构建、测试、黄金图像、发布）
```

## 快捷键

| 按键 | 操作 |
|-----|--------|
| 鼠标右键 + 拖动 | 旋转相机 |
| W/A/S/D | 移动相机 |
| Q/E | 向下/向上移动 |
| 渲染按钮 | 触发重新渲染 |
| 累积（Accumulate） | 切换渐进式渲染 |
| 重置（Reset） | 清空累积缓冲区 |

## 测试

### 单元测试
```bash
xmake build RayTracing_test && xmake run RayTracing_test
# 28 个 Catch2 测试：GGX BRDF、TraceRay、PCGHash、ConvertToRGBA 等
```

### 黄金图像回归测试
```bash
xmake build GoldenRenderer
cd scripts/golden
uv sync                    # 安装依赖（numpy, scikit-image, Pillow）
uv run python test_golden.py    # 与黄金图像比较
uv run python test_golden.py --generate  # 生成新的黄金图像
```
CI 在自一致性模式下运行黄金测试：在同一运行中渲染两次并比较 SSIM。

## 演示

![随机采样](/screenshots/example-random.png)
![累积](/screenshots/example-accumulate.png)

## 故障排除

| 现象 | 原因 | 解决方案 |
|---------|-------|----------|
| 找不到 `Peanut\Peanut\src\...` | 子模块未初始化 | `git submodule update --init --recursive` |
| VS 中找不到 `.vcxproj` | `vsxmake2026\RayTracing.slnx` 过时 | 重新运行 `scripts\Setup.bat` 或执行 `xmake project -k vsxmake` 然后 `dotnet sln vsxmake2026\RayTracing.sln migrate` |
| 视口为黑色 | CUDA 架构不匹配 | 检查 GPU 是否支持 `xmake.lua` 中的 `sm_XX` |
| `no kernel image is available` | nvcc 未针对您的 GPU 生成代码 | 在 `xmake.lua` 中添加匹配的 `add_cugencodes("compute_XX", "sm_XX")` |
| `CUDA_PATH` 未设置 / `.cu` 未编译 | 环境变量缺失 | 在系统环境变量中设置 `CUDA_PATH`，重启终端 |
| `cannot match add_files("Peanut\Peanut\src\**.cpp")` | 未运行 `git submodule update --init` | 见上第一行 |
| 找不到 ISPC（无 SIMD） | ISPC 不在 `vendor/ispc/` 中 | `Setup.bat` 会自动下载；必要时重新运行 |
| `dotnet sln migrate` 失败 | 未安装 .NET SDK | 安装 [.NET SDK](https://dotnet.microsoft.com/) 或直接打开 `vsxmake2026\RayTracing.sln` |

## 许可证

MIT 许可证。详见 [LICENSE](LICENSE)。

## 版权

版权所有 (c) 2026 Cle2ment
