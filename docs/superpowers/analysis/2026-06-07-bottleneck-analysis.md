# RayTracing 瓶颈分析

**分析日期:** 2026-06-07 | **更新:** 2026-06-22
**代码版本:** `ea8b7fe` | **分支:** `master`

## 执行摘要

全部 13 批重构完成。25+ 个 PR (#13->#55)。三后端 (CPU/GPU/ISPC) 均 BVH 加速。Phase 1 Golden Image 测试已完成 (PR #55)。0 个 open issue。

## 远期方向（修订版，2026-06-22）

### 渲染改进
- **Golden Image 回归测试 (P0)** ✅ — PR #55，Python (uv + scikit-image) CI 自洽比对
- **NEE / MIS 重要性采样 (P1)** — Next Event Estimation + Multiple Importance Sampling，直接光照与 BRDF 采样结合
- **Glass / Transmission BRDF (P1)** — 玻璃、透明材质折射/透射
- **glTF 场景加载 (P2)** — 标准 glTF 2.0 三角网格+PBR 材质导入

### 工具链 / 多语言
- **TypeScript 场景 DSL (P1)** — Deno TypeScript builder API -> `.ray.json`，C++ `Scene::LoadFromJSON`
- **Rust 场景编译器 CLI (P1)** — `clap`+`serde`+`rayon` 生态，接收 `.ray.json`+glTF，离线 BVH 构建，输出优化二进制 `.rayscene`
- **Rust 资产管线 (P2)** — glTF 解析 + BVH 烘焙独立 CLI
- **Rust 程序化场景生成 (P2)** — `noise`+`rand` 生态生成地形/城市/植被散射
- **CI 现代化 (P2)** — PowerShell -> Python，`zig cc` 可选 Linux 实验

### Rust 策略
- **推荐**：独立 CLI 工具链（场景编译器、资产管线、BVH 构建库、程序化场景生成），零 FFI 耦合
- **不推荐**：替代 CUDA/ISPC/C++ 核心渲染路径，wgpu 渲染后端

详见 `docs/superpowers/analysis/2026-06-22-multi-language-feasibility.md`。
