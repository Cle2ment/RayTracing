# Ray Tracing: Démo RT accélérée

[English](/README.md) | [中文](/docs/README_zh-CN.md) | [Français](/docs/README_fr-FR.md)

![Static Badge](https://img.shields.io/badge/Language-C++23-blue?logo=cplusplus)
![Static Badge](https://img.shields.io/badge/GPU-CUDA-green?logo=nvidia)
![Static Badge](https://img.shields.io/badge/CPU-ISPC-cyan?logo=intel)
![Static Badge](https://img.shields.io/badge/Built_by-Premake-blue?logo=lua)
![Static Badge](https://img.shields.io/badge/License-MIT-green)
[![Build](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml/badge.svg)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml)

## Aperçu

Un path tracer interactif en temps réel construit avec C++23 sur le framework d'application [Walnut](https://github.com/TheCherno/Walnut). **Accéléré GPU via NVIDIA CUDA** et **accéléré CPU via Intel ISPC** — l'ensemble du pipeline de path tracing s'exécute sur le GPU lorsque CUDA est disponible, avec un fallback CPU SIMD via ISPC (AVX2/AVX-512).

### Backends de rendu

| Backend | Accélération | Quand utilisé |
|---------|-------------|---------------|
| **CUDA GPU** | GPU NVIDIA (SM 7.5+) | `CUDA_PATH` détecté |
| **ISPC CPU** | AVX2 + AVX-512 SIMD | `vendor/ispc/bin/ispc.exe` détecté |
| **C++ CPU** | `std::execution::par` multi-threaded | Secours |

### Architecture

| Composant | CPU | GPU (CUDA) |
|-----------|-----|------------|
| Génération de rayons | ISPC `foreach` ou `std::execution::par` | Kernel CUDA — un thread par pixel |
| Intersection rayon-sphère | Boucle force brute | Fonction `__device__` |
| Path Tracing (5 rebonds) | BRDF Lambertienne diffuse | BRDF Lambertienne diffuse |
| Génération de nombres aléatoires | PCG Hash | PCG Hash (`__device__`) |
| Roulette russe | Après 3 rebonds | Après 3 rebonds |
| Affichage | Walnut::Image (Vulkan) | Walnut::Image (Vulkan) via copie D2H |

**Disposition des kernels GPU** : Blocs de threads 16×16, un thread CUDA par pixel.

## Prérequis

- **GPU NVIDIA** (optionnel) avec capacité de calcul ≥ 7.5
  - sm_75 : GTX 16xx, RTX 20xx
  - sm_86 : RTX 30xx
  - sm_89 : RTX 40xx
  - sm_120 : RTX 50xx
- **CUDA Toolkit 12.0+** (optionnel, 13.x recommandé)
- **Vulkan SDK 1.4+**
- **Visual Studio 2026** (ou 2022) avec support C++23
- **ISPC** — téléchargé automatiquement par `scripts\Setup.bat`, aucune installation manuelle nécessaire

## Démarrage rapide

```bash
# Clonez avec les sous-modules
git clone --recursive https://github.com/Cle2ment/RayTracing.git
cd RayTracing

# Générer la solution VS2026 (télécharge automatiquement premake5 + ISPC)
scripts\Setup.bat

# Ouvrez RayTracing.slnx dans Visual Studio, construisez Release x64
```

## Structure des fichiers

```
RayTracing/
├── src/
│   ├── WalnutApp.cpp          # Point d'entrée, interface ImGui, configuration de la scène
│   ├── Renderer.h/cpp         # Rendu (répartition CPU/GPU/ISPC)
│   ├── Camera.h/cpp           # Caméra FPS, pré-calcul de la direction des rayons
│   ├── Ray.h                  # Structure Ray
│   ├── Scene.h                # Données Material, Sphere, Scene
│   ├── PathTracer.ispc        # Kernel ISPC SIMD de path tracing
│   ├── CUDATypes.cuh          # Structures de données GPU
│   ├── CUDARenderer.cuh       # Kernels GPU + fonctions device
│   ├── CUDARenderer.cu        # Wrappers hôte CUDA (liaison C)
│   └── CUDARenderer.h         # Interface hôte C++ + helpers de packing
├── premake5.lua               # Configuration de build (détection CUDA + ISPC)
├── scripts/Setup.bat          # Génération de projet en un clic
└── .github/workflows/         # CI/CD (CUDA 13.3 + Vulkan)
```

## Raccourcis clavier

| Touche | Action |
|--------|--------|
| Bouton droit de la souris + glisser | Tourner la caméra |
| W/A/S/D | Déplacer la caméra |
| Q/E | Descendre/monter |
| Bouton de rendu | Déclencher un nouveau rendu |
| Accumuler | Activer/désactiver le rendu progressif |
| Réinitialiser | Vider le tampon d'accumulation |

## Démonstration

![Échantillonnage aléatoire](screenshots/example-random.png)
![Accumulé](screenshots/example-accumulate.png)

## Dépannage

| Symptôme | Cause | Solution |
|----------|-------|----------|
| La zone d'affichage est noire | Incompat
