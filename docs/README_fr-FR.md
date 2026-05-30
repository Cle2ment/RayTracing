# Ray Tracing — Traceur de chemins accéléré par GPU NVIDIA

[English](/README.md) | [中文](/docs/README_zh-CN.md) | [Français](/docs/README_fr-FR.md)

![Static Badge](https://img.shields.io/badge/Inspired_by-TheCherno-yellow?logo=Github)
![Static Badge](https://img.shields.io/badge/Language-C++23-blue?logo=cplusplus)
![Static Badge](https://img.shields.io/badge/GPU-CUDA-green?logo=nvidia)
![Static Badge](https://img.shields.io/badge/Built_by-Premake-blue?logo=lua)
![Static Badge](https://img.shields.io/badge/License-MIT-green)

## Description

Un traceur de chemins interactif en temps réel construit avec C++23 et le framework d'application Walnut. **Accéléré par GPU via NVIDIA CUDA** — l'ensemble du pipeline de lancer de rayons (génération de rayons, intersection, ombrage, accumulation) s'exécute sur le GPU. Retour au rendu multithreadé CPU lorsque CUDA n'est pas disponible.

### Architecture

| Composant | CPU multithreadé | GPU (CUDA) |
|-----------|------------------|------------|
| Génération de rayons | `std::execution::par` sur plusieurs threads CPU | Kernel CUDA — un thread par pixel |
| Intersection rayon-sphère | Boucle de force brute (CPU) | Fonction `__device__` (GPU) |
| Lancer de chemins (5 rebonds) | Boucle scalaire CPU | Parallélisme SIMT du GPU |
| Génération de nombres aléatoires | PCG Hash (CPU) | PCG Hash (`__device__` GPU) |
| Tampon d'accumulation | `glm::vec4[]` hôte | `float4[]` périphérique |
| Affichage | Walnut::Image (Vulkan) | Walnut::Image (Vulkan) via copie D2H |
| Roulette russe | CPU | GPU |

**Disposition du kernel GPU** : blocs de threads 16×16, chaque pixel reçoit un thread CUDA. Le `RenderKernel` effectue le lancer de chemins complet par pixel, incluant l'intersection rayon-sphère, la terminaison par roulette russe, l'échantillonnage de la BRDF diffuse lambertienne et l'accumulation progressive.

## Prérequis

- **GPU NVIDIA** (optionnel) avec une capacité de calcul ≥ 7.5 (Turing / Ampere / Ada / Blackwell)
  - sm_75 : GTX 16xx, RTX 20xx
  - sm_86 : RTX 30xx
  - sm_89 : RTX 40xx
  - sm_120 : RTX 50xx
- **CUDA Toolkit 12.0+** (optionnel, recommandé 13.x, pour l'accélération GPU)
- **Vulkan SDK 1.4+**
- **Visual Studio 2026** (ou 2022, rétrocompatible) avec support C++23

## Comment construire

### 1. Cloner le dépôt
```bash
git clone --recursive https://github.com/Cle2ment/RayTracing.git
cd RayTracing
```

### 2. Installer les dépendances
- **[CUDA Toolkit](https://developer.nvidia.com/cuda-downloads)** — Requis pour le rendu GPU. La variable d'environnement `CUDA_PATH` est définie automatiquement par l'installateur.
- **[Vulkan SDK](https://vulkan.lunarg.com/)** — Requis pour l'affichage. Installez-le dans l'emplacement par défaut.

### 3. Générer les fichiers de projet
```bash
cd scripts
Setup.bat
```
Ceci exécute **Premake5 5.0.0-beta8** pour générer les fichiers de solution Visual Studio 2026. Le script télécharge automatiquement premake5 s'il n'est pas présent (la version fournie avec Walnut ne supporte pas `cppdialect "C++23"`). Le système de build détecte automatiquement CUDA et active l'accélération GPU.

### 4. Construire et exécuter
Ouvrez `RayTracing.slnx` dans Visual Studio 2026 et construisez (les modes Release ou Dist sont recommandés pour les performances).

### Construire sans CUDA
Si CUDA Toolkit n'est pas installé, le projet se construit comme un traceur de chemins uniquement CPU utilisant `std::execution::par`. Le système de build définit `WL_CUDA` seulement lorsque CUDA est détecté.

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
│   ├── WalnutApp.cpp          # Point d'entrée, interface ImGui, configuration de la scène
│   ├── Renderer.h/cpp         # Classe Renderer (distribution CPU/GPU)
│   ├── Camera.h/cpp           # Caméra (CPU), génération des directions de rayons
│   ├── Ray.h                  # Structure Ray (CPU)
│   ├── Scene.h                # Données de la scène (sphères CPU + matériaux)
│   ├── CUDATypes.cuh          # Structures de données GPU
│   ├── CUDARenderer.cuh       # Kernels GPU + fonctions de périphérique
│   ├── CUDARenderer.cu        # Wrappers hôte CUDA (liaison C)
│   └── CUDARenderer.h         # Interface C++ hôte + helpers d'empaquetage
├── premake5.lua               # Configuration de build (+ support CUDA)
├── .github/workflows/build.yml # Pipeline CI/CD
└── README.md
```

## CI/CD

[![Build (CUDA + Vulkan)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml/badge.svg)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml)

GitHub Actions construit à chaque push et pull request :
- **Windows Server 2025** avec CUDA 13.3 + Vulkan SDK
- Configurations Debug et Release
- Les artefacts de build sont disponibles en téléchargement pour les builds Release

## Raccourcis clavier

| Touche | Action |
|--------|--------|
| Clic droit + glisser | Tourner la caméra |
| W/A/S/D | Déplacer la caméra |
| Q/E | Descendre/monter |
| Bouton Render | Déclencher un nouveau rendu |
| Case Accumulate | Activer/désactiver le rendu progressif |

## Démonstration

Le projet Ray Tracing est encore en développement.

Voici la démonstration actuelle du projet.\
![Ray Tracing Default Example](https://github.com/BoningtonChen/RayTracing/blob/master/Materials/RayTracing-example01.png)
![Ray Tracing Example](https://github.com/BoningtonChen/RayTracing/blob/master/Materials/RayTracing-example02.png)

## À propos de WalnutAppTemplate
- **Description**\
Ceci est un modèle d'application simple pour Walnut – contrairement à l'exemple dans le dépôt Walnut, celui-ci garde Walnut comme un sous-module externe et est bien plus adapté pour construire des applications réelles. Voir le dépôt Walnut pour plus de détails.
- **Pour commencer**\
Après avoir cloné, vous pouvez personnaliser les fichiers `premake5.lua` et `WalnutApp/premake5.lua` selon vos besoins (par exemple, changer le nom de "WalnutApp" en autre chose). Une fois satisfait, exécutez `scripts/Setup.bat` pour générer les fichiers de solution/projet Visual Studio 2022. Votre application se trouve dans le répertoire `WalnutApp/`, avec un exemple de code de base dans `WalnutApp/src/WalnutApp.cpp`. Je recommande de modifier ce projet WalnutApp pour créer votre propre application, car tout est configuré et prêt à l'emploi.

## Dépannage

| 现象 | 原因 | 解决 |
|------|------|------|
| Viewport 全黑 | CUDA 架构不匹配 | 确认 GPU 型号，检查 `premake5.lua` 中 `cudaArchs` 是否包含对应 `sm_XX` |
| `no kernel image is available` | nvcc 未为目标 GPU 编译内核 | 添加对应 `-gencode=arch=compute_XX,code=sm_XX` |
| `CUDA_PATH` 未设置 | 环境变量缺失 | 系统属性 → 环境变量 → 新建 `CUDA_PATH`，指向 CUDA Toolkit 目录 |
| 构建时 `.cu` 文件未编译 | `CUDA_PATH` 未在生成时生效 | 重启终端，确认 `echo %CUDA_PATH%` 非空后重新 `Setup.bat` |
| 链接器报 `CUDARenderer_*` 未定义 | `CUDARenderer.obj` 未被链接 | 检查 `premake5.lua` 中 `linkoptions { "$(IntDir)CUDARenderer.obj" }` |
| `invalid value 'C++23' for cppdialect` | 使用了 Walnut 自带的旧版 premake5 | 运行 `scripts\Setup.bat`（它会自动下载新版）或手动下载 premake5 5.0.0-beta8 |

## LICENSE
Le projet utilise la licence MIT.

## Copyright
© Bonity, 2024