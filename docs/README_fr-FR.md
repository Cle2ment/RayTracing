# Ray Tracing : Démo de Ray Tracing Accéléré

[English](/README.md) | [中文](/docs/README_zh-CN.md) | [Français](/docs/README_fr-FR.md)

[![License: MIT](https://img.shields.io/badge/License-MIT-green)](LICENSE)

[![Release](https://img.shields.io/github/v/release/Cle2ment/RayTracing?label=Release&color=blue)](https://github.com/Cle2ment/RayTracing/releases)
[![Build](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml/badge.svg)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml)

[![C++23](https://img.shields.io/badge/C%2B%2B-23-00599C?logo=c%2B%2B&logoColor=white)](https://en.cppreference.com/w/)
[![CUDA](https://img.shields.io/badge/GPU-CUDA-76B900?logo=nvidia&logoColor=white)](https://developer.nvidia.com/cuda-toolkit)
[![ISPC](https://img.shields.io/badge/CPU_Accel-ISPC-0071C5?logo=intel&logoColor=white)](https://ispc.github.io/)
[![Python](https://img.shields.io/badge/Test-Python_3-3776AB?logo=python&logoColor=white)](https://python.org/)

[![uv](https://img.shields.io/badge/Package-uv-7248B9?logo=uv&logoColor=white)](https://docs.astral.sh/uv/)
[![Xmake](https://img.shields.io/badge/Build-Xmake-brightgreen)](https://xmake.io/)

[![DeepSeek](https://img.shields.io/badge/Translate-DeepSeek-5786FE?logo=deepseek)](https://deepseek.com/)

## Présentation

Un path tracer interactif temps réel construit en C++23 sur le framework d'application [Peanut](https://github.com/Cle2ment/Peanut). **Accéléré par GPU via NVIDIA CUDA** et **accéléré par CPU via Intel ISPC** — l'ensemble du pipeline de path tracing s'exécute sur le GPU lorsque CUDA est disponible, avec un repli CPU SIMD via ISPC (AVX2/AVX-512).

### Backends de rendu

| Backend | Accélération | Utilisé quand |
|---------|-------------|---------------|
| **CUDA GPU** | GPU NVIDIA (SM 7.5+) | `CUDA_PATH` détecté |
| **ISPC CPU** | SIMD AVX2 + AVX-512 | `vendor/ispc/bin/ispc.exe` détecté |
| **C++ CPU** | Multi-thread `std::execution::par` | Repli |

### Architecture

| Composant | CPU | GPU (CUDA) |
|-----------|-----|------------|
| Génération de rayons | `foreach` ISPC ou `std::execution::par` | Kernel CUDA — un thread par pixel |
| Intersection Rayon-Sphère | Boucle force brute | Fonction `__device__` |
| Path Tracing (5 rebonds) | BRDF GGX Microfacet | BRDF GGX Microfacet |
| Génération de nombres aléatoires | Hachage PCG | Hachage PCG (`__device__`) |
| Roulette russe | Après 3 rebonds | Après 3 rebonds |
| Affichage | Peanut::Image (Vulkan) | Peanut::Image (Vulkan) via copie D2H |

**Disposition du kernel GPU** : blocs de threads 16×16, un thread CUDA par pixel.

## Démarrage rapide

```bash
# Cloner avec les sous-modules (RECOMMANDÉ)
git clone --recursive https://github.com/Cle2ment/RayTracing.git
cd RayTracing

# Si vous avez cloné sans --recursive, initialisez les sous-modules manuellement :
git submodule update --init --recursive

# Configuration, build et génération de solution VS en un clic
scripts\Setup.bat

# Exécuter le path tracer
xmake run RayTracing
```

## Build & Exécution

### Prérequis

| Dépendance | Requis | Notes |
|-----------|--------|-------|
| [Visual Studio 2026](https://visualstudio.microsoft.com/) (ou 2022) | ✅ | Charge de travail C++ desktop, MSVC v143+ |
| [Vulkan SDK 1.4+](https://vulkan.lunarg.com/) | ✅ | Définir la variable d'environnement `VULKAN_SDK` |
| [CUDA Toolkit 12.0+](https://developer.nvidia.com/cuda-downloads) | Optionnel | Définir `CUDA_PATH` ; détection automatique par xmake |
| [ISPC](https://ispc.github.io/) | Optionnel | Téléchargé automatiquement par `Setup.bat` → `vendor/ispc/` |
| [.NET SDK](https://dotnet.microsoft.com/) | Optionnel | Requis uniquement pour la migration `.sln` → `.slnx` |
| [xmake](https://xmake.io/) | Automatique | `Setup.bat` utilise xmake ; installer globalement pour utilisation en ligne de commande |

### Méthode 1 : Un clic (Setup.bat)

```bash
scripts\Setup.bat
```

Cette commande unique gère tout :

1. Vérifie le sous-module Peanut — initialisation automatique si manquant
2. Configure xmake (`xmake f -m release`)
3. Construit toutes les cibles (`xmake build`) — Peanut.lib + RayTracing.exe
4. Génère la solution Visual Studio (`xmake project -k vsxmake`)
5. Convertit `.sln` → `.slnx` via `dotnet sln migrate`

**Sortie** :

- `build/windows/x64/release/RayTracing.exe`
- `vsxmake2026/RayTracing.slnx` — ouvrez ce fichier dans Visual Studio

### Méthode 2 : Build en ligne de commande

```bash
# Configuration et build (Release)
xmake f -m release
xmake build

# Exécution
xmake run RayTracing

# Build Debug
xmake f -m debug
xmake build
```

### Méthode 3 : Visual Studio

```bash
# Générer la solution VS (après le build initial)
xmake project -k vsxmake -y -m release

# Convertir en .slnx
dotnet sln vsxmake2026\RayTracing.sln migrate
```

Ouvrez `vsxmake2026\RayTracing.slnx` dans Visual Studio, définissez RayTracing comme projet de démarrage, et appuyez sur F5.

> **Remarque** : Après avoir modifié `xmake.lua`, relancez `xmake project -k vsxmake -y -m release && dotnet sln vsxmake2026\RayTracing.sln migrate` pour actualiser les fichiers de projet VS.

### Matrice de build

| Commande | CUDA | ISPC | Sortie |
|---------|------|------|--------|
| `xmake f -m release && xmake build` | Détection automatique | Détection automatique | `build/windows/x64/release/RayTracing.exe` |
| `xmake f -m debug && xmake build` | Détection automatique | Détection automatique | `build/windows/x64/debug/RayTracing.exe` |
| `xmake run RayTracing` | — | — | Exécute l'exécutable construit |

## Structure des fichiers

```
RayTracing/
├── RayTracing/src/             # Code source de l'application
│   ├── PeanutApp.cpp           # Point d'entrée, interface ImGui, configuration de la scène
│   ├── Renderer.h/cpp          # Rendu (dispatch CPU/GPU/ISPC)
│   ├── Camera.h/cpp            # Caméra FPS, pré-calcul des directions de rayons
│   ├── Ray.h                   # Structure Ray
│   ├── Scene.h                 # Données Material, Sphere, Scene
│   ├── PathTracer.ispc         # Kernel de path tracing SIMD ISPC
│   ├── CUDATypes.cuh           # Structures de données GPU
│   ├── CUDARenderer.cuh        # Kernels GPU + fonctions device
│   ├── CUDARenderer.cu         # Wrappers hôte CUDA (liaison C)
│   ├── CUDARenderer.h          # Interface hôte C++ + fonctions de packing
│   ├── VkCUDAInterop.h/cpp     # Partage mémoire zéro-copie Vulkan-CUDA
│   └── OptiXDenoiser.h/cpp     # Intégration du débruitage AI OptiX
├── xmake.lua                   # Configuration de build (détection CUDA + ISPC + OptiX)
├── scripts/
│   ├── Setup.bat               # Build et génération de solution en un clic
│   └── golden/                  # Test d'image de référence Python (géré par uv)
│       ├── pyproject.toml
│       ├── test_golden.py        # Test de régression SSIM/MSE
│       └── uv.lock
├── test/golden/                 # Images de référence CPU
├── RayTracing/tools/            # Rendu sans interface
│   └── GoldenRenderer.cpp       # Path tracer CPU en ligne de commande pour CI
├── Peanut/                     # Sous-module Git (fork indépendant, modifiable pour des améliorations générales)
│   ├── Peanut/src/             # Framework Peanut
│   ├── vendor/GLFW/            # Gestion des fenêtres GLFW
│   ├── vendor/imgui/           # Bibliothèque d'interface ImGui
│   └── vendor/glm/             # Bibliothèque mathématique GLM
├── Directory.Build.props       # Configuration IntelliSense VS (découverte automatique par MSBuild)
└── .github/workflows/          # CI/CD (build, test, image de référence, release)
```

## Raccourcis clavier

| Touche | Action |
|-------|--------|
| Clic droit + Glisser | Rotation de la caméra |
| W/A/S/D | Déplacement de la caméra |
| Q/E | Descendre/Monter |
| Bouton Render | Déclencher un nouveau rendu |
| Accumulate | Activer le rendu progressif |
| Reset | Effacer le buffer d'accumulation |

## Tests

### Tests unitaires
```bash
xmake build RayTracing_test && xmake run RayTracing_test
# 28 tests Catch2 : GGX BRDF, TraceRay, PCGHash, ConvertToRGBA, etc.
```

### Régression d'image de référence
```bash
xmake build GoldenRenderer
cd scripts/golden
uv sync                    # Installer les dépendances (numpy, scikit-image, Pillow)
uv run python test_golden.py    # Comparer avec les images de référence
uv run python test_golden.py --generate  # Générer de nouvelles images de référence
```
Le CI exécute le test d'image de référence en mode cohérence interne : rendu deux fois dans la même exécution et compare le SSIM.

## Démonstration

![Échantillonnage aléatoire](/screenshots/example-random.png)
![Accumulé](/screenshots/example-accumulate.png)

## Dépannage

| Symptôme | Cause | Solution |
|---------|-------|----------|
| `Peanut\Peanut\src\...` introuvable | Sous-module non initialisé | `git submodule update --init --recursive` |
| `.vcxproj` introuvable dans VS | `vsxmake2026\RayTracing.slnx` obsolète | Relancez `scripts\Setup.bat` ou `xmake project -k vsxmake` puis `dotnet sln vsxmake2026\RayTracing.sln migrate` |
| Le viewport est noir | Incompatibilité d'architecture CUDA | Vérifiez que le GPU supporte `sm_XX` dans `xmake.lua` |
| `no kernel image is available` | nvcc ne cible pas votre GPU | Ajoutez `add_cugencodes("compute_XX", "sm_XX")` correspondant dans `xmake.lua` |
| `CUDA_PATH` non défini / `.cu` non compilé | Variable d'environnement manquante | Définissez `CUDA_PATH` dans les variables d'environnement système, redémarrez le terminal |
| `cannot match add_files("Peanut\Peanut\src\**.cpp")` | `git submodule update --init` non exécuté | Voir la première ligne ci-dessus |
| ISPC introuvable (pas de SIMD) | ISPC absent de `vendor/ispc/` | `Setup.bat` le télécharge automatiquement ; relancez si nécessaire |
| `dotnet sln migrate` échoue | .NET SDK non installé | Installez le [.NET SDK](https://dotnet.microsoft.com/) ou ouvrez `vsxmake2026\RayTracing.sln` directement |

## Licence

Licence MIT. Voir [LICENSE](LICENSE) pour les détails.

## Copyright

Copyright (c) 2026 Cle2ment
