"""Regression checks for the opt-in staged-Doxygen hook."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import shutil
import subprocess
import tempfile
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
            "tools/doxygen_cache.py", "tools/engine_version.py",
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

    def test_staged_version_sync_runs_even_without_doxygen(self) -> None:
        with patch.object(MODULE, "staged_paths", return_value=["VERSION"]), \
             patch.object(MODULE, "sync_readme_release", return_value=True) as sync, \
             patch.object(MODULE, "docs_current", return_value=False), \
             patch.object(MODULE.shutil, "which", return_value=None):
            self.assertEqual(MODULE.main(), 0)
            sync.assert_called_once_with(MODULE.ROOT)

    def test_readme_badge_is_staged_from_index_version_without_overwriting_edits(self) -> None:
        if not shutil.which("git"):
            self.skipTest("git unavailable")
        with tempfile.TemporaryDirectory(prefix="xengine-readme-version-") as directory:
            root = Path(directory)
            subprocess.run(["git", "init", "-q"], cwd=root, check=True)
            subprocess.run(["git", "config", "--local", "core.autocrlf", "false"],
                           cwd=root, check=True)
            readme = root / "README.md"
            readme.write_bytes(b"Intro\r\nEngine release: <strong>2.0.0</strong>\r\nOther\r\n")
            version_file = root / "VERSION"
            version_file.write_text("2.0.0\n", encoding="ascii")
            subprocess.run(["git", "add", "README.md", "VERSION"], cwd=root, check=True)

            version_file.write_text("2.0.1\n", encoding="ascii")
            subprocess.run(["git", "add", "VERSION"], cwd=root, check=True)
            self.assertTrue(MODULE.sync_readme_release(root))
            expected = b"Intro\r\nEngine release: <strong>2.0.1</strong>\r\nOther\r\n"
            self.assertEqual(readme.read_bytes(), expected)
            self.assertEqual(MODULE._staged_file(root, "README.md"), expected)
            self.assertFalse(MODULE.sync_readme_release(root))

            # Only the INDEX version counts: do not use an unstaged value.
            version_file.write_text("9.9.9\n", encoding="ascii")
            self.assertFalse(MODULE.sync_readme_release(root))

            version_file.write_text("2.0.2\n", encoding="ascii")
            subprocess.run(["git", "add", "VERSION"], cwd=root, check=True)
            readme.write_bytes(expected + b"Unstaged personal README edits\r\n")
            with self.assertRaisesRegex(ValueError, "unstaged edits"):
                MODULE.sync_readme_release(root)
            self.assertEqual(MODULE._staged_file(root, "README.md"), expected)
            self.assertIn(b"Unstaged personal", readme.read_bytes())

    def test_missing_optional_doxygen_warns_but_allows_commit(self) -> None:
        with patch.object(MODULE, "staged_paths", return_value=["engine/src/A.cpp"]), \
             patch.object(MODULE, "docs_current", return_value=False), \
             patch.object(MODULE.shutil, "which", return_value=None):
            self.assertEqual(MODULE.main(), 0)

    def test_docs_failure_blocks_commit(self) -> None:
        with patch.object(MODULE, "staged_paths", return_value=["engine/src/A.cpp"]), \
             patch.object(MODULE, "docs_current", return_value=False), \
             patch.object(MODULE.shutil, "which", return_value="doxygen"), \
             patch.object(MODULE.subprocess, "run", return_value=subprocess.CompletedProcess([], 2)) as run:
            self.assertEqual(MODULE.main(), 2)
            self.assertEqual(run.call_count, 1)

    def test_up_to_date_docs_skip_doxygen_and_still_check_api(self) -> None:
        with patch.object(MODULE, "staged_paths", return_value=["engine/include/A.h"]), \
             patch.object(MODULE, "docs_current", return_value=True), \
             patch.object(MODULE.shutil, "which") as which, \
             patch.object(MODULE.subprocess, "run", return_value=subprocess.CompletedProcess([], 0)) as run:
            self.assertEqual(MODULE.main(), 0)
            which.assert_not_called()
            self.assertEqual(run.call_count, 1)
            self.assertEqual(Path(run.call_args.args[0][-1]).name, "check_doxygen_docs.py")

    def test_bad_api_still_blocks_commit_on_cache_hit(self) -> None:
        with patch.object(MODULE, "staged_paths", return_value=["docs/GITHUB_PAGES.md"]), \
             patch.object(MODULE, "docs_current", return_value=True), \
             patch.object(MODULE.shutil, "which") as which, \
             patch.object(MODULE.subprocess, "run", return_value=subprocess.CompletedProcess([], 3)) as run:
            self.assertEqual(MODULE.main(), 3)
            which.assert_not_called()
            self.assertEqual(run.call_count, 1)


if __name__ == "__main__":
    unittest.main()
