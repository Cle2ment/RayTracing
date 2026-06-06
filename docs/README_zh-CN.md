# 光线追踪：加速的光线追踪演示

[English](/README.md) | [中文](/docs/README_zh-CN.md) | [Français](/docs/README_fr-FR.md)

![Static Badge](https://img.shields.io/badge/License-MIT-green)
<br/>
![Static Badge](https://img.shields.io/badge/Language-C++23-00599C?logo=cplusplus)
![Static Badge](https://img.shields.io/badge/GPU-CUDA-76B900?logo=nvidia)
![Static Badge](https://img.shields.io/badge/CPU_Acceleration-ISPC-0071C5?logo=intel)
![Static Badge](https://img.shields.io/badge/Poject-Xmake-brightgreen?logo=data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA2NCA2NCI+PHBvbHlnaW9uIHBvaW50cz0iNjQsNCA2NCw2MiA2Miw2NCAxLDY0IDAsNjMgMCw0MCA2MCwzIiBmaWxsPSIjZmZmIi8+PC9zdmc+)
<br/>
![Static Badge](https://img.shields.io/badge/Auto_Translation-Deepseek-5786FE?logo=deepseek)
<br/>
[![Build](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml/badge.svg)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml)

## 概述

基于 [Peanut](https://github.com/Cle2ment/Peanut) 应用框架，使用 C++23 构建的实时交互式路径追踪器。**通过 NVIDIA CUDA 实现 GPU 加速**，**通过 Intel ISPC 实现 CPU 加速**——当 CUDA 可用时，整个路径追踪管线在 GPU 上运行；否则通过 ISPC（AVX2/AVX-512）提供 CPU SIMD 回退。

### 渲染后端

| 后端 | 加速方式 | 使用条件 |
|------|---------|---------|
| **CUDA GPU** | NVIDIA GPU（SM 7.5+） | 检测到 `CUDA_PATH` |
| **ISPC CPU** | AVX2 + AVX-512 SIMD | 检测到 `vendor/ispc/bin/ispc.exe` |
| **C++ CPU** | `std::execution::par` 多线程 | 回退方案 |

### 架构

| 组件 | CPU | GPU（CUDA） |
|------|-----|-------------|
| 光线生成 | ISPC `foreach` 或 `std::execution::par` | CUDA 内核——每个像素一个线程 |
| 光线-球体相交 | 暴力循环 | `__device__` 函数 |
| 路径追踪（5 次反弹） | GGX 微表面 BRDF | GGX 微表面 BRDF |
| 随机数生成 | PCG 哈希 | PCG 哈希（`__device__`） |
| 俄罗斯轮盘赌 | 3 次反弹后 | 3 次反弹后 |
| 显示 | Peanut::Image（Vulkan） | Peanut::Image（Vulkan）通过 D2H 拷贝 |

**GPU 内核布局**：16×16 线程块，每个像素一个 CUDA 线程。

## 快速开始

```bash
# 克隆仓库并包含子模块（推荐）
git clone --recursive https://github.com/Cle2ment/RayTracing.git
cd RayTracing

# 如果已克隆但未使用 --recursive，则手动初始化子模块：
git submodule update --init --recursive

# 一键配置、构建并生成 VS 解决方案
scripts\Setup.bat

# 运行路径追踪器
xmake run RayTracing
```

## 构建与运行

### 前提条件

| 依赖项 | 必需 | 说明 |
|--------|------|------|
| [Visual Studio 2026](https://visualstudio.microsoft.com/)（或 2022） | ✅ | C++ 桌面工作负载，MSVC v143+ |
| [Vulkan SDK 1.4+](https://vulkan.lunarg.com/) | ✅ | 设置 `VULKAN_SDK` 环境变量 |
| [CUDA Toolkit 12.0+](https://developer.nvidia.com/cuda-downloads) | 可选 | 设置 `CUDA_PATH`；xmake 自动检测 |
| [ISPC](https://ispc.github.io/) | 可选 | 由 `Setup.bat` 自动下载到 `vendor/ispc/` |
| [.NET SDK](https://dotnet.microsoft.com/) | 可选 | 仅当需要 `.sln` → `.slnx` 迁移时必需 |
| [xmake](https://xmake.io/) | 自动 | `Setup.bat` 会使用 xmake；若要 CLI 使用需全局安装 |

### 方法一：一键脚本（Setup.bat）

```bash
scripts\Setup.bat
```

该命令一步完成所有操作：
1. 检查 Peanut 子模块——若缺失则自动初始化
2. 配置 xmake（`xmake f -m release`）
3. 构建所有目标（`xmake build`）—— Peanut.lib + RayTracing.exe
4. 生成 Visual Studio 解决方案（`xmake project -k vsxmake`）
5. 通过 `dotnet sln migrate` 将 `.sln` 转换为 `.slnx`

**输出**：
- `build/windows/x64/release/RayTracing.exe`
- `vsxmake2026/RayTracing.slnx` —— 在 Visual Studio 中打开此文件

### 方法二：CLI 构建

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

### 方法三：Visual Studio

```bash
# 生成 VS 解决方案（初次构建后执行）
xmake project -k vsxmake -y -m release

# 转换为 .slnx
dotnet sln vsxmake2026\RayTracing.sln migrate
```

在 Visual Studio 中打开 `vsxmake2026\RayTracing.slnx`，将 RayTracing 设为启动项目，按下 F5。

> **注意**：编辑 `xmake.lua` 后，重新执行 `xmake project -k vsxmake -y -m release && dotnet sln vsxmake2026\RayTracing.sln migrate` 以刷新 VS 项目文件。

### 构建矩阵

| 命令 | CUDA | ISPC | 输出 |
|------|------|------|------|
| `xmake f -m release && xmake build` | 自动检测 | 自动检测 | `build/windows/x64/release/RayTracing.exe` |
| `xmake f -m debug && xmake build` | 自动检测 | 自动检测 | `build/windows/x64/debug/RayTracing.exe` |
| `xmake run RayTracing` | — | — | 运行构建的可执行文件 |

## 文件结构

```
RayTracing/
├── RayTracing/src/             # 应用程序源代码
│   ├── PeanutApp.cpp           # 入口点、ImGui UI、场景设置
│   ├── Renderer.h/cpp          # 渲染器（CPU/GPU/ISPC 分发）
│   ├── Camera.h/cpp            # FPS 相机、光线方向预计算
│   ├── Ray.h                   # 光线结构体
│   ├── Scene.h                 # 材质、球体、场景数据
│   ├── PathTracer.ispc         # ISPC SIMD 路径追踪内核
│   ├── CUDATypes.cuh           # GPU 数据结构
│   ├── CUDARenderer.cuh        # GPU 内核 + 设备函数
│   ├── CUDARenderer.cu         # CUDA 主机封装（C 链接）
│   ├── CUDARenderer.h          # 主机 C++ 接口 + 打包辅助函数
│   ├── VkCUDAInterop.h/cpp     # Vulkan-CUDA 零拷贝内存共享
│   └── OptiXDenoiser.h/cpp     # OptiX AI 降噪器集成
├── xmake.lua                   # 构建配置（CUDA + ISPC 检测）
├── scripts/
│   └── Setup.bat               # 一键构建 + 解决方案生成
├── Peanut/                     # Git 子模块——请勿直接修改
│   ├── Peanut/src/             # Peanut 框架
│   ├── vendor/glfw/            # GLFW 窗口管理
│   ├── vendor/imgui/           # ImGui UI 库
│   └── vendor/glm/             # GLM 数学库
└── .github/workflows/          # CI/CD（CUDA 13.3 + Vulkan）
```

## 快捷键

| 按键 | 功能 |
|------|------|
| 鼠标右键 + 拖动 | 旋转相机 |
| W/A/S/D | 移动相机 |
| Q/E | 向下/向上移动 |
| 渲染按钮 | 触发重新渲染 |
| 累积按钮 | 切换渐进式渲染 |
| 重置按钮 | 清除累积缓冲区 |

## 演示

![随机采样](/screenshots/example-random.png)
![累积渲染](/screenshots/example-accumulate.png)

## 故障排除

| 症状 | 原因 | 解决方案 |
|------|------|----------|
| `Peanut\Peanut\src\...` 未找到 | 子模块未初始化 | `git submodule update --init --recursive` |
| VS 中找不到 `.vcxproj` | `vsxmake2026\RayTracing.slnx` 过期 | 重新运行 `scripts\Setup.bat` 或依次执行 `xmake project -k vsxmake` 然后 `dotnet sln vsxmake2026\RayTracing.sln migrate` |
| 视口为黑色 | CUDA 架构不匹配 | 检查 `xmake.lua` 中 GPU 是否支持 `sm_XX` |
| `no kernel image is available` | nvcc 未针对你的 GPU | 在 `xmake.lua` 中添加匹配的 `add_cugencodes("compute_XX", "sm_XX")` |
| `CUDA_PATH` 未设置 / `.cu` 未编译 | 缺少环境变量 | 在系统环境变量中设置 `CUDA_PATH`，重启终端 |
| `cannot match add_files("Peanut\Peanut\src\**.cpp")` | 未执行 `git submodule update --init` | 见上表第一行 |
| 找不到 ISPC（无 SIMD） | ISPC 未在 `vendor/ispc/` 目录 | `Setup.bat` 会自动下载；如有必要重新运行 |
| `dotnet sln migrate` 失败 | 未安装 .NET SDK | 安装 [.NET SDK](https://dotnet.microsoft.com/) 或直接打开 `vsxmake2026\RayTracing.sln` |

## 许可证

MIT 许可证。详见 [LICENSE](LICENSE) 文件。
