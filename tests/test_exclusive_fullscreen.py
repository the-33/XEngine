"""Regression checks for the opt-in, physical fullscreen video mode API."""

import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class ExclusiveFullscreenTests(unittest.TestCase):
    def test_public_api_is_documented_for_doxygen(self):
        header = (ROOT / 'engine/include/WindowManager.h').read_text(encoding='utf-8')
        signature = 'bool SetExclusiveFullscreen(int newWidth, int newHeight) noexcept;'
        before = header[:header.index(signature)]
        docs = before[before.rfind('/**'):]
        for marker in ('@brief ', '@param newWidth ', '@param newHeight ', '@return '):
            self.assertIn(marker, docs)

    def test_exclusive_fullscreen_does_not_replace_existing_desktop_mode(self):
        source = (ROOT / 'engine/src/WindowManager.cpp').read_text(encoding='utf-8')
        self.assertIn('SDL_WINDOW_FULLSCREEN_DESKTOP', source)
        self.assertIn('SDL_WINDOW_FULLSCREEN)', source)
        self.assertIn('SDL_GetClosestDisplayMode(', source)
        self.assertIn('SDL_SetWindowDisplayMode(', source)
        self.assertIn('supported.w != newWidth || supported.h != newHeight', source)
        self.assertIn('exclusiveFullscreen = false;', source)
        self.assertIn('exclusiveFullscreen = true;', source)

    def test_game_documentation_has_exclusive_fullscreen_example(self):
        guide = (ROOT / 'docs/reference/game/RENDERING_AND_UI.md').read_text(encoding='utf-8')
        self.assertIn('Window->SetExclusiveFullscreen(1280, 720)', guide)


if __name__ == '__main__':
    unittest.main()
