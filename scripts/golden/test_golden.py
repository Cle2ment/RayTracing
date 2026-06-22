#!/usr/bin/env python3
"""
Golden Image Regression Test for RayTracing
============================================
CI mode: renders twice and compares for self-consistency within the same run.
Local mode: compares against committed golden images in test/golden/.

Usage:
    cd scripts/golden

    # CI self-consistency check
    uv run python test_golden.py --ci

    # Local regression test against golden images
    uv run python test_golden.py

    # Generate golden images locally
    uv run python test_golden.py --generate
"""

import argparse
import os
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
GOLDEN_DIR = REPO_ROOT / "test" / "golden"

SCENES = {
    "default_scene": {
        "width": 800,
        "height": 600,
        "frames": 100,
        "bounces": 5,
    },
    "default_scene_lowres": {
        "width": 400,
        "height": 300,
        "frames": 50,
        "bounces": 3,
    },
}


def find_executable():
    candidates = list(REPO_ROOT.glob("build/**/GoldenRenderer.exe"))
    candidates += list(REPO_ROOT.glob("build/**/GoldenRenderer"))
    if not candidates:
        raise FileNotFoundError(
            "GoldenRenderer not found. Run 'xmake build GoldenRenderer' first."
        )
    return str(candidates[0])


def render_scene(exe_path, scene_name, config, suffix=""):
    """Run GoldenRenderer and return path to output PNG."""
    label = f"{scene_name}{suffix}"
    output_path = GOLDEN_DIR / f"{label}.png"
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
    for line in result.stderr.strip().splitlines():
        if "Frame" in line or "saved" in line:
            print(f"  {line}")
    return output_path


def compute_metrics(img_path, ref_path):
    """Compute SSIM and MSE between two images."""
    from skimage.metrics import structural_similarity as ssim
    from skimage.io import imread
    import numpy as np

    img = imread(str(img_path))
    ref = imread(str(ref_path))

    if img.shape != ref.shape:
        raise ValueError(f"Image size mismatch: {img.shape} vs {ref.shape}")

    if img.ndim == 3 and img.shape[2] >= 3:
        # Compare RGB channels only — alpha is always 255, dilutes SSIM
        ssim_val = ssim(img[:,:,:3], ref[:,:,:3], channel_axis=2, data_range=255)
        mse_val = np.mean((img[:,:,:3].astype(np.float64) - ref[:,:,:3].astype(np.float64)) ** 2)
    else:
        ssim_val = ssim(img, ref, data_range=255)
        mse_val = np.mean((img.astype(np.float64) - ref.astype(np.float64)) ** 2)

    return ssim_val, mse_val


def run_ci_test(exe_path):
    """CI mode: render twice and compare for self-consistency.

    This avoids cross-machine floating-point differences — both renders
    run with the same MSVC compiler version on the same CI runner.
    """
    print("=== CI Golden Image Test (self-consistency) ===\n")
    all_pass = True

    for name, config in SCENES.items():
        print(f"Scene: {name}")
        try:
            run1 = render_scene(exe_path, name, config, suffix="_ci_run1")
            run2 = render_scene(exe_path, name, config, suffix="_ci_run2")
        except RuntimeError as e:
            print(f"  FAIL: {e}\n")
            all_pass = False
            continue

        ssim_val, mse_val = compute_metrics(run1, run2)
        # CI uses stricter threshold since both renders use identical binary
        ci_threshold = 0.9999
        passed = ssim_val >= ci_threshold

        status = "PASS" if passed else "FAIL"
        print(f"  SSIM: {ssim_val:.6f} (threshold: {ci_threshold})")
        print(f"  MSE:  {mse_val:.4f}")
        print(f"  Result: {status}\n")

        if not passed:
            all_pass = False
        # Cleanup temp files
        run1.unlink(missing_ok=True)
        run2.unlink(missing_ok=True)

    return all_pass


def generate_golden(exe_path):
    """Generate golden reference images for local testing."""
    GOLDEN_DIR.mkdir(parents=True, exist_ok=True)
    print("=== Generating Golden Reference Images ===\n")
    for name, config in SCENES.items():
        print(f"Scene: {name}")
        output = render_scene(exe_path, name, config, suffix="_temp")
        ref_path = GOLDEN_DIR / f"{name}.png"
        os.replace(output, ref_path)
        print(f"  Golden image saved: {ref_path}\n")
    print("Done. Commit the images in test/golden/ to the repository.")


def run_local_test(exe_path, tolerance):
    """Local mode: compare against committed golden images."""
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
            output = render_scene(exe_path, name, config, suffix="_current")
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
        "--ci",
        action="store_true",
        help="CI mode: render twice and compare self-consistency",
    )
    parser.add_argument(
        "--generate",
        action="store_true",
        help="Generate golden reference images for local testing",
    )
    parser.add_argument(
        "--tolerance",
        type=float,
        default=0.999,
        help="SSIM threshold for local mode (default: 0.999)",
    )
    args = parser.parse_args()

    if not args.ci and not args.generate:
        # Try installing deps early for better error messages
        try:
            from skimage.metrics import structural_similarity  # noqa: F401
        except ImportError:
            print("ERROR: scikit-image or numpy not installed.")
            print("Run: cd scripts/golden && uv sync")
            sys.exit(1)

    exe_path = find_executable()

    if args.ci:
        success = run_ci_test(exe_path)
    elif args.generate:
        generate_golden(exe_path)
        return
    else:
        success = run_local_test(exe_path, args.tolerance)

    if success:
        print("=== All golden image tests PASSED ===")
        sys.exit(0)
    else:
        print("=== Golden image tests FAILED ===")
        sys.exit(1)


if __name__ == "__main__":
    main()
