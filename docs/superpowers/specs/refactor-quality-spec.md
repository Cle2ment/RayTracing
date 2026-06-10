# RayTracing 代码质量规范

> **适用范围**: Phase 1-7 (全部完成) → Phase 8+ | **更新**: 2026-06-10 (Phase 1 关闭)

---

## 1. Peanut 修改策略

Peanut 为独立 fork (已脱离 Walnut 上游)，可自由修改。约束：
- 改动须为**通用改进** (性能、bugfix、新功能)，避免 RayTracing 专属逻辑
- RayTracing 专属代码保持在 `RayTracing/src/`
- 每次 PR 独立审核，不与 RayTracing 改动捆绑

---

## 2. 手术式修改

| 规则 | 说明 |
|------|------|
| 每次 PR 一种关注点 | bug fix / 重构 / 新功能，不混搭 |
| 不改相邻代码格式 | 除非该文件正在被全面重构 |
| 清理 orphan | 你的改动引入的 → 必须清；注意到的既有死代码 → 标注但勿擅自删 |

---

## 3. 内存管理

| 规则 | 强制 | 状态 |
|------|------|------|
| 禁止裸 `new` / `delete` / `new[]` / `delete[]` | ✅ | ✅ MOD-01 |
| 堆数组 → `std::vector<T>` | ✅ | ✅ MOD-01 |
| CUDA 资源 → RAII wrapper | ✅ | ✅ MOD-03 (`unique_ptr<CUDARenderState>`) |
| 禁止 `malloc` / `free` | ✅ | ✅ P2-04 |

### GPU/Host 内存布局契约 (保持)

```cpp
// CUDARenderer.h — 任何新增 GPU 结构体必须添加此模式
static_assert(sizeof(GPUPackedSphere) == sizeof(GPUSphere),
    "Host/device sphere layout mismatch");
static_assert(offsetof(GPUPackedSphere, Radius) == offsetof(GPUSphere, Radius),
    "Sphere::Radius offset mismatch");
```

---

## 4. 错误处理

| 规则 | 强制 | 状态 |
|------|------|------|
| CUDA error → Debug abort / Release flag | ✅ | ✅ MOD-02 |
| 不允许静默吞错误 | ✅ | ✅ MOD-02 |
| GPU kernel 不做错误处理 (无异常机制) | — | N/A |
| GPU error → CPU 自动回退 | — | ⬚ MOD-02b (Batch 9) |

---

## 5. 类型安全

| 规则 | 强制 | 状态 |
|------|------|------|
| 禁止 `void*` 参数 (C ABI 边界除外) | ✅ | ✅ P2-15 |
| Magic number → `constexpr` 命名常量 | ✅ | ✅ P2-12/13/14 + Constants.h |
| C-style cast → `static_cast`/`reinterpret_cast` | ✅ | ✅ P2-08/09/15 (全部 48 处) |
| 枚举 → `enum class` | ✅ | N/A (未使用枚举) |
| `std::optional<T>` 替代 sentinel value | 推荐 | 远期 |

---

## 6. 三后端一致性

| 规则 | 强制 | 状态 |
|------|------|------|
| BRDF 数学 → 单一事实来源 (`ggx_shared.h`) | ✅ | 远期 (Phase 8) |
| 渲染行为 (天空/终止/材质) → 三后端一致 | ✅ | ✅ FIX-02 |
| 新增渲染特性 → 三后端同步 (或标注"仅 GPU") | ✅ | — |

---

## 7. 命名约定

| 类别 | 规则 | 示例 |
|------|------|------|
| 类/结构体 | PascalCase | `CUDARenderState` |
| 方法/成员 | camelCase | `OnResize()`, `m_FrameIndex` |
| 成员变量 | `m_` 前缀 | `m_Camera` |
| GPU 类型 | `GPU` 前缀 | `GPUMaterial` |
| Host 打包类型 | `GPUPacked` 前缀 | `GPUPackedSphere` |
| 接口类 | `I` 前缀 | `IRenderBackend` |
| 常量 | `constexpr` + `k` 前缀 | `constexpr float kEpsilon = 1e-5f;` |
| CUDA kernel | PascalCase + `Kernel` 后缀 | `RenderKernel` |

---

## 8. 条件编译

| 规则 | 强制 | 状态 |
|------|------|------|
| `#ifdef` 仅用于平台差异/加速器选择 | ✅ | — |
| 禁止 `#ifdef` 实现功能差异 (用接口多态) | ✅ | 远期 (IRenderBackend) |
| `#ifdef` 嵌套 ≤ 2 层 | ✅ | — |
| 每 `#ifdef` 块 ≤ 30 行 (否则提取文件) | ✅ | — |

---

## 9. 测试要求

| Phase | 最低覆盖 |
|-------|---------|
| **Phase 8 (当前)** | **Catch2 单元测试 — GGX BRDF, TraceRay, PCGHash, ConvertToRGBA (MOD-09)** |
| Phase 9 (IRenderBackend) | 后端接口正确性测试 |
| Phase 11 (BVH) | AABB 求交测试 |
| Phase 12 (BRDF) | 三后端输出差异 < 0.1% Golden Image |
| 远期 | Golden Image 自动化回归 (Python + imagehash) |

---

## 10. 评审清单

每个 PR 必须通过：

```
□ 无裸 new/delete (CUDA API 除外，有 RAII wrapper)
□ 无 void* 参数 (extern "C" 除外)
□ 无新增 #ifdef 嵌套 > 2 层
□ 新增 GPU 结构体有 static_assert 验证布局
□ 新增渲染特性三后端同步 (或明确标注例外)
□ 构建通过 (Debug + Release)
□ 新增代码有对应单元测试 (Batch 8 起)
```

---

## 变更日志

| 日期 | 变更 |
|------|------|
| 2026-06-07 | 初始版本 |
| 2026-06-08 | 添加 Peanut 策略，标记 MOD-01/02/03 完成 |
| 2026-06-10 | Phase 1 关闭：全部状态更新至 ✅，添加测试要求 Phase 8，评审清单增测试项 |
