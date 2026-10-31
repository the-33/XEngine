{
  "maps": {
    "{{NAME}}": {
      "actions": {
        "Move": {
          "type": "Axis2D",
          "combine": "Strongest",
          "bindings": [
            "Keyboard/WASD",
            "Keyboard/Arrows",
            {"path": "Gamepad/LeftStick", "processors": [{"type": "Deadzone", "value": 0.18}]},
            "Gamepad/Dpad"
          ]
        },
        "Jump": {
          "type": "Button",
          "interaction": {"type": "Press"},
          "bindings": ["Keyboard/Space", "Gamepad/South"]
        }
      }
    }
  }
}
