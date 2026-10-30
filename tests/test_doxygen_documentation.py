"""Documentation tooling regressions; Doxygen remains an optional dependency."""
from __future__ import annotations

import pathlib
import sys
import tempfile
import unittest


PROJECT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT / "tools"))
import check_doxygen_docs  # noqa: E402


class DoxygenDocumentationTests(unittest.TestCase):
    def test_responsive_ui_public_fields_have_doxygen_comments(self):
        header = (PROJECT / "engine/include/UIRectTransform.h").read_text(encoding="utf-8")
        for field in (
            "scaleMode", "referenceResolution", "anchorMin", "anchorMax",
            "pivot", "anchoredPosition", "sizeDelta",
        ):
            with self.subTest(field=field):
                pattern = rf"/\*\*\s*@brief\s+[^*]+\*/\s+(?:UICanvasScaleMode|Vec2)\s+{field}\b"
                self.assertRegex(header, pattern)

    def test_all_exported_headers_explain_their_purpose(self):
        conf = (PROJECT / "Doxyfile_Games").read_text(encoding="utf-8")
        self.assertIn("EXTRACT_PRIVATE        = NO", conf)
        self.assertIn("WARN_AS_ERROR          = FAIL_ON_WARNINGS", conf)
        self.assertIn("docs/API_REFERENCE.md",
                      (PROJECT / "Doxyfile_Engine").read_text(encoding="utf-8"))
        internal = {
            "AdaptiveBroadphase.h", "BuildConfig.h", "ParallelExecutor.h",
            "RenderPreparation.h", "RendererBackendPolicy.h",
            "SpatialContextClassifier.h", "SpatialHashBroadphase.h",
            "SpriteProjection.h", "StatsOverlayLayout.h",
        }
        headers = PROJECT / "engine" / "include"
        public = [p for p in headers.glob("*.h") if p.name not in internal]
        self.assertGreaterEqual(len(public), 30)
        for path in public:
            with self.subTest(header=path.name):
                self.assertIn(f"@file {path.name}", path.read_text(encoding="utf-8"))

    def test_checker_rejects_missing_docs_and_accepts_complete_xml(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = pathlib.Path(tmp)
            (path / "index.xml").write_text(
                '<doxygenindex><compound kind="class" refid="classFoo"/></doxygenindex>',
                encoding="utf-8",
            )
            src = '''<doxygen><compounddef>
                <compoundname>Foo</compoundname>
                <sectiondef><memberdef prot="public">
                <name>Run</name><briefdescription><para>Runs a task.</para></briefdescription>
                <detaileddescription/>
                </memberdef><memberdef prot="public"><name>Mode</name>
                <briefdescription><para>Modes.</para></briefdescription>
                <detaileddescription/>
                <enumvalue><name>One</name><briefdescription/></enumvalue>
                </memberdef></sectiondef></compounddef></doxygen>'''
            (path / "classFoo.xml").write_text(src, encoding="utf-8")
            members, enums, missing = check_doxygen_docs.inspect(path)
            self.assertEqual((members, enums), (2, 1))
            self.assertEqual(missing, ["Foo::Mode::One"])
            src = src.replace('<name>One</name><briefdescription/>',
                              '<name>One</name><briefdescription><para>First mode.</para></briefdescription>')
            (path / "classFoo.xml").write_text(src, encoding="utf-8")
            _, _, missing = check_doxygen_docs.inspect(path)
            self.assertFalse(missing)

    def test_readme_and_guides_crosslink_reference(self):
        for relative in (
            "README.md", "docs/ARCHITECTURE.md", "docs/DEVELOPER_GUIDE.md",
            "docs/PERSISTENCE_AND_SERIALIZATION.md",
        ):
            with self.subTest(path=relative):
                content = (PROJECT / relative).read_text(encoding="utf-8")
                self.assertIn("API_REFERENCE.md", content)


if __name__ == "__main__":
    unittest.main()
