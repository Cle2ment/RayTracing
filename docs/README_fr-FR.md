# Ray Tracing : Démo RT accélérée

[English](/README.md) | [中文](/docs/README_zh-CN.md) | [Français](/docs/README_fr-FR.md)

![Static Badge](https://img.shields.io/badge/License-MIT-green)
<br/>
![Static Badge](https://img.shields.io/badge/Language-C++23-00599C?logo=cplusplus)
![Static Badge](https://img.shields.io/badge/GPU-CUDA-76B900?logo=nvidia)
![Static Badge](https://img.shields.io/badge/CPU_Acceleration-ISPC-0071C5?logo=intel)
![Static Badge](https://img.shields.io/badge/Poject-Xmake-brightgreen?logo=data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA2NCA2NCI+PHBvbHlnb24gcG9pbnRzPSI2NCw0IDY0LDYyIDYyLDY0IDEsNjQgMCw2MyAwLDQwIDYwLDMiIGZpbGw9IiNmZmYiLz48L3N2Zz4=)
<br/>
![Static Badge](https://img.shields.io/badge/Auto_Translation-Deepseek-5786FE?logo=deepseek)
<br/>
[![Build](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml/badge.svg)](https://github.com/Cle2ment/RayTracing/actions/workflows/build.yml)

## Aperçu

Un path tracer interactif en temps réel construit avec C++23 sur le framework d'application [Peanut](https://github.com/Cle2ment/Peanut). **Accéléré par GPU NVIDIA CUDA** et **accéléré par CPU Intel ISPC** — l'ensemble du pipeline de path tracing s'exécute sur le GPU lorsque CUDA est disponible, avec un repli CPU SIMD via ISPC (AVX2/AVX-512).

### Backends de rendu

| Backend | Accélération | Utilisation |
|---------|-------------|-----------|
| **CUDA GPU** | GPU NVIDIA (SM 7.5+) | `CUDA_PATH` détecté |
| **ISPC CPU** | AVX2 + AVX-512 SIMD | `vendor/ispc/bin/ispc.exe` détecté |
| **CPU C++** | `std::execution::par` multi-thread | Solution de repli |

### Architecture

| Composant | CPU | GPU (CUDA) |
|-----------|-----|------------|
| Génération des rayons | ISPC `foreach` ou `std::execution::par` | Noyau CUDA — un thread par pixel |
| Intersection rayon-sphère | Boucle de force brute | Fonction `__device__` |
| Path tracing (5 rebonds) | BRDF de microfacette GGX | BRDF de microfacette GGX |
| Génération de nombres aléatoires | Hachage PCG | Hachage PCG (`__device__`) |
| Roulette russe | Après 3 rebonds | Après 3 rebonds |
| Affichage | Peanut::Image (Vulkan) | Peanut::Image (Vulkan) via copie D2H |

**Disposition du noyau GPU** : blocs de threads 16×16, un thread CUDA par pixel.

## Démarrage rapide

```bash
# Clonez avec les sous-modules (RECOMMANDÉ)
git clone --recursive https://github.com/Cle2ment/RayTracing.git
cd RayTracing

#
