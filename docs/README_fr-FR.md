# Lancer de rayons : Démo RT accélérée

[English](/README.md) | [中文](/docs/README_zh-CN.md) | [Français](/docs/README_fr-FR.md)

![Static Badge](https://img.shields.io/badge/License-MIT-green)
<br/>
![Static Badge](https://img.shields.io/badge/Language-C++23-00599C?logo=cplusplus)
![Static Badge](https://img.shields.io/badge/GPU-CUDA-76B900?logo=nvidia)
![Static Badge](https://img.shields.io/badge/CPU_Acceleration-ISPC-0071C5?logo=intel)
![Static Badge](https://img.shields.io/badge/Project-Xmake-brightgreen?logo=data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA2NCA2NCI+PHBvbHlnb24gcG9pbnRzPSI2NCw0IDY0LDYyIDYyLDY0IDEsNjQgMCw2MyAwLDQwIDYwLDMiIGZpbGw9IiNmZmYiLz48L3N2Zz4=)
<br/>
![Static Badge](https://img.shields.io/badge/Auto_Translation-Deepseek-5786FE?logo=deepseek)
<br/>
[![Build](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml/badge.svg)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml)

## Aperçu

Un traceur de chemins interactif en temps réel construit avec C++23 sur le framework d'application [Peanut](https://github.com/Cle2ment/Peanut). **Accéléré GPU via NVIDIA CUDA** et **accéléré CPU via Intel ISPC** — l'ensemble du pipeline de lancer de chemins s'exécute sur le GPU lorsque CUDA est disponible, avec un repli CPU SIMD via ISPC (AVX2/AVX-512).

### Moteurs de rendu

| Moteur | Accélération | Quand utilisé |
|---------|-------------|-----------|
| **CUDA GPU** | NVIDIA GPU (SM 7.5+) | `CUDA_PATH` détecté |
| **ISPC CPU** | AVX2 + AVX-512 SIMD | `vendor/ispc/bin/ispc.exe` détecté |
| **C++ CPU** | `std::execution::par` multi-thread | Repli |

### Architecture

| Composant | CPU | GPU (CUDA) |
|-----------|-----|------------|
| Génération de rayons | ISPC `foreach` ou `std::execution::par` | Noyau CUDA — un thread par pixel |
| Intersection rayon-sphère | Boucle force brute | Fonction `__device__` |
| Lancer de chemin (5 rebonds) | BRDF GGX Microfacet | BRDF GGX Microfacet |
| Génération de nombres aléatoires | PCG Hash | PCG Hash (`__device__`) |
| Roulette russe | Après 3 rebonds | Après 3 rebonds |
| Affichage | Peanut::Image (Vulkan) | Peanut::Image (Vulkan) via copie D2H |

**Disposition du noyau GPU** : blocs de 16×16 threads, un thread CUDA par pixel.

## Démarrage rapide

```bash
# Clone with submodules (RECOMMENDED)
git clone --recursive https://github.com/Cle2ment/RayTracing.git
cd RayTracing

# If you cloned without --recursive, initialize submodules manually:
git submodule update --init --recursive

# One-click setup, build, and VS solution generation
scripts\Setup.bat

# Run the path tracer
xmake run RayTracing
```

## Construction et exécution

### Prérequis

| Dépendance | Requis | Remarques |
|-----------|----------|-------|
| [Visual Studio 2026](https://visualstudio.microsoft.com/) (ou 2022) | ✅ | Charge de travail C++ de bureau, MSVC v143+ |
| [Vulkan SDK 1.4+](https://vulkan.lunarg.com/) | ✅ | Définir la variable d'environnement `VULKAN_SDK` |
| [CUDA Toolkit 12.0+](https://developer.nvidia.com/cuda-downloads) | Optionnel | Définir `CUDA_PATH` ; détection automatique par xmake |
| [ISPC](https://ispc.github.io/) | Optionnel | Téléchargé automatiquement par `Setup.bat` → `vendor/ispc/` |
| [.NET SDK](https://dotnet.microsoft.com/) | Optionnel | Requis uniquement pour la migration `.sln` → `.slnx` |
| [xmake](https://xmake.io/) | Auto | `Setup.bat` utilise xmake ; installer globalement pour utilisation en ligne de commande |

### Méthode 1 : En un clic (Setup.bat)

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

### Méthode 2 : Construction en ligne de commande

```bash
# Configure and build (Release)
xmake f -m release
xmake build

# Run
xmake run RayTracing

# Debug build
xmake f -m debug
xmake build
```

### Méthode 3 : Visual Studio

```bash
# Generate VS solution (after initial build)
xmake project -k vsxmake -y -m release

# Convert to .slnx
dotnet sln vsxmake2026\RayTracing.sln migrate
```

Ouvrez `vsxmake2026\RayTracing.slnx` dans Visual Studio, définissez RayTracing comme projet de démarrage, et appuyez sur F5.

> **Remarque** : Après avoir modifié `xmake.lua`, réexécutez `xmake project -k vsxmake -y -m release && dotnet sln vsxmake2026\RayTracing.sln migrate` pour rafraîchir les fichiers du projet VS.

### Matrice de construction

| Commande | CUDA | ISPC | Sortie |
|---------|------|------|--------|
| `xmake f -m release && xmake build` | Détection automatique | Détection automatique | `build/windows/x64/release/RayTracing.exe` |
| `xmake f -m debug && xmake build` | Détection automatique | Détection automatique | `build/windows/x64/debug/RayTracing.exe` |
| `xmake run RayTracing` | — | — | Exécute l'exécutable construit |

## Structure des fichiers

```
RayTracing/
├── RayTracing/src/             # Source de l'application
│   ├── PeanutApp.cpp           # Point d'entrée, interface ImGui, configuration de la scène
│   ├── Renderer.h/cpp          # Rendu (dispatch CPU/GPU/ISPC)
│   ├── Camera.h/cpp            # Caméra FPS, pré-calcul des directions de rayons
│   ├── Ray.h                   # Structure Ray
│   ├── Scene.h                 # Données de matériau, sphère, scène
│   ├── PathTracer.ispc         # Noyau de lancer de chemin SIMD ISPC
│   ├── CUDATypes.cuh           # Structures de données GPU
│   ├── CUDARenderer.cuh        # Noyaux GPU + fonctions device
│   ├── CUDARenderer.cu         # Wrappers hôte CUDA (liaison C)
│   ├── CUDARenderer.h          # Interface hôte C++ + helpers de paquetage
│   ├── VkCUDAInterop.h/cpp     # Partage mémoire zéro-copie Vulkan-CUDA
│   └── OptiXDenoiser.h/cpp     # Intégration du débruitage IA OptiX
├── xmake.lua                   # Configuration de construction (détection CUDA + ISPC)
├── scripts/
│   └── Setup.bat               # Construction en un clic + génération de solution
├── Peanut/                     # Sous-module Git (fork indépendant, modifiable pour des améliorations générales)
│   ├── Peanut/src/             # Framework Peanut
│   ├── vendor/GLFW/            # Fenêtrage GLFW
│   ├── vendor/imgui/           # Bibliothèque d'interface ImGui
│   └── vendor/glm/             # Bibliothèque mathématique GLM
├── Directory.Build.props       # Configuration IntelliSense VS (découverte automatique par MSBuild)
└── .github/workflows/          # CI/CD (construction, publication, traduction)
```

## Raccourcis clavier

| Touche | Action |
|-----|--------|
| Bouton droit de la souris + glisser | Rotation de la caméra |
| W/A/S/D | Déplacement de la caméra |
| Q/E | Déplacement vers le bas/haut |
| Bouton de rendu | Déclencher un nouveau rendu |
| Accumuler | Activer/désactiver le rendu progressif |
| Réinitialiser | Vider le buffer d'accumulation |

## Démonstration

![Échantillonnage aléatoire](/screenshots/example-random.png)
![Accumulé](/screenshots/example-accumulate.png)

## Résolution des problèmes

| Symptôme | Cause | Solution |
|---------|-------|----------|
| `Peanut\Peanut\src\...` introuvable | Sous-module non initialisé | `git submodule update --init --recursive` |
| `.vcxproj` introuvable dans VS | `vsxmake2026\RayTracing.slnx` obsolète | Réexécutez `scripts\Setup.bat` ou `xmake project
