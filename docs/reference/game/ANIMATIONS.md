# Animation clips, spritesheets and Animator {#xengine_animations}

XEngine's animation system is **Unity-inspired**, but uses editable JSON assets
and C++ Components. An `AnimationClip` describes **what changes over time**;
an `AnimatorController` describes **which clip is active and when to switch**;
an `Animator` Component evaluates the controller for one GameObject.

Animation is independent of the input system: a Behaviour can read `InputAction`
in `Update`, update Animator parameters, and receive animation events.

## Start a new animation

From your game's directory, create the two assets:

```bash
./new_asset.sh animation Idle
./new_asset.sh animator Player
```

The generator creates `assets/animations/Idle.animation.json` and
`assets/animators/Player.animator.json` without overwriting existing files.
Change the controller's `entry` and its state's clip path as needed.

```cpp
GameObject* player = scene->CreateObject("Player");
player->AddComponent<SpriteRenderer>();

Animator* animator = player->AddComponent<Animator>();
if (!animator->SetController("animators/Player.animator.json")) {
    Console.Error("Cannot start player animator", "Animation");
}
```

For sprite animation, assign a SpriteRenderer to the same object or the named
child. The animation may assign its texture automatically. Asset paths in
controllers and clips are relative to the **game's assets folder** and are
cached by `AssetManager`; an asset that fails validation returns `nullptr`.

The Animator runs in `LateUpdate(dt)`: all normal gameplay `Update` calls run
first, then the animation writes component properties, then rendering begins.
The engine uses scaled `dt`, so game time-scale changes affect animation.

## Animation and Animator file versions

Both `.animation.json` clips and `.animator.json` controllers require
`"version": 1`. **Version 1 is the current format version for both asset
types.** These are two independent JSON schema versions: changing the
Animator controller format does not require changing the Animation clip format.
They are also independent of the XEngine release number in `VERSION` and
of the game's save-file version.

At present, the loaders accept **only format version 1**. Missing version
fields and other version numbers are rejected with a format error. There is
no animation/controller asset migration mechanism yet; a future format 2
would require explicit reader compatibility or migration support.

## Clip: keyframes and interpolation

Complete example, `assets/animations/Pulse.animation.json`:

```json
{
  "version": 1,
  "name": "Pulse",
  "duration": 1.0,
  "speed": 1.0,
  "loop": "PingPong",
  "exitBehavior": "UseNextState",
  "tracks": [
    {
      "target": "Self",
      "component": "Transform",
      "property": "localScale",
      "type": "Vec2",
      "keyframes": [
        {"time": 0.0, "value": [1.0, 1.0], "interpolation": "EaseInOut"},
        {"time": 0.5, "value": [1.3, 1.3], "interpolation": "Linear"},
        {"time": 1.0, "value": [1.0, 1.0]}
      ]
    },
    {
      "target": "Self",
      "component": "SpriteRenderer",
      "property": "tint",
      "type": "Color",
      "keyframes": [
        {"time": 0.0, "value": [255, 255, 255, 255]},
        {"time": 1.0, "value": [255, 100, 100, 255]}
      ]
    }
  ],
  "events": [
    {"time": 0.5, "name": "PulsePeak"}
  ]
}
```

`duration` and `speed` are positive numbers (a speed of zero pauses the
playhead). Keyframe `time` is always **seconds from the clip's start**. Key
times within one track must strictly increase and values must share one type.
If `duration` is omitted, the last keyframe and sprite-frame durations
determine the clip length. Event times must be within the clip duration.

| `type` | Value representation | Blending |
|---|---|---|
| `Bool` | `true` / `false` | Switches at 50% |
| `Int` | Integer | Switches at 50% |
| `Float` | Number | Interpolated |
| `Vec2` | `[x,y]` | Component-wise |
| `Vec3` | `[x,y,z]` | Component-wise |
| `Color` | `[r,g,b,a]`, 0–255 integers | RGBA channels |
| `RectI` | `[x,y,w,h]` | Switches at 50% |
| `String` | Asset path | Switches at 50% |

Keyframe `interpolation` is `Step`, `Linear`, `EaseIn`, `EaseOut`,
`EaseInOut` or `Cubic`. `Cubic` uses Hermite interpolation for Float values;
set optional `outTangent` and `inTangent` (slope per second). For other types
it uses smooth interpolation. `Step` is especially useful for toggles,
sprite rectangles and discrete settings.

