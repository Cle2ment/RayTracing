# 重构执行顺序

> **日期**: 2026-06-08 | **更新**: 2026-06-10 (第 1-7 批完成，第 8 批 NEXT)

---

## 第 1 批：P0 — Peanut 崩溃修复 (~15min) ✅ 完成

| 优先级 | ID | 修复 | 状态 |
|--------|----|------|------|
| 🔴 P0 | P2-01 | `Application.cpp:267` — 加一行 `vkDestroySurfaceKHR` | ✅ PR #14 |
| 🔴 P0 | P2-02 | `Image.cpp:161` — 加一行 `ImGui_ImplVulkan_RemoveTexture` | ✅ PR #14 |
| 🟡 P2 | P2-17 | 同文件视检 AddTexture/RemoveTexture 配对 | ✅ |

---

## 第 2 批：P1 — Peanut 内存安全 (~20min) ✅ 完成

| 优先级 | ID | 修复 | 状态 |
|--------|----|------|------|
| 🟠 P1 | P2-03 | `EntryPoint.h:16` — `delete app` → `unique_ptr` | ✅ PR #15 |
| 🟠 P1 | P2-04 | `Application.cpp:94,132,159` — 3× `malloc/free` → `vector` | ✅ PR #15 |
| — | — | `Renderer.h` — 默认 Interop=true, SlowRandom=false | ✅ PR #15 |

---

## 第 3 批：P1 — RayTracing 性能 (~40min) ✅ 完成

| 优先级 | ID | 修复 | 状态 |
|--------|----|------|------|
| 🟠 P1 | P2-06 | `CUDARenderer.cu:295` — event 替代 CPU 阻塞 sync | ✅ PR #16 |
| 🟡 P2 | P2-05 | `CUDARenderer.cu:304` — 删冗余 `cudaStreamSynchronize` | ✅ PR #16 |
| 🟡 P2 | P2-07 | `Renderer.cpp:331` — 缓存 `pixelIndex` | ✅ PR #16 |
| 🟠 P1 | P2-13 | `MaxBounces` 集中化 + ImGui slider (1-20) | ✅ PR #16 |

---

## 第 4 批：P1 — 基础设施 (~30min) ✅ 完成

| 优先级 | ID | 修复 | 状态 |
|--------|----|------|------|
| 🟠 P1 | MOD-04 | ISPC 场景版本追踪 — 静态场景不再每帧 SoA 重打包 | ✅ PR #17 |
| 🟠 P1 | MOD-07 | ASan + UBSan 构建配置 | ❌ blocked (MSVC multi-target 限制) |

> MOD-07: Peanut.lib 是独立 xmake target，无法与 RayTracing.exe 统一 `/fsanitize=address` → LNK2038 不匹配。Debug 模式已有 `/RTC1` 运行时检查。

---

## 第 5 批：P2 — 代码质量 (~60min) ✅ 完成

| 优先级 | ID | 修复 | 状态 |
|--------|----|------|------|
| 🟡 P2 | P2-08 | `CUDARenderer.cu` C-style casts (6 处) | ✅ PR #18 |
| 🟡 P2 | P2-09 | `OptiXDenoiser.cpp` C-style casts (13 处) + `VkCUDAInterop.cpp` (1 处) | ✅ PR #18 |
| 🟡 P2 | P2-10 | `Renderer.cpp:318` — `#define MT` → `constexpr` | ✅ PR #18 |
| 🟡 P2 | P2-11 | `std::iota` 替换 raw for loop | ✅ PR #18 |
| 🟡 P2 | P2-12 | π + epsilon 常量集中化 (GPU/ISPC/CPU 路径) | ✅ PR #18 |
| 🟡 P2 | P2-14 | `Constants.h` — 共享 `constexpr` 常量 (kPi + 7 epsilon 值) | ✅ PR #18 |
| 🟡 P2 | P2-15 | Peanut C-style casts (28 处) | ✅ PR #18 (Peanut PR #1) |

---

## 第 6 批：P2 — 剩余基础设施 (~45min) ✅ 完成

| 优先级 | ID | 修复 | 状态 |
|--------|----|------|------|
| 🟡 P2 | MOD-06 | `std::move` 优化 ISPC SoA buffer 交换 | ⬚ N/A — ISPC 向量是成员变量，非局部临时变量，零候选 |
| 🟡 P2 | MOD-08 | `constexpr` 局部变量 | ⬚ N/A — Renderer.cpp 已干净，Constants.h 已就绪 |

---

## 第 7 批：P3 — 锦上添花 (~15min) ✅ 完成

| 优先级 | ID | 修复 | 状态 |
|--------|----|------|------|
| 🔵 P3 | MOD-05 | `noexcept` 标注 (29 个函数，10 个文件) | ✅ PR #19 |
| ~~🔵 P3~~ | ~~P2-16~~ | Peanut 全局变量 → 成员变量 — 远期，跳过 | ⬚ deferred |

---

## ⬜ 第 8 批：P0 — Catch2 单元测试 (~1h) **← NEXT**

| 优先级 | ID | 修复 | 状态 |
|--------|----|------|------|
| 🟡 P0 | MOD-09 | Catch2 测试框架 + 核心 CPU 函数测试 (GGX BRDF, TraceRay, PCGHash, ConvertToRGBA) | ⬚ |

---

## 第 9-12 批：远期

| 批次 | 项目 | 说明 |
|------|------|------|
| Batch 9 | MOD-02b | CPU fallback on GPU error (需要 IRenderBackend) |
| Batch 10 | P2-16 | Peanut mutable static globals → member variables |
| Batch 11 | GPU BVH | Bounding volume hierarchy |
| Batch 12 | OptiX fix | Driver/Runtime API context bridge |

---

## 汇总

| 批次 | 优先级 | 项目数 | 累积 | 关键里程碑 |
|------|--------|--------|------|------------|
| 1 | P0 | 3 | ✅ | Vulkan 验证层零报错 |
| 2 | P1 | 3 | ✅ | Peanut 零 malloc/free |
| 3 | P1 | 4 | ✅ | MaxBounces slider 可用 |
| 4 | P1 | 2 | ✅ | ISPC 场景缓存 |
| 5 | P2 | 7 | ✅ | RayTracing + Peanut 零 C-cast, Constants.h |
| 6 | P2 | 2 | ✅ | 零候选 (N/A) |
| 7 | P3 | 1 | ✅ | 29 noexcept |
| **8** | **P0** | **1** | **⬚ NEXT** | **首个单元测试通过** |
| 9-12 | P0-P3 | 4 | ⬚ deferred | — |
