# Architecture, ownership and extension points {#xengine_engine_internals}

Unlike the game-facing reference, this profile includes code from both
`engine/include/` and `engine/src/`, private implementation records and
helper types. You can inspect the actual manager implementations and their
dependencies without mistaking them for supported game APIs.

## Initialization and main loop

@ref Engine manages SDL initialization, runtime configuration, scene and
manager startup, event handling, fixed update and drawing. The game entry
point calls `Engine::Start` / `StartFromJson`, sets `SetInitCallback`, then
enters `Engine::Run`. The startup functions are not interchangeable with
ordinary per-frame APIs.

Look at `engine/src/Engine.cpp`, @ref SceneManager and @ref WindowManager
to understand lifecycle boundaries. Avoid exposing ownership of SDL objects
or platform handles through gameplay-facing APIs unless absolutely needed.

## Scene ownership and component callbacks

@ref Scene owns its entities; @ref GameObject owns components; @ref Behaviour
provides overridable gameplay hooks. Private flags such as `Component::mAwoken`
and internal `Awake_`/`Start_` forwarding methods exist to preserve callback
ordering. These internals should **not** appear in the game developer profile.

Scene transitions are queued and applied at a safe boundary. Any change to
component ownership, object destruction or persistent scenes requires
lifecycle regression tests.

## Rendering and SDL boundaries

@ref RenderManager and the files `RenderPreparation.*`,
`RendererBackendPolicy.*`, and `SpriteProjection.*` implement public drawing
and internal optimization decisions. Review cache invalidation, texture
lifetime, scaling and renderer fallback when changing these paths.
The benchmark and adaptive-selector manuals are evidence and diagnostic
material, **not** universal performance guarantees.

## Physics, collision and concurrency

@ref PhysicsManager and @ref CollisionManager coordinate body steps,
collision broadphase and narrow contacts. The spatial and adaptive
broadphase headers are internal. Preserve deterministic fixed-step behavior,
contact enter/stay/exit ordering and trigger filtering when modifying them.

Worker-thread tasks must not call SDL APIs that belong to the main thread.
Read the threading and render diagnostics manuals before introducing
parallel work, especially to paths with shared object pointers.

## Persistence and platform integration

@ref FileSystem exposes direct file operations. @ref AtomicFile implements
single-file publish and candidate recovery; its OS-specific durability steps
belong in the engine implementation. Format, version, magic, checksums and
multi-file transactions belong in the game. @ref PlayerPrefs uses
platform-specific user preferences, not Unity's exact file format.

## Configuration, tooling and tests

- `tools/build.py` owns native/cross target selection and incremental C++
  builds; its optional Doxygen step should not run on game-only changes.
- `tools/sync_engine_configs.py` and `tools/sync_game_configs.py` synchronize
  schemas without deleting unknown per-game settings.
- Run `python -B -m unittest discover -s tests -q` before distributing a patch.
- Regenerate both documentation audiences with `python tools/build_doxygen.py`.

Add Doxygen descriptions to new exported classes and values, and use
`@ingroup` to place APIs in the appropriate subsystem. Internal symbols can
be described in this profile without exposing them as supported game APIs.

For the complete contributor checklist, linking conventions, theme markers
and generation commands, see
[Maintaining XEngine documentation](DOCUMENTATION_WORKFLOW.md).

## Documentation branding and Markdown image assets

The generated website uses `docs/reference/theme/xengine.css` and Doxygen's
own light/dark mode. Both XEngine logo variants are bundled into each HTML
output. Markdown source files may also contain GitHub-specific
`#gh-light-mode-only` / `#gh-dark-mode-only` fragments; those fragments work
on GitHub but are **not understood by Doxygen**.

Use `python tools/build_doxygen.py` to publish or preview the API site. After
generating HTML, it resolves the Markdown logo images to copies available in
the website, shows the correct light/dark variant and groups the Markdown
navigation tree. It does not rewrite the original `.md` files, so their
GitHub rendering remains intact.

Benchmarking, profiling and renderer-diagnostic documentation intentionally
has the **green/teal XEngine Benchmark** palette and header logo. Place
`<span data-xengine-doc-theme="profiling"></span>` under a new profiling
Markdown's heading; the generator detects this even if Doxygen changes the
output HTML filename. `PROFILING_PAGE_MARKERS` remains a fallback for generated
C++ diagnostic symbols and legacy pages. Ordinary game/engine API pages use
the **purple XEngine** palette. Regenerate both audiences and check the logo,
navigation, highlighted headings and links in both light and dark modes.
