# Lancer de rayons — Traceur de chemins accéléré par GPU NVIDIA

[English](/README.md) | [中文](/docs/README_zh-CN.md) | [Français](/docs/README_fr-FR.md)

![Static Badge](https://img.shields.io/badge/Inspired_by-TheCherno-yellow?logo=Github)
![Static Badge](https://img.shields.io/badge/Language-C++23-blue?logo=cplusplus)
![Static Badge](https://img.shields.io/badge/GPU-CUDA-green?logo=nvidia)
![Static Badge](https://img.shields.io/badge/Built_by-Premake-blue?logo=lua)
![Static Badge](https://img.shields.io/badge/License-MIT-green)

## Description

Un traceur de chemins interactif en temps réel construit avec C++23 et le framework d'application Walnut. **Accéléré par GPU via NVIDIA CUDA** — l'ensemble du pipeline de lancer de rayons (génération de rayons, intersection, ombrage, accumulation) s'exécute sur le GPU. Replie sur un rendu multithreadé CPU lorsque CUDA n'est pas disponible.

### Architecture

| Composant | CPU multithreadé | GPU (CUDA) |
|-----------|------------------|------------|
| Génération de rayons | `std::execution::par` sur les threads CPU | Kernel CUDA — un thread par pixel |
| Intersection rayon-sphère | Boucle brute-force (CPU) | Fonction `__device__` (GPU) |
| Lancer de chemins (5 rebonds) | Boucle scalaire CPU | Parallélisme SIMT GPU |
| Génération de nombres aléatoires | PCG Hash (CPU) | PCG Hash (`__device__` GPU) |
| Tampon d'accumulation | `glm::vec4[]` hôte | `float4[]` périphérique |
| Affichage | Walnut::Image (Vulkan) | Walnut::Image (Vulkan) via copie D2H |
| Roulette russe | CPU | GPU |

**Disposition du kernel GPU** : blocs de threads 16×16, chaque pixel reçoit un thread CUDA. Le `RenderKernel` effectue le lancer de chemins complet par pixel, incluant l'intersection rayon-sphère, la terminaison par roulette russe, l'échantillonnage de la BRDF lambertienne diffuse et l'accumulation progressive.

## Prérequis

- **GPU NVIDIA** (optionnel) avec capacité de calcul ≥ 7.5 (Turing / Ampere / Ada / Blackwell)
  - sm_75 : GTX 16xx, RTX 20xx
  - sm_86 : RTX 30xx
  - sm_89 : RTX 40xx
  - sm_120 : RTX 50xx
- **Kit d'outils CUDA 12.0+** (optionnel, 13.x recommandé pour l'accélération GPU)
- **SDK Vulkan 1.4+**
- **Visual Studio 2026** (ou 2022, rétrocompatible) avec prise en charge C++23

## Comment construire

### 1. Cloner le dépôt
```bash
git clone --recursive https://github.com/Cle2ment/RayTracing.git
cd RayTracing
```

### 2. Installer les dépendances
- **[Kit d'outils CUDA](https://developer.nvidia.com/cuda-downloads)** — Requis pour le rendu GPU. L'installateur définit automatiquement la variable d'environnement `CUDA_PATH`.
- **[SDK Vulkan](https://vulkan.lunarg.com/)** — Requis pour l'affichage. Installez-le dans le chemin par défaut.

### 3. Générer les fichiers de projet
```bash
cd scripts
Setup.bat
```
Ceci exécute **Premake5 5.0.0-beta8** pour générer les fichiers de solution Visual Studio 2026. Le script télécharge automatiquement Premake5 s'il n'est pas présent (la version fournie avec Walnut ne prend pas en charge `cppdialect "C++23"`). Le système de build détecte automatiquement CUDA et active l'accélération GPU.

### 4. Construire et exécuter
Ouvrez `RayTracing.slnx` dans Visual Studio 2026 et construisez (les modes Release ou Dist sont recommandés pour les performances).

### Construire sans CUDA
Si le kit d'outils CUDA n'est pas installé, le projet se construit en tant que traceur de chemins CPU uniquement utilisant `std::execution::par`. Le système de build définit `WL_CUDA` uniquement lorsque CUDA est détecté.

### Accélération ISPC (optionnelle)
[ISPC](https://github.com/ispc/ispc) (Intel SPMD Program Compiler) fournit une accélération SIMD pour le traceur de chemins CPU. Le script `Setup.bat` télécharge automatiquement ISPC v1.30.0 dans `vendor/ispc/`. Lorsqu'il est détecté, le système de build définit `WL_ISPC` et active le lancer de chemins vectorisé AVX2+AVX-512.

Pour une installation manuelle : téléchargez `ispc-v1.30.0-windows.zip` depuis la [page des versions d'ISPC](https://github.com/ispc/ispc/releases/tag/v1.30.0) et extrayez son contenu dans `vendor/ispc/`.

## Pipeline de rendu

```
Caméra (CPU)                    Scène (CPU)
    │                                │
    ├─ Directions des rayons ─┐  ┌─── Sphères + matériaux
    │                          │  │
    ▼                          ▼  ▼
┌─────────────────────────────────────────┐
│           CUDARenderer.cu               │
│                                         │
│  RenderKernel <<<grille, bloc>>>        │
│    └─ PerPixel(x, y)                    │
│         └─ pour rebond dans 0..5 :      │
│              └─ TraceRay()              │
│                   └─ boucle sphères (GPU)│
│              └─ Roulette russe          │
│              └─ BRDF diffuse            │
│    └─ Accumuler + Mappage tonal         │
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
│   ├── WalnutApp.cpp          # Point d'entrée, interface ImGui, configuration de la scène
│   ├── Renderer.h/cpp         # Classe de rendu (répartition CPU/GPU)
│   ├── Camera.h/cpp           # Caméra (CPU), génération des directions de rayons
│   ├── Ray.h                  # Structure Ray (CPU)
│   ├── Scene.h                # Données de la scène (sphères + matériaux CPU)
│   ├── CUDATypes.cuh          # Structures de données GPU
│   ├── CUDARenderer.cuh       # Kernels GPU + fonctions périphériques
│   ├── CUDARenderer.cu        # Wrappers hôtes CUDA (liaison C)
│   └── CUDARenderer.h         # Interface hôte C++ + fonctions d'empaquetage
├── premake5.lua               # Configuration de build (+ prise en charge CUDA)
├── .github/workflows/build.yml # Pipeline CI/CD
└── README.md
```

## CI/CD

[![Build (CUDA + Vulkan)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml/badge.svg)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml)

GitHub Actions construit à chaque push et pull request :
- **Windows Server 2025** avec CUDA 13.3 + SDK Vulkan
- Configurations Debug et Release
- Artefacts de build disponibles en téléchargement pour les builds Release

## Raccourcis clavier

| Touche | Action |
|--------|--------|
| Clic droit + glisser | Tourner la caméra |
| W/A/S/D | Déplacer la caméra |
| Q/E | Descendre / Monter |
| Bouton Rendu | Déclencher un nouveau rendu |
| Case à cocher Accumuler | Activer/désactiver le rendu progressif |

## Démonstration

Le projet de lancer de rayons est encore en développement.

Voici la démonstration actuelle du projet.\
![Ray Tracing Aléatoire](screenshots/example-random.png)
![Ray Tracing Accumulé](screenshots/example-accumulate.png)

## À propos de WalnutAppTemplate
- **Description**\
Ceci est un modèle d'application simple pour Walnut — contrairement à l'exemple dans le dépôt Walnut, celui-ci conserve Walnut en tant que sous-module externe et est beaucoup plus sensé pour construire des applications réelles. Consultez le dépôt Walnut pour plus de détails.
- **Pour commencer**\
Une fois que vous avez cloné, vous pouvez personnaliser les fichiers `premake5.lua` et `WalnutApp/premake5.lua` à votre guise (par exemple, changer le nom de "WalnutApp" en autre chose). Une fois satisfait, exécutez `scripts/Setup.bat` pour générer les fichiers de solution/projet Visual Studio 2022. Votre application se trouve dans le répertoire `WalnutApp/`, avec un exemple de code de base dans `WalnutApp/src/WalnutApp.cpp`. Je recommande de modifier ce projet WalnutApp pour créer votre propre application, car tout est déjà configuré et prêt.

## Dépannage

| Symptôme | Cause | Solution |
|----------|-------|----------|
| La fenêtre d'affichage est noire | Inadéquation d'architecture CUDA | Vérifiez le modèle GPU, assurez-vous que `cudaArchs` dans `premake5.lua` inclut le bon `sm_XX` |
| `no kernel image is available` | nvcc n'a pas compilé pour le GPU cible | Ajoutez le `-gencode=arch=compute_XX,code=sm_XX` correspondant |
| `CUDA_PATH` non défini | Variable d'environnement manquante | Propriétés système → Variables d'environnement → Nouvelle `CUDA_PATH`, pointez vers le répertoire du kit d'outils CUDA |
| Les fichiers `.cu` ne sont pas compilés pendant la build | `CUDA_PATH` non effectif au moment de la génération | Redémarrez le terminal, vérifiez que `echo %CUDA_PATH%` n'est pas vide, puis relancez `Setup.bat` |
| L'éditeur de liens signale `CUDARenderer_*` non défini | `CUDARenderer.obj` non lié | Vérifiez `linkoptions { "$(IntDir)CUDARenderer.obj" }` dans `premake5.lua` |
| `invalid value 'C++23' for cppdialect` | Utilisation d'une vieille version de premake5 fournie avec Walnut | Exécutez `scripts\Setup.bat` (télécharge automatiquement une version plus récente) ou téléchargez manuellement premake5 5.0.0-beta8 |

## Licence
Le projet utilise la `licence MIT`.

## Droits d'auteur
© Bonity, 2024
