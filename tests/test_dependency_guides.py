"""Global dependency checker: optional guidance is independent of games and future targets."""
from __future__ import annotations

import importlib.util
from pathlib import Path
import subprocess
import sys
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / 'tools'
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))

import dependency_guides as guides
import check_dependencies as checker


class OptionalTargetGuidesTests(unittest.TestCase):
    @staticmethod
    def capture(host: str) -> str:
        lines = []
        guides.show_optional_targets(host, emit=lines.append)
        return '\n'.join(lines)

    def test_linux_instructions_for_windows(self):
        out = self.capture('linux')
        self.assertIn('g++-mingw-w64-x86-64', out)
        self.assertIn('binutils-mingw-w64-x86-64', out)
        self.assertIn('Windows x86_64 SDK', out)
        self.assertIn('linux.windows', out)
        self.assertNotIn('games/', out)
        self.assertNotIn('build.platform', out)

    def test_windows_instructions_for_linux(self):
        out = self.capture('windows')
        self.assertIn('pacman -S --needed', out)
        self.assertIn('mingw-w64-ucrt-x86_64-clang', out)
        self.assertIn('Linux x86_64 sysroot', out)
        self.assertIn('windows.linux', out)
        self.assertNotIn('games/', out)

    def test_future_target_can_be_added_without_changing_checker(self):
        android = guides.TargetGuide(
            'linux', 'android', 'Android ARM64 (future)',
            ('sdkmanager "ndk;<version>"',),
            ('Configure the Android SDK/NDK once the backend is implemented.',),
        )
        with mock.patch.object(guides, 'OPTIONAL_TARGETS', guides.OPTIONAL_TARGETS + (android,)):
            out = self.capture('linux')
        self.assertIn('Android ARM64 (future)', out)
        self.assertIn('sdkmanager', out)
        self.assertIn('g++-mingw-w64', out)

    def test_unknown_host_has_no_wrong_instructions(self):
        self.assertIn('No cross-compilation setup instructions', self.capture('darwin'))

    def test_guidance_does_not_execute_commands(self):
        with mock.patch.object(subprocess, 'run', side_effect=AssertionError('exec forbidden')):
            self.capture('linux')
            self.capture('windows')

    def test_checker_prints_guidance_even_when_native_checks_fail(self):
        # Simulate missing host tools: this should produce a native error,
        # but cross-compilation guidance must still be shown without running commands.
        import io
        from contextlib import redirect_stdout
        with mock.patch.object(checker, 'which', return_value=None):
            with redirect_stdout(io.StringIO()) as capture:
                result = checker.main()
        self.assertEqual(result, 1)
        self.assertIn('Optional cross-compilation setup', capture.getvalue())

    def test_checker_native_success_still_displays_optional_guidance(self):
        import io
        from contextlib import redirect_stdout
        fake = subprocess.CompletedProcess([], 0, stdout='2.30.0\n', stderr='')
        with mock.patch.object(checker, 'which', side_effect=lambda name, alt=None: '/fake/' + name):
            with mock.patch.object(checker, 'run', return_value=fake):
                with redirect_stdout(io.StringIO()) as capture:
                    result = checker.main()
        self.assertEqual(result, 0)
        self.assertIn('Optional cross-compilation setup', capture.getvalue())
        self.assertIn('Tools and headers are ready', capture.getvalue())

    def test_checker_source_never_reads_game_configs(self):
        source = (TOOLS / 'check_dependencies.py').read_text(encoding='utf-8')
        self.assertNotIn("config.json", source)
        self.assertNotIn("games/", source)


if __name__ == '__main__':
    unittest.main()
