# Ray Tracing — Path Tracer Accéléré par GPU NVIDIA

[English](/README.md) | [中文](/docs/README_zh-CN.md) | [Français](/docs/README_fr-FR.md)

![Static Badge](https://img.shields.io/badge/Inspired_by-TheCherno-yellow?logo=Github)
![Static Badge](https://img.shields.io/badge/Language-C++23-blue?logo=cplusplus)
![Static Badge](https://img.shields.io/badge/GPU-CUDA-green?logo=nvidia)
![Static Badge](https://img.shields.io/badge/Built_by-Premake-blue?logo=lua)
![Static Badge](https://img.shields.io/badge/License-MIT-green)

## Description

Un path tracer interactif en temps réel construit avec C++23 et le framework applicatif Walnut. **Accéléré par GPU via NVIDIA CUDA** — l'ensemble du pipeline de path tracing (génération de rayons, intersection, shading, accumulation) s'exécute sur le GPU. Retour au rendu CPU multi-threadé lorsque CUDA n'est pas disponible.

### Architecture

| Composant | CPU Multi-Threadé | GPU (CUDA) |
|-----------|-------------------|------------------------|
| Génération de rayons | `std::execution::par` sur les threads CPU | Kernel CUDA — un thread par pixel |
| Intersection rayon-sphère | Boucle force brute (CPU) | Fonction `__device__` (GPU) |
| Path Tracing (5 rebonds) | Boucle scalaire CPU | Parallélisme SIMT GPU |
| Génération de nombres aléatoires | PCG Hash (CPU) | PCG Hash (GPU `__device__`) |
| Tampon d'accumulation | Hôte `glm::vec4[]` | Périphérique `float4[]` |
| Affichage | Walnut::Image (Vulkan) | Walnut::Image (Vulkan) via copie D2H |
| Roulette russe | CPU | GPU |

**Disposition du kernel GPU** : Blocs de threads 16×16, chaque pixel reçoit un thread CUDA. Le `RenderKernel` effectue le path tracing complet par pixel, incluant l'intersection rayon-sphère, la terminaison par roulette russe, l'échantillonnage BRDF diffus Lambertien et l'accumulation progressive.

## Prérequis

- **NVIDIA GPU** (optionnel) avec Compute Capability ≥ 7.5 (Turing / Ampere / Ada / Blackwell)
  - sm_75: GTX 16xx, RTX 20xx
  - sm_86: RTX 30xx
  - sm_89: RTX 40xx
  - sm_120: RTX 50xx
- **CUDA Toolkit 12.0+** (optionnel, 13.x recommandé pour l'accélération GPU)
- **Vulkan SDK 1.4+**
- **Visual Studio 2026** (ou 2022, rétrocompatible) avec support C++23

## Compilation

### 1. Cloner le Dépôt
```bash
git clone --recursive https://github.com/Cle2ment/RayTracing.git
cd RayTracing
```

### 2. Installer les Dépendances
- **[CUDA Toolkit](https://developer.nvidia.com/cuda-downloads)** — Requis pour le rendu GPU. La variable d'environnement `CUDA_PATH` est définie automatiquement par l'installateur.
- **[Vulkan SDK](https://vulkan.lunarg.com/)** — Requis pour l'affichage. Installer à l'emplacement par défaut.

### 3. Générer les Fichiers du Projet
```bash
cd scripts
Setup.bat
```
Ceci exécute **Premake5 5.0.0-beta8** pour générer les fichiers solution Visual Studio 2026. Le script télécharge automatiquement premake5 s'il n'est pas présent (la version fournie avec Walnut ne supporte pas `cppdialect "C++23"`). Le système de build détecte automatiquement CUDA et active l'accélération GPU.

### 4. Compiler et Exécuter
Ouvrir `RayTracing.slnx` dans Visual Studio 2026 et compiler (mode Release ou Dist recommandé pour les performances).

### Compilation Sans CUDA
Si CUDA Toolkit n'est pas installé, le projet compile en tant que path tracer CPU uniquement utilisant `std::execution::par`. Le système de build définit `WL_CUDA` uniquement lorsque CUDA est détecté.

### Accélération ISPC (Optionnelle)
[ISPC](https://github.com/ispc/ispc) (Intel SPMD Program Compiler) fournit une accélération SIMD pour le path tracer CPU. Le script `Setup.bat` télécharge automatiquement ISPC v1.30.0 dans `vendor/ispc/`. Lorsqu'il est détecté, le système de build définit `WL_ISPC` et active le path tracing vectorisé AVX2+AVX-512.

Pour installer manuellement : télécharger `ispc-v1.30.0-windows.zip` depuis la [page des releases ISPC](https://github.com/ispc/ispc/releases/tag/v1.30.0) et extraire son contenu dans `vendor/ispc/`.

## Pipeline de Rendu

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
Walnut::Image (Vulkan) ──► Affichage
```

## Structure des Fichiers

```
RayTracing/
├── src/
│   ├── WalnutApp.cpp          # Point d'entrée, UI ImGui, configuration de scène
│   ├── Renderer.h/cpp         # Classe Renderer (dispatch CPU/GPU)
│   ├── Camera.h/cpp           # Caméra (CPU), génération de directions de rayons
│   ├── Ray.h                  # Structure Ray (CPU)
│   ├── Scene.h                # Données de scène (sphères + matériaux CPU)
│   ├── CUDATypes.cuh          # Structures de données GPU
│   ├── CUDARenderer.cuh       # Kernels GPU + fonctions device
│   ├── CUDARenderer.cu        # Wrappers hôte CUDA (liaison C)
│   └── CUDARenderer.h         # Interface hôte C++ + helpers de packing
├── premake5.lua               # Configuration de build (+ support CUDA)
├── .github/workflows/build.yml # Pipeline CI/CD
└── README.md
```

## CI/CD

[![Build (CUDA + Vulkan)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml/badge.svg)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml)

GitHub Actions compile à chaque push et pull request :
- **Windows Server 2025** avec CUDA 13.3 + Vulkan SDK
- Configurations Debug et Release
- Artefacts de build disponibles au téléchargement pour les builds Release

## Raccourcis Clavier

| Touche | Action |
|--------|--------|
| Clic droit + Glisser | Rotation de la caméra |
| W/A/S/D | Déplacer la caméra |
| Q/E | Descendre/Monter |
| Bouton Render | Déclencher le re-rendu |
| Case Accumulate | Activer/Désactiver le rendu progressif |

## Démonstration

Le projet Ray Tracing est encore en développement.

Voici la démonstration actuelle du projet.\
![Ray Tracing Random](screenshots/example-random.png)
![Ray Tracing Accumulate](screenshots/example-accumulate.png)

## À propos de WalnutAppTemplate
- Description\
Ceci est un template d'application simple pour Walnut — contrairement à l'exemple dans le dépôt Walnut, ceci garde Walnut comme un sous-module externe et est beaucoup plus pertinent pour construire des applications. Voir le dépôt Walnut pour plus de détails.
- Pour commencer\
Une fois cloné, vous pouvez personnaliser les fichiers `premake5.lua` et `WalnutApp/premake5.lua` à votre convenance (par exemple, changer le nom de "WalnutApp" en autre chose). Une fois satisfait, exécutez `scripts/Setup.bat` pour générer les fichiers solution Visual Studio 2022. Votre application se trouve dans le répertoire `WalnutApp/`, avec du code exemple de base pour vous aider à démarrer dans `WalnutApp/src/WalnutApp.cpp`. Je recommande de modifier ce projet WalnutApp pour créer votre propre application, car tout devrait être configuré et prêt à l'emploi.

## Dépannage

| Symptôme | Cause | Solution |
|----------|-------|----------|
| Viewport noir | Incompatibilité d'architecture CUDA | Vérifier le modèle GPU, vérifier que `cudaArchs` dans `premake5.lua` inclut le bon `sm_XX` |
| `no kernel image is available` | nvcc n'a pas compilé pour le GPU cible | Ajouter le `-gencode=arch=compute_XX,code=sm_XX` correspondant |
| `CUDA_PATH` non défini | Variable d'environnement manquante | Propriétés Système → Variables d'environnement → Nouveau `CUDA_PATH`, pointer vers le répertoire CUDA Toolkit |
| Les fichiers `.cu` ne sont pas compilés | `CUDA_PATH` non effectif au moment de la génération | Redémarrer le terminal, vérifier que `echo %CUDA_PATH%` est non vide, puis ré-exécuter `Setup.bat` |
| L'éditeur de liens signale `CUDARenderer_*` non défini | `CUDARenderer.obj` non lié | Vérifier `linkoptions { "$(IntDir)CUDARenderer.obj" }` dans `premake5.lua` |
| `invalid value 'C++23' for cppdialect` | Ancienne version de premake5 fournie avec Walnut | Exécuter `scripts\Setup.bat` (télécharge automatiquement une version plus récente) ou télécharger manuellement premake5 5.0.0-beta8 |

## LICENCE
Le projet utilise la `Licence MIT`.

## Copyright
© Bonity, 2024
