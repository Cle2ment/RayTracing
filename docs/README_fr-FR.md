# Ray Tracing : Démo RT accélérée

[English](/README.md) | [中文](/docs/README_zh-CN.md) | [Français](/docs/README_fr-FR.md)

![Static Badge](https://img.shields.io/badge/Language-C++23-blue?logo=cplusplus)
![Static Badge](https://img.shields.io/badge/GPU-CUDA-green?logo=nvidia)
![Static Badge](https://img.shields.io/badge/CPU-ISPC-cyan?logo=intel)
<br>
![Static Badge](https://img.shields.io/badge/Build-Xmake-brightgreen?logo=xmake)
![Static Badge](https://img.shields.io/badge/Project-Premake-blue?logo=lua)
[![Build](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml/badge.svg)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml)
![Static Badge](https://img.shields.io/badge/License-MIT-green)

## Aperçu

Un traceur de chemin interactif en temps réel construit avec C++23 sur le framework d'application [Walnut](https://github.com/TheCherno/Walnut). **Accéléré par GPU via NVIDIA CUDA** et **accéléré par CPU via Intel ISPC** — l'ensemble du pipeline de path tracing s'exécute sur le GPU lorsque CUDA est disponible, avec un repli CPU SIMD via ISPC (AVX2/AVX-512).

### Moteurs de rendu

| Backend | Accélération | Quand utilisé |
|---------|-------------|-----------|
| **CUDA GPU** | GPU NVIDIA (SM 7.5+) | `CUDA_PATH` détecté |
| **ISPC CPU** | AVX2 + AVX-512 SIMD | `vendor/ispc/bin/ispc.exe` détecté |
| **C++ CPU** | `std::execution::par` multi-threadé | Repli |

### Architecture

| Composant | CPU | GPU (CUDA) |
|-----------|-----|------------|
| Génération des rayons | ISPC `foreach` ou `std::execution::par` | CUDA kernel — un thread par pixel |
| Intersection rayon-sphère | Boucle force brute | Fonction `__device__` |
| Path Tracing (5 rebonds) | BRDF diffuse lambertienne | BRDF diffuse lambertienne |
| Génération de nombres aléatoires | PCG Hash | PCG Hash (`__device__`) |
| Roulette russe | Après 3 rebonds | Après 3 rebonds |
| Affichage | Walnut::Image (Vulkan) | Walnut::Image (Vulkan) via copie D2H |

**Disposition du noyau GPU** : blocs de 16×16 threads, un thread CUDA par pixel.

## Prérequis

- **GPU NVIDIA** (optionnel) avec Compute Capability ≥ 7.5
  - sm_75 : GTX 16xx, RTX 20xx
  - sm_86 : RTX 30xx
  - sm_89 : RTX 40xx
  - sm_120 : RTX 50xx
- **CUDA Toolkit 12.0+** (optionnel, 13.x recommandé)
- **Vulkan SDK 1.4+**
- **Visual Studio 2026** (ou 2022) avec support C++23
- **ISPC** — téléchargé automatiquement par `scripts\Setup.bat`, aucune installation manuelle nécessaire.

## Démarrage rapide

```bash
# Clone avec sous-modules
git clone --recursive https://github.com/Cle2ment/RayTracing.git
cd RayTracing

# Configuration et build en un clic (télécharge automatiquement ISPC)
scripts\Setup.bat

# Ou construction manuelle :
xmake f -m release && xmake build
xmake run RayTracing
```

## Structure des fichiers

```
RayTracing/
├── src/
│   ├── WalnutApp.cpp          # Point d'entrée, interface ImGui, configuration de la scène
│   ├── Renderer.h/cpp         # Moteur de rendu (CPU/GPU/ISPC dispatch)
│   ├── Camera.h/cpp           # Caméra FPS, pré-calcul des directions de rayons
│   ├── Ray.h                  # Structure Ray
│   ├── Scene.h                # Données Matériau, Sphère, Scène
│   ├── PathTracer.ispc        # Noyau de path tracing SIMD ISPC
│   ├── CUDATypes.cuh          # Structures de données GPU
│   ├── CUDARenderer.cuh       # Noyaux GPU + fonctions device
│   ├── CUDARenderer.cu        # Wrappers hôte CUDA (liaison C)
│   └── CUDARenderer.h         # Interface hôte C++ + helpers d'empaquetage
├── xmake.lua                   # Configuration de build (détection CUDA + ISPC)
├── scripts/Setup.bat          # Génération du projet en un clic
└── .github/workflows/         # CI/CD (CUDA 13.3 + Vulkan)
```

## Raccourcis clavier

| Touche | Action |
|--------|--------|
| Bouton droit + glisser | Tourner la caméra |
| W/A/S/D | Déplacer la caméra |
| Q/E | Descendre/monter |
| Bouton Render | Déclencher un nouveau rendu |
| Accumuler | Activer/désactiver le rendu progressif |
| Réinitialiser | Effacer le buffer d'accumulation |

## Démonstration

![Échantillonnage aléatoire](/screenshots/example-random.png)
![Accumulé](/screenshots/example-accumulate.png)

## Dépannage

| Symptôme | Cause | Solution |
|---------|-------|----------|
| La zone d'affichage est noire | Incompatibilité d'architecture CUDA | Vérifiez que le GPU supporte `sm_XX` dans `xmake.lua` |
| `no kernel image is available` | nvcc ne cible pas votre GPU | Ajoutez `-gencode=arch=compute_XX,code=sm_XX` |
| `CUDA_PATH` non défini | Variable d'environnement manquante | Système → Variables d'environnement → `CUDA_PATH` → répertoire CU
