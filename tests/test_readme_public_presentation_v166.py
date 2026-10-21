"""Public README link and branding regressions; no network or SDL required."""

from __future__ import annotations

import re
import unittest
from pathlib import Path
from urllib.parse import unquote


ROOT = Path(__file__).resolve().parents[1]


class ReadmePublicPresentationTests(unittest.TestCase):
    def test_local_links_in_public_documents_exist(self) -> None:
        documents = (
            ROOT / "README.md",
            ROOT / "docs" / "DEVELOPER_GUIDE.md",
            ROOT / "docs" / "LEGACY_MIGRATION_PLAN.md",
        )
        for document in documents:
            contents = document.read_text(encoding="utf-8")
            links = re.findall(r"(?<!!)\]\(([^)]+)\)", contents)
            links += re.findall(r'<img\s+src="([^"]+)"', contents)
            for link in links:
                if link.startswith(("http://", "https://", "mailto:", "#")):
                    continue
                target = unquote(link.split("#", 1)[0])
                with self.subTest(document=document.name, link=link):
                    self.assertTrue((document.parent / target).exists())

    def test_brand_image_is_the_unmodified_template_logo(self) -> None:
        logo = ROOT / "docs" / "branding" / "xengine-logo.png"
        template_icon = ROOT / "tools" / "templates" / "BlankGame" / "assets" / "icons" / "xengine.png"
        self.assertEqual(logo.read_bytes(), template_icon.read_bytes())
        self.assertEqual(logo.read_bytes()[:8], b"\x89PNG\r\n\x1a\n")

    def test_readme_has_no_missing_private_migration_links_or_tools(self) -> None:
        readme = (ROOT / "README.md").read_text(encoding="utf-8")
        for retired in (
            "migrate_to_hpp.py",
            "migrate_blob_quest_modular.py",
            "Blob's%20Quest/MIGRATION.md",
            "My%20Princess%20Run/MIGRATION.md",
        ):
            self.assertNotIn(retired, readme)
        self.assertIn("LEGACY_MIGRATION_PLAN.md", readme)
        self.assertIn("not yet implemented", readme)


if __name__ == "__main__":
    unittest.main()
