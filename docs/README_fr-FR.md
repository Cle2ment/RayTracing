# Ray Tracing — Traceur de chemins accéléré par GPU NVIDIA

[English](README.md) | [中文](docs/README_zh-CN.md) | [Français](/docs/README_fr-FR.md)

![Static Badge](https://img.shields.io/badge/Inspired_by-TheCherno-yellow?logo=Github)
![Static Badge](https://img.shields.io/badge/Language-C++20-blue?logo=cplusplus)
![Static Badge](https://img.shields.io/badge/GPU-CUDA-green?logo=nvidia)
![Static Badge](https://img.shields.io/badge/Built_by-Premake-blue?logo=lua)
![Static Badge](https://img.shields.io/badge/License-MIT-green)

## Description

Un traceur de chemins interactif en temps réel construit avec C++20 et le framework d’application Walnut. **Accéléré par GPU via NVIDIA CUDA** — l’ensemble du pipeline de lancer de rayons (génération, intersection, ombrage, accumulation) s’exécute sur le GPU. Retour au rendu multithreadé CPU lorsque CUDA n’est pas disponible.

### Architecture

| Composant | CPU multithreadé | GPU (CUDA) |
|-----------|---------------------|------------------------|
| Génération de rayons | `std::execution::par` sur les threads CPU | Kernel CUDA — un thread par pixel |
| Intersection rayon-sphère | Boucle force brute (CPU) | Fonction `__device__` (GPU) |
| Traçage de chemins (5 rebonds) | Boucle scalaire CPU | Parallélisme SIMT GPU |
| Génération de nombres aléatoires | PCG Hash (CPU) | PCG Hash (`__device__` GPU) |
| Tampon d’accumulation | `glm::vec4[]` hôte | `float4[]` périphérique |
| Affichage | Walnut::Image (Vulkan) | Walnut::Image (Vulkan) via copie D2H |
| Roulette russe | CPU | GPU |

**Disposition du kernel GPU** : blocs de threads 16×16, chaque pixel obtient un thread CUDA. Le `RenderKernel` effectue le traçage de chemins complet par pixel, incluant l’intersection rayon-sphère, la terminaison par roulette russe, l’échantillonnage de la BRDF lambertienne diffuse et l’accumulation progressive.

## Prérequis

- **GPU NVIDIA**（可选）avec une capacité de calcul ≥ 7.5（Turing / Ampere / Ada / Blackwell）
  - sm_75 : GTX 16xx, RTX 20xx
  - sm_86 : RTX 30xx
  - sm_89 : RTX 40xx
  - sm_120 : RTX 50xx
- **CUDA Toolkit 12.0+**（可选，推荐 13.x，用于 GPU 加速）
- **Vulkan SDK 1.4+**
- **Visual Studio 2026**（ou 2022, rétrocompatible）avec prise en charge C++20

## Comment construire

### 1. Cloner le dépôt
```bash
git clone --recursive https://github.com/Cle2ment/RayTracing.git
cd RayTracing
```

### 2. Installer les dépendances
- **[CUDA Toolkit](https://developer.nvidia.com/cuda-downloads)** — Requis pour le rendu GPU. L’installateur définit automatiquement la variable d’environnement `CUDA_PATH`.
- **[Vulkan SDK](https://vulkan.lunarg.com/)** — Requis pour l’affichage. Installer dans l’emplacement par défaut.

### 3. Générer les fichiers de projet
```bash
cd scripts
Setup.bat
```
Ceci exécute Premake5 pour générer les fichiers de solution Visual Studio 2026. Le système de build détecte automatiquement CUDA et active l’accélération GPU.

### 4. Compiler et exécuter
Ouvrez `RayTracing.slnx` dans Visual Studio 2026 et compilez (les modes Release ou Dist sont recommandés pour les performances).

### Compilation sans CUDA
Si le CUDA Toolkit n’est pas installé, le projet se compile en tant que traceur de chemins CPU uniquement utilisant `std::execution::par`. Le système de build définit `WL_CUDA` uniquement lorsque CUDA est détecté.

## Pipeline de rendu

```
Camera (CPU)                    Scene (CPU)
    │                                │
    ├─ Directions de rayon ─┐    ┌─── Sphères + matériaux
    │                  │         │
    ▼                  ▼         ▼
┌─────────────────────────────────────────┐
│           CUDARenderer.cu               │
│                                         │
│  RenderKernel <<<grid, block>>>         │
│    └─ PerPixel(x, y)                    │
│         └─ pour bounce de 0 à 5 :      │
│              └─ TraceRay()              │
│                   └─ boucle sphères (GPU)│
│              └─ Roulette russe          │
│              └─ BRDF diffuse            │
│    └─ Accumuler + mappage tonal         │
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
│   ├── WalnutApp.cpp          # Point d’entrée, interface ImGui, configuration de scène
│   ├── Renderer.h/cpp         # Classe Renderer (distribution CPU/GPU)
│   ├── Camera.h/cpp           # Caméra (CPU), génération des directions de rayon
│   ├── Ray.h                  # Structure Ray (CPU)
│   ├── Scene.h                # Données de scène (sphères CPU + matériaux)
│   ├── CUDATypes.cuh          # Structures de données GPU
│   ├── CUDARenderer.cuh       # Kernels GPU + fonctions device
│   ├── CUDARenderer.cu        # Wrappers hôtes CUDA (liaison C)
│   └── CUDARenderer.h         # Interface hôte C++ + helpers d’empaquetage
├── premake5.lua               # Configuration de build (+ support CUDA)
├── .github/workflows/build.yml # Pipeline CI/CD
└── README.md
```

## CI/CD

[![Build (CUDA + Vulkan)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml/badge.svg)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml)

GitHub Actions compile à chaque push et pull request :
- **Windows Server 2022** avec CUDA 12.8 + Vulkan SDK
- Configurations Debug et Release
- Artéfacts de compilation disponibles en téléchargement pour les builds Release

## Raccourcis clavier

| Touche | Action |
|--------|--------|
| Clic droit + glisser | Rotation de la caméra |
| W/A/S/D | Déplacement de la caméra |
| Q/E | Descendre/monter |
| Bouton Render | Déclencher un nouveau rendu |
| Case Accumulate | Activer/désactiver le rendu progressif |

## Démonstration

Le projet de lancer de rayons est encore en développement.

Voici la démonstration actuelle du projet.\
![Ray Tracing Default Example](https://github.com/BoningtonChen/RayTracing/blob/master/Materials/RayTracing-example01.png)
![Ray Tracing Example](https://github.com/BoningtonChen/RayTracing/blob/master/Materials/RayTracing-example02.png)

## À propos de WalnutAppTemplate
- **Description**\
Ceci est un modèle d’application simple pour Walnut – contrairement à l’exemple fourni dans le dépôt Walnut, celui-ci garde Walnut comme sous-module externe et est bien plus pratique pour construire des applications réelles. Voir le dépôt Walnut pour plus de détails.
- **Pour commencer**\
Une fois cloné, vous pouvez personnaliser les fichiers `premake5.lua` et `WalnutApp/premake5.lua` à votre guise (par exemple, changer le nom de "WalnutApp" en autre chose). Une fois satisfait, exécutez `scripts/Setup.bat` pour générer les fichiers de solution/projet Visual Studio 2022. Votre application se trouve dans le dossier `WalnutApp/`, avec un exemple de code basique dans `WalnutApp/src/WalnutApp.cpp`. Je recommande de modifier ce projet WalnutApp pour créer votre propre application, car tout est déjà configuré et prêt.

## Dépannage

| 现象 | 原因 | 解决 |
|------|------|------|
| Viewport noir | Architecture CUDA non compatible | Vérifiez le modèle de votre GPU, assurez-vous que `cudaArchs` dans `premake5.lua` contient le `sm_XX` correspondant |
| `no kernel image is available` | nvcc n’a pas compilé le noyau pour le GPU cible | Ajoutez le paramètre `-gencode=arch=compute_XX,code=sm_XX` approprié |
| `CUDA_PATH` non défini | Variable d’environnement manquante | Propriétés système → Variables d’environnement → Nouvelle variable `CUDA_PATH` pointant vers le répertoire CUDA Toolkit |
| Le fichier `.cu` n’est pas compilé lors de la construction | `CUDA_PATH` non pris en compte lors de la génération | Redémarrez le terminal, vérifiez que `echo %CUDA_PATH%` retourne un chemin non vide, puis relancez `Setup.bat` |
| Erreur du linker `CUDARenderer_*` non défini | `CUDARenderer.obj` n’est pas lié | Vérifiez dans `premake5.lua` la ligne `linkoptions { "$(IntDir)CUDARenderer.obj" }` |

## LICENSE
Le projet utilise la **MIT License**.

## Copyright
© Bonity, 2024