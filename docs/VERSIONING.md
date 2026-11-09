# XEngine versions, compatibility and releases {#xengine_versioning}

XEngine uses **Semantic Versioning 2.0.0** for engine releases. Its single
official version is the plain UTF-8 string in the repository-root **`VERSION`**
file, currently **`0.4.4`**. Do not prefix the value with `v`, put a release
number directly into Doxygen configuration, or increment it for every commit
or conversation.

The old **`v16.37`** label was the last *development-iteration identifier*,
not a SemVer major release. Existing archived reports, older ZIP filenames
and historical `AI_CONTEXT.md` entries retain their original labels. Engine
`0.1.0` is the start of the new official release series and corresponds to
the code state that followed that last iteration.

## What MAJOR.MINOR.PATCH means

| Number | When XEngine increments it after 1.0.0 | Example |
| --- | --- | --- |
| **MAJOR** | A previously supported public gameplay API changes incompatibly, requiring game code or data migration. | `1.4.2` → `2.0.0` |
| **MINOR** | A backwards-compatible game feature or public API becomes available. Deprecations are also announced here. | `1.4.2` → `1.5.0` |
| **PATCH** | A backwards-compatible fix or documentation-only release, with no new required migration. | `1.4.2` → `1.4.3` |

Reset lower-order numbers to zero when incrementing MAJOR or MINOR.
Version `1.0.0` is the planned stable-API milestone. The current development
release is `0.4.4`, which follows the 0.x compatibility policy described below.

## Historical 0.x.y development policy

XEngine used the following *project policy* while its major version was zero:

- `0.1.0`: first officially numbered development release.
- `0.1.1`: backwards-compatible corrections (including published
  documentation-only fixes) to the `0.1` release series.
- `0.1.2`: documentation-navigation reliability and theme corrections with
  no C++ API or gameplay-serialization changes.
- `0.1.3`: consistent, contrast-safe Doxygen footer attribution and complete
  Benchmark search popup palette; no C++ API changes.
- `0.1.4`: clear and consistent selected/hovered navigation row backgrounds
  and text contrast in both sidebars, all four color schemes; no C++ API changes.
- `0.1.5`: backwards-compatible runtime correctness and stability fixes:
  deferred `Engine::Stop()` shutdown, cycle-safe scene parenting, consistent
  world transforms and mouse deltas, cached asset key aliases, content-based
  UI widget IDs and updated scene name lookup. Also adjusts HiDPI mouse/UI
  coordinates and the affected API documentation. Game project versions do
  not change.
- `0.2.0`: new capabilities, substantial engine changes or an intentional
  incompatible API adjustment. Explicitly describe any migration.
- `0.2.1`: preceding development release, before the new UI line/layer API.
- `0.3.0`: new UI line drawing (canvas-scaled thickness), numeric UI layers,
  topmost-hit interaction priority, and per-element layer parameters; also
  accumulates DPI, UI-layout validation, rendering and diagnostic corrections.
  Existing widget calls and `PushLayer`/`PopLayer` remain supported. The game
  save format and animation/controller asset schema versions do not change.
- `0.4.0`: retained widget canvases, keyed/typed element lookup, chainable
  UI control properties and per-control callbacks (click, value changes,
  pointer/focus/editing, and dropdown state). The existing immediate-mode
  calls, rendering backends, canvas scaling and serialization formats stay
  compatible. This is a new development API; native Windows/Linux validation
  is required before considering it fully platform-verified.
- `0.4.1`: documentation build fix for the retained canvas widgets: all
  `DrawContent` overrides now name their deferred event parameter so that
  Doxygen's `@param events` check matches the C++ declaration. The API,
  behavior, save formats, and game project versions are unchanged.
- `0.4.2`: documentation build fix for the two templated `UIManager::Find`
  definitions: remove duplicate Doxygen `@param path` and `@tparam T` sections
  already supplied by their public declarations. No runtime/API changes.
- `0.4.3`: fixed the Doxygen argument name for the per-corner dropdown
  clipping geometry helper (`outerRadii`) and added geometry documentation
  signature validation. No runtime/API changes.
- `0.4.4`: documented the `UIWidgetCanvas` friend access in `UIWidget` and
  taught the Doxygen XML API checker not to treat friend access declarations
  as separately documented functions or public API contracts.
