"""Keep the canonical XEngine release, README and current docs in sync."""

from pathlib import Path
import subprocess
import sys
import unittest


ROOT = Path(__file__).resolve().parents[1]


class VersionConsistencyTests(unittest.TestCase):
    def test_canonical_version_matches_readme_and_current_documentation(self):
        version = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
        readme = (ROOT / "README.md").read_text(encoding="utf-8")
        policy = (ROOT / "docs/VERSIONING.md").read_text(encoding="utf-8")
        workflow = (ROOT / "docs/reference/engine/DOCUMENTATION_WORKFLOW.md").read_text(encoding="utf-8")
        self.assertIn(f"Engine release: <strong>{version}</strong>", readme)
        self.assertIn(f"currently **`{version}`**", policy)
        self.assertIn(f"current release: **{version}**", workflow)
        self.assertTrue((ROOT / f"docs/RELEASE_NOTES_{version}.md").is_file())

    def test_version_script_uses_the_same_canonical_version(self):
        version = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
        for arguments, expected in (
            ([], version),
            (["--archive-name", "patch"], f"XEngine-{version}-patch.zip"),
            (["--archive-name", "source"], f"XEngine-{version}-source.zip"),
        ):
            process = subprocess.run(
                [sys.executable, str(ROOT / "tools/engine_version.py"), *arguments],
                capture_output=True, text=True, cwd=ROOT,
            )
            self.assertEqual(process.returncode, 0, process.stderr)
            self.assertEqual(process.stdout.strip(), expected)


if __name__ == "__main__":
    unittest.main()
