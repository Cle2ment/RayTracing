# Ray Tracing: Démonstration de Ray Tracing Accéléré

[English](/README.md) | [中文](/docs/README_zh-CN.md) | [Français](/docs/README_fr-FR.md)

![Static Badge](https://img.shields.io/badge/License-MIT-green)
<br/>
![Static Badge](https://img.shields.io/badge/Langage-C++23-00599C?logo=cplusplus)
![Static Badge](https://img.shields.io/badge/GPU-CUDA-76B900?logo=nvidia)
![Static Badge](https://img.shields.io/badge/Accélération_CPU-ISPC-0071C5?logo=intel)
![Static Badge](https://img.shields.io/badge/Projet-Xmake-brightgreen?logo=data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA2NCA2NCI+PHBvbHlnb24gcG9pbnRzPSI2NCw0IDY0LDYyIDYyLDY0IDEsNjQgMCw2MyAwLDQwIDYwLDMiIGZpbGw9IiNmZmYiLz48L3N2Zz4=)
<br/>
![Static Badge](https://img.shields.io/badge/Traduction_Automatique-Deepseek-5786FE?logo=deepseek)
<br/>
[![Build](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml/badge.svg)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml)

## Aperçu

Un traceur de chemins interactif en temps réel construit avec C++23 sur le framework applicatif [Peanut](https://github.com/Cle2ment/Peanut). **Accéléré GPU via NVIDIA CUDA** et **accéléré CPU via Intel ISPC** — l'ensemble du pipeline de lancer de rayons s'exécute sur le GPU lorsque CUDA est disponible, avec un repli SIMD CPU via ISPC (AVX2/AVX-512).

### Moteurs de rendu

| Moteur | Accélération | Quand est-il utilisé |
|--------|-------------|----------------------|
| **CUDA GPU** | GPU NVIDIA (SM 7.5+) | `CUDA_PATH` détecté |
| **ISPC CPU** | AVX2 + AVX-512 SIMD | `vendor/ispc/bin/ispc.exe` détecté |
| **C++ CPU** | `std::execution::par` multi-thread | Repli |

### Architecture

| Composant | CPU | GPU (CUDA) |
|-----------|-----|------------|
| Génération de rayons | `foreach` ISPC ou `std::execution::par` | Kernel CUDA — un thread par pixel |
| Intersection Rayon-Sphère | Boucle naïve | Fonction `__device__` |
| Lancer de chemins (5 rebonds) | BRDF GGX Microfacet | BRDF GGX Microfacet |
| Génération de nombres aléatoires | PCG Hash | PCG Hash (`__device__`) |
| Roulette russe | Après 3 rebonds | Après 3 rebonds |
| Affichage | Peanut::Image (Vulkan) | Peanut::Image (Vulkan) via copie D2H |

**Disposition des kernels GPU** : blocs de threads 16×16, un thread CUDA par pixel.

## Démarrage rapide

```bash
# Clonez avec les sous-modules (RECOMMANDÉ)
git clone --recursive https://github.com/Cle2ment/RayTracing.git
cd RayTracing

# Si vous avez cloné sans --recursive, initialisez les sous-modules manuellement :
git submodule update --init --recursive

# Configuration, compilation et génération de solution VS en un clic
scripts\Setup.bat

# Lancez le traceur de chemins
xmake run RayTracing
```

## Compilation et exécution

### Prérequis

| Dépendance | Requis | Notes |
|------------|--------|-------|
| [Visual Studio 2026](https://visualstudio.microsoft.com/) (ou 2022) | ✅ | Charge de travail C++ pour postes de travail, MSVC v143+ |
| [Vulkan SDK 1.4+](https://vulkan.lunarg.com/) | ✅ | Définir la variable d'environnement `VULKAN_SDK` |
| [CUDA Toolkit 12.0+](https://developer.nvidia.com/cuda-downloads) | Optionnel | Définir `CUDA_PATH` ; détecté automatiquement par xmake |
| [ISPC](https://ispc.github.io/) | Optionnel | Téléchargé automatiquement par `Setup.bat` → `vendor/ispc/` |
| [.NET SDK](https://dotnet.microsoft.com/) | Optionnel | Nécessaire uniquement pour la migration `.sln` → `.slnx` |
| [xmake](https://xmake.io/) | Automatique | `Setup.bat` utilise xmake ; installez-le globalement pour une utilisation en ligne de commande |

### Méthode 1 : Un clic (Setup.bat)

```bash
scripts\Setup.bat
```

Cette commande unique gère tout :

1. Vérifie le sous-module Peanut — l'initialise automatiquement s'il manque
2. Configure xmake (`xmake f -m release`)
3. Compile toutes les cibles (`xmake build`) — Peanut.lib + RayTracing.exe
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

> **Remarque** : Après avoir modifié `xmake.lua`, relancez `xmake project -k vsxmake -y -m release && dotnet sln vsxmake2026\RayTracing.sln migrate` pour rafraîchir les fichiers de projet VS.

### Matrice de compilation

| Commande | CUDA | ISPC | Sortie |
|----------|------|------|--------|
| `xmake f -m release && xmake build` | Détection automatique | Détection automatique | `build/windows/x64/release/RayTracing.exe` |
| `xmake f -m debug && xmake build` | Détection automatique | Détection automatique | `build/windows/x64/debug/RayTracing.exe` |
| `xmake run RayTracing` | — | — | Exécute l'exécutable compilé |

## Structure des fichiers

```
RayTracing/
├── RayTracing/src/             # Code source de l'application
│   ├── PeanutApp.cpp           # Point d'entrée, interface ImGui, configuration de la scène
│   ├── Renderer.h/cpp          # Moteur de rendu (répartition CPU/GPU/ISPC)
│   ├── Camera.h/cpp            # Caméra FPS, pré-calcul des directions de rayons
│   ├── Ray.h                   # Structure Ray
│   ├── Scene.h                 # Données de matériau, sphère, scène
│   ├── PathTracer.ispc         # Kernel ISPC SIMD de lancer de chemins
│   ├── CUDATypes.cuh           # Structures de données GPU
│   ├── CUDARenderer.cuh        # Kernels GPU + fonctions device
│   ├── CUDARenderer.cu         # Wrappers CUDA côté hôte (liaison C)
│   ├── CUDARenderer.h          # Interface C++ hôte + helpers d'empaquetage
│   ├── VkCUDAInterop.h/cpp     # Partage mémoire zéro-copie Vulkan-CUDA
│   └── OptiXDenoiser.h/cpp     # Intégration du débruitage IA OptiX
├── xmake.lua                   # Configuration de compilation (détection CUDA + ISPC)
├── scripts/
│   └── Setup.bat               # Compilation et génération de solution en un clic
├── Peanut/                     # Sous-module Git (fork indépendant, modifiable pour des améliorations générales)
│   ├── Peanut/src/             # Framework Peanut
│   ├── vendor/GLFW/            # Gestion des fenêtres GLFW
│   ├── vendor/imgui/           # Bibliothèque d'interface ImGui
│   └── vendor/glm/             # Bibliothèque mathématique GLM
├── Directory.Build.props       # Configuration IntelliSense VS (découverte automatique par MSBuild)
└── .github/workflows/          # CI/CD (compilation, publication, traduction)
```

## Raccourcis clavier

| Touche | Action |
|--------|--------|
| Clic droit + glisser | Rotation de la caméra |
| W/A/S/D | Déplacement de la caméra |
| Q/E | Déplacement vers le bas/haut |
| Bouton Rendu | Déclencher un nouveau rendu |
| Accumuler | Activer/désactiver le rendu progressif |
| Réinitialiser | Vider le tampon d'accumulation |

## Démonstration

![Échantillonnage aléatoire](/screenshots/example-random.png)
![Accumulé](/screenshots/example-accumulate.png)

## Dépannage

| Symptôme | Cause | Solution |
|----------|-------|----------|
| `Peanut\Peanut\src\...` introuvable | Sous-module non initialisé | `git submodule update --init --recursive` |
| `.vcxproj` introuvable dans VS | `vsxmake2026\RayTracing.slnx` est obsolète | Relancez `scripts\Setup.bat` ou `xmake project -k vsxmake` puis `dotnet sln vsxmake2026\RayTracing.sln migrate` |
| La fenêtre d'affichage est noire | Incompatibilité d'architecture CUDA | Vérifiez que le GPU supporte `sm_XX` dans `xmake.lua` |
| `no kernel image is available` | nvcc ne cible pas votre GPU | Ajoutez `add_cugencodes("compute_XX", "sm_XX")` correspondant dans `xmake.lua` |
| `CUDA_PATH` non défini / `.cu` non compilé | Variable d'environnement manquante | Définissez `CUDA_PATH` dans les variables d'environnement système, redémarrez le terminal |
| `cannot match add_files("Peanut\Peanut\src\**.cpp")` | `git submodule update --init` pas exécuté | Voir première ligne ci-dessus |
| ISPC introuvable (pas de SIMD) | ISPC absent de `vendor/ispc/` | `Setup.bat` le télécharge automatiquement ; relancez si nécessaire |
| `dotnet sln migrate` échoue | .NET SDK non installé | Installez [.NET SDK](https://dotnet.microsoft.com/) ou ouvrez directement `vsxmake2026\RayTracing.sln` |

## Licence

Licence MIT. Voir [LICENSE](LICENSE) pour les détails.

## Copyright

Copyright (c) 2026 Cle2ment
