"""Comprobaciones de regresión para edición de texto y entrada con mando."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


def source(path):
    return (ROOT / path).read_text(encoding="utf-8")


class UiTextGamepadTests(unittest.TestCase):
    def test_connected_gamepads_autojoin_on_attach_and_profile_load(self):
        devices = source("engine/src/InputDevices.cpp")
        self.assertIn("HandleJoin(id);", devices)
        self.assertIn("mPlayers[player] = std::move(settings);", devices)
        self.assertIn("if (!mPadOwners.contains(id)) HandleJoin(id);", devices)

    def test_repeated_editing_keys_are_exposed(self):
        header = source("engine/include/InputManager.h")
        implementation = source("engine/src/InputManager.cpp")
        self.assertIn("KeyPressedOrRepeated(Key key)", header)
        self.assertIn("e.key.repeat", implementation)
        self.assertIn("mRepeatedKeys.fill(0)", implementation)

    def test_text_input_supports_selection_mouse_clipboard_and_repeat(self):
        ui = source("engine/src/UIManager.cpp")
        for token in ("mTextSelectionAnchorByte", "mTextMouseSelecting", "mouseCaret()",
                      "SDL_SetClipboardText", "KeyPressedOrRepeated(Key::BACKSPACE)",
                      "KeyPressed(Key::A)", "GetTextInput()", "SetTextInput(true)"):
            self.assertIn(token, ui)

    def test_dropdown_keeps_scrolled_list_visible(self):
        ui = source("engine/src/UIManager.cpp")
        scroll = ui[ui.index("const int scroll = input->GetMouseWheel().y;"):]
        self.assertIn("mDropdownHighlighted = std::clamp(mDropdownHighlighted", scroll)


if __name__ == "__main__":
    unittest.main()
