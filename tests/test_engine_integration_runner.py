"""Regression tests for the native SDL2 integration runner's test fixtures."""
from __future__ import annotations

import importlib.util
from pathlib import Path
import tempfile
import unittest
import wave


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    'xengine_integration_runner', ROOT / 'tools/test_engine_integration.py')
assert SPEC and SPEC.loader
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)


class EngineIntegrationRunnerTests(unittest.TestCase):
    def test_wav_fixtures_are_valid_and_do_not_modify_template(self):
        for existing_audio in (False, True):
            with self.subTest(existing_audio=existing_audio), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                source = root / 'original'
                source.mkdir()
                (source / 'preserved.txt').write_text('unchanged', encoding='utf-8')
                if existing_audio:
                    (source / 'audio').mkdir()
                    (source / 'audio' / '.gitkeep').write_text('', encoding='utf-8')

                destination = root / 'copy'
                runner.prepare_fixture_assets(source, destination)

                self.assertEqual((destination / 'preserved.txt').read_text(), 'unchanged')
                self.assertEqual((source / 'preserved.txt').read_text(), 'unchanged')
                self.assertEqual((source / 'audio').exists(), existing_audio)
                self.assertEqual((destination / 'audio' / '.gitkeep').exists(), existing_audio)

                for filename, expected_samples in (('tone.wav', 2205), ('other.wav', 4410)):
                    with self.subTest(filename=filename), wave.open(
                        str(destination / 'audio' / filename), 'rb'
                    ) as audio:
                        self.assertEqual(audio.getnchannels(), 1)
                        self.assertEqual(audio.getsampwidth(), 2)
                        self.assertEqual(audio.getframerate(), 22050)
                        self.assertEqual(audio.getnframes(), expected_samples)

    def test_sanitizer_ci_is_independent_of_regular_matrix(self):
        workflow = (ROOT / '.github/workflows/test-engine.yml').read_text(
            encoding='utf-8')
        self.assertIn('linux_sanitizers:', workflow)
        self.assertIn('--config Debug --sanitizers --frames 120', workflow)
        self.assertIn('ASAN_OPTIONS: detect_leaks=1:halt_on_error=1', workflow)
        self.assertIn('matrix:\n        configuration: [Debug, Release]', workflow)


if __name__ == '__main__':
    unittest.main()
