# RayTracing 项目语言重构分析

**日期**: 2026-05-30  
**分支**: `feat/language-refactor-analysis`  
**项目规模**: 11 个源文件，~1,898 行有效代码  
**当前技术栈**: C++23 + CUDA 13.3 + Vulkan 1.4 (via Walnut) + ImGui + GLFW + glm

---

## ⚠️ 核心原则：性能边界不可跨越

本项目是**实时交互式路径追踪器**，核心渲染路径每帧执行 `width×height×bounces` 次。性能敏感的热路径（PerPixel、TraceRay、RenderKernel）必须保持在编译型语言中。

**Python 的建议严格限定在离线/启动时路径**：场景描述、材质预设、资产管线。Python 代码**永不进入渲染循环**。

```
热路径（每帧数百万次）               离线路径（启动一次）
═══════════════════════              ══════════════════
PerPixel()         → C++/Rust      场景定义    → Python DSL
TraceRay()         → CUDA/ISPC     材质预设    → Python/YAML
RenderKernel()     → CUDA          资产管线    → Python/USD
RayDirections()    → C++/Rust      批处理      → Python 脚本
═══════════════════                  ══════════════════
    必须编译型语言                       可以用脚本语言
```

**类比**：Blender Cycles 核心内核是纯 C++/CUDA，但场景构建、UI 是 Python。两者之间有严格的序列化边界。

---

## 项目模块清单

| # | 模块 | 文件 | 行数 | 角色 | 运行时调用频率 |
|---|------|------|------|------|---------------|
| 1 | **应用入口** | WalnutApp.cpp | 262 | ImGui UI + 场景初始化 + 渲染调度 | 每帧 UI 渲染 |
| 2 | **渲染调度器** | Renderer.cpp + .h | 538 | CPU/GPU 路径追踪调度 + 场景上传 | 每帧 1 次 |
| 3 | **相机** | Camera.cpp + .h | 223 | FPS 相机 + 射线方向预计算 | 每帧 1 次（移动时） |
| 4 | **GPU 内核** | CUDARenderer.cuh | 347 | 路径追踪内核 + 球体求交 | 每像素 × bounce |
| 5 | **GPU 主机端** | CUDARenderer.cu | 303 | 显存分配/释放/上传/内核启动 | 每帧 1 次 |
| 6 | **GPU 接口** | CUDARenderer.h | 101 | C ABI + 主机端打包结构体 | 编译时 |
| 7 | **GPU 类型** | CUDATypes.cuh | 82 | 设备端结构体定义 | 编译时 |
| 8 | **场景数据** | Scene.h | 33 | Material、Sphere、Scene 结构体 | 每帧读取 |
| 9 | **射线** | Ray.h | 9 | 射线结构体 | 每像素 |

---

## 逐模块分析

### 1. WalnutApp.cpp — 应用入口 & ImGui UI

| 维度 | 评估 |
|------|------|
| **Walnut 耦合度** | 极高。继承 `Walnut::Layer`，依赖 `Walnut::Application`/`Image`/`Timer`/`EntryPoint` |
| **性能敏感性** | 低。仅 UI 渲染和场景初始化 |
| **Rust 替代** | ❌ 不可行。Walnut 是 C++ 框架，深度绑定 Vulkan/GLFW 生命周期 |
| **Python 替代** | ❌ 不可行。需要 Vulkan 窗口管理 + ImGui 渲染 |
| **改进建议** | 将硬编码的场景初始化（第 22-76 行）提取到外部数据源 |

**结论**：保持 C++，不迁移。

#### Walnut 框架全貌

| 文件 | 行数 | 角色 | RayTracing 使用? |
|------|------|------|-----------------|
| `Application.cpp/h` | 650+58 | Vulkan 实例/设备/交换链 + GLFW 窗口 + ImGui 后端 + 帧循环 | ✅ WalnutApp.cpp |
| `Image.cpp/h` | 252+38 | Vulkan 纹理创建 + ImGui DescriptorSet | ✅ Renderer 输出显示 |
| `EntryPoint.h` | 29 | `#define main()` → `CreateApplication()` | ✅ WalnutApp.cpp |
| `Layer.h` | 12 | Layer 基类 (OnUpdate/OnUIRender) | ✅ ExampleLayer 继承 |
| `Timer.h` | 42 | 渲染时间计时 | ✅ WalnutApp.cpp |
| `Random.cpp/h` | 5+40 | Mersenne Twister RNG | ✅ CPU 路径 (SlowRandom) |
| `ImGui/ImGuiBuild.cpp` | 2 | ImGui Vulkan/GLFW 后端合并编译 | ✅ Walnut framework |
| `Input/Input.cpp` | 29 | GLFW 键盘/鼠标回调 | ❌ 未用 (Camera.cpp 直接调 GLFW) |
| `Input/Input.h` | 13 | 输入 API 声明 | ❌ 未用 |
| `Input/KeyCodes.h` | 166 | 键码枚举 | ❌ 未用 |

