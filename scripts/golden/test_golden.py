#!/usr/bin/env python3
"""
Golden Image Regression Test for RayTracing
============================================
Compares CPU path tracer output against reference golden images
using SSIM and MSE. Intended for CI integration.

Usage:
    cd scripts/golden
    uv run python test_golden.py [--generate] [--tolerance T]

    --generate   Generate new golden images (first run).
                 Should be committed after generation.
    --tolerance  SSIM threshold below which test fails (default: 0.999).
"""

import argparse
import os
import subprocess
import sys
from pathlib import Path

# REPO_ROOT is two levels up from scripts/golden/
REPO_ROOT = Path(__file__).resolve().parent.parent.parent
GOLDEN_DIR = REPO_ROOT / "test" / "golden"

# Scene definitions for golden images
SCENES = {
    "default_scene": {
        "width": 800,
        "height": 600,
        "frames": 100,
        "bounces": 5,
    },
    "emissive_scene": {
        "width": 400,
        "height": 300,
        "frames": 50,
        "bounces": 3,
    },
}


def find_executable():
    """Find the GoldenRenderer executable in xmake build output."""
    candidates = list(REPO_ROOT.glob("build/**/GoldenRenderer.exe"))
    candidates += list(REPO_ROOT.glob("build/**/GoldenRenderer"))
    if not candidates:
        raise FileNotFoundError(
            "GoldenRenderer not found. Run 'xmake build GoldenRenderer' first."
        )
    return str(candidates[0])


def render_scene(exe_path, scene_name, config):
    """Run GoldenRenderer and return path to output PNG."""
    output_path = GOLDEN_DIR / f"{scene_name}_current.png"
    cmd = [
        exe_path,
        "--output", str(output_path),
        "--width", str(config["width"]),
        "--height", str(config["height"]),
        "--frames", str(config["frames"]),
        "--bounces", str(config["bounces"]),
    ]
    print(f"  Running: {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"  STDERR: {result.stderr}")
        raise RuntimeError(f"GoldenRenderer failed with code {result.returncode}")
    print(result.stderr.strip())
    return output_path


def compute_metrics(img_path, ref_path):
    """Compute SSIM and MSE between two images."""
    try:
        from skimage.metrics import structural_similarity as ssim
        from skimage.io import imread
        import numpy as np
    except ImportError:
        print("ERROR: scikit-image or numpy not installed. Run: cd scripts/golden && uv sync")
        sys.exit(1)

    img = imread(str(img_path))
    ref = imread(str(ref_path))

    if img.shape != ref.shape:
        raise ValueError(
            f"Image size mismatch: {img.shape} vs {ref.shape}"
        )

    if img.ndim == 3 and img.shape[2] >= 3:
        ssim_val = ssim(img, ref, channel_axis=2, data_range=255)
        mse_val = np.mean((img.astype(np.float64) - ref.astype(np.float64)) ** 2)
    else:
        ssim_val = ssim(img, ref, data_range=255)
        mse_val = np.mean((img.astype(np.float64) - ref.astype(np.float64)) ** 2)

    return ssim_val, mse_val


def generate_golden(exe_path):
    """Generate golden reference images."""
    GOLDEN_DIR.mkdir(parents=True, exist_ok=True)
    print("=== Generating Golden Reference Images ===\n")
    for name, config in SCENES.items():
        print(f"Scene: {name}")
        output = render_scene(exe_path, name, config)
        ref_path = GOLDEN_DIR / f"{name}.png"
        os.replace(output, ref_path)
        print(f"  Golden image saved: {ref_path}\n")
    print("Done. Commit the images in test/golden/ to the repository.")


def run_tests(exe_path, tolerance):
    """Run regression test against existing golden images."""
    print("=== Golden Image Regression Test ===\n")
    all_pass = True

    for name, config in SCENES.items():
        ref_path = GOLDEN_DIR / f"{name}.png"
        if not ref_path.exists():
            print(f"  SKIP {name}: golden image not found at {ref_path}")
            print(f"  Run 'cd scripts/golden && uv run python test_golden.py --generate' first.\n")
            all_pass = False
            continue

        print(f"Scene: {name}")
        try:
            output = render_scene(exe_path, name, config)
        except RuntimeError as e:
            print(f"  FAIL: {e}\n")
            all_pass = False
            continue

        ssim_val, mse_val = compute_metrics(output, ref_path)
        passed = ssim_val >= tolerance

        status = "PASS" if passed else "FAIL"
        print(f"  SSIM: {ssim_val:.6f} (threshold: {tolerance})")
        print(f"  MSE:  {mse_val:.4f}")
        print(f"  Result: {status}\n")

        if not passed:
            all_pass = False
        if passed and output.exists():
            output.unlink()

    return all_pass


def main():
    parser = argparse.ArgumentParser(description="RayTracing Golden Image Test")
    parser.add_argument(
        "--generate",
        action="store_true",
        help="Generate golden reference images",
    )
    parser.add_argument(
        "--tolerance",
        type=float,
        default=0.999,
        help="SSIM threshold (default: 0.999)",
    )
    args = parser.parse_args()

    exe_path = find_executable()

    if args.generate:
        generate_golden(exe_path)
        return

    success = run_tests(exe_path, args.tolerance)
    if success:
        print("=== All golden image tests PASSED ===")
        sys.exit(0)
    else:
        print("=== Golden image tests FAILED ===")
        sys.exit(1)


if __name__ == "__main__":
    main()
