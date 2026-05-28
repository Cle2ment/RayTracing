# Ray Tracing — Traceur de Chemins Accéléré par GPU NVIDIA

[English](README.md) | [中文](docs/README_zh-CN.md) 

![Static Badge](https://img.shields.io/badge/Inspired_by-TheCherno-yellow?logo=Github)
![Static Badge](https://img.shields.io/badge/Language-C++20-blue?logo=cplusplus)
![Static Badge](https://img.shields.io/badge/GPU-CUDA-green?logo=nvidia)
![Static Badge](https://img.shields.io/badge/Built_by-Premake-blue?logo=lua)
![Static Badge](https://img.shields.io/badge/License-MIT-green)

## Description

Un traceur de chemins interactif en temps réel construit avec C++20 et le framework d'application Walnut. **Accéléré par GPU via NVIDIA CUDA** — l'ensemble du pipeline de traçage de chemins (génération de rayons, intersection, ombrage, accumulation) s'exécute sur le GPU. Retour au rendu multi-threadé CPU lorsque CUDA n'est pas disponible.

### Architecture

| Composant | CPU Multi-Thread | GPU (CUDA) |
|-----------|------------------|----------------------------|
| Génération de rayons | `std::execution::par` sur les threads CPU | Kernel CUDA — un thread par pixel |
| Intersection Rayon-Sphère | Boucle brute (CPU) | Fonction `__device__` (GPU) |
| Traçage de chemins (5 rebonds) | Boucle scalaire CPU | Parallélisme SIMT GPU |
| Génération de nombres aléatoires | PCG Hash (CPU) | PCG Hash (GPU `__device__`) |
| Buffer d'accumulation | `glm::vec4[]` hôte | `float4[]` périphérique |
| Affichage | Walnut::Image (Vulkan) | Walnut::Image (Vulkan) via copie D2H |
| Roulette russe | CPU | GPU |

**Disposition du Kernel GPU** : blocs de 16×16 threads, chaque pixel reçoit un thread CUDA. Le `RenderKernel` effectue le traçage de chemin complet par pixel, incluant l'intersection rayon-sphère, la terminaison par roulette russe, l'échantillonnage BRDF diffus de Lambert et l'accumulation progressive.

## Prérequis

- **GPU NVIDIA** (optionnel) avec capacité de calcul ≥ 7.5 (Turing / Ampere / Ada / Blackwell)
  - sm_75 : GTX 16xx, RTX 20xx
  - sm_86 : RTX 30xx
  - sm_89 : RTX 40xx
  - sm_120 : RTX 50xx
- **CUDA Toolkit 12.0+** (optionnel, recommandé 13.x, pour l'accélération GPU)
- **Vulkan SDK 1.4+**
- **Visual Studio 2026** (ou 2022, rétrocompatible) avec support C++20

## Comment construire

### 1. Cloner le dépôt
```bash
git clone --recursive https://github.com/Cle2ment/RayTracing.git
cd RayTracing
```

### 2. Installer les dépendances
- **[CUDA Toolkit](https://developer.nvidia.com/cuda-downloads)** — Requis pour le rendu GPU. La variable d'environnement `CUDA_PATH` est définie automatiquement par l'installateur.
- **[Vulkan SDK](https://vulkan.lunarg.com/)** — Requis pour l'affichage. Installer dans l'emplacement par défaut.

### 3. Générer les fichiers de projet
```bash
cd scripts
Setup.bat
```
Cela exécute Premake5 pour générer les fichiers de solution Visual Studio 2026. Le système de construction détecte automatiquement CUDA et active l'accélération GPU.

### 4. Construire et exécuter
Ouvrez `RayTracing.slnx` dans Visual Studio 2026 et construisez (mode Release ou Dist recommandé pour les performances).

### Construction sans CUDA
Si CUDA Toolkit n'est pas installé, le projet se construit comme un traceur de chemins CPU uniquement utilisant `std::execution::par`. Le système de construction définit `WL_CUDA` uniquement lorsque CUDA est détecté.

## Pipeline de rendu

```
Caméra (CPU)                    Scène (CPU)
    │                                │
    ├─ Directions de rayons ─┐    ┌─── Sphères + matériaux
    │                        │    │
    ▼                        ▼    ▼
┌─────────────────────────────────────────┐
│           CUDARenderer.cu               │
│                                         │
│  RenderKernel <<<grid, block>>>         │
│    └─ ParPixel(x, y)                    │
│         └─ pour rebond dans 0..5 :      │
│              └─ TraceRay()              │
│                   └─ boucle sphères (GPU)│
│              └─ Roulette russe          │
│              └─ BRDF diffus             │
│    └─ Accumuler + Tone Map              │
│    └─ Écrire sortie RGBA8               │
└─────────────────────────────────────────┘
    │
    ▼ (cudaMemcpy D2H)
Walnut::Image (Vulkan) ──► Affichage
```

## Structure des fichiers

```
RayTracing/
├── src/
│   ├── WalnutApp.cpp          # Point d'entrée, interface ImGui, configuration de la scène
│   ├── Renderer.h/cpp         # Classe Renderer (répartition CPU/GPU)
│   ├── Camera.h/cpp           # Caméra (CPU), génération de directions de rayons
│   ├── Ray.h                  # Structure Ray (CPU)
│   ├── Scene.h                # Données de la scène (sphères CPU + matériaux)
│   ├── CUDATypes.cuh          # Structures de données GPU
│   ├── CUDARenderer.cuh       # Kernels GPU + fonctions device
│   ├── CUDARenderer.cu        # Wrappers hôte CUDA (liaison C)
│   └── CUDARenderer.h         # Interface C++ hôte + helpers de packing
├── premake5.lua               # Configuration de construction (+ support CUDA)
├── .github/workflows/build.yml # Pipeline CI/CD
└── README.md
```

## CI/CD

[![Build (CUDA + Vulkan)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml/badge.svg)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml)

GitHub Actions construit à chaque push et pull request :
- **Windows Server 2022** avec CUDA 12.8 + Vulkan SDK
- Configurations Debug et Release
- Artefacts de construction disponibles en téléchargement pour les builds Release

## Raccourcis clavier

| Touche | Action |
|--------|--------|
| Bouton droit de la souris + glisser | Rotation de la caméra |
| W/A/S/D | Déplacement de la caméra |
| Q/E | Descendre/monter |
| Bouton Render | Déclencher un nouveau rendu |
| Case à cocher Accumulate | Activer/désactiver le rendu progressif |

## Démonstration

Le projet Ray Tracing est encore en développement.

Voici la démonstration actuelle du projet.\
![Ray Tracing Default Example](https://github.com/BoningtonChen/RayTracing/blob/master/Materials/RayTracing-example01.png)
![Ray Tracing Example](https://github.com/BoningtonChen/RayTracing/blob/master/Materials/RayTracing-example02.png)

## À propos de WalnutAppTemplate
- Description\
Ceci est un modèle d'application simple pour Walnut - contrairement à l'exemple dans le dépôt Walnut, celui-ci garde Walnut comme sous-module externe et est bien plus sensé pour construire des applications réelles. Consultez le dépôt Walnut pour plus de détails.
- Pour commencer\
Une fois cloné, vous pouvez personnaliser les fichiers `premake5.lua` et `WalnutApp/premake5.lua` à votre convenance (par exemple changer le nom de "WalnutApp" en autre chose). Une fois satisfait, exécutez `scripts/Setup.bat` pour générer les fichiers de solution/projet Visual Studio 2022. Votre application se trouve dans le répertoire `WalnutApp/`, avec un exemple de code basique pour vous lancer dans `WalnutApp/src/WalnutApp.cpp`. Je recommande de modifier ce projet WalnutApp pour créer votre propre application, car tout est configuré et prêt à l'emploi.

## Dépannage

| 现象 | 原因 | 解决 |
|------|------|------|
| Viewport 全黑 | CUDA 架构不匹配 | 确认 GPU 型号，检查 `premake5.lua` 中 `cudaArchs` 是否包含对应 `sm_XX` |
| `no kernel image is available` | nvcc 未为目标 GPU 编译内核 | 添加对应 `-gencode=arch=compute_XX,code=sm_XX` |
| `CUDA_PATH` 未设置 | 环境变量缺失 | 系统属性 → 环境变量 → 新建 `CUDA_PATH`，指向 CUDA Toolkit 目录 |
| 构建时 `.cu` 文件未编译 | `CUDA_PATH` 未在生成时生效 | 重启终端，确认 `echo %CUDA_PATH%` 非空后重新 `Setup.bat` |
| 链接器报 `CUDARenderer_*` 未定义 | `CUDARenderer.obj` 未被链接 | 检查 `premake5.lua` 中 `linkoptions { "$(IntDir)CUDARenderer.obj" }` |

## LICENCE
Le projet utilise la `Licence MIT`.

## Copyright
© Bonity, 2024