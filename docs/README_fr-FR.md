# Ray Tracing — Traceur de chemins accéléré par GPU NVIDIA

[English](/README.md) | [中文](/docs/README_zh-CN.md) | [Français](/docs/README_fr-FR.md)

![Static Badge](https://img.shields.io/badge/Inspired_by-TheCherno-yellow?logo=Github)
![Static Badge](https://img.shields.io/badge/Language-C++23-blue?logo=cplusplus)
![Static Badge](https://img.shields.io/badge/GPU-CUDA-green?logo=nvidia)
![Static Badge](https://img.shields.io/badge/Built_by-Premake-blue?logo=lua)
![Static Badge](https://img.shields.io/badge/License-MIT-green)

## Description

Un traceur de chemins interactif temps réel construit avec C++23 et le framework d'application Walnut. **Accéléré par GPU via NVIDIA CUDA** — l'ensemble du pipeline de lancer de rayons (génération de rayons, intersection, ombrage, accumulation) s'exécute sur le GPU. Il retombe sur le rendu multi-thread CPU lorsque CUDA n'est pas disponible.

### Architecture

| Composant | CPU multi-thread | GPU (CUDA) |
|-----------|------------------|------------|
| Génération de rayons | `std::execution::par` sur les threads CPU | Kernel CUDA — un thread par pixel |
| Intersection rayon-sphère | Boucle de force brute (CPU) | Fonction `__device__` (GPU) |
| Lancer de chemins (5 rebonds) | Boucle scalaire CPU | Parallélisme SIMT GPU |
| Génération de nombres aléatoires | Hash PCG (CPU) | Hash PCG (`__device__` GPU) |
| Buffer d'accumulation | `glm::vec4[]` hôte | `float4[]` périphérique |
| Affichage | Walnut::Image (Vulkan) | Walnut::Image (Vulkan) via copie D2H |
| Roulette russe | CPU | GPU |

**Disposition du kernel GPU** : blocs de threads 16×16, chaque pixel obtient un thread CUDA. Le `RenderKernel` effectue le lancer de chemins complet par pixel, incluant l'intersection rayon-sphère, la terminaison par roulette russe, l'échantillonnage de la BRDF diffuse Lambertienne et l'accumulation progressive.

## Configuration requise

- **GPU NVIDIA**（可选）avec capacité de calcul ≥ 7.5（Turing / Ampere / Ada / Blackwell）
  - sm_75 : GTX 16xx, RTX 20xx
  - sm_86 : RTX 30xx
  - sm_89 : RTX 40xx
  - sm_120 : RTX 50xx
- **CUDA Toolkit 12.0+**（可选，推荐 13.x，用于 GPU 加速）
- **Vulkan SDK 1.4+**
- **Visual Studio 2026**（ou 2022, rétrocompatible）avec prise en charge de C++23

## Comment compiler

### 1. Cloner le dépôt
```bash
git clone --recursive https://github.com/Cle2ment/RayTracing.git
cd RayTracing
```

### 2. Installer les dépendances
- **[CUDA Toolkit](https://developer.nvidia.com/cuda-downloads)** — Requis pour le rendu GPU. L'installateur définit automatiquement la variable d'environnement `CUDA_PATH`.
- **[Vulkan SDK](https://vulkan.lunarg.com/)** — Requis pour l'affichage. Installez-le dans l'emplacement par défaut.

### 3. Générer les fichiers de projet
```bash
cd scripts
Setup.bat
```
Ceci exécute Premake5 pour générer les fichiers de solution Visual Studio 2026. Le système de build détecte automatiquement CUDA et active l'accélération GPU.

### 4. Compiler et exécuter
Ouvrez `RayTracing.slnx` dans Visual Studio 2026 et compilez (les modes Release ou Dist sont recommandés pour les performances).

### Compilation sans CUDA
Si CUDA Toolkit n'est pas installé, le projet se compile en tant que traceur de chemins uniquement CPU utilisant `std::execution::par`. Le système de build définit `WL_CUDA` uniquement lorsque CUDA est détecté.

## Pipeline de rendu

```
Camera (CPU)                    Scene (CPU)
    │                                │
    ├─ Ray Directions ─┐         ┌─── Spheres + Materials
    │                  │         │
    ▼                  ▼         ▼
┌─────────────────────────────────────────┐
│           CUDARenderer.cu               │
│                                         │
│  RenderKernel <<<grid, block>>>         │
│    └─ PerPixel(x, y)                    │
│         └─ for bounce in 0..5:          │
│              └─ TraceRay()              │
│                   └─ sphere loop (GPU)  │
│              └─ Russian Roulette        │
│              └─ Diffuse BRDF            │
│    └─ Accumulate + Tone Map             │
│    └─ Write RGBA8 output                │
└─────────────────────────────────────────┘
    │
    ▼ (cudaMemcpy D2H)
Walnut::Image (Vulkan) ──► Display
```

## Structure des fichiers

```
RayTracing/
├── src/
│   ├── WalnutApp.cpp          # Entry point, ImGui UI, scene setup
│   ├── Renderer.h/cpp         # Renderer class (CPU/GPU dispatch)
│   ├── Camera.h/cpp           # Camera (CPU), ray direction generation
│   ├── Ray.h                  # Ray struct (CPU)
│   ├── Scene.h                # Scene data (CPU spheres + materials)
│   ├── CUDATypes.cuh          # GPU data structures
│   ├── CUDARenderer.cuh       # GPU kernels + device functions
│   ├── CUDARenderer.cu        # CUDA host wrappers (C linkage)
│   └── CUDARenderer.h         # Host C++ interface + packing helpers
├── premake5.lua               # Build configuration (+ CUDA support)
├── .github/workflows/build.yml # CI/CD pipeline
└── README.md
```

## CI/CD

[![Build (CUDA + Vulkan)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml/badge.svg)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml)

GitHub Actions construit à chaque push et pull request :
- **Windows Server 2025** avec CUDA 13.2 + Vulkan SDK
- Configurations Debug et Release
- Les artefacts de build sont disponibles en téléchargement pour les builds Release

## Raccourcis clavier

| Touche | Action |
|--------|--------|
| Bouton droit de la souris + glisser | Rotation de la caméra |
| W/A/S/D | Déplacement de la caméra |
| Q/E | Descendre / monter |
| Bouton Render | Déclencher un nouveau rendu |
| Case à cocher Accumulate | Activer / désactiver le rendu progressif |

## Démonstration

Le projet Ray Tracing est encore en développement.

Voici la démonstration actuelle du projet.\
![Ray Tracing Default Example](https://github.com/BoningtonChen/RayTracing/blob/master/Materials/RayTracing-example01.png)
![Ray Tracing Example](https://github.com/BoningtonChen/RayTracing/blob/master/Materials/RayTracing-example02.png)

## À propos de WalnutAppTemplate
- Description\
Ceci est un modèle d'application simple pour Walnut – contrairement à l'exemple présent dans le dépôt Walnut, celui-ci conserve Walnut comme sous-module externe et est bien plus adapté pour construire des applications réelles. Voir le dépôt Walnut pour plus de détails.
- Pour commencer\
Une fois que vous avez cloné, vous pouvez personnaliser les fichiers `premake5.lua` et `WalnutApp/premake5.lua` à votre convenance (par exemple, changer le nom de "WalnutApp" en autre chose). Lorsque vous êtes satisfait, exécutez `scripts/Setup.bat` pour générer les fichiers de solution/projet Visual Studio 2022. Votre application se trouve dans le répertoire `WalnutApp/`, avec un exemple de code de base dans `WalnutApp/src/WalnutApp.cpp` pour vous lancer. Je recommande de modifier ce projet WalnutApp pour créer votre propre application, tout étant déjà configuré et prêt à l'emploi.

## Dépannage

| 现象 | 原因 | 解决 |
|------|------|------|
| Viewport 全黑 | CUDA 架构不匹配 | 确认 GPU 型号，检查 `premake5.lua` 中 `cudaArchs` 是否包含对应 `sm_XX` |
| `no kernel image is available` | nvcc 未为目标 GPU 编译内核 | 添加对应 `-gencode=arch=compute_XX,code=sm_XX` |
| `CUDA_PATH` 未设置 | 环境变量缺失 | 系统属性 → 环境变量 → 新建 `CUDA_PATH`，指向 CUDA Toolkit 目录 |
| 构建时 `.cu` 文件未编译 | `CUDA_PATH` 未在生成时生效 | 重启终端，确认 `echo %CUDA_PATH%` 非空后重新 `Setup.bat` |
| 链接器报 `CUDARenderer_*` 未定义 | `CUDARenderer.obj` 未被链接 | 检查 `premake5.lua` 中 `linkoptions { "$(IntDir)CUDARenderer.obj" }` |

## LICENSE
Le projet utilise `MIT License`.

## Copyright
© Bonity, 2024