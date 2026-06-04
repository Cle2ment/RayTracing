# Ray Tracing : Démo de Ray Tracing Accéléré

[English](/README.md) | [中文](/docs/README_zh-CN.md) | [Français](/docs/README_fr-FR.md)

![Static Badge](https://img.shields.io/badge/Langue-C++23-blue?logo=cplusplus)
![Static Badge](https://img.shields.io/badge/GPU-CUDA-green?logo=nvidia)
![Static Badge](https://img.shields.io/badge/CPU-ISPC-cyan?logo=intel)
<br>
![Static Badge](https://img.shields.io/badge/Build-Xmake-brightgreen?logo=data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA2NCA2NCI+PHBvbHlnb24gcG9pbnRzPSI2NCw0IDY0LDYyIDYyLDY0IDEsNjQgMCw2MyAwLDQwIDYwLDMiIGZpbGw9IiNmZmYiLz48L3N2Zz4=)
![Static Badge](https://img.shields.io/badge/Projet-Xmake-blue?logo=lua)
[![Build](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml/badge.svg)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml)
![Static Badge](https://img.shields.io/badge/Licence-MIT-green)

## Aperçu

Un traceur de chemins interactif en temps réel construit avec C++23 sur le framework d'application [Walnut](https://github.com/TheCherno/Walnut). **Accéléré par GPU via NVIDIA CUDA** et **accéléré par CPU via Intel ISPC** — l'intégralité du pipeline de tracé de chemins s'exécute sur le GPU lorsque CUDA est disponible, avec un repli CPU SIMD via ISPC (AVX2/AVX-512).

### Moteurs de rendu

| Moteur | Accélération | Quand utilisé |
|--------|--------------|---------------|
| **CUDA GPU** | GPU NVIDIA (SM 7.5+) | Détection de `CUDA_PATH` |
| **ISPC CPU** | AVX2 + AVX-512 SIMD | Détection de `vendor/ispc/bin/ispc.exe` |
| **C++ CPU** | `std::execution::par` multi-thread | Repli |

### Architecture

| Composant | CPU | GPU (CUDA) |
|-----------|-----|------------|
| Génération de rayons | ISPC `foreach` ou `std::execution::par` | Kernel CUDA — un thread par pixel |
| Intersection rayon-sphère | Boucle brute | Fonction `__device__` |
| Tracé de chemins (5 rebonds) | BRDF diffuse Lambertienne | BRDF diffuse Lambertienne |
| Génération de nombres aléatoires | Hachage PCG | Hachage PCG (`__device__`) |
| Roulette russe | Après 3 rebonds | Après 3 rebonds |
| Affichage | Walnut::Image (Vulkan) | Walnut::Image (Vulkan) via copie D2H |

**Disposition du kernel GPU** : blocs de threads 16×16, un thread CUDA par pixel.

## Démarrage rapide

```bash
# Cloner avec les sous-modules (RECOMMANDÉ)
git clone --recursive https://github.com/Cle2ment/RayTracing.git
cd RayTracing

# Si vous avez cloné sans --recursive, initialisez les sous-modules manuellement :
git submodule update --init --recursive

# Configuration, compilation et génération de solution VS en un clic
scripts\Setup.bat

# Exécuter le traceur de chemins
xmake run RayTracing
```

## Compilation et exécution

### Prérequis

| Dépendance | Requis | Notes |
|------------|--------|-------|
| [Visual Studio 2026](https://visualstudio.microsoft.com/) (ou 2022) | ✅ | Charge de travail C++ desktop, MSVC v143+ |
| [Vulkan SDK 1.4+](https://vulkan.lunarg.com/) | ✅ | Définir la variable d'environnement `VULKAN_SDK` |
| [CUDA Toolkit 12.0+](https://developer.nvidia.com/cuda-downloads) | Optionnel | Définir `CUDA_PATH` ; détection automatique par xmake |
| [ISPC](https://ispc.github.io/) | Optionnel | Téléchargé automatiquement par `Setup.bat` → `vendor/ispc/` |
| [.NET SDK](https://dotnet.microsoft.com/) | Optionnel | Requis uniquement pour la migration `.sln` → `.slnx` |
| [xmake](https://xmake.io/) | Automatique | `Setup.bat` utilise xmake ; installer globalement pour usage en ligne de commande |

### Méthode 1 : Un clic (Setup.bat)

```bash
scripts\Setup.bat
```

Cette commande unique gère tout :
1. Vérifie le sous-module Walnut — initialisation automatique si manquant
2. Configure xmake (`xmake f -m release`)
3. Compile toutes les cibles (`xmake build`) — Walnut.lib + RayTracing.exe
4. Génère la solution Visual Studio (`xmake project -k vsxmake`)
5. Convertit `.sln` → `.slnx` via `dotnet sln migrate`

**Sortie** :
- `build/windows/x64/release/RayTracing.exe`
- `vsxmake2026/RayTracing.slnx` — ouvrez ce fichier dans Visual Studio

### Méthode 2 : Compilation en ligne de commande

```bash
# Configurer et compiler (Release)
xmake f -m release
xmake build

# Exécuter
xmake run RayTracing

# Compilation Debug
xmake f -m debug
xmake build
```

### Méthode 3 : Visual Studio

```bash
# Générer la solution VS (après la première compilation)
xmake project -k vsxmake -y -m release

# Convertir en .slnx
dotnet sln vsxmake2026\RayTracing.sln migrate
```

Ouvrez `vsxmake2026\RayTracing.slnx` dans Visual Studio, définissez RayTracing comme projet de démarrage, puis appuyez sur F5.

> **Remarque** : Après avoir modifié `xmake.lua`, réexécutez `xmake project -k vsxmake -y -m release && dotnet sln vsxmake2026\RayTracing.sln migrate` pour actualiser les fichiers de projet VS.

### Matrice de compilation

| Commande | CUDA | ISPC | Sortie |
|----------|------|------|--------|
| `xmake f -m release && xmake build` | Détection auto | Détection auto | `build/windows/x64/release/RayTracing.exe` |
| `xmake f -m debug && xmake build` | Détection auto | Détection auto | `build/windows/x64/debug/RayTracing.exe` |
| `xmake run RayTracing` | — | — | Exécute l'exécutable compilé |

## Structure des fichiers

```
RayTracing/
├── RayTracing/src/             # Source de l'application
│   ├── WalnutApp.cpp           # Point d'entrée, interface ImGui, configuration de scène
│   ├── Renderer.h/cpp          # Moteur de rendu (dispatch CPU/GPU/ISPC)
│   ├── Camera.h/cpp            # Caméra FPS, pré-calcul de direction de rayon
│   ├── Ray.h                   # Structure Ray
│   ├── Scene.h                 # Données de matériau, sphère, scène
│   ├── PathTracer.ispc         # Kernel de tracé de chemins ISPC SIMD
│   ├── CUDATypes.cuh           # Structures de données GPU
│   ├── CUDARenderer.cuh        # Kernels GPU + fonctions device
│   ├── CUDARenderer.cu         # Wrappers hôtes CUDA (liaison C)
│   └── CUDARenderer.h          # Interface hôte C++ + helpers d'empaquetage
├── xmake.lua                   # Configuration de compilation (détection CUDA + ISPC)
├── scripts/
│   └── Setup.bat               # Compilation en un clic + génération de solution
├── Walnut/                     # Sous-module Git — ne pas modifier directement
│   ├── Walnut/src/             # Framework Walnut
│   ├── vendor/glfw/            # Gestion des fenêtres GLFW
│   ├── vendor/imgui/           # Bibliothèque d'interface ImGui
│   └── vendor/glm/             # Bibliothèque mathématique GLM
└── .github/workflows/          # CI/CD (CUDA 13.3 + Vulkan)
```

## Raccourcis clavier

| Touche | Action |
|--------|--------|
| Souris droite + Glisser | Rotation de la caméra |
| W/A/S/D | Déplacement de la caméra |
| Q/E | Déplacement vers le bas/haut |
| Bouton "Render" | Déclencher un nouveau rendu |
| "Accumulate" | Activer/désactiver le rendu progressif |
| "Reset" | Vider le buffer d'accumulation |

## Démonstration

![Échantillonnage aléatoire](/screenshots/example-random.png)
![Accumulé](/screenshots/example-accumulate.png)

## Dépannage

| Symptôme | Cause | Solution |
|----------|-------|----------|
| `Walnut\Walnut\src\...` introuvable | Sous-module non initialisé | `git submodule update --init --recursive` |
| `.vcxproj` introuvable dans VS | `vsxmake2026\RayTracing.slnx` obsolète | Réexécuter `scripts\Setup.bat` ou `xmake project -k vsxmake` puis `dotnet sln vsxmake2026\RayTracing.sln migrate` |
| Fenêtre de rendu noire | Incompatibilité d'architecture CUDA | Vérifier que le GPU supporte `sm_XX` dans `xmake.lua` |
| `no kernel image is available` | nvcc ne cible pas votre GPU | Ajouter `add_cugencodes("compute_XX", "sm_XX")` correspondant dans `xmake.lua` |
| `CUDA_PATH` non défini / `.cu` non compilé | Variable d'environnement manquante | Définir `CUDA_PATH` dans les variables d'environnement système, redémarrer le terminal |
| `cannot match add_files("Walnut\Walnut\src\**.cpp")` | `git submodule update --init` non exécuté | Voir la première ligne ci-dessus |
| ISPC introuvable (pas de SIMD) | ISPC absent de `vendor/ispc/` | `Setup.bat` le télécharge automatiquement ; réexécuter si nécessaire |
| `dotnet sln migrate` échoue | .NET SDK non installé | Installer le [.NET SDK](https://dotnet.microsoft.com/) ou ouvrir `vsxmake2026\RayTracing.sln` directement |

## Licence

Licence MIT. Voir [LICENSE](LICENSE) pour les détails.
