"""Check restricted cache invalidation access used by Font cleanup."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class FontCacheAccessTests(unittest.TestCase):
    def test_font_is_allowed_to_invalidate_private_text_cache(self):
        header = (ROOT / "engine/include/RenderManager.h").read_text(encoding="utf-8")
        self.assertIn("friend class Font;", header)
        private_section = header.split("private:", 1)[1].split("public:", 1)[0]
        self.assertIn("void ClearTextCache_() noexcept;", private_section)
        self.assertNotIn("void ClearTextCache_() noexcept;", header.split("public:", 1)[1])

    def test_font_cleanup_invalidates_cache_before_closing_font(self):
        assets = (ROOT / "engine/src/Assets.cpp").read_text(encoding="utf-8")
        for body in [
            assets.split("Font::~Font() noexcept", 1)[1].split("Font::Font(Font&&", 1)[0],
            assets.split("Font& Font::operator=(Font&& other) noexcept", 1)[1].split("TTF_Font* Font::GetSDL", 1)[0],
        ]:
            self.assertIn("render->ClearTextCache_()", body)
            self.assertLess(body.index("render->ClearTextCache_()"), body.index("TTF_CloseFont(mFont)"))


if __name__ == "__main__":
    unittest.main()