Walnut 总计 ~1,336 行（10 个源文件），其中 **7 个被 RayTracing 使用**，3 个 (`Input/`) 未被使用。Walnut 是子模块，**不允许修改**——只能通过 `Walnut::Layer` 子类扩展。

#### GLM 依赖映射

Rust 迁移时需用 `glam` 替代所有 glm 调用：

| glm API | 使用位置 | Rust glam 等效 |
|---------|---------|---------------|
| `glm::vec3` / `glm::vec4` | Scene.h, Ray.h, Camera, Renderer | `glam::Vec3` / `glam::Vec4` |
| `glm::mat4` | Camera.h | `glam::Mat4` |
| `glm::perspectiveFov` | Camera.cpp:117 | `glam::Mat4::perspective_rh` |
| `glm::lookAt` | Camera.cpp:129 | `glam::Mat4::look_at_rh` |
| `glm::inverse` | Camera.cpp:124,135 | `glam::Mat4::inverse` |
| `glm::cross` | Camera.cpp:36,79,81 | `glam::Vec3::cross` |
| `glm::dot` | Renderer.cpp:295-297 | `glam::Vec3::dot` |
| `glm::normalize` | Camera, Renderer | `glam::Vec3::normalize` |
| `glm::rotate` | Camera.cpp:86 | `glam::Quat::mul_vec3` |
| `glm::quat` / `glm::angleAxis` | Camera.cpp:78,81 | `glam::Quat::from_axis_angle` |
| `glm::value_ptr` | WalnutApp.cpp (ImGui) | N/A — 仅 ImGui 绑定需要 |
| `glm::clamp` | Renderer.cpp:182 | 手工 `min(max(v,lo),hi)` |
| `glm::radians` | Camera.cpp:118 | `f32::to_radians` |

**关键障碍**：`glm::value_ptr` 用于 ImGui 的 `DragFloat3`/`ColorEdit3`。Rust 中没有直接等效项——需要借用 `&mut [f32; 3]` 并通过 FFI 传给 ImGui 的 C 函数。

---

### 2. Renderer.cpp/h — 渲染调度器

| 维度 | 评估 |
|------|------|
| **性能敏感性** | 🔥🔥🔥 极高。CPU 路径每帧 `width×height` 次 PerPixel，每像素 5 bounces |
| **当前实现** | CPU 路径：`std::execution::par` 并行 + PCG RNG。GPU 路径：场景打包 + cudaMemcpy + 内核启动 |
| **已知问题** | ① `cudaMalloc` 返回值未检查；② `m_ActiveScene`/`m_ActiveCamera` 为可能悬空的裸指针；③ 手动 `new[]`/`delete[]` 内存管理；④ 假共享（false sharing）——外层按 Y 行并行，内层按 X 串行，相邻像素位于同一 cache line 时存在性能退化 |
| **Rust 替代** | ✅ CPU 路径可行（`rayon` 并行 → 安全并行；`glam` 数学库；所有权消除悬空指针） |
| **ISPC 替代** | ✅ CPU 路径可行（SPMD 编译到 SIMD，预期 4-8x 加速，无需换语言） |
| **Python 替代** | ❌ 绝对不行。每像素循环在 Python 中慢 100-1000x |

**结论**：
- **CPU 路径**：可用 Rust（`rayon`+`glam`）或 ISPC（SIMD 加速，零语言迁移）
- **GPU 调度**：可用 Rust（`cudarc` 提供安全 CUDA 绑定）
- 渲染循环本身必须保持编译型语言

#### ISPC 深度分析：为什么值得考虑

**ISPC (Intel SPMD Program Compiler) v1.30.0 (2026-02)** 是一个基于 LLVM 的开源编译器（BSD 许可），将类 C 的 SPMD 代码编译为标准 x86 SIMD 指令。**不限于 Intel CPU**——AMD Zen 2/3 支持 SSE4+AVX2（8 路），Zen 4/5（Ryzen 7000+）额外支持 AVX-512（16 路）。本项目目标平台支持 AVX-512，推荐使用 `--target=avx2,avx512skx-i32x16` 多目标编译，ISPC 运行时自动选择最优路径。你编写串行风格代码，编译器自动生成 SIMD 向量化版本。

