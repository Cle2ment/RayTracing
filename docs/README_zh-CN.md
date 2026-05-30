# 光线追踪：加速RT演示

[English](/README.md) | [中文](/docs/README_zh-CN.md) | [Français](/docs/README_fr-FR.md)

![静态徽章](https://img.shields.io/badge/Language-C++23-blue?logo=cplusplus)
![静态徽章](https://img.shields.io/badge/GPU-CUDA-green?logo=nvidia)
![静态徽章](https://img.shields.io/badge/CPU-ISPC-cyan?logo=intel)
![静态徽章](https://img.shields.io/badge/Built_by-Premake-blue?logo=lua)
![静态徽章](https://img.shields.io/badge/License-MIT-green)
![构建状态](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml/badge.svg)

## 概述

基于 C++23 和 [Walnut](https://github.com/TheCherno/Walnut) 应用框架构建的实时交互式路径追踪器。**通过 NVIDIA CUDA 进行 GPU 加速**，**通过 Intel ISPC 进行 CPU 加速**——当 CUDA 可用时，整个路径追踪管线在 GPU 上运行，否则通过 ISPC (AVX2/AVX-512) 提供 CPU SIMD 回退方案。

### 渲染后端

| 后端 | 加速方式 | 使用条件 |
|---------|-------------|-----------|
| **CUDA GPU** | NVIDIA GPU (SM 7.5+) | 检测到 `CUDA_PATH` |
| **ISPC CPU** | AVX2 + AVX-512 SIMD | 检测到 `vendor/ispc/bin/ispc.exe` |
| **C++ CPU** | `std::execution::par` 多线程 | 回退方案 |

### 架构

| 组件 | CPU | GPU (CUDA) |
|-----------|-----|------------|
| 射线生成 | ISPC `foreach` 或 `std::execution::par` | CUDA 内核——每个像素一个线程 |
| 射线-球体相交 | 暴力循环 | `__device__` 函数 |
| 路径追踪（5次反弹） | 朗伯体漫反射 BRDF | 朗伯体漫反射 BRDF |
| 随机数生成 | PCG 哈希 | PCG 哈希 (`__device__`) |
| 俄罗斯轮盘赌 | 3次反弹后 | 3次反弹后 |
| 显示 | Walnut::Image (Vulkan) | Walnut::Image (Vulkan) 通过 D2H 拷贝 |

**GPU 内核布局**：16×16 线程块，每个像素一个 CUDA 线程。

## 系统要求

- **NVIDIA GPU**（可选）计算能力 ≥ 7.5
  - sm_75: GTX 16xx, RTX 20xx
  - sm_86: RTX 30xx
  - sm_89: RTX 40xx
  - sm_120: RTX 50xx
- **CUDA Toolkit 12.0+**（可选，推荐 13.x）
- **Vulkan SDK 1.4+**
- **Visual Studio 2026**（或 2022）支持 C++23
- **ISPC** — 由 `scripts\Setup.bat` 自动下载，无需手动安装

## 快速开始

```bash
# 克隆包含子模块
git clone --recursive https://github.com/Cle2ment/RayTracing.git
cd RayTracing

# 生成 VS2026 解决方案（自动下载 premake5 + ISPC）
scripts\Setup.bat

# 在 Visual Studio 中打开 RayTracing.slnx，构建 Release x64
```

## 文件结构

```
RayTracing/
├── src/
│   ├── WalnutApp.cpp          # 入口点，ImGui UI，场景设置
│   ├── Renderer.h/cpp         # 渲染器（CPU/GPU/ISPC 调度）
│   ├── Camera.h/cpp           # FPS 相机，射线方向预计算
│   ├── Ray.h                  # 射线结构体
│   ├── Scene.h                # 材质、球体、场景数据
│   ├── PathTracer.ispc        # ISPC SIMD 路径追踪内核
│   ├── CUDATypes.cuh          # GPU 数据结构
│   ├── CUDARenderer.cuh       # GPU 内核 + 设备函数
│   ├── CUDARenderer.cu        # CUDA 主机封装（C 链接）
│   └── CUDARenderer.h         # 主机 C++ 接口 + 打包辅助函数
├── premake5.lua               # 构建配置（CUDA + ISPC 检测）
├── scripts/Setup.bat          # 一键项目生成
└── .github/workflows/         # CI/CD（CUDA 13.3 + Vulkan）
```

## 按键绑定

| 按键 | 操作 |
|-----|--------|
| 鼠标右键 + 拖动 | 旋转相机 |
| W/A/S/D | 移动相机 |
| Q/E | 向下/向上移动 |
| 渲染按钮 | 触发重新渲染 |
| 累积 | 切换渐进渲染 |
| 重置 | 清除累积缓冲区 |

## 演示

![随机采样](/screenshots/example-random.png)
![累积结果](/screenshots/example-accumulate.png)

## 故障排除

| 症状 | 原因 | 解决方案 |
|---------|-------|----------|
| 视口为黑色 | CUDA 架构不匹配 | 检查 GPU 是否支持 `premake5.lua` 中的 `sm_XX` |
| `no kernel image is available` | nvcc 未针对你的 GPU 进行编译 | 添加 `-gencode=arch=compute_XX,code=sm_XX` |
| `CUDA_PATH` 未设置 | 缺少环境变量 | 系统 → 环境变量 → `CUDA_PATH` → CUDA 目录 |
| `.cu` 文件未编译 | 生成时未设置 `CUDA_PATH` | 重启终端，`echo %CUDA_PATH%`，重新运行 `Setup.bat` |
| `CUDARenderer_*` 未定义 | 目标文件未链接 | 检查 `linkoptions { "$(IntDir)CUDARenderer.obj" }` |
| `invalid value 'C++23'` | Walnut 附带的 premake5 版本过旧 | 重新运行 `scripts\Setup.bat` 以获取 premake5 5.0.0-beta8 |

## 许可证

MIT 许可证。详见 [LICENSE](LICENSE)。
