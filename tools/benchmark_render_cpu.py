#!/usr/bin/env python3
"""Compile and run deterministic CPU workload benchmark (no SDL requirement).

This checks preparation and AABB pair-work parallelism, NOT real game FPS.
Run real-game comparisons via tools/compare_profiles.py with identical scene load.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "tools/benchmark_render_cpu.cpp"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    for flag, default in (("sprites", 2048), ("colliders", 256),
                          ("workers", 3), ("repeats", 30), ("warmup", 5)):
        parser.add_argument(f"--{flag}", type=int, default=default)
    parser.add_argument("--cxx", default=os.environ.get("CXX"), help="C++20 compiler, e.g. g++ or clang++")
    parser.add_argument("--csv", type=Path, default=Path("render_cpu_benchmark.csv"))
    args = parser.parse_args()
    if any(getattr(args, x) <= 0 for x in ("sprites", "colliders", "workers", "repeats")):
        parser.error("workload and repeats values must be positive")
    if args.warmup < 0:
        parser.error("--warmup must not be negative")
    cxx = args.cxx or next((candidate for candidate in ("g++", "clang++")
                            if shutil.which(candidate)), None)
    if cxx is None:
        parser.error("a C++20 compiler is required (use --cxx)")
    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="xengine-render-bench-") as directory:
        exe = Path(directory) / ("render_bench.exe" if os.name == "nt" else "render_bench")
        compile_cmd = [cxx, "-std=c++20", "-O2", "-pthread", "-I",
                       str(ROOT / "engine/include"), str(SOURCE), "-o", str(exe)]
        print("[BUILD] Compiling CPU-only C++20 benchmark...", flush=True)
        subprocess.run(compile_cmd, check=True)
        run_cmd = [str(exe)]
        for param in ("sprites", "colliders", "workers", "repeats", "warmup"):
            run_cmd.extend([f"--{param}", str(getattr(args, param))])
        run_cmd.extend(["--csv", str(args.csv.resolve())])
        return subprocess.run(run_cmd, check=False).returncode


if __name__ == "__main__":
    sys.exit(main())