**业界验证：**

| 项目 | 用途 | CPU 加速比 |
|------|------|-----------|
| **MoonRay** (DreamWorks) | 生产级路径追踪 | 整体 1.3–2.3×；集成（BSDF+光采样）2.7–3.0×；着色 4.2–6.2× |
| **OSPRay** (Intel) | 科学可视化 | 所有计算密集操作均用 ISPC |
| **Embree** | 光线求交库 | 提供原生 ISPC API |
| **Aras ToyPathTracer** | 参照实现 | ISPC AVX2: 246 Mray/s vs 纯 C++: 45-90 Mray/s |

OSPRay 论文结论：*"经过大量实验，ISPC 提供了性能、可移植性、易用性和表达能力之间的最佳平衡。"*

**与 C++ 互操作**：ISPC 生成 `.obj` 文件 + `.h` 头文件，直接从 C++ 调用——零序列化、零复制、零运行时开销。示例：
```cpp
#include "path_tracer_ispc.h"
ispc::trace_rays(origins, dirs, output, pixelCount); // 直接函数调用
```

**构建集成**：Premake5 通过自定义构建命令支持 ISPC：
```lua
filter "files:**.ispc"
    buildcommands {
        'ispc --target=avx2,avx512skx-i32x16 --arch=x86-64 "%{file.relpath}" '
            .. '-o "%{cfg.objdir}/%{file.basename}.ispc.obj" '
            .. '-h "%{cfg.objdir}/%{file.basename}_ispc.h"'
    }
```

**与本项目的契合度**：`PerPixel()`（Lambertian BRDF 采样、俄罗斯轮盘赌）和 `TraceRay()`（球体求交二次方程）都是 ISPC 的理想目标——算术密集、统一控制流。预计在现有 `std::execution::par` 线程并行基础上，额外获得 2–4× SIMD 加速。

**局限性**：无模板/继承/虚函数/异常/C++ 标准库。需要手动处理 AOS→AOSOA 数据布局。调试支持较弱（建议保留 C++ 参考路径，与现有 `#ifndef WL_CUDA` 模式相同）。ISPC 的 `launch[]`/`task[]` 并行系统与 `std::execution::par` 在不同抽象层级运行——ISPC 处理 SIMD 通道内并行，`par` 处理线程级跨行并行，两者互补而非冲突。

---

### 3. Camera.cpp/h — 相机系统

| 维度 | 评估 |
|------|------|
| **性能敏感性** | 中。`RecalculateRayDirections()` 每帧执行 `width×height` 次矩阵运算 |
| **代码特性** | 纯数学：透视投影、lookAt、四元数旋转、射线方向逆算 |
| **Walnut 耦合** | 仅 `Walnut::Input`（鼠标/键盘输入），可解耦 |
| **Rust 替代** | ✅ 可行。`glam` 替代 `glm`，`winit` 替代 `Walnut::Input` |
| **Python 替代** | ❌ 不适合实时相机控制。✅ 适合离线相机路径脚本 |

**结论**：实时相机保持 C++ 或迁移到 Rust。离线相机动画可用 Python。

---

### 4. CUDARenderer.cuh — GPU 内核

| 维度 | 评估 |
|------|------|
| **性能敏感性** | 🔥🔥🔥 最高。GPU 内核，每像素每弹射执行 |
| **CUDA 特性** | `__global__`/`__device__`、`float3`/`float4`、`blockIdx`/`threadIdx`、`rsqrtf`/`fminf`/`fmaxf` |
| **CUDA 特性未使用** | 共享内存、warp intrinsics、纹理对象、动态并行、协作组 —— 内核非常简单 |
| **Rust-CUDA 替代** | ❌ `rust-cuda` 重启中、`cuda-oxide` 实验阶段 —— 均不可生产使用 |
| **wgpu (Vulkan 计算) 替代** | ⚠️ 可行但工作量大。需将 347 行 CUDA C++ 重写为 WGSL。失去 OptiX 硬件光追加速 |
| **SyCL/oneAPI 替代** | ⚠️ 单源 C++ → NVIDIA/Intel/AMD，但本项目当前仅需 NVIDIA |

