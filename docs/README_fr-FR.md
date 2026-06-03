# Ray Tracing : Démo de Ray Tracing Accéléré

[English](/README.md) | [中文](/docs/README_zh-CN.md) | [Français](/docs/README_fr-FR.md)

![Static Badge](https://img.shields.io/badge/Langage-C++23-blue?logo=cplusplus)
![Static Badge](https://img.shields.io/badge/GPU-CUDA-green?logo=nvidia)
![Static Badge](https://img.shields.io/badge/CPU-ISPC-cyan?logo=intel)
![Static Badge](https://img.shields.io/badge/Développé_avec-Premake-blue?logo=lua)
![Static Badge](https://img.shields.io/badge/Licence-MIT-green)
[![Build](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml/badge.svg)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml)

## Aperçu

Un path tracer interactif en temps réel construit avec C++23 sur le framework d'application [Walnut](https://github.com/TheCherno/Walnut). **Accéléré par GPU via NVIDIA CUDA** et **accéléré par CPU via Intel ISPC** — l'ensemble du pipeline de path tracing s'exécute sur le GPU lorsque CUDA est disponible, avec un repli CPU SIMD via ISPC (AVX2/AVX-512).

### Backends de Rendu

| Backend | Accélération | Quand utilisé |
|---------|--------------|--------------|
| **CUDA GPU** | GPU NVIDIA (SM 7.5+) | `CUDA_PATH` détecté |
| **ISPC CPU** | SIMD AVX2 + AVX-512 | `vendor/ispc/bin/ispc.exe` détecté |
| **C++ CPU** | Multi-thread `std::execution::par` | Repli |

### Architecture

| Composant | CPU | GPU (CUDA) |
|-----------|-----|------------|
| Génération de rayons | `foreach` ISPC ou `std::execution::par` | Kernel CUDA — un thread par pixel |
| Intersection Rayon-Sphère | Boucle brute-force | Fonction `__device__` |
| Path Tracing (5 rebonds) | BRDF diffuse Lambertienne | BRDF diffuse Lambertienne |
| Génération de nombres aléatoires | Hachage PCG | Hachage PCG (`__device__`) |
| Roulette russe | Après 3 rebonds | Après 3 rebonds |
| Affichage | Walnut::Image (Vulkan) | Walnut::Image (Vulkan) via copie D2H |

**Disposition du Kernel GPU** : blocs de threads 16×16, un thread CUDA par pixel.

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
# Cloner avec les sous-modules
git clone --recursive https://github.com/Cle2ment/RayTracing.git
cd RayTracing

# Configuration et compilation en un clic (télécharge automatiquement ISPC)
scripts\Setup.bat

# Ou compilation manuelle :
xmake f -m release && xmake build
xmake run RayTracing
```

## Structure des fichiers

```
RayTracing/
├── src/
│   ├── WalnutApp.cpp          # Point d'entrée, interface ImGui, configuration de la scène
│   ├── Renderer.h/cpp         # Renderer (répartition CPU/GPU/ISPC)
│   ├── Camera.h/cpp           # Caméra FPS, pré-calcul des directions de rayons
│   ├── Ray.h                  # Structure Ray
│   ├── Scene.h                # Données Matériau, Sphère, Scène
│   ├── PathTracer.ispc        # Kernel SIMD ISPC pour le path tracing
│   ├── CUDATypes.cuh          # Structures de données GPU
│   ├── CUDARenderer.cuh       # Kernels GPU + fonctions device
│   ├── CUDARenderer.cu        # Wrappers hôte CUDA (liaison C)
│   └── CUDARenderer.h         # Interface C++ hôte + helpers d'empaquetage
├── xmake.lua                   # Configuration de compilation (détection CUDA + ISPC)
├── scripts/Setup.bat          # Génération de projet en un clic
└── .github/workflows/         # CI/CD (CUDA 13.3 + Vulkan)
```

## Raccourcis clavier

| Touche | Action |
|--------|--------|
| Souris droite + glisser | Rotation de la caméra |
| W/A/S/D | Déplacement de la caméra |
| Q/E | Descendre/monter |
| Bouton Render | Déclencher un nouveau rendu |
| Accumulate | Activer le rendu progressif |
| Reset | Vider le buffer d'accumulation |

## Démonstration

![Échantillonnage aléatoire](/screenshots/example-random.png)
![Accumulé](/screenshots/example-accumulate.png)

## Dépannage

| Symptôme | Cause | Solution |
|----------|-------|----------|
| Le viewport est noir | Incompatibilité d'architecture CUDA | Vérifiez que le GPU supporte `sm_XX` dans `xmake.lua` |
| `no kernel image is available` | nvcc ne cible pas votre GPU | Ajoutez `-gencode=arch=compute_XX,code=sm_XX` |
| `CUDA_PATH` non défini | Variable d'environnement manquante | Système → Variables d'environnement → `CUDA_PATH` → répertoire CUDA |
| Fichiers `.cu` non compilés | `CUDA_PATH` non défini au moment de la génération | Redémarrez le terminal, `echo %CUDA_PATH%`, relancez `Setup.bat` |
| `CUDARenderer_*` non défini | Fichier objet non lié | Vérifiez `linkoptions { "$(IntDir)CUDARenderer.obj" }` |
| `invalid value 'C++23'` | Ancien premake5 fourni avec Walnut | Relancez `scripts\Setup.bat` pour premake5 5.0.0-beta8 |

## Licence

Licence MIT. Voir [LICENSE](LICENSE) pour plus de détails.
