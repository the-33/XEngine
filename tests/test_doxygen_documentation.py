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

    def test_contract_checker_requires_brief_arguments_templates_and_returns(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = pathlib.Path(tmp)
            (path / "index.xml").write_text(
                '<doxygenindex><compound kind="class" refid="classWidget"/></doxygenindex>',
                encoding="utf-8",
            )
            xml = '''<doxygen><compounddef kind="class">
              <compoundname>Widget</compoundname><briefdescription/>
              <sectiondef><memberdef kind="function" prot="public">
              <name>Find</name><type>bool</type>
              <templateparamlist><param><declname>T</declname></param></templateparamlist>
              <param><type>int</type><declname>index</declname></param>
              <briefdescription><para>Look up a widget.</para></briefdescription>
              <detaileddescription/>
              </memberdef></sectiondef></compounddef></doxygen>'''
            (path / "classWidget.xml").write_text(xml, encoding="utf-8")
            _, _, missing = check_doxygen_docs.inspect(path, require_contracts=True)
            self.assertEqual(missing, [
                "Widget: @brief (type or namespace)",
                "Widget::Find: @param index",
                "Widget::Find: @tparam T",
                "Widget::Find: @return",
            ])
            xml = xml.replace("<briefdescription/>",
                              "<briefdescription><para>Interactive widget.</para></briefdescription>", 1)
            xml = xml.replace("<detaileddescription/>", '''<detaileddescription><para>
              <parameterlist kind="param"><parameteritem><parameternamelist>
              <parametername>index</parametername></parameternamelist></parameteritem>
              </parameterlist><parameterlist kind="templateparam"><parameteritem>
              <parameternamelist><parametername>T</parametername></parameternamelist>
              </parameteritem></parameterlist><simplesect kind="return">
              <para>Whether the widget exists.</para></simplesect></para></detaileddescription>''')
            (path / "classWidget.xml").write_text(xml, encoding="utf-8")
            _, _, missing = check_doxygen_docs.inspect(path, require_contracts=True)
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