`loop` can be `Once`, `Loop`, `PingPong` or `ClampForever`.
`Once` and `ClampForever` retain their last pose; the difference is intended
to distinguish a completed one-shot from an indefinitely held pose. In this
release both clamp the playhead at the end. `PingPong` reverses the sample
direction on alternating passes. Events fire when crossed in either direction.

## Clip: spritesheet without manual keyframes

This example plays a 4×2 sprite grid, using frames `(1,0)` through `(2,1)`
in **row-major** order, starting at the top-left corner:

```json
{
  "version": 1,
  "name": "Run",
  "loop": "Loop",
  "spritesheet": {
    "texture": "textures/player_run.png",
    "target": "Self",
    "componentIndex": 0,
    "grid": {"columns": 4, "rows": 2},
    "start": [1, 0],
    "end": [2, 1],
    "direction": "Horizontal",
    "frameDuration": 0.08
  },
  "tracks": [],
  "events": []
}
```

- The engine interprets `start` and `end` as **inclusive** `[column,row]`
  coordinates, zero-based. `Horizontal` means row-major ordering;
  `Vertical` traverses columns first. Both may cross row/column boundaries.
- `grid` accepts `columns`/`rows` or `cellWidth`/`cellHeight` in pixels.
  The frame grid must divide the actual PNG dimensions evenly.
- `frameDuration` sets a constant number of seconds per frame. Optional
  `frameDurations` is an array containing one strictly positive number per
  selected frame; it overrides `frameDuration` for those frames.
- You can combine `tracks` and `spritesheet` in the same clip. The
  SpriteRenderer's `sprite` and `source` are updated automatically.
- When specifying only **pixel cell sizes**, Animator derives the full grid
  and precise total clip duration after loading the texture. An explicit
  `duration` overrides automatic duration selection.

Frame changes are normally discrete. During an Animator transition,
`spriteBlend: "Switch"` switches at its midpoint, while
`spriteBlend: "CrossFade"` draws the outgoing and incoming frames with
interpolated opacity when both sheets target the same SpriteRenderer.

## AnimatorController: states and transitions

`assets/animators/Player.animator.json`:

```json
{
  "version": 1,
  "entry": "Idle",
  "parameters": [
    {"name": "Speed", "type": "Float", "default": 0.0},
    {"name": "Jump", "type": "Trigger"},
    {"name": "Grounded", "type": "Bool", "default": true}
  ],
  "states": [
    {"name": "Idle", "clip": "animations/Idle.animation.json", "speed": 1.0},
    {"name": "Run", "clip": "animations/Run.animation.json", "speed": 1.0},
    {"name": "Jump", "clip": "animations/Jump.animation.json", "speed": 1.0}
  ],
  "transitions": [
    {
      "from": "Idle", "to": "Run",
      "hasExitTime": false,
      "durationMode": "Normalized", "duration": 0.15,
      "blendCurve": "EaseInOut",
      "spriteBlend": "CrossFade",
      "conditions": [{"parameter": "Speed", "comparison": "Greater", "value": 0.2}]
    },
    {
      "from": "Run", "to": "Idle",
      "hasExitTime": false, "durationMode": "Seconds", "duration": 0.1,
      "conditions": [{"parameter": "Speed", "comparison": "Less", "value": 0.2}]
    },
    {
      "from": "AnyState", "to": "Jump",
      "hasExitTime": false,
      "durationMode": "Seconds", "duration": 0.06,
      "conditions": [{"parameter": "Jump", "comparison": "Triggered"}]
    },
    {
      "from": "Jump", "to": "Idle",
      "hasExitTime": true, "exitTime": 0.9,
      "durationMode": "Normalized", "duration": 0.1,
      "offset": 0.0,
      "interruptionSource": "None",
      "orderedInterruption": true,
      "conditions": []
    }
  ],
  "layers": [{"name": "Base", "mode": "Override", "weight": 1.0}]
}
```

`entry` is the initial state. State names and parameter names must be unique.
`AnyState` is a special source matching any active state, and must have at
least one condition to prevent accidental per-frame retriggering.
Conditions on one transition are combined using **AND**. A transition that
has no `hasExitTime` must have at least one condition; a transition driven
only by Exit Time may have an empty conditions array.

### Understanding transition settings

