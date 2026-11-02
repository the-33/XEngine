# Animation architecture and extension points {#xengine_animation_internals}

The animation subsystem is a data-driven, opt-in component/property evaluator.
It does **not** use pointer arithmetic, C++ reflection, or arbitrary JSON
field mutation. Source order in the `AnimatorController::transitions` vector
is priority order. Asset parsing never touches live scene objects.

## Ownership and frame order

- `AssetManager` owns immutable `AnimationClip` and `AnimatorController`
  instances in path-indexed caches. Pointers returned by its load methods
  remain valid until the cache is cleared on `Init`/`Shutdown`.
- A Scene owns each `GameObject`, which owns its `Animator` Behaviour.
  Parameters, playheads, active blend and listeners live **on the Animator**,
  never on the shared controller. The same controller may be reused by
  multiple players with independent states.
- `Engine::DoUpdate_` calls the user callback, `SceneManager::Update`,
  then `SceneManager::LateUpdate`. A Scene copies its entity pointer list
  before iterating, consistent with the existing Update path.
- All Animator writes and event callbacks run on the main thread, before
  `SceneManager::Render`. There is no animation worker thread.

## Modules

| File | Responsibility |
|---|---|
| `engine/include/Animation.h`, `engine/src/Animation.cpp` | JSON schema validation, typed tracks, interpolation, spritesheet frame extraction, shared controller model |
| `engine/include/AnimationPropertyRegistry.h`, `engine/src/AnimationPropertyRegistry.cpp` | Typed registration, built-in properties, non-owning entity-ID bindings |
| `engine/include/Animator.h`, `engine/src/Animator.cpp` | Runtime parameters, state transitions, `LateUpdate`, events and blending |
| `engine/include/SpriteRenderer.h`, `engine/src/SpriteRenderer.cpp` | Optional two-sprite opacity crossfade when transition mode requests it |
| `engine/include/AssetManager.h`, `engine/src/AssetManager.cpp` | Bounded asset reads, JSON parse, cache lifetime and diagnostic warning |

AnimationClip (`.animation.json`) and AnimatorController (`.animator.json`)
have **independent schema version counters**, each currently at
**`"version": 1`**. Neither follows XEngine's release `VERSION` or the
game-save version. Both parsers currently require exactly 1: missing,
older or future schema numbers are rejected, and no migration exists yet.
Increment these counters only for a deliberate, documented format change.

## Universal properties: safety invariants

Registration uses `AnimationPropertyRegistry::Register<T,V>` with callable
getter and setter, a direct `V T::*` member or an owner-bound writable
`Property<>` wrapper. The underlying value type must be an alternative of
`AnimationValue`; `PropertyRO<>` is not writable. The runtime registry
indexes a component name and property name, and stores a resolver for the
component type. `XENGINE_ANIMATABLE(Type, Member)` works with both plain
public members and writable `Property<>` wrappers.

`RegisterBuiltins` covers the built-in component classes `Transform`,
`SpriteRenderer`, `Behaviour`, `Collider2D`, `RigidBody2D`, `Animator`
and `PlayerInput`. Values are assigned through public component setters:
physical values retain the existing validation/clamping behavior, and
enum/bit-mask properties use `Int` tracks. The signed bit representation
preserves all 32 layer/mask bits. `Camera2D` is scene-owned rather than a
`Component`; the UI service is also outside the Component registry.

At bind time, slash-separated target names are resolved relative to the
Animator's owner. A binding captures a Scene pointer and target `EntityID`;
each read/write looks up that ID again, preventing use-after-free when a
child object is destroyed. Scene lifetime must still outlive any Animator
using its bindings. Duplicate registrations are rejected.

For a new built-in property, register it in
`AnimationPropertyRegistry::RegisterBuiltins`, using the **public** property
setter to preserve validation and side effects. For arbitrary game scripts,
encourage registration during `InitGame` before any Animator evaluation.
Every newly exposed property needs Doxygen documentation, tests and a
clear statement of its expected type and interpolation behavior.

## Transition evaluation

1. Advance the current state at `dt * AnimatorSpeed * ClipSpeed * StateSpeed`.
2. Dispatch bounded event crossings (including Loop and PingPong cycles).
3. Look for the first eligible transition in controller declaration order.
   Conditions are ANDed. If Exit Time is enabled, its normalized threshold
   must also be crossed in this frame.
4. Start a transition with a duration in seconds or a fraction of the source
   clip duration. Source and destination playheads advance independently.
5. Sample registered tracks and blend continuous values. Discrete values
   switch at a weight of 0.5. An `AnimationInterpolation` easing curve
   maps normalized blend progress to its weight.
6. Evaluate interruptions according to their declared source and order.
   Consume triggers on the edge that actually starts.
7. On completion, promote the destination playhead and optionally restore
   properties no longer animated by the destination.

An interruption currently restarts blending from its selected state branch;
it does **not** snapshot a synthetic intermediate source pose. This can
cause a visible discontinuity under successive rapid interruptions.
Complex multiple layer composition and additive blending remain pending:
the parser preserves layer data, but `Animator::SetController` rejects
controllers with multiple layers or an additive base layer.

## Sprite blending

`AnimationSpriteSheet::BuildFrames` validates the texture dimensions,
rectangular cell grid, inclusive range and frame durations. When the clip
has a spritesheet, Animator samples a frame and writes the `SpriteRenderer`
texture/source. During `CrossFade`, `SpriteRenderer` draws outgoing and
incoming frame regions with opacity weights; both must refer to the same
renderer target. The asset cache owns both textures. Because two alpha
draws are composited sequentially, this is an **approximate visual crossfade**
and may differ slightly from a shader blending two images directly.

## Regression requirements

- Parser: rejected future versions, duplicate state/parameter names,
  invalid condition types, negative times and malformed sprite ranges.
- Playback: Once, Loop, PingPong, thresholds across a loop boundary,
  `Normalized`/`Seconds` duration, triggers and interruption priority.
- Properties: register custom members, `Property<>` wrappers and accessors;
  verify built-in collider/rigid-body values, enum validation, bit masks and
  enabled state; bind child path and invalidate after entity destruction.
- Rendering: crossfade on both SDL render backends and distinct sprite sizes.
- Game integration: no regression to `InputAction` context navigation,
  serialization, physics, or window/framebuffer scaling.

Run `python -m unittest discover -s tests -p 'test_*.py' -q` and native
`tools/test_engine_integration.py` on Windows and Linux. The latter needs
SDL2 and nlohmann/json development headers and an SDL display (for Linux
CI, `xvfb-run`); a syntax-only test without those dependencies does **not**
count as complete runtime verification.
