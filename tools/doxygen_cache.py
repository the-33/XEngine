"""Content-based cache shared by XEngine's Doxygen entry points.

The cache is independent of the engine's numeric VERSION, Git index state,
compiler flags and platform build directories. It tracks the actual inputs
used by the two documentation profiles and their post-processing scripts.
"""

from __future__ import annotations

import hashlib
from pathlib import Path


STAMP_NAME = ".xengine-doxygen-inputs.sha256"
OUTPUT_FILES = (
    "index.html",
    "game/html/index.html",
    "engine/html/index.html",
    "game/xml/index.xml",
    "engine/xml/index.xml",
)
SINGLE_INPUTS = (
    "VERSION",
    "Doxyfile_Games",
    "Doxyfile_Engine",
    "tools/build_doxygen.py",
    "tools/check_doxygen_docs.py",
    "tools/doxygen_cache.py",
    "tools/engine_version.py",
)
INPUT_DIRECTORIES = ("engine/include", "engine/src", "docs")


def documentation_fingerprint(root: Path) -> str:
    """Hash relative filenames and bytes, including additions and deletions.

    An existing engine library might be recompiled for a different toolchain
    without changing these inputs; that must not regenerate the same HTML.
    Conversely, a docs/Doxyfile edit must invalidate the cache even when the
    engine library does not need recompilation.
    """
    paths = [root / relative for relative in SINGLE_INPUTS]
    for directory in INPUT_DIRECTORIES:
        base = root / directory
        if base.is_dir():
            paths.extend(path for path in base.rglob("*") if path.is_file()
                         and "__pycache__" not in path.parts)
    digest = hashlib.sha256()
    for path in sorted(set(paths), key=lambda item: item.relative_to(root).as_posix()):
        relative = path.relative_to(root).as_posix().encode("utf-8")
        digest.update(relative)
        digest.update(b"\0")
        if path.is_file():
            digest.update(hashlib.sha256(path.read_bytes()).digest())
        else:
            digest.update(b"<missing>")
        digest.update(b"\0")
    return digest.hexdigest()


def docs_current(root: Path) -> bool:
    """True only if all expected output is present for the current inputs."""
    output = root / "build" / "docs"
    if not all((output / relative).is_file() for relative in OUTPUT_FILES):
        return False
    try:
        return (output / STAMP_NAME).read_text(encoding="ascii").strip() == \
            documentation_fingerprint(root)
    except (OSError, UnicodeError):
        return False


def invalidate_docs_cache(root: Path) -> None:
    """A failed or interrupted Doxygen run must never leave a valid stamp."""
    (root / "build" / "docs" / STAMP_NAME).unlink(missing_ok=True)


def stamp_docs_current(root: Path, initial_fingerprint: str) -> bool:
    """Record success atomically only if inputs did not change during build."""
    if documentation_fingerprint(root) != initial_fingerprint:
        return False
    output = root / "build" / "docs"
    if not all((output / relative).is_file() for relative in OUTPUT_FILES):
        return False
    stamp = output / STAMP_NAME
    temporary = stamp.with_name(STAMP_NAME + ".tmp")
    temporary.write_text(initial_fingerprint + "\n", encoding="ascii")
    temporary.replace(stamp)
    return True