**结论**：CUDA C++ 保持不变。这是**行业标准选择**（PBRT v4、Cycles 同样选择）。Rust GPU 生态至少还需 2-3 年才能成熟。

---

### 5. CUDARenderer.cu — GPU 主机端包装器

| 维度 | 评估 |
|------|------|
| **性能敏感性** | 低。仅每帧一次场景上传和内核启动 |
| **已知问题** | 🔴 ① `cudaMalloc` 返回值从不检查（静默失败 → nullptr → 段错误）；② 部分 GPU 分配失败时内存泄漏；③ `void*` 参数零类型安全；④ 手动 new/delete CUDA State |
| **Rust 替代** | ✅🔥 **最高优先级重构目标**。`cudarc`/`cust` 提供安全 CUDA 绑定，`Drop` trait 自动清理，`bytemuck` 消除手动打包 |

**Rust 等效对比**：
```cpp
// 当前 C++：无错误检查，void* 无类型
cudaMalloc(&state->d_Spheres, sphereCount * sizeof(GPUSphere));
cudaMemcpy(state->d_Spheres, spheres, ..., cudaMemcpyHostToDevice);
```
```rust
// Rust cudarc：编译时类型安全，?传播错误
let spheres_dev = device.htod_copy(&spheres)?;
// Drop trait 自动释放 — 无泄漏可能
```

**结论**：**CUDARenderer.cu 是 Rust 迁移收益最高、风险最低的模块。**

#### Rust cudarc FFI 策略：具体方案

**Crate 选择**：`cudarc` v0.19.7 (2026-05)，来自 `chelsea0x3b/cudarc`（1,144 ★，440 万总下载量，过去 90 天 240 万）。支持 CUDA 11.4–13.x，拥有完整的 Runtime API 包装（cudaMalloc/cudaMemcpy/kernel launch），216 个反向依赖。

> ❌ `cust` v0.3.2 (2022-02) —— 4 年未更新，项目"正在重启"，不可生产使用。仅 `cudarc` 是当前可行的选择。

**FFI 边界设计**：现有的 `extern "C"` ABI（`CUDARenderer.h`）**保持 100% 不变**。Rust 重新实现 `CUDARenderer.cu` 的内部逻辑，编译为 `staticlib` → MSVC 链接。

```
之前:                        之后:
CUDARenderer.h (C ABI)      CUDARenderer.h (C ABI) — 不变
     ↓                              ↓
CUDARenderer.cu (NVCC)      Rust crate (cargo build)
     ↓                              ↓
CUDARenderer.obj → link     libcuda_renderer.a → link
```

**Rust 实现骨架**：
```rust
use cudarc::driver::*;

#[repr(C)] struct GPUSphere { position: Float3, radius: f32, material_index: i32 }
unsafe impl DeviceRepr for GPUSphere {}

struct CudaCtx { dev: CudaDevice, stream: CudaStream, module: CudaModule, /* ... */ }

#[no_mangle]
pub extern "C" fn CUDARenderer_Create() -> *mut CudaCtx {
    Box::into_raw(Box::new(CudaCtx::new()))
}

#[no_mangle]
pub extern "C" fn CUDARenderer_UploadScene(
    state: *mut CudaCtx, spheres: *const GPUSphere, n: u32, /* ... */
) {
    let ctx = unsafe { &mut *state };
    // cudarc: clone_htod 替代 cudaMemcpy（编译时类型安全）
    ctx.d_spheres = Some(ctx.stream.clone_htod(
        unsafe { slice::from_raw_parts(spheres, n as usize) }
    ).unwrap()); // Result<T> — 错误必须处理
}
// Drop trait 自动释放所有 GPU 内存 — 零泄漏可能
```

**内核编译**：`build.rs` 调用 NVCC 编译 `.cuh` → PTX → `include_str!()` 嵌入二进制：
```rust
// build.rs
Command::new("nvcc").args(["-ptx", "-o", "kernels.ptx", "src/kernels.cu"]).status()?;
// lib.rs
static PTX: &str = include_str!(concat!(env!("OUT_DIR"), "/kernels.ptx"));
let module = ctx.load_module(Ptx::from_str(PTX))?;
```

**构建集成 (premake5 + Cargo)**：
```lua
-- premake5.lua 新增
if cudaFound then
    prebuildcommands {
        'cargo build --release --manifest-path "cuda_renderer_host/Cargo.toml"'
    }
    linkoptions { '"cuda_renderer_host/target/release/cuda_renderer_host.lib"' }
end
```