| JSON field | Meaning | Default |
|---|---|---|
| `hasExitTime` | Require a normalized exit point to be crossed | `false` |
| `exitTime` | Exit point as a fraction of source duration; `0.9` = 90%, `1.5` = halfway through second loop | `1` |
| `durationMode` | `Normalized` multiplies duration by the source clip's length; `Seconds` is absolute | `Normalized` |
| `duration` | Blend length, `0` is an instant switch | `0.1` |
| `offset` | Normalized position to start the destination clip from, in `[0,1)` | `0` |
| `blendCurve` | `Step`, `Linear`, `EaseIn`, `EaseOut`, `EaseInOut` or `Cubic` | `EaseInOut` |
| `spriteBlend` | `Switch` or `CrossFade` | `Switch` |
| `interruptionSource` | `None`, `CurrentState`, `NextState`, `CurrentThenNext`, `NextThenCurrent` | `None` |
| `orderedInterruption` | If true, only earlier transition declarations can interrupt an active one | `true` |
| `canTransitionToSelf` | Permit a state to retrigger itself | `false` |

For example, a 2-second Idle clip and a transition with `durationMode:
"Normalized"` and `duration: 0.1` blend over **0.2 seconds**. Changing
`durationMode` to `Seconds` keeps the blend at **0.1 seconds** instead.
`blendCurve` changes how quickly the incoming clip gains influence; it does
not change the total duration. `Exit Time` is evaluated when the playhead
**crosses** its threshold, not continuously afterward. For looping clips,
an exit time of at most 1 can trigger again on later loops.

The engine chooses the first eligible edge in JSON order. During an active
transition, `interruptionSource` limits which state can originate a new edge.
When `orderedInterruption` is true, the interrupting edge must appear earlier
in the controller's `transitions` array. An interruption restarts blending
from the selected current/next state; it does not preserve every intermediate
value of the previous blend.

Conditions support `Equal`, `NotEqual`, `Greater`, `Less`, `GreaterEqual`,
`LessEqual` and `Triggered`. The last is only valid for `Trigger` parameters;
trigger parameters reset automatically when their matching transition starts.

## Change Animator parameters and receive events

```cpp
class PlayerAnimation : public Behaviour {
    Animator* animator_ = nullptr;
    unsigned footstepListener_ = 0;

    void Start() override {
        animator_ = gameObject->GetComponent<Animator>();
        if (!animator_) return;
        footstepListener_ = animator_->OnEvent(
            [](std::string_view event, std::string_view state) {
                if (event == "Footstep") Console.Log(std::string(state), "Footstep");
            });
    }

    void Update(float) override {
        if (animator_) animator_->SetFloat("Speed", 1.4f);
    }

    void OnDestroy() override {
        if (animator_ && footstepListener_)
            animator_->RemoveEventListener(footstepListener_);
    }
};
```

The Animator offers `SetBool`, `SetInt`, `SetFloat`, `SetTrigger`,
`ResetTrigger`, `Play`, `CrossFade`, `Stop`, `Resume`, `SetSpeed`,
`GetCurrentState`, `GetNormalizedTime` and `IsInTransition`.
Parameter setters return `false` when the parameter does not exist or has
the wrong type. `OnEvent` returns a listener token, removable with
`RemoveEventListener`; listeners run on the main thread.

## Animate your own C++ Component properties

XEngine automatically registers the animatable properties of its built-in
components. Use these exact `component` and `property` names in a clip:

| Component | Properties (`type`) |
|---|---|
| `Transform` | `localPosition` (Vec3), `localScale` (Vec2), `localRotation` (Vec3) |
| `SpriteRenderer` | `tint` (Color), `source` (RectI), `texturePath` (String), `offset` (Vec2), `flipX`, `flipY`, `enabled` (Bool) |
| `Collider2D` | `shape`, `layer`, `mask` (Int); `isTrigger`, `showGizmo`, `inheritRotation`, `enabled` (Bool); `gizmoColor` (Color); `localOffset`, `size` (Vec2); `rotationOffset`, `radius` (Float) |
| `RigidBody2D` | `bodyType`, `collisionDetection`, `constraints` (Int); `mass`, `inertia`, `gravityScale`, `linearDamping`, `angularDamping`, `restitution`, `angularVelocity` (Float); `velocity` (Vec2); `enabled` (Bool) |
| `Animator` | `speed` (Float), `playing`, `enabled` (Bool) |
| `PlayerInput` | `enabled` (Bool) |
| `Behaviour` | `enabled` (Bool), for a generic Behaviour component selection |

