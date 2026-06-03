# 光线追踪：加速实时光线追踪演示

[English](/README.md) | [中文](/docs/README_zh-CN.md) | [Français](/docs/README_fr-FR.md)

![Static Badge](https://img.shields.io/badge/Language-C++23-blue?logo=cplusplus)
![Static Badge](https://img.shields.io/badge/GPU-CUDA-green?logo=nvidia)
![Static Badge](https://img.shields.io/badge/CPU-ISPC-cyan?logo=intel)
<br>
![Static Badge](https://img.shields.io/badge/Build-Xmake-brightgreen?logo=data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA2NCA2NCI+PHBvbHlnb24gcG9pbnRzPSI2NCw0IDY0LDYyIDYyLDY0IDEsNjQgMCw2MyAwLDQwIDYwLDMiIGZpbGw9IiNmZmYiLz48L3N2Zz4=)
![Static Badge](https://img.shields.io/badge/Project-Premake-blue?logo=lua)
[![Build](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml/badge.svg)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml)
![Static Badge](https://img.shields.io/badge/License-MIT-green)

## 概述

一个基于 C++23 和 [Walnut](https://github.com/TheCherno/Walnut) 应用框架构建的实时交互式路径追踪器。**通过 NVIDIA CUDA 进行 GPU 加速**，**通过 Intel ISPC 进行 CPU 加速** —— 当 CUDA 可用时，整个路径追踪管线在 GPU 上运行，否则通过 ISPC（AVX2/AVX-512）使用 CPU SIMD 作为后备方案。

### 渲染后端

| 后端 | 加速方式 | 使用条件 |
|---------|-------------|-----------|
| **CUDA GPU** | NVIDIA GPU (SM 7.5+) | 检测到 `CUDA_PATH` |
| **ISPC CPU** | AVX2 + AVX-512 SIMD | 检测到 `vendor/ispc/bin/ispc.exe` |
| **C++ CPU** | `std::execution::par` 多线程 | 后备方案 |

### 架构

| 组件 | CPU | GPU (CUDA) |
|-----------|-----|------------|
| 光线生成 | ISPC `foreach` 或 `std::execution::par` | CUDA kernel —— 每像素一个线程 |
| 光线-球体求交 | 暴力循环 | `__device__` 函数 |
| 路径追踪（5次弹射） | 兰伯特漫反射 BRDF | 兰伯特漫反射 BRDF |
| 随机数生成 | PCG Hash | PCG Hash (`__device__`) |
| 俄罗斯轮盘赌 | 3次弹射后 | 3次弹射后 |
| 显示 | Walnut::Image (Vulkan) | Walnut::Image (Vulkan) 通过 D2H 拷贝 |

**GPU 内核布局**：16×16 线程块，每个像素一个 CUDA 线程。

## 系统要求

- **NVIDIA GPU**（可选）计算能力 ≥ 7.5
  - sm_75：GTX 16xx，RTX 20xx
  - sm_86：RTX 30xx
  - sm_89：RTX 40xx
  - sm_120：RTX 50xx
- **CUDA Toolkit 12.0+**（可选，推荐 13.x）
- **Vulkan SDK 1.4+**
- **Visual Studio 2026**（或 2022）支持 C++23
- **ISPC** —— 通过 `scripts\Setup.bat` 自动下载，无需手动安装

## 快速开始

```bash
# 克隆包含子模块
git clone --recursive https://github.com/Cle2ment/RayTracing.git
cd RayTracing

# 一键设置和构建（自动下载 ISPC）
scripts\Setup.bat

# 或手动构建：
xmake f -m release && xmake build
xmake run RayTracing
```

## 文件结构

```
RayTracing/
├── src/
│   ├── WalnutApp.cpp          # 入口点，ImGui 界面，场景设置
│   ├── Renderer.h/cpp         # 渲染器（CPU/GPU/ISPC 分发）
│   ├── Camera.h/cpp           # FPS 相机，光线方向预计算
│   ├── Ray.h                  # 光线结构体
│   ├── Scene.h                # 材质、球体、场景数据
│   ├── PathTracer.ispc        # ISPC SIMD 路径追踪内核
│   ├── CUDATypes.cuh          # GPU 数据结构
│   ├── CUDARenderer.cuh       # GPU 内核 + 设备函数
│   ├── CUDARenderer.cu        # CUDA 主机包装器（C 链接）
│   └── CUDARenderer.h         # 主机 C++ 接口 + 打包辅助
├── xmake.lua                   # 构建配置（CUDA + ISPC 检测）
├── scripts/Setup.bat          # 一键项目生成
└── .github/workflows/         # CI/CD（CUDA 13.3 + Vulkan）
```

## 快捷键

| 按键 | 操作 |
|-----|--------|
| 右键 + 拖动 | 旋转相机 |
| W/A/S/D | 移动相机 |
| Q/E | 向下/向上移动 |
| 渲染按钮 | 触发重新渲染 |
| 累积 | 切换渐进式渲染 |
| 重置 | 清除累积缓冲区 |

## 演示

![随机采样](/screenshots/example-random.png)
![累积采样](/screenshots/example-accumulate.png)

## 故障排除

| 症状 | 原因 | 解决办法 |
|---------|-------|----------|
| 视口为黑色 | CUDA 架构不匹配 | 检查 GPU 是否支持 `xmake.lua` 中的 `sm_XX` |
| `no kernel image is available` | nvcc 未针对你的 GPU 编译 | 添加 `-gencode=arch=compute_XX,code=sm_XX` |
| `CUDA_PATH` 未设置 | 缺少环境变量 | 系统 → 环境变量 → `CUDA_PATH` → CUDA 目录 |
| `.cu` 文件未编译 | 生成时未设置 `CUDA_PATH` | 重启终端，`echo %CUDA_PATH%`，重新运行 `Setup.bat` |
| `CUDARenderer_*` 未定义 | 目标文件未链接 | 检查 `linkoptions { "$(IntDir)CUDARenderer.obj" }` |
| `invalid value 'C++23'` | Walnut 自带的 premake5 版本过旧 | 重新运行 `scripts\Setup.bat` 以获取 premake5 5.0.0-beta8 |

## 许可证

MIT 许可证。详见 [LICENSE](LICENSE)。