---

### 6. CUDARenderer.h / CUDATypes.cuh — GPU 类型系统

| 维度 | 评估 |
|------|------|
| **关键契约** | 主机端 `GPUPackedSphere`(20B) ↔ 设备端 `GPUSphere`(20B)，`static_assert` 强制 |
| **打包开销** | `Renderer.cpp:356-384`：30 行手动逐字段复制 glm::vec3 → float[3] |
| **Rust 改进** | `bytemuck::Pod` + `#[repr(C)]` → 零成本安全类型强制转换，消除 30 行样板代码 |

**结论**：与 CUDA 主机端一起迁移到 Rust（`bytemuck` 大幅简化打包）。

---

### 7-9. Scene.h / Ray.h / 纯数据结构

| 模块 | 评估 |
|------|------|
| **Scene.h** | 33 行纯数据 + 1 行 getter。**Python DSL 目标**：定义场景 → 序列化为二进制/JSON → C++/Rust 渲染器反序列化加载。渲染器内部表示保持 C++/Rust。 |
| **Ray.h** | 9 行，两个 vec3。随渲染器一起迁移。 |

**Python DSL 示例**（离线，不进入渲染循环）：
```python
# scenes/cornell_box.py — 离线场景描述
scene = SceneBuilder()
scene.add_material("red", albedo=(1,0,0), roughness=0.5)
scene.add_sphere(pos=(0,0,1), radius=1.0, material="red")
scene.export_binary("cornell_box.bin")  # 序列化为二进制
```
C++/Rust 渲染器启动时加载 `.bin`，之后所有渲染都在编译型代码中执行。

---

## 已发现的问题

| # | 严重度 | 文件:行 | 问题 |
|---|--------|---------|------|
| 1 | 🔴 高 | CUDARenderer.cu:120-122 | `cudaMalloc` 返回值从不检查 — 静默失败 → nullptr → 段错误 |
| 2 | 🔴 高 | CUDARenderer.cu:63-67 | 部分 GPU 分配失败时，已分配内存泄漏 |
| 3 | 🟡 中 | Renderer.h:77-78 | `m_ActiveScene`/`m_ActiveCamera` 裸指针，生命周期不明确 |
| 4 | 🟡 中 | Renderer.cpp:80-81 | 手动 `new[]`/`delete[]`，应使用 `std::vector` |
| 5 | 🟢 低 | Renderer.cpp:163-187 | 假共享（false sharing）：外层按 Y 行并行，内层按 X 串行。相邻像素写入同一 cache line 导致 cache line bouncing，影响性能但不影响正确性 |

---

## 重构方案

### 方案 A0：ISPC CPU 加速（新增推荐）

| 项目 | 详情 |
|------|------|
| **描述** | 将 `Renderer.cpp` 中的 `PerPixel()` + `TraceRay()` 提取为 ISPC 函数。ISPC 编译为 SIMD（SSE/AVX2/AVX-512），实现 4-8x CPU 路径追踪加速 |
| **语言变更** | 无。ISPC 是 C 语法的 SPMD 编译器，与 C++ 无缝互操作 |
| **参考** | MoonRay (DreamWorks)、Embree、OSPRay 均使用 ISPC |
| **工作量** | ~3 天 |
| **风险** | 极低。仅优化现有代码，不改变架构 |
| **示例** | DreamWorks 用 ISPC 实现路径追踪 → 4-8x CPU 加速 |

### 方案 A：Python 场景 DSL

| 项目 | 详情 |
|------|------|
| **描述** | Python 脚本定义场景 → 序列化为二进制 → C++ 渲染器启动时加载。ImGui UI 保持不变 |
| **Python 代码** | 仅离线运行（场景编辑器脚本），不进入渲染循环 |
| **工作量** | 1-2 周 |
| **风险** | 极低。渲染热路径零影响 |
| **收益** | 艺术家友好、场景热重载、材质预设库 |

### 方案 B：部分 Rust 化

| 迁移模块 | 工作量 | 收益 |
|---------|--------|------|
| CUDARenderer.cu → Rust `cudarc` | 4-6 天 | 类型安全 GPU 管理，消除所有泄漏 |
| Renderer.cpp CPU 路径 → Rust `rayon`+`glam` | 5-8 天 | 无悬空指针，安全并行 |
| Camera → Rust `glam`+`winit` | 2-3 天 | 纯数学，迁移简单 |
| Scene 类型 → Rust `bytemuck` (Pod) | 1-2 天 | 消除手动打包 |
| FFI 边界 + 构建系统 | 3-5 天 | C ABI 桥接，premake5 + cargo 集成 |
| CI/CD 更新 | 1-2 天 | Rust toolchain 安装 |
| **总计** | **4-6 周** | +40% 缓冲（glm→glam 兼容性适配） |

