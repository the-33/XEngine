"""Regression guards for per-monitor DPI and pixel-sharp scalable UI drawing."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


def source(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


class CrispUiTests(unittest.TestCase):
    def test_windows_dpi_policy_is_set_before_sdl_video(self):
        engine = source("engine/src/Engine.cpp")
        window = source("engine/src/WindowManager.cpp")
        self.assertIn('#if defined(_WIN32)\n', engine)
        self.assertIn('SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2"', engine)
        self.assertIn('SDL_HINT_WINDOWS_DPI_SCALING, "0"', engine)
        self.assertLess(engine.index('SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2"'),
                        engine.index('SDL_Init(SDL_INIT_VIDEO'))
        self.assertIn('!SDL_GetHint(SDL_HINT_WINDOWS_DPI_AWARENESS)', engine)
        self.assertIn('SDL_GetWindowSizeInPixels(', window)
        self.assertIn('SDL_WINDOW_ALLOW_HIGHDPI', window)

    def test_font_is_rasterized_at_output_size_and_cached_per_scale(self):
        renderer = source("engine/src/RenderManager.cpp")
        header = source("engine/include/RenderManager.h")
        fonts = source("engine/include/Assets.h")
        assets = source("engine/src/AssetManager.cpp")
        self.assertIn('std::string mSourcePath;', fonts)
        self.assertIn('up->mSourcePath = fullPath;', assets)
        self.assertIn('TTF_OpenFont(font.mSourcePath.c_str(), pointSize)', renderer)
        self.assertIn('FontForScreenScale_(font, safeScale)', renderer)
        self.assertIn('TTF_RenderUTF8_Blended(rasterFont,', renderer)
        self.assertIn('rasterAtOutputSize ? pixels', renderer)
        self.assertIn('std::lround(safeScale * 1024.f)', renderer)
        self.assertIn('MeasureTextScreen(const Font& font', header)
        self.assertIn('for (auto& variant : mScaledFonts)', renderer)
        self.assertIn('TTF_CloseFont(variant.second)', renderer)

    def test_widget_layout_and_caret_measure_the_actual_rendered_font(self):
        ui = source("engine/src/UIManager.cpp")
        self.assertIn('renderer->MeasureTextScreen(', ui)
        self.assertIn('render->MeasureTextScreen(', ui)
        self.assertIn('const float textWidth = static_cast<float>(measured.x);', ui)
        self.assertNotIn('TextWidth_(ttf,', ui)
        self.assertNotIn('static_cast<float>(tw) * c.textScale', ui)

    def test_rounded_ui_uses_pixel_alignment_and_feathered_edges(self):
        renderer = source("engine/src/RenderManager.cpp")
        self.assertIn('constexpr int kRoundedArcSteps_ = 20;', renderer)
        self.assertIn('void RoundedBand_(', renderer)
        self.assertIn('Color transparent = color;', renderer)
        self.assertIn('transparent.a = 0;', renderer)
        self.assertIn('RoundedBand_(renderer, outside, radius + feather, transparent,', renderer)
        self.assertIn('const float left = std::round(r.x);', renderer)
        self.assertIn('std::round(r.x + r.w) - left', renderer)
        self.assertIn('std::round(requestedStroke)', renderer)


if __name__ == "__main__":
    unittest.main()