- `1.0.0`: declare a documented, tested, supported gameplay API stable.

This list records the historical development series; it does not redefine
the post-1.0 compatibility rules for current releases.

## When to make a release

**A code edit is not automatically a new release.** Many patches, tests and
documentation adjustments may be accumulated into one release. Raise the
version only when publishing a distinct snapshot intended for use by other
developers. Never change the bytes of an already-published version's ZIP;
publish the next version instead.

**At the end of every development iteration, review the version decision.**
Compare the accumulated changes with the latest *released* engine, decide
whether to publish now, and record the conclusion (including **no version
bump**) in `AI_CONTEXT.md` or the iteration handover. If a release is planned,
choose PATCH/MINOR/MAJOR using the compatibility rules above; do not increment
the engine version automatically just because a commit, build, or iteration
finished.

Before a release:

1. Identify changes against the preceding released engine, not against the
   latest individual patch. Check public header compatibility, serialized
   data contracts and engine configuration migration.
2. Select the appropriate version increment, update the root `VERSION`
   **once**, and record compatibility, supported targets and important changes
   in the release notes. Old entries in `AI_CONTEXT.md` remain historical.
3. Run the [Python regression suite and real SDL2 integration tests](TESTING.md)
   in Windows/Linux **Debug and Release**, review the CI matrix, perform
   targeted manual checks for affected features, and build/validate both
   Doxygen references. Passing tests supports a release decision but cannot
   prove that all hardware and gameplay paths are stable. Review `README.md`,
   `docs/` and Doxygen source comments for current API behavior.
4. Distribute engine source with a filename derived from `VERSION`, e.g.
   `XEngine-0.4.4-source.zip`, and use the corresponding immutable Git tag
   `v0.4.4` when releasing through Git. The `v` is a *tag convention*;
   it is not part of the canonical `VERSION` content.

Optional pre-releases follow normal SemVer notation, e.g.
`0.2.0-beta.1`; these are not stable final releases.

## One source for docs, engine archives and metadata

From the XEngine repository root:

```sh
python tools/engine_version.py
# 0.4.4
python tools/engine_version.py --archive-name source
# XEngine-0.4.4-source.zip
python tools/engine_version.py --archive-name patch
# XEngine-0.4.4-patch.zip
python tools/build_doxygen.py
```

`tools/build_doxygen.py` reads the root `VERSION` and passes
`XENGINE_VERSION` to Doxygen; both `Doxyfile_Games` and `Doxyfile_Engine`
render that exact value through `PROJECT_NUMBER = "$(XENGINE_VERSION)"`.
Running raw `doxygen Doxyfile_Games` does not perform the mandatory generator
post-processing or supply the version environment variable.

The optional `.githooks/pre-commit` calls
`tools/build_doxygen_if_staged.py`. **When `VERSION` is staged**, this script
reads the staged SemVer value, updates only the `Engine release` label in
the root `README.md`, and automatically stages that README update before
checking or generating Doxygen. This works even if Doxygen is not installed.
If `README.md` contains other unstaged edits, the hook stops with a clear
error so it never commits those edits without your approval. Stage or stash
them first, then retry the commit. Without the optional hook, update the
README label yourself when changing `VERSION`.

The engine build keeps its normal `libXEngine.a` archive name so existing
link commands work. The *distributed engine source archive* and its tag
carry the engine release version, not the engine library's internal filename.
Never substitute a game's own version into the engine's distribution name.

## Game versions are independent

Each game sets its own `project.version` in
`games/<Game>/config.json`. That version controls **the game's** Release
package and is preserved by the project's configuration synchronization.
It must not read or overwrite XEngine's root `VERSION`.

For example, a game may be `2.3.4` while using XEngine `0.3.0`; upgrading
the engine does not implicitly publish a new game version. The game developer
decides when to version the game itself.

## Further reading

- [First game and project creation](GETTING_STARTED.md)
- [Game build manifests](GAME_CONFIG_SYNC.md)
- [Developer Guide](DEVELOPER_GUIDE.md)
- Engine contributors: `docs/reference/engine/DOCUMENTATION_WORKFLOW.md`
  documents how to keep the public and private references synchronized.
- [Semantic Versioning 2.0.0 specification](https://semver.org/spec/v2.0.0.html)
