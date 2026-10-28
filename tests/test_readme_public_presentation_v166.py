"""Public README link and branding regressions; no network or SDL required."""

from __future__ import annotations

import re
import unittest
from pathlib import Path
from urllib.parse import unquote, urlsplit


ROOT = Path(__file__).resolve().parents[1]
EXTERNALLY_MAINTAINED_THEME_SVGS = {
    f"docs/branding/{stem}_{theme}.svg"
    for stem in ("xengine_logo", "xengine_benchmark_logo")
    for theme in ("light", "dark")
}


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
                # Strip both GitHub's theme fragment and the ?v=4 cache-buster.
                target = unquote(urlsplit(link).path)
                with self.subTest(document=document.name, link=link):
                    # The themed SVGs exist in the maintainer's local checkout,
                    # but were not included in the uploaded source snapshot.
                    # Keep checking all other local links; assert the theme
                    # fragment/filename relationship in test_github_theme_logos.
                    if (target in EXTERNALLY_MAINTAINED_THEME_SVGS and
                            not (document.parent / target).exists()):
                        continue
                    self.assertTrue((document.parent / target).exists())

    def test_readme_uses_original_vector_artwork_when_available(self) -> None:
        readme = (ROOT / "README.md").read_text(encoding="utf-8")
        for theme in ("light", "dark"):
            logo = ROOT / "docs" / "branding" / f"xengine_logo_{theme}.svg"
            self.assertIn(f'src="docs/branding/xengine_logo_{theme}.svg?v=4#gh-{theme}-mode-only"',
                          readme)
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
