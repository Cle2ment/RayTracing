# RayTracing 重构路线图

> **日期**: 2026-06-07 | **更新**: 2026-06-10 (Phase 1 关闭，#13→#19 合并，Batch 1-7 完成)
> **分支**: `master` @ `e045c19`

## 当前状态

| 阶段 | 状态 | 内容 |
|------|------|------|
| Phase 0: 紧急修复 | ✅ 完成 | FIX-01~09, PR #13 |
| Phase 1: C++ 现代化 | ✅ 完成 | MOD-01~09 全部评估，7/9 完成，2/9 N/A |
| Phase 2: Peanut 稳定性 | ✅ 完成 | P2-01/02, PR #14 |
| Phase 3: Peanut 内存安全 | ✅ 完成 | P2-03/04, PR #15 |
| Phase 4: RayTracing 性能 | ✅ 完成 | P2-05/06/07 + P2-13, PR #16 |
| Phase 5: 基础设施 | ✅ 完成 | MOD-04 ISPC 版本追踪, PR #17 |
| Phase 6: 代码质量 | ✅ 完成 | P2-08~15, Constants.h, PR #18 + Peanut PR #1 |
| Phase 7: 收尾 | ✅ 完成 | MOD-05 noexcept, PR #19 |
| **Phase 8: 测试** | ⬚ **NEXT** | **MOD-09 Catch2 单元测试** |
| 远期 | ⏸️ 规划中 | NEE/BVH/BRDF统一/CPU fallback/Rust/DSL |

**核心方针**: 先修地基 (Phase 0-7 ✅)，再盖楼 (Phase 8+)。每批独立可交付。

---

## ✅ Phase 0: 紧急修复 — 全部完成

> PR #13 已合并。三后端渲染一致，零 warning 编译。

| ID | 任务 | 文件 | 状态 |
|----|------|------|------|
| FIX-01 | CPU/ISPC VNDF 采样添加 ONB 局部帧变换 | `Renderer.cpp`, `PathTracer.ispc` | ✅ |
| FIX-02 | 移除 CPU 路径天空颜色，统一三后端 | `Renderer.cpp:463` | ✅ |
| FIX-03 | 修复 ISPC RNG 有符号整数溢出 | `PathTracer.ispc:11` | ✅ |
| FIX-04 | 修复 Camera 半像素偏移 | `Camera.cpp:149-151` | ✅ |
| FIX-05 | 开启 `-Wall -Wextra`，消除所有 warning | `xmake.lua` | ✅ |
| FIX-06 | VNDF PDF 添加 G1 项 | 三后端同步 | ✅ |
| FIX-07 | GPU GGX 死代码清理 | `CUDARenderer.cuh` | ✅ |
| FIX-08 | PostProcessKernel 条件写入 d_DenoiseBuffer | `CUDARenderer.cuh` | ✅ |
| FIX-09 | Russian Roulette 使用 luminance | 三后端同步 | ✅ |

---

## ✅ Phase 1: C++ 现代化 — 全部评估完成

| ID | 任务 | 状态 | PR |
|----|------|------|----|
| MOD-01 | `m_ImageData`/`m_AccumulationData` → `std::vector` | ✅ | #14 |
| MOD-02 | CUDA_CHECK: Debug abort / Release flag | ✅ | #14 |
| MOD-03 | `CUDARenderState` RAII (`unique_ptr` + `CUDARenderStateDeleter`) | ✅ | #14 |
| MOD-04 | ISPC 路径场景版本追踪 (跳过每帧 SoA 重打包) | ✅ | #17 |
| MOD-05 | `noexcept` 标注 (29 函数, 10 文件) | ✅ | #19 |
| MOD-06 | ISPC `std::move` | ⬚ N/A — 零候选 (成员变量, 非局部临时) |
| MOD-07 | ASan + UBSan 构建配置 | ❌ blocked — MSVC multi-target 限制 |
| MOD-08 | `constexpr` 局部变量 | ⬚ N/A — Renderer.cpp 已干净 |
| MOD-09 | Catch2 单元测试 | ⬚ **NEXT** |

---

## Phase 2-7: 全部完成

详细状态参见 `docs/superpowers/plans/2026-06-08-execution-order.md`。

---

## Phase 8: 架构演进 (远期)

### 近期 (按优先级)
| 优先级 | ID | 描述 | 批次 |
|--------|----|------|------|
| P0 | MOD-09 | Catch2 单元测试 — 核心 CPU 函数 | Batch 8 |
| P0 | MOD-02b | GPU error → 自动 CPU 回退 (需 IRenderBackend) | Batch 9 |
| P2 | P2-16 | Peanut 全局静态变量 → 成员变量 | Batch 10 |
| P3 | GPU BVH | Bounding volume hierarchy | Batch 11 |
| P4 | OptiX fix | Driver/Runtime API context bridge | Batch 12 |

### 远期技术栈
| 方向 | 评估 | 触发条件 |
|------|------|----------|
| NEE + MIS | 直接光照重要性采样 | BVH 完成后 |
| Glass/Fresnel | 反射+折射 BTDF | — |
| BRDF 统一 | `ggx_shared.h` 宏适配三后端 | — |
| Rust 增量引入 | CPU 侧模块 → C ABI | cuda-oxide 成熟后 |
| Python DSL | 场景生成 + 批量渲染 | JSON Scene 完成 |

---

## 变更日志

| 日期 | 变更 |
|------|------|
| 2026-06-07 | 初始版本 (Phase 0-8 完整路线) |
| 2026-06-08 | Phase 1 部分完成 + Peanut 审计结果并入 |
| 2026-06-08 | 重组：消除重复 Phase 2，清晰 DONE/PENDING/FUTURE 三区 |
| 2026-06-10 | Phase 1 关闭：全部 9 MOD 评估，#13→#19 合并，Batch 1-7 完成 |
