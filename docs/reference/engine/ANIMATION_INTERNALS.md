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

Assets are **versioned independently** of XEngine's `VERSION` and game
save files. Both JSON schemas currently accept `"version": 1` only.
The parser rejects future/malformed schema versions; it does not attempt to
migrate animation assets.

## Universal properties: safety invariants

Registration uses `AnimationPropertyRegistry::Register<T,V>` with callable
getter and setter or a direct `V T::*` member. `V` must be an alternative
of `AnimationValue`. The runtime registry indexes a component name and
property name, and stores a resolver for the component type. The shorthand
`XENGINE_ANIMATABLE(Type, Member)` expands to direct-member registration.

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
- Properties: register custom members and accessors, bind child path,
  invalidate an entity binding after destruction, verify getters/setters.
- Rendering: crossfade on both SDL render backends and distinct sprite sizes.
- Game integration: no regression to `InputAction` context navigation,
  serialization, physics, or window/framebuffer scaling.

Run `python -m unittest discover -s tests -p 'test_*.py' -q` and native
`tools/test_engine_integration.py` on Windows and Linux. The latter needs
SDL2 and nlohmann/json development headers and an SDL display (for Linux
CI, `xvfb-run`); a syntax-only test without those dependencies does **not**
count as complete runtime verification.
