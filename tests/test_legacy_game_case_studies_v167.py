"""Public Legacy case-study links, privacy, and artifact consistency regressions."""

from __future__ import annotations

import re
import unittest
from pathlib import Path
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parents[1]
README = ROOT / "README.md"
PLAN = ROOT / "docs" / "LEGACY_MIGRATION_PLAN.md"

CASES = (
    ("Blob's Quest", "blobs-quest", "blobs_quest.gif", "Legacy Game1"),
    ("My Princess Run", "my-princess-run", "my_princess_run.gif", "Legacy Game2"),
)


class LegacyGameCaseStudyTests(unittest.TestCase):
    def test_public_links_and_local_original_documents(self) -> None:
        readme = README.read_text(encoding="utf-8")
        plan = PLAN.read_text(encoding="utf-8")
        for name, slug, _, _ in CASES:
            original = ROOT / "games" / name / "MIGRATION.md"
            public = ROOT / "docs" / "migrations" / slug / "MIGRATION.md"
            with self.subTest(game=name):
                self.assertTrue(original.is_file())
                self.assertTrue(public.is_file())
                self.assertIn("# " + name, original.read_text(encoding="utf-8"))
                self.assertIn("# " + name, public.read_text(encoding="utf-8"))
                public_link = f"docs/migrations/{slug}/MIGRATION.md"
                self.assertIn(public_link, readme)
                self.assertIn(f"migrations/{slug}/MIGRATION.md", plan)
                self.assertNotIn(f"games/{name.replace(' ', '%20')}/MIGRATION.md", readme)

    def test_legacy_context_and_previews_are_explicit(self) -> None:
        readme = README.read_text(encoding="utf-8")
        plan = PLAN.read_text(encoding="utf-8")
        self.assertIn("## Games migrated from XEngine-Legacy", readme)
        self.assertIn("## Real-world migration case studies", plan)
        self.assertIn("docs/LEGACY_MIGRATION_PLAN.md#real-world-migration-case-studies", readme)
        self.assertIn("not yet implemented", readme)
        self.assertIn("games are **not published by default**", readme)
        self.assertIn("<details>", readme)
        for _, _, gif, original in CASES:
            with self.subTest(game=original):
                self.assertIn(original, readme)
                self.assertIn(
                    f"https://raw.githubusercontent.com/the-33/XEngine-Legacy/main/docs/gameplay/{gif}",
                    readme,
                )
                self.assertIn(gif, plan)
                self.assertFalse((ROOT / "docs" / "gameplay" / gif).exists())

    def test_no_retired_one_game_tools_in_public_readme(self) -> None:
        readme = README.read_text(encoding="utf-8")
        for retired in ("migrate_to_hpp.py", "migrate_blob_quest_modular.py"):
            self.assertNotIn(retired, readme)
        public_blob = (ROOT / "docs" / "migrations" / "blobs-quest" / "MIGRATION.md").read_text(encoding="utf-8")
        self.assertIn("retired and not distributed", public_blob)
        self.assertNotIn("py -3 tools\\migrate_", public_blob)

    def test_public_readme_and_case_study_local_markdown_links_resolve(self) -> None:
        docs = [README, PLAN] + [ROOT / "docs" / "migrations" / slug / "MIGRATION.md" for _, slug, _, _ in CASES]
        for doc in docs:
            content = doc.read_text(encoding="utf-8")
            for match in re.finditer(r"(?<!!)\]\(([^)]+)\)", content):
                link = match.group(1)
                if link.startswith(("http://", "https://", "#", "mailto:")):
                    continue
                local = unquote(link.split("#", 1)[0])
                with self.subTest(file=doc.relative_to(ROOT), link=link):
                    self.assertTrue((doc.parent / local).exists(), f"Broken documentation link: {link}")


if __name__ == "__main__":
    unittest.main()