**FFI 策略**：继承现有 `extern "C"` 接口（`CUDARenderer.h`）。Rust 编译为静态库 → MSVC 链接。自然边界：`CUDARenderer_Create/Destroy/UploadScene/Render/GetOutput`。

### 方案 C：全量 Rust（不推荐）

| 项目 | 详情 |
|------|------|
| **描述** | 所有模块迁移到 Rust，CUDA 内核用 wgpu/WGSL 重写 |
| **工作量** | 8-12 周 |
| **风险** | 🚫 Rust GPU 生态不成熟（rust-cuda/cuda-oxide 实验阶段）；wgpu 无 OptiX 硬件光追加速、无降噪器；失去 CUDA 生态 |
| **建议** | ❌ 当前不推荐。3 年后重新评估 |

---

## CI/CD 影响评估

当前 CI（`.github/workflows/build.yml`）使用 `windows-2025-vs2026` 运行器，安装 CUDA 13.3 + Vulkan SDK 1.4.309.0 + Premake5 → MSBuild。

| 方案 | CI 变更 | 影响 |
|------|---------|------|
| **A0 (ISPC)** | 添加 ISPC 二进制下载（~50MB），添加 `ISPC_PATH` 环境变量 | 低。ISPC 是单个 exe，下载后直接可用 |
| **A (Python DSL)** | 添加 Python 3.12+ 安装步骤 | 低。`actions/setup-python@v5` |
| **B (Rust)** | 添加 Rust toolchain 安装（`actions-rs/toolchain@v1` 或 `rustup`），`cargo build` 步骤 | 中。Rust 工具链 ~500MB，需缓存 |
| **C (全量 Rust)** | 替换整个构建管线：Premake5 → Cargo，MSVC → rustc，NVCC → 待定 | 高。需完全重写 CI |

**CI 缓存策略**（适用于方案 B）：
```yaml
- name: Cache Rust
  uses: actions/cache@v4
  with:
    path: |
      ~/.cargo/registry
      ~/.cargo/git
      cuda_renderer_host/target
    key: rust-cuda-${{ hashFiles('cuda_renderer_host/Cargo.lock') }}
```

---

## 测试策略建议

项目当前无测试（仅视觉验证）。**任何重构前必须先添加回归测试防线**：

1. **参考图像哈希对比**：渲染固定场景 + 固定帧索引 → 保存 RGBA 像素数据的 SHA-256 哈希。重构后对比哈希，确保渲染结果像素级一致。
2. **CUDA 单元测试**：测试 `cudaMalloc`/`cudaFree` 正确性、内存泄漏检测（在 Rust 中用 `#[test]` 覆盖所有 FFI 函数）
3. **性能基准**：`PerPixel()` 吞吐量（Mray/s）、`TraceRay()` 求交速度
4. **CI 集成**：上述测试作为 CI 步骤运行

---

## 推荐路线

| 阶段 | 内容 | 时间 | 收益 |
|------|------|------|------|
| **立即** | 修复漏洞 #1-4（5 个已知问题） | 1 天 | 安全基线 |
| **Phase 1** | 方案 A0：ISPC CPU 加速 | 3 天 | 4-8x CPU 性能提升 |
| **Phase 2** | 方案 A：Python 场景 DSL | 1-2 周 | 艺术家工作流 |
| **Phase 3** | 方案 B 第一步：Rust CUDARenderer.cu 替换 | 1 周 | 最大安全收益 |
| **Phase 4** | 方案 B 完整：Rust 渲染核心 | 3-5 周 | 全栈安全 |
| **未来** | 方案 C：Rust GPU（当生态成熟） | 2-3 年后重新评估 | |

---

## 总结

- **Python 用于离线场景描述**，不进入渲染循环 ✓
- **ISPC 用于 CPU 路径加速**，零语言变更 ✓
- **Rust 用于 CUDA 主机端包装器**，安全收益最高 ✓
- **CUDA C++ 用于 GPU 内核**，行业标准，保持不变 ✓
- **全量 Rust 暂不可行**，GPU 生态不成熟 ✗
