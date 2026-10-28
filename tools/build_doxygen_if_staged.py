#!/usr/bin/env python3
"""Refresh local Doxygen on commits that modify documented XEngine inputs.

This helper is called by the optional versioned .githooks/pre-commit hook.
GitHub Pages still regenerates the documentation from committed files on main,
including when a contributor has not installed Doxygen or enabled this hook.
"""

from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def affects_documentation(path: str) -> bool:
    """Return whether a staged file is a Doxygen input or affects generation."""
    normalized = path.replace("\\", "/")
    return (
        normalized.startswith(("engine/", "docs/"))
        or normalized in {
            "VERSION",
            "Doxyfile_Games",
            "Doxyfile_Engine",
            "tools/build_doxygen.py",
            "tools/check_doxygen_docs.py",
        }
    )


def staged_paths(root: Path = ROOT) -> list[str]:
    """Use the index (and NUL separators) rather than unstaged file changes."""
    result = subprocess.run(
        # --no-renames reports both the removed and added path. This also
        # catches deletions and moves *out of* engine/ or docs/.
        ["git", "diff", "--cached", "--name-only", "--no-renames", "-z"],
        cwd=root,
        check=True,
        capture_output=True,
    )
    return [os.fsdecode(path) for path in result.stdout.split(b"\0") if path]


def main() -> int:
    try:
        relevant = [p for p in staged_paths() if affects_documentation(p)]
    except (OSError, subprocess.CalledProcessError) as exc:
        print(f"[ERROR] Cannot inspect staged changes for Doxygen: {exc}", file=sys.stderr)
        return 1
    if not relevant:
        return 0

    doxygen = shutil.which("doxygen")
    if not doxygen:
        print("[WARN] Doxygen not installed; skipping optional local refresh. "
              "GitHub Pages will rebuild the committed documentation on main.",
              file=sys.stderr)
        return 0

    print(f"[DOCS] {len(relevant)} staged documentation/engine inputs changed; "
          "regenerating both local Doxygen references...")
    for tool in (
        [sys.executable, str(ROOT / "tools" / "build_doxygen.py"), "--doxygen", doxygen],
        [sys.executable, str(ROOT / "tools" / "check_doxygen_docs.py")],
    ):
        try:
            result = subprocess.run(tool, cwd=ROOT, check=False)
        except OSError as exc:
            print(f"[ERROR] Failed to run documentation check: {exc}", file=sys.stderr)
            return 1
        if result.returncode:
            print("[ERROR] Doxygen generation/check failed; commit stopped. "
                  "Fix the documentation or disable the optional hook locally.",
                  file=sys.stderr)
            return result.returncode
    print("[DOCS] Local reference refreshed. The GitHub Pages workflow will "
          "rebuild from the committed source on main.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
