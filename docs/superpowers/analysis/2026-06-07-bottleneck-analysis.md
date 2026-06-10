# RayTracing 瓶颈分析

> **分析日期**: 2026-06-07 | **更新**: 2026-06-10 (Phase 1 关闭，Batch 1-7 完成)
> **代码版本**: `e045c19` | **分支**: `master`

## 1. 执行摘要

Phase 0-7 重构全部完成。剩余问题：测试基础设施（P0）、GPU 故障回退（P1）、远期的渲染质量和架构改进。

---

## 2. 已解决

### Phase 0: 紧急修复 ✅ (PR #13)
9 bugfixes — 三后端渲染一致性、零 warning 编译。

### Phase 1: C++ 现代化 ✅ (PR #14, #17, #19)
| 模块 | 说明 |
|------|------|
| MOD-01 | `std::vector` 替代裸 `new[]`/`delete[]` |
| MOD-02 | `CUDA_CHECK`: Debug→`abort()`, Release→`state->cudaError` |
| MOD-03 | `CUDARenderState` RAII (`unique_ptr` + `CUDARenderStateDeleter`) |
| MOD-04 | ISPC 场景版本追踪 — 静态场景不再每帧 SoA 重打包 |
| MOD-05 | `noexcept` 标注 29 函数, 10 文件 |
| MOD-06 | N/A — ISPC `std::move` 零候选 |
| MOD-07 | blocked — MSVC multi-target ASan 不兼容 |
| MOD-08 | N/A — Renderer.cpp constexpr 已干净 |

### Phase 2-3: Peanut 修复 ✅ (PR #14, #15)
- VkSurfaceKHR 泄漏修复
- Descriptor Set 泄漏修复
- `unique_ptr<Application>` + 3× `malloc`→`vector`

### Phase 4: 性能 ✅ (PR #16)
- 冗余 `cudaStreamSynchronize` 移除
- CUDA event 替代 CPU 阻塞 sync
- CPU 路径 pixelIndex 缓存
- MaxBounces 集中化 + ImGui slider (1-20)

### Phase 5-7: 代码质量 ✅ (PR #17, #18, #19)
- 全部 C-style cast → `static_cast`/`reinterpret_cast` (RayTracing 20 + Peanut 28)
- `Constants.h`: kPi + 7 epsilon 常量 (含 `CUDARenderer.cuh` + `PathTracer.ispc`)
- `#define MT` → `constexpr`, `std::iota` 替换 raw loop
- `noexcept` 标注 29 函数

---

## 3. 待解决 — RayTracing

### 3.1 基础设施
| 问题 | 优先级 | 批次 |
|------|--------|------|
| 无单元测试 | 🔴 P0 | Batch 8 (MOD-09 Catch2) |
| GPU 故障无 CPU 回退 | 🔴 P0 | Batch 9 (MOD-02b, 需 IRenderBackend) |
| ASan/UBSan 不可用 | 🟡 | blocked (MSVC 限制) |

### 3.2 架构
| 问题 | 优先级 |
|------|--------|
| BRDF 三份重复实现 (CUDA/CPU/ISPC) | 🟡 远期 |
| 条件编译选择后端而非运行时多态 | 🟡 远期 (IRenderBackend) |
| Peanut 全局静态可变变量 | 🟡 Batch 10 |

### 3.3 渲染质量
| 问题 | 优先级 |
|------|--------|
| 无 BVH 空间加速 (O(N) 射线求交) | 🔴 远期 |
| 无 NEE (Next Event Estimation) | 🔴 远期 |
| 无 MIS (Multiple Importance Sampling) | 🔴 远期 |
| 无 Glass/Transmission | 🟡 远期 |
| OptiX 降噪器无 Albedo/Normal guide AOV | 🟡 远期 |
| 无纹理贴图 / 三角网格 / glTF | 🟡 远期 |

---

## 4. 已解决 — Peanut 框架

所有已知问题已修复 (PR #14, #15, #18 + Peanut PR #1)：

| 问题 | 严重度 | 状态 |
|------|--------|------|
| VkSurfaceKHR 泄漏 → VUID 报错 | 🔴 | ✅ P2-01 |
| Descriptor Set 泄漏 | 🔴 | ✅ P2-02 |
| `delete app` 无异常安全 | 🟠 | ✅ P2-03 |
| 3× `malloc`/`free` | 🟠 | ✅ P2-04 |
| 28 C-style casts | 🟡 | ✅ P2-15 |
| 全局变量 → 成员变量 | 🟡 | ⬚ P2-16 远期 |

---

## 5. 技术栈评估 (远期)

| 方向 | 评估 | 时机 |
|------|------|------|
| **C++23/26** | 已标 C++23 但实际用 C++17。推荐逐步升级 `std::span`/`std::expected`/`std::print` | 即刻 |
| **Rust** | CUDA 主机端 (cudarc) 成熟，kernel 端 (cuda-oxide) Alpha | cuda-oxide 1.0 |
| **Zig** | CUDA (zcuda) 可用，comptime 着色器差异化。OptiX 无绑定 | 2027 重新评估 |
| **Python DSL** | 场景生成 + JSON IPC → C++ 渲染 | JSON Scene 完成 |
| **Web 编辑器** | TypeScript + WebGPU 预览 | BVH + glTF 完成 |
