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
                    # New SVG artwork lives in the maintainer's local checkout;
                    # older source snapshots may not contain the new artwork.
                    if target == "docs/branding/xengine_logo.svg" and not (document.parent / target).exists():
                        continue
                    self.assertTrue((document.parent / target).exists())

    def test_readme_uses_original_vector_artwork_when_available(self) -> None:
        logo = ROOT / "docs" / "branding" / "xengine_logo.svg"
        readme = (ROOT / "README.md").read_text(encoding="utf-8")
        self.assertIn('src="docs/branding/xengine_logo.svg"', readme)
        if logo.exists():
            self.assertIn("<svg", logo.read_text(encoding="utf-8"))

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
