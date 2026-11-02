#!/usr/bin/env python3
"""Synchronize the README release and refresh Doxygen for staged engine inputs.

This helper is called by the optional versioned .githooks/pre-commit hook.
GitHub Pages still regenerates the documentation from committed files on main,
including when a contributor has not installed Doxygen or enabled this hook.
"""

from __future__ import annotations

import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

# Also loaded with importlib by tests; make the sibling helper importable.
TOOLS_DIR = Path(__file__).resolve().parent
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))
from doxygen_cache import docs_current
from engine_version import SEMVER

ROOT = Path(__file__).resolve().parents[1]
README_RELEASE = re.compile(rb'(Engine release:\s*<strong>)([^<\r\n]+)(</strong>)')


def _staged_file(root: Path, name: str) -> bytes:
    """Read the file that will actually be committed, not its worktree copy."""
    return subprocess.run(
        ["git", "show", f":{name}"], cwd=root, check=True,
        capture_output=True,
    ).stdout


def sync_readme_release(root: Path = ROOT) -> bool:
    """Sync the README release badge from staged VERSION without losing edits.

    Only the release badge is changed. The updated README is staged when its
    worktree matches the index; unrelated unstaged edits cause a clear error
    instead of being silently swept into the commit.
    """
    version = _staged_file(root, "VERSION").decode("utf-8").strip()
    if not SEMVER.fullmatch(version):
        raise ValueError(f"Staged VERSION is not valid SemVer: {version!r}")

    indexed_readme = _staged_file(root, "README.md")
    matches = list(README_RELEASE.finditer(indexed_readme))
    if len(matches) != 1:
        raise ValueError("README.md must contain exactly one Engine release badge")
    if matches[0].group(2) == version.encode("ascii"):
        return False

    status = subprocess.run(
        ["git", "diff", "--quiet", "--", "README.md"], cwd=root, check=False,
    ).returncode
    if status == 1:
        raise ValueError("README.md contains unstaged edits. Stage or stash those "
                         "changes before committing VERSION; the hook will not overwrite them.")
    if status != 0:
        raise OSError(f"Cannot check unstaged README.md changes (git diff exit {status})")

    path = root / "README.md"
    worktree_readme = path.read_bytes()
    worktree_matches = list(README_RELEASE.finditer(worktree_readme))
    if len(worktree_matches) != 1:
        raise ValueError("Worktree README.md must contain exactly one Engine release badge")
    current = worktree_matches[0]
    updated = (worktree_readme[:current.start(2)] + version.encode("ascii") +
               worktree_readme[current.end(2):])
    path.write_bytes(updated)
    subprocess.run(["git", "add", "--", "README.md"], cwd=root, check=True)
    print(f"[DOCS] README.md engine release synchronized to {version} and staged")
    return True


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
            "tools/doxygen_cache.py",
            "tools/engine_version.py",
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

    if "VERSION" in relevant:
        try:
            sync_readme_release(ROOT)
        except (OSError, ValueError, UnicodeError, subprocess.CalledProcessError) as exc:
            print(f"[ERROR] Cannot synchronize README.md release: {exc}", file=sys.stderr)
            return 1

    if docs_current(ROOT):
        print("[DOCS] Local documentation already matches the current sources; "
              "skipping regeneration, checking API...")
        commands = [[sys.executable, str(ROOT / "tools" / "check_doxygen_docs.py")]]
    else:
        doxygen = shutil.which("doxygen")
        if not doxygen:
            print("[WARN] Doxygen not installed; skipping optional local refresh. "
                  "GitHub Pages will rebuild the committed documentation on main.",
                  file=sys.stderr)
            return 0
        print(f"[DOCS] {len(relevant)} staged documentation/engine inputs changed; "
              "regenerating both local Doxygen references...")
        commands = [
            [sys.executable, str(ROOT / "tools" / "build_doxygen.py"), "--doxygen", doxygen],
            [sys.executable, str(ROOT / "tools" / "check_doxygen_docs.py")],
        ]

    for tool in commands:
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
