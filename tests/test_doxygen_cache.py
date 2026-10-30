"""Content changes, not version numbers or timestamps, govern local docs."""
from __future__ import annotations

import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "xengine_doxygen_cache", ROOT / "tools" / "doxygen_cache.py")
assert SPEC and SPEC.loader
cache = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(cache)


def create_output(root: Path) -> None:
    for relative in cache.OUTPUT_FILES:
        destination = root / "build" / "docs" / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text("generated HTML/XML", encoding="utf-8")


class DoxygenCacheTests(unittest.TestCase):
    def test_manual_generator_short_circuits_before_doxygen_lookup(self):
        module_spec = importlib.util.spec_from_file_location(
            "xengine_doxygen_generator_cached", ROOT / "tools" / "build_doxygen.py")
        assert module_spec and module_spec.loader
        module = importlib.util.module_from_spec(module_spec)
        module_spec.loader.exec_module(module)
        with mock.patch.object(module, "docs_current", return_value=True), \
             mock.patch.object(module.shutil, "which") as which, \
             mock.patch.object(module, "build") as build, \
             mock.patch.object(module.sys, "argv", ["build_doxygen.py"]):
            self.assertEqual(module.main(), 0)
            which.assert_not_called()
            build.assert_not_called()

    def test_same_contents_reused_after_rebuild_and_touch(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            header = root / "engine" / "include" / "Example.h"
            header.parent.mkdir(parents=True)
            header.write_text("// unchanged", encoding="utf-8")
            create_output(root)
            before = cache.documentation_fingerprint(root)
            self.assertTrue(cache.stamp_docs_current(root, before))
            self.assertTrue(cache.docs_current(root))
            header.touch()
            self.assertEqual(cache.documentation_fingerprint(root), before)
            self.assertTrue(cache.docs_current(root))

    def test_modified_added_removed_inputs_invalidate_even_same_engine_version(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            header = root / "engine" / "include" / "Example.h"
            header.parent.mkdir(parents=True)
            header.write_text("// v1", encoding="utf-8")
            (root / "VERSION").write_text("0.1.5", encoding="ascii")
            create_output(root)
            original = cache.documentation_fingerprint(root)
            self.assertTrue(cache.stamp_docs_current(root, original))
            header.write_text("// v2", encoding="utf-8")
            self.assertFalse(cache.docs_current(root))
            header.write_text("// v1", encoding="utf-8")
            self.assertTrue(cache.docs_current(root))
            extra = root / "docs" / "NEW.md"
            extra.parent.mkdir()
            extra.write_text("# New", encoding="utf-8")
            self.assertFalse(cache.docs_current(root))
            extra.unlink()
            self.assertTrue(cache.docs_current(root))

    def test_partial_outputs_and_incomplete_generation_never_hit_cache(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            signature = cache.documentation_fingerprint(root)
            self.assertFalse(cache.stamp_docs_current(root, signature))
            create_output(root)
            self.assertTrue(cache.stamp_docs_current(root, signature))
            (root / "build/docs/game/html/index.html").unlink()
            self.assertFalse(cache.docs_current(root))
            self.assertFalse(cache.stamp_docs_current(root, signature))
            cache.invalidate_docs_cache(root)
            self.assertFalse(cache.docs_current(root))

    def test_source_changed_during_generation_prevents_stamping(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            create_output(root)
            previous = cache.documentation_fingerprint(root)
            file = root / "Doxyfile_Games"
            file.write_text("PROJECT_NAME = Changed", encoding="ascii")
            self.assertFalse(cache.stamp_docs_current(root, previous))
            self.assertFalse(cache.docs_current(root))


if __name__ == "__main__":
    unittest.main()
