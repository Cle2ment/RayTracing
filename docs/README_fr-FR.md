# Ray Tracing : Démo de RT accélérée

[English](/README.md) | [中文](/docs/README_zh-CN.md) | [Français](/docs/README_fr-FR.md)

![Static Badge](https://img.shields.io/badge/Language-C++23-blue?logo=cplusplus)
![Static Badge](https://img.shields.io/badge/GPU-CUDA-green?logo=nvidia)
![Static Badge](https://img.shields.io/badge/CPU-ISPC-cyan?logo=intel)
<br>
![Static Badge](https://img.shields.io/badge/Build-Xmake-brightgreen?logo=data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA2NCA2NCI+PHBvbHlnb24gcG9pbnRzPSI2NCw0IDY0LDYyIDYyLDY0IDEsNjQgMCw2MyAwLDQwIDYwLDMiIGZpbGw9IiNmZmYiLz48L3N2Zz4=)
![Static Badge](https://img.shields.io/badge/Project-Premake-blue?logo=lua)
[![Build](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml/badge.svg)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml)
![Static Badge](https://img.shields.io/badge/License-MIT-green)

## Aperçu

Un traceur de chemins interactif en temps réel construit avec C++23 sur le framework d'application [Walnut](https://github.com/TheCherno/Walnut). **Accéléré GPU via NVIDIA CUDA** et **accéléré CPU via Intel ISPC** — l'ensemble du pipeline de lancer de chemins s'exécute sur le GPU lorsque CUDA est disponible, avec un repli CPU SIMD via ISPC (AVX2/AVX-512).

### Backends de rendu

| Backend | Accélération | Quand utilisé |
|---------|-------------|-----------|
| **CUDA GPU** | GPU NVIDIA (SM 7.5+) | `CUDA_PATH` détecté |
| **ISPC CPU** | AVX2 + AVX-512 SIMD | `vendor/ispc/bin/ispc.exe` détecté |
| **C++ CPU** | `std::execution::par` multi-threadé | Repli |

### Architecture

| Composant | CPU | GPU (CUDA) |
|-----------|-----|------------|
| Génération de rayons | `ISPC foreach` ou `std::execution::par` | Kernel CUDA — un thread par pixel |
| Intersection rayon-sphère | Boucle de force brute | Fonction `__device__` |
| Lancer de chemins (5 rebonds) | BRDF diffuse lambertienne | BRDF diffuse lambertienne |
| Génération de nombres aléatoires | PCG Hash | PCG Hash (`__device__`) |
| Roulette russe | Après 3 rebonds | Après 3 rebonds |
| Affichage | Walnut::Image (Vulkan) | Walnut::Image (Vulkan) via copie D2H |

**Disposition du kernel GPU** : blocs de threads 16×16, un thread CUDA par pixel.

## Prérequis

- **GPU NVIDIA** (optionnel) avec capacité de calcul ≥ 7.5
  - sm_75 : GTX 16xx, RTX 20xx
  - sm_86 : RTX 30xx
  - sm_89 : RTX 40xx
  - sm_120 : RTX 50xx
- **CUDA Toolkit 12.0+** (optionnel, 13.x recommandé)
- **Vulkan SDK 1.4+**
- **Visual Studio 2026** (ou 2022) avec prise en charge de C++23
- **ISPC** — téléchargé automatiquement par `scripts\Setup.bat`, aucune installation manuelle nécessaire

## Démarrage rapide

```bash
# Clone with submodules
git clone --recursive https://github.com/Cle2ment/RayTracing.git
cd RayTracing

# One-click setup & build (auto-downloads ISPC)
scripts\Setup.bat

# Or manual build:
xmake f -m release && xmake build
xmake run RayTracing
```

## Structure des fichiers

```
RayTracing/
├── src/
│   ├── WalnutApp.cpp          # Entry point, ImGui UI, scene setup
│   ├── Renderer.h/cpp         # Renderer (CPU/GPU/ISPC dispatch)
│   ├── Camera.h/cpp           # FPS camera, ray direction pre-computation
│   ├── Ray.h                  # Ray struct
│   ├── Scene.h                # Material, Sphere, Scene data
│   ├── PathTracer.ispc        # ISPC SIMD path tracing kernel
│   ├── CUDATypes.cuh          # GPU data structures
│   ├── CUDARenderer.cuh       # GPU kernels + device functions
│   ├── CUDARenderer.cu        # CUDA host wrappers (C linkage)
│   └── CUDARenderer.h         # Host C++ interface + packing helpers
├── xmake.lua                   # Build config (CUDA + ISPC detection)
├── scripts/Setup.bat          # One-click project generation
└── .github/workflows/         # CI/CD (CUDA 13.3 + Vulkan)
```

## Raccourcis clavier

| Touche | Action |
|--------|--------|
| Bouton droit de la souris + glisser | Rotation de la caméra |
| W/A/S/D | Déplacer la caméra |
| Q/E | Descendre/monter |
| Bouton de rendu | Déclencher un nouveau rendu |
| Accumuler | Activer/désactiver le rendu progressif |
| Réinitialiser | Vider le tampon d'accumulation |

## Démonstration

![Échantillonnage aléatoire](/screenshots/example-random.png)
![Accumulé](/screenshots/example-accumulate.png)

## Dépannage

| Symptôme | Cause | Solution |
|----------|-------|----------|
| La fenêtre d'affichage est noire | Incompatibilité d'architecture CUDA | Vérifiez que le GPU prend en charge `sm_XX` dans `xmake.lua` |
| `no kernel image is available` | nvcc ne cible pas votre GPU | Ajoutez `-gencode=arch=compute_XX,code=sm_XX` |
| `CUDA_PATH` not set | Variable d'environnement manquante | Système → Variables d'environnement → `CUDA_PATH` → répertoire CUDA |
| `.cu` files not compiled | `CUDA_PATH` non défini au moment de la génération | Redémarrez le terminal, `echo %CUDA_PATH%`, relancez `Setup.bat` |
