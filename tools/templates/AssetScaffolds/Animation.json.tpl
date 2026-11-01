{
  "version": 1,
  "name": "{{NAME}}",
  "duration": 1.0,
  "speed": 1.0,
  "loop": "Loop",
  "exitBehavior": "UseNextState",
  "tracks": [
    {
      "target": "Self",
      "component": "Transform",
      "property": "localScale",
      "type": "Vec2",
      "keyframes": [
        {"time": 0.0, "value": [1.0, 1.0], "interpolation": "EaseInOut"},
        {"time": 0.5, "value": [1.1, 1.1], "interpolation": "EaseInOut"},
        {"time": 1.0, "value": [1.0, 1.0]}
      ]
    }
  ],
  "events": []
}
