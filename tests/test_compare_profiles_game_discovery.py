"""Automatic per-game CSV discovery for the XEngine frame profiler."""
from __future__ import annotations

from contextlib import redirect_stdout, redirect_stderr
import csv
import importlib.util
import io
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("compare_profiles_game_test", ROOT / "tools/compare_profiles.py")
compare = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(compare)


class CompareProfilesDiscoveryTests(unittest.TestCase):
    def setUp(self):
        self.tempdir = tempfile.TemporaryDirectory()
        self.addCleanup(self.tempdir.cleanup)
        self.root = Path(self.tempdir.name)
        self.games = self.root / "games"
        self.games.mkdir()
        self.game = self.games / "Blob's Quest"
        self.game.mkdir()
        (self.game / "config.json").write_text("{}", encoding="utf-8")
        self.other = self.games / "My Princess Run"
        self.other.mkdir()
        (self.other / "config.json").write_text("{}", encoding="utf-8")
        self.patch_root = patch.object(compare, "ROOT", self.root)
        self.patch_root.start()
        self.addCleanup(self.patch_root.stop)

    @staticmethod
    def sample(path: Path, frame_ms: float = 2):
        path.parent.mkdir(parents=True, exist_ok=True)
        keys = [key for key, _ in compare.METRICS] + ["narrowphase_tests", "fixed_steps"]
        with path.open("w", newline="", encoding="utf-8") as stream:
            writer = csv.DictWriter(stream, fieldnames=keys)
            writer.writeheader()
            for _ in range(130):
                writer.writerow({k: frame_ms for k in keys})
        return path

    def test_game_name_case_insensitive_and_absolute_game_path(self):
        self.assertEqual(compare.resolve_game("blob's quest"), self.game.resolve())
        self.assertEqual(compare.resolve_game(str(self.game)), self.game.resolve())
        self.assertEqual(compare.resolve_game("games/Blob's Quest"), self.game.resolve())

    def test_finds_debug_assets_profiles(self):
        a = self.sample(self.game / "assets/xengine_profile_single.csv")
        b = self.sample(self.game / "assets/xengine_profile_parallel.csv")
        self.assertEqual(compare.discover_profiles(self.game), (a, b, None))

    def test_finds_release_profiles_inside_platform_version(self):
        d = self.game / "dist/windows-x86_64/0.1.0"
        a = self.sample(d / "xengine_profile_single.csv")
        b = self.sample(d / "xengine_profile_parallel.csv")
        self.assertEqual(compare.discover_profiles(self.game), (a, b, None))

    def test_prefer_latest_complete_pair_not_individually_newest(self):
        old = self.game / "assets"
        new = self.game / "dist/linux-x86_64/0.2.0"
        old_a = self.sample(old / "xengine_profile_single.csv")
        old_b = self.sample(old / "xengine_profile_parallel.csv")
        new_a = self.sample(new / "xengine_profile_single.csv")
        new_b = self.sample(new / "xengine_profile_parallel.csv")
        os.utime(old_a, (100, 100))
        os.utime(old_b, (100, 100))
        os.utime(new_a, (200, 200))
        os.utime(new_b, (300, 300))
        self.assertEqual(compare.discover_profiles(self.game), (new_a, new_b, None))
        # A newer single-mode run must not be mixed with another directory's pair.
        os.utime(old_a, (500, 500))
        self.assertEqual(compare.discover_profiles(self.game), (new_a, new_b, None))

    def test_cross_directory_fallback_warns(self):
        a = self.sample(self.game / "assets/xengine_profile_single.csv")
        b = self.sample(self.game / "dist/windows-x86_64/0.1.0/xengine_profile_parallel.csv")
        s, p, warning = compare.discover_profiles(self.game)
        self.assertEqual((s, p), (a, b))
        self.assertIn("different directories", warning)

    def test_never_uses_other_games_csvs(self):
        self.sample(self.other / "assets/xengine_profile_single.csv")
        self.sample(self.other / "assets/xengine_profile_parallel.csv")
        with self.assertRaisesRegex(ValueError, "missing xengine_profile_single.csv"):
            compare.discover_profiles(self.game)

    def test_missing_profile_has_helpful_diagnostic(self):
        self.sample(self.game / "assets/xengine_profile_single.csv")
        with self.assertRaisesRegex(ValueError, "Enable Engine.profileCsv"):
            compare.discover_profiles(self.game)
        with self.assertRaisesRegex(ValueError, "Available games: Blob's Quest, My Princess Run"):
            compare.resolve_game("Unknown Game")

    def run_cli(self, *args: str) -> str:
        output = io.StringIO()
        with patch.object(sys, "argv", ["compare_profiles.py", *args]), redirect_stdout(output):
            self.assertEqual(compare.main(), 0)
        return output.getvalue()

    def test_cli_game_flag_and_positional_name(self):
        a = self.sample(self.game / "assets/xengine_profile_single.csv", 3)
        b = self.sample(self.game / "assets/xengine_profile_parallel.csv", 2)
        for arguments in (("--game", "Blob's Quest"), ("Blob's Quest",), (str(self.game),)):
            output = self.run_cli(*arguments, "--warmup", "0")
            self.assertIn("[GAME] Blob's Quest", output)
            self.assertIn(str(a), output)
            self.assertIn(str(b), output)
            self.assertIn("Frame wall time", output)

    def test_explicit_two_file_mode_keeps_working(self):
        a = self.sample(self.root / "a.csv")
        b = self.sample(self.root / "b.csv")
        output = self.run_cli(str(a), str(b), "--warmup", "0")
        self.assertNotIn("[GAME]", output)
        self.assertIn("Frame wall time", output)

    def test_argument_errors(self):
        error = io.StringIO()
        with patch.object(sys, "argv", ["compare_profiles.py", "--game", "Blob's Quest", "a.csv"]), redirect_stderr(error):
            with self.assertRaises(SystemExit) as raised:
                compare.main()
        self.assertEqual(raised.exception.code, 2)
        self.assertIn("not both", error.getvalue())


if __name__ == "__main__":
    unittest.main()