For `Collider2D.shape`, use `0` = Box, `1` = Circle and `2` = Capsule.
For `RigidBody2D.bodyType`, use `0` = Static, `1` = Dynamic and
`2` = Kinematic; `collisionDetection` uses `0` = Discrete,
`1` = Continuous. `constraints` is a combined bit mask:
`1` freezes X, `2` freezes Y, `4` freezes rotation.
Collider `layer` and `mask` use the signed 32-bit representation of
their underlying bit fields (`-1` means all bits set).
Use **Step** keyframes for enums, masks and other discrete settings.
Invalid enum values are ignored; regular component setters retain their
own validation and clamping behavior.

For example, a clip can pulse the size of a Collider2D:

```json
{
  "version": 1,
  "name": "ColliderPulse",
  "loop": "PingPong",
  "tracks": [{
    "target": "Self",
    "component": "Collider2D",
    "property": "size",
    "type": "Vec2",
    "keyframes": [
      {"time": 0.0, "value": [1.0, 1.0]},
      {"time": 0.5, "value": [1.5, 1.5]}
    ]
  }]
}
```

Animated colliders and rigid bodies are updated during `LateUpdate`, after
the fixed-step physics calculations for that frame. Physics uses the updated
settings on subsequent steps. Animating `Animator.playing` or `enabled`
on the same Animator may stop its own playback.

For custom components, **choose** which fields are safe to expose once
during game initialization; C++ does not provide automatic reflection of
arbitrary user-defined members:

```cpp
class Door : public Behaviour {
public:
    float openness = 0.f;
};

// Simple member registration:
XENGINE_ANIMATABLE(Door, openness);

// Alternatively, use getter/setter registration for validation:
AnimationPropertyRegistry::Register<Door, float>("Door", "openness",
    [](const Door& door) { return door.openness; },
    [](Door& door, float value) { door.openness = std::clamp(value, 0.f, 1.f); });
```

The `XENGINE_ANIMATABLE` shorthand also accepts a writable XEngine
`Property<>` wrapper, using its getter and setter rather than writing raw
memory. It does not support read-only `PropertyRO<>` wrappers.

Pick **one** of these approaches for the same component/property pair;
registering a duplicate returns `false`. Register before your Animator
starts; registration is main-thread only. The registry never writes raw
memory offsets, and bindings resolve their target by entity ID on access.

Clips address a child with a slash-delimited relative name, e.g.
`"target": "Arm/Hand"`. Names must resolve unambiguously within the
Scene. To select a second component of the same type on a GameObject, set
`"componentIndex": 1`. Built-in property bindings are installed
automatically the first time a clip needs one.

The shorthand macro cannot access private fields; use the explicit
getter/setter form for those. Custom components work with
`Bool`, `Int`, `Float`, `Vec2`, `Vec3`, `Color`,
`RectI` and `String` (`std::string`).

## Layers and property ownership: current limits

- One **base Override layer** is supported, with `weight` from `0` to `1`.
  Optional `mask` is an array of exact target paths, for example
  `["Arm", "Arm/Hand"]`. Multiple parallel layers and `Additive` modes
  are represented in the asset model but **rejected at runtime**. Do not
  rely on parallel state-machine playback in this release.
- When a state ends, animated properties keep the last written value by
  default. `exitBehavior: "RestoreOriginal"` restores properties no longer
  controlled by the destination; `KeepLastValue` is explicitly available.
- Animation writes a property every frame. If a gameplay script changes the
  same property during `Update`, the Animator's `LateUpdate` value wins.
- The Animator is not a physics solver: animate movement using a Rigidbody
  with care, since directly writing Transform pose can override collision
  response. Use keyframed cosmetic children when possible.
- Animation asset JSON version 1 is **not** the game's save-file version.
  Clips and controllers are read-only assets, not player save data.

## Errors and testing

The JSON loader rejects unsupported versions, malformed values, missing
state references, invalid transitions, oversized assets and invalid
keyframe ordering. `AssetManager::LoadAnimation` and
`LoadAnimatorController` return `nullptr` and report a `Console.Warning`.
An invalid sprite grid is skipped rather than used for drawing.

If something does not animate:

1. Check that the JSON files live under the correct `assets` subfolders.
2. Verify the controller's `entry`, state clip paths and parameter spelling.
3. Verify the Component and property strings in the clip match registered names.
4. Check that the SpriteRenderer is on the named `target` and its PNG grid fits.
5. Ensure the GameObject, component and Animator are active, and that
   `SetController` returned `true`.

See [Input Actions](INPUT_ACTIONS.md) for controller-driven parameters and
[Rendering and UI](RENDERING_AND_UI.md) for sprite coordinates and scaling.
