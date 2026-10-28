"""Regression checks for the opt-in staged-Doxygen hook."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import subprocess
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "xengine_staged_docs_hook", ROOT / "tools" / "build_doxygen_if_staged.py")
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class DocsPreCommitTests(unittest.TestCase):
    def test_relevant_inputs(self) -> None:
        for name in (
            "engine/include/Engine.h", "engine/src/Engine.cpp",
            "docs/reference/theme/xengine.css", "docs/GITHUB_PAGES.md",
            "VERSION", "Doxyfile_Games", "Doxyfile_Engine",
            "tools/build_doxygen.py", "tools/check_doxygen_docs.py",
        ):
            with self.subTest(name=name):
                self.assertTrue(MODULE.affects_documentation(name))

    def test_unrelated_inputs_are_ignored(self) -> None:
        for name in ("games/My Game/src/main.cpp", "README.md", ".gitignore",
                     "tools/build.py", "tests/test_docs_pre_commit.py"):
            with self.subTest(name=name):
                self.assertFalse(MODULE.affects_documentation(name))

    def test_index_only_and_paths_with_spaces(self) -> None:
        staged = subprocess.CompletedProcess(
            [], 0, stdout=b"engine/include/My Type.h\0docs/new guide.md\0")
        with patch.object(MODULE.subprocess, "run", return_value=staged) as run:
            self.assertEqual(MODULE.staged_paths(ROOT),
                             ["engine/include/My Type.h", "docs/new guide.md"])
            self.assertEqual(run.call_args.args[0], [
                "git", "diff", "--cached", "--name-only", "--no-renames", "-z",
            ])

    def test_no_relevant_changes_short_circuit(self) -> None:
        with patch.object(MODULE, "staged_paths", return_value=["README.md"]), \
             patch.object(MODULE.shutil, "which") as which:
            self.assertEqual(MODULE.main(), 0)
            which.assert_not_called()

    def test_missing_optional_doxygen_warns_but_allows_commit(self) -> None:
        with patch.object(MODULE, "staged_paths", return_value=["engine/src/A.cpp"]), \
             patch.object(MODULE.shutil, "which", return_value=None):
            self.assertEqual(MODULE.main(), 0)

    def test_docs_failure_blocks_commit(self) -> None:
        with patch.object(MODULE, "staged_paths", return_value=["engine/src/A.cpp"]), \
             patch.object(MODULE.shutil, "which", return_value="doxygen"), \
             patch.object(MODULE.subprocess, "run", return_value=subprocess.CompletedProcess([], 2)) as run:
            self.assertEqual(MODULE.main(), 2)
            self.assertEqual(run.call_count, 1)


if __name__ == "__main__":
    unittest.main()
