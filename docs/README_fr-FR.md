# Ray Tracing — Traceur de Chemins Accéléré par GPU NVIDIA

[English](/README.md) | [中文](/docs/README_zh-CN.md) | [Français](/docs/README_fr-FR.md)

![Static Badge](https://img.shields.io/badge/Inspired_by-TheCherno-yellow?logo=Github)
![Static Badge](https://img.shields.io/badge/Language-C++23-blue?logo=cplusplus)
![Static Badge](https://img.shields.io/badge/GPU-CUDA-green?logo=nvidia)
![Static Badge](https://img.shields.io/badge/Built_by-Premake-blue?logo=lua)
![Static Badge](https://img.shields.io/badge/License-MIT-green)

## Description

Un traceur de chemins interactif en temps réel construit avec C++23 et le framework d'application Walnut. **Accéléré par GPU via NVIDIA CUDA** — l'ensemble du pipeline de lancer de rayons (génération de rayons, intersection, ombrage, accumulation) s'exécute sur le GPU. Retour au rendu multi-thread CPU lorsque CUDA n'est pas disponible.

### Architecture

| Composant | CPU Multi-Thread | GPU (CUDA) |
|-----------|---------------------|------------------------|
| Génération de rayons | `std::execution::par` sur les threads CPU | Kernel CUDA — un thread par pixel |
| Intersection Rayon-Sphère | Boucle brute-force (CPU) | Fonction `__device__` (GPU) |
| Lancer de chemins (5 rebonds) | Boucle scalaire CPU | Parallélisme SIMT GPU |
| Génération de nombres aléatoires | PCG Hash (CPU) | PCG Hash (GPU `__device__`) |
| Tampon d'accumulation | `glm::vec4[]` hôte | `float4[]` périphérique |
| Affichage | Walnut::Image (Vulkan) | Walnut::Image (Vulkan) via copie D2H |
| Roulette russe | CPU | GPU |

**Disposition du kernel GPU** : Blocs de threads 16×16, chaque pixel obtient un thread CUDA. Le `RenderKernel` effectue le lancer de chemins complet par pixel, incluant l'intersection rayon-sphère, la terminaison par roulette russe, l'échantillonnage de la BRDF lambertienne diffuse et l'accumulation progressive.

## Prérequis

- **GPU NVIDIA** (optionnel) avec capacité de calcul ≥ 7.5 (Turing / Ampere / Ada / Blackwell)
  - sm_75 : GTX 16xx, RTX 20xx
  - sm_86 : RTX 30xx
  - sm_89 : RTX 40xx
  - sm_120 : RTX 50xx
- **CUDA Toolkit 12.0+** (optionnel, 13.x recommandé pour l'accélération GPU)
- **Vulkan SDK 1.4+**
- **Visual Studio 2026** (ou 2022, rétrocompatible) avec support C++23

## Comment Construire

### 1. Cloner le Dépôt
```bash
git clone --recursive https://github.com/Cle2ment/RayTracing.git
cd RayTracing
```

### 2. Installer les Dépendances
- **[CUDA Toolkit](https://developer.nvidia.com/cuda-downloads)** — Requis pour le rendu GPU. La variable d'environnement `CUDA_PATH` est définie automatiquement par l'installateur.
- **[Vulkan SDK](https://vulkan.lunarg.com/)** — Requis pour l'affichage. Installer à l'emplacement par défaut.

### 3. Générer les Fichiers de Projet
```bash
cd scripts
Setup.bat
```
Ceci exécute **Premake5 5.0.0-beta8** pour générer les fichiers de solution Visual Studio 2026. Le script télécharge automatiquement premake5 s'il n'est pas présent (la version fournie avec Walnut ne supporte pas `cppdialect "C++23"`). Le système de build détecte automatiquement CUDA et active l'accélération GPU.

### 4. Construire et Exécuter
Ouvrez `RayTracing.slnx` dans Visual Studio 2026 et construisez (les modes Release ou Dist sont recommandés pour les performances).

### Construire Sans CUDA
Si CUDA Toolkit n'est pas installé, le projet se construit comme un traceur de chemins uniquement CPU utilisant `std::execution::par`. Le système de build définit `WL_CUDA` uniquement lorsque CUDA est détecté.

### Accélération ISPC (Optionnelle)
[ISPC](https://github.com/ispc/ispc) (Intel SPMD Program Compiler) fournit une accélération SIMD pour le traceur de chemins CPU. Le script `Setup.bat` télécharge automatiquement ISPC v1.30.0 dans `vendor/ispc/`. Lorsqu'il est détecté, le système de build définit `WL_ISPC` et active le lancer de chemins vectorisé AVX2+AVX-512.

Pour installer manuellement : téléchargez `ispc-v1.30.0-windows.zip` depuis la [page des versions ISPC](https://github.com/ispc/ispc/releases/tag/v1.30.0) et extrayez son contenu dans `vendor/ispc/`.

## Pipeline de Rendu

```
Caméra (CPU)                    Scène (CPU)
    │                                │
    ├─ Directions de rayons ─┐   ┌─── Sphères + Matériaux
    │                        │   │
    ▼                        ▼   ▼
┌─────────────────────────────────────────┐
│           CUDARenderer.cu               │
│                                         │
│  RenderKernel <<<grille, bloc>>>        │
│    └─ ParPixel(x, y)                    │
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

## Structure des Fichiers

```
RayTracing/
├── src/
│   ├── WalnutApp.cpp          # Point d'entrée, interface ImGui, configuration de scène
│   ├── Renderer.h/cpp         # Classe Renderer (distribution CPU/GPU)
│   ├── Camera.h/cpp           # Caméra (CPU), génération de directions de rayons
│   ├── Ray.h                  # Structure Ray (CPU)
│   ├── Scene.h                # Données de scène (sphères CPU + matériaux)
│   ├── CUDATypes.cuh          # Structures de données GPU
│   ├── CUDARenderer.cuh       # Kernels GPU + fonctions périphériques
│   ├── CUDARenderer.cu        # Wrappers hôte CUDA (liaison C)
│   └── CUDARenderer.h         # Interface C++ hôte + helpers d'empaquetage
├── premake5.lua               # Configuration de build (+ support CUDA)
├── .github/workflows/build.yml # Pipeline CI/CD
└── README.md
```

## CI/CD

[![Build (CUDA + Vulkan)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml/badge.svg)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml)

GitHub Actions construit à chaque push et pull request :
- **Windows Server 2025** avec CUDA 13.3 + Vulkan SDK
- Configurations Debug et Release
- Artefacts de build disponibles en téléchargement pour les builds Release

## Raccourcis Clavier

| Touche | Action |
|--------|--------|
| Souris Droite + Glisser | Rotation de la caméra |
| W/A/S/D | Déplacement de la caméra |
| Q/E | Déplacement vers le bas/haut |
| Bouton Render | Déclencher un nouveau rendu |
| Case Accumulate | Activer/désactiver le rendu progressif |

## Démonstration

Le projet Ray Tracing est encore en développement.

Voici la démonstration actuelle du projet.\
![Ray Tracing Aléatoire](screenshots/example-random.png)
![Ray Tracing Accumulation](screenshots/example-accumulate.png)

## À Propos de WalnutAppTemplate
- Description\
Ceci est un modèle d'application simple pour Walnut - contrairement à l'exemple dans le dépôt Walnut, celui-ci garde Walnut comme sous-module externe et est beaucoup plus adapté pour construire des applications réelles. Voir le dépôt Walnut pour plus de détails.
- Pour Commencer\
Une fois cloné, vous pouvez personnaliser les fichiers `premake5.lua` et `WalnutApp/premake5.lua` selon vos préférences (par exemple, changer le nom de "WalnutApp" en autre chose). Une fois satisfait, exécutez `scripts/Setup.bat` pour générer les fichiers de solution/projet Visual Studio 2022. Votre application se trouve dans le répertoire `WalnutApp/`, avec un exemple de code de base dans `WalnutApp/src/WalnutApp.cpp`. Je recommande de modifier ce projet WalnutApp pour créer votre propre application, car tout devrait être configuré et prêt à l'emploi.

## Dépannage

| Symptôme | Cause | Solution |
|----------|-------|----------|
| La fenêtre est noire | Incompatibilité d'architecture CUDA | Vérifier le modèle GPU, s'assurer que `cudaArchs` dans `premake5.lua` inclut le bon `sm_XX` |
| `no kernel image is available` | nvcc n'a pas compilé pour le GPU cible | Ajouter le `-gencode=arch=compute_XX,code=sm_XX` correspondant |
| `CUDA_PATH` non défini | Variable d'environnement manquante | Propriétés Système → Variables d'Environnement → Nouvelle `CUDA_PATH`, pointer vers le répertoire CUDA Toolkit |
| Les fichiers `.cu` ne sont pas compilés pendant la construction | `CUDA_PATH` non effectif au moment de la génération | Redémarrer le terminal, vérifier que `echo %CUDA_PATH%` n'est pas vide, puis relancer `Setup.bat` |
| L'éditeur de liens signale `CUDARenderer_*` non défini | `CUDARenderer.obj` non lié | Vérifier `linkoptions { "$(IntDir)CUDARenderer.obj" }` dans `premake5.lua` |
| `invalid value 'C++23' for cppdialect` | Utilisation de l'ancien premake5 fourni avec Walnut | Exécuter `scripts\Setup.bat` (télécharge automatiquement une version plus récente) ou télécharger manuellement premake5 5.0.0-beta8 |

## LICENCE
Le projet utilise la `Licence MIT`.

## Copyright
© Bonity, 2024
