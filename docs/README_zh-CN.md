# 光线追踪：加速版RT演示

[English](/README.md) | [中文](/docs/README_zh-CN.md) | [Français](/docs/README_fr-FR.md)

![静态徽章](https://img.shields.io/badge/语言-C++23-00599C?logo=cplusplus)
![静态徽章](https://img.shields.io/badge/GPU-CUDA-76B900?logo=nvidia)
![静态徽章](https://img.shields.io/badge/CPU加速-ISPC-0071C5?logo=intel)
![静态徽章](https://img.shields.io/badge/项目-Xmake-brightgreen?logo=data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdib3g9IjAgMCA2NCA2NCI+PHBvbHlnb24gcG9pbnRzPSI2NCw0IDY0LDYyIDYyLDY0IDEsNjQgMCw2MyAwLDQwIDYwLDMiIGZpbGw9IiNmZmYiLz48L3N2Zz4=)
<br/>
![静态徽章](https://img.shields.io/badge/自动翻译-Deepseek-5786FE?logo=deepseek)
<br/>
[![构建](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml/badge.svg)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml)
<br/>
![静态徽章](https://img.shields.io/badge/许可证-MIT-green)

## 概述

一个基于C++23和[Walnut](https://github.com/TheCherno/Walnut)应用框架构建的实时交互式路径追踪器。**通过NVIDIA CUDA实现GPU加速**，**通过Intel ISPC实现CPU加速**——当CUDA可用时，整个路径追踪管线在GPU上运行；否则通过ISPC（AVX2/AVX-512）提供CPU SIMD回退方案。

### 渲染后端

| 后端 | 加速方式 | 使用条件 |
|---------|-------------|-----------|
| **CUDA GPU** | NVIDIA GPU（SM 7.5+） | 检测到 `CUDA_PATH` |
| **ISPC CPU** | AVX2 + AVX-512 SIMD | 检测到 `vendor/ispc/bin/ispc.exe` |
| **C++ CPU** | `std::execution::par` 多线程 | 回退方案 |

### 架构

| 组件 | CPU | GPU (CUDA) |
|-----------|-----|------------|
| 光线生成 | ISPC `foreach` 或 `std::execution::par` | CUDA内核——每个像素一个线程 |
| 光线-球体相交 | 暴力循环 | `__device__` 函数 |
| 路径追踪（5次反弹） | GGX微表面BRDF | GGX微表面BRDF |
| 随机数生成 | PCG哈希 | PCG哈希（`__device__`） |
| 俄罗斯轮盘赌 | 3次反弹后 | 3次反弹后 |
| 显示 | Walnut::Image (Vulkan) | 通过D2H拷贝的Walnut::Image (Vulkan) |

**GPU内核布局**：16×16线程块，每个像素一个CUDA线程。

## 快速开始

```bash
# 克隆并包含子模块（推荐）
git clone --recursive https://github.com/Cle2ment/RayTracing.git
cd RayTracing

# 若未使用 --recursive 克隆，请手动初始化子模块：
git submodule update --init --recursive

# 一键配置、构建并生成VS解决方案
scripts\Setup.bat

# 运行路径追踪器
xmake run RayTracing
```

## 构建与运行

### 前提条件

| 依赖项 | 必须 | 说明 |
|-----------|----------|-------|
| [Visual Studio 2026](https://visualstudio.microsoft.com/)（或2022） | ✅ | C++桌面工作负载，MSVC v143+ |
| [Vulkan SDK 1.4+](https://vulkan.lunarg.com/) | ✅ | 设置 `VULKAN_SDK` 环境变量 |
| [CUDA Toolkit 12.0+](https://developer.nvidia.com/cuda-downloads) | 可选 | 设置 `CUDA_PATH`；xmake自动检测 |
| [ISPC](https://ispc.github.io/) | 可选 | 由 `Setup.bat` 自动下载至 `vendor/ispc/` |
| [.NET SDK](https://dotnet.microsoft.com/) | 可选 | 仅需用于 `.sln` → `.slnx` 迁移 |
| [xmake](https://xmake.io/) | 自动 | `Setup.bat` 使用xmake；全局安装以便命令行使用 |

### 方法1：一键脚本（Setup.bat）

```bash
scripts\Setup.bat
```

该命令自动完成以下所有步骤：
1. 检查Walnut子模块——若缺失则自动初始化
2. 配置xmake（`xmake f -m release`）
3. 构建所有目标（`xmake build`）——Walnut.lib + RayTracing.exe
4. 生成Visual Studio解决方案（`xmake project -k vsxmake`）
5. 通过 `dotnet sln migrate` 将 `.sln` 转换为 `.slnx`

**输出文件**：
- `build/windows/x64/release/RayTracing.exe`
- `vsxmake2026/RayTracing.slnx` —— 在Visual Studio中打开此文件

### 方法2：命令行构建

```bash
# 配置并构建（Release版）
xmake f -m release
xmake build

# 运行
xmake run RayTracing

# Debug版构建
xmake f -m debug
xmake build
```

### 方法3：Visual Studio

```bash
# 生成VS解决方案（首次构建后）
xmake project -k vsxmake -y -m release

# 转换为 .slnx
dotnet sln vsxmake2026\RayTracing.sln migrate
```

在Visual Studio中打开 `vsxmake2026\RayTracing.slnx`，将RayTracing设为启动项目，按F5运行。

> **注意**：编辑 `xmake.lua` 后，需重新运行 `xmake project -k vsxmake -y -m release && dotnet sln vsxmake2026\RayTracing.sln migrate` 以刷新VS项目文件。

### 构建矩阵

| 命令 | CUDA | ISPC | 输出 |
|---------|------|------|--------|
| `xmake f -m release && xmake build` | 自动检测 | 自动检测 | `build/windows/x64/release/RayTracing.exe` |
| `xmake f -m debug && xmake build` | 自动检测 | 自动检测 | `build/windows/x64/debug/RayTracing.exe` |
| `xmake run RayTracing` | — | — | 运行构建的可执行文件 |

## 文件结构

```
RayTracing/
├── RayTracing/src/             # 应用程序源代码
│   ├── WalnutApp.cpp           # 入口点、ImGui UI、场景设置
│   ├── Renderer.h/cpp          # 渲染器（CPU/GPU/ISPC分配）
│   ├── Camera.h/cpp            # FPS相机、光线方向预计算
│   ├── Ray.h                   # 光线结构体
│   ├── Scene.h                 # 材质、球体、场景数据
│   ├── PathTracer.ispc         # ISPC SIMD路径追踪内核
│   ├── CUDATypes.cuh           # GPU数据结构
│   ├── CUDARenderer.cuh        # GPU内核+设备函数
│   ├── CUDARenderer.cu         # CUDA主机包装器（C链接）
│   ├── CUDARenderer.h          # 主机C++接口+打包辅助函数
│   ├── VkCUDAInterop.h/cpp     # Vulkan-CUDA零拷贝内存共享
│   └── OptiXDenoiser.h/cpp     # OptiX AI降噪器集成
├── xmake.lua                   # 构建配置（CUDA + ISPC检测）
├── scripts/
│   └── Setup.bat               # 一键构建+解决方案生成
├── Walnut/                     # Git子模块——请勿直接修改
│   ├── Walnut/src/             # Walnut框架
│   ├── vendor/glfw/            # GLFW窗口管理
│   ├── vendor/imgui/           # ImGui UI库
│   └── vendor/glm/             # GLM数学库
└── .github/workflows/          # CI/CD（CUDA 13.3 + Vulkan）
```

## 按键绑定

| 按键 | 动作 |
|-----|--------|
| 鼠标右键 + 拖动 | 旋转相机 |
| W/A/S/D | 移动相机 |
| Q/E | 向下/向上移动 |
| 渲染按钮 | 触发重新渲染 |
| 累积 | 切换渐进式渲染 |
| 重置 | 清除累积缓冲区 |

## 演示

![随机采样](/screenshots/example-random.png)
![累积渲染](/screenshots/example-accumulate.png)

## 故障排除

| 现象 | 原因 | 解决方案 |
|---------|-------|----------|
| `Walnut\Walnut\src\...` 未找到 | 子模块未初始化 | `git submodule update --init --recursive` |
| VS中找不到 `.vcxproj` | `vsxmake2026\RayTracing.slnx` 已过时 | 重新运行 `scripts\Setup.bat` 或 `xmake project -k vsxmake` 然后 `dotnet sln vsxmake2026\RayTracing.sln migrate` |
| 视口为黑色 | CUDA架构不匹配 | 检查GPU是否支持 `xmake.lua` 中的 `sm_XX` |
| `no kernel image is available` | nvcc未针对您的GPU | 在 `xmake.lua` 中添加匹配的 `add_cugencodes("compute_XX", "sm_XX")` |
| `CUDA_PATH` 未设置 / `.cu` 未编译 | 环境变量缺失 | 在系统环境变量中设置 `CUDA_PATH`，重启终端 |
| `cannot match add_files("Walnut\Walnut\src\**.cpp")` | 未运行 `git submodule update --init` | 参见上表第一行 |
| 未找到ISPC（无SIMD） | ISPC不在 `vendor/ispc/` 中 | `Setup.bat` 会自动下载；如有需要重新运行 |
| `dotnet sln migrate` 失败 | 未安装 .NET SDK | 安装 [.NET SDK](https://dotnet.microsoft.com/) 或直接打开 `vsxmake2026\RayTracing.sln` |

## 许可证

MIT许可证。详见 [LICENSE](LICENSE) 文件。
