# XEngine — Engine Developer Reference

This is the **complete codebase reference** for contributors modifying
XEngine itself. It includes public APIs, protected/private implementation
members, internal engine headers and `.cpp` implementation files. Internal
fields and symbols shown here are **not** automatically supported gameplay
APIs; they may change between engine versions.

If you are implementing a game instead, use the **Game Developer Reference**
at `build/docs/game/html/index.html` and the engine's public headers.

## Start here

- @subpage xengine_engine_internals "Architecture, ownership and extension points"
- @subpage xengine_documentation_workflow "Maintaining XEngine documentation"
- @subpage xengine_engine_manuals "Developer manuals and current guides"
- @subpage xengine_game_reference "Game API and feature tutorials"
- @subpage xengine_animation_internals "Animation evaluator and property registry"

## Explore the implementation

Use the **Files** list to navigate `engine/src/*.cpp` and internal headers;
use **Classes** for private and public members and **Modules** for the
subsystem groupings. Search is available for individual symbols. The
generated source browser is a navigation tool, not a replacement for
building and testing the engine.

For every public change, preserve the distinction between an implementation
detail and a supported developer API. Expose validated state via public
methods or `Property`/`PropertyRO`; do not encourage game code to access
internal backing fields or their adapter type aliases.

### File Members versus class members

The Doxygen **File Members** section indexes declarations that belong to an
individual translation unit or header rather than to a class. Its **Variables**
view is not an inventory of every engine field: most engine state is owned
by classes, whose fields are documented in **Classes**. It is normal for
File Members → Variables to contain relatively few entries even with
`EXTRACT_ALL`, `EXTRACT_PRIVATE` and `EXTRACT_STATIC` enabled. For example,
the engine currently exposes a small set of file-scope UI state in
`UIManager.cpp`; class-owned state belongs in the corresponding class page.
Use the source browser or XML reference to audit private code symbols.

### Screen-space UI geometry and focus

`UIManager` queues immediate-mode widget commands, including styled rounded
rectangles and vector dropdown chevrons. `RenderManager::DrawRoundedRectScreen`
and `DrawLineScreen` submit their geometry through SDL and also record distinct
`DrawCmd::Type::ScreenRoundedRect` and `ScreenLine` commands for the deferred
snapshot path. These commands bypass camera projection, use drawable pixels
and count as UI work. Keep the immediate and recorded paths equivalent when
changing their drawing logic.

The UI focus list is rebuilt every frame. `SetInitialFocus` retains a requested
ID until the next matching widget registration; `SetAutoSelectFirst(false)`
prevents the normal fallback that selects the first widget. Directional
navigation uses `UINavigation` ID overrides where present and geometric
nearest-neighbour selection otherwise. Explicit targets must exist in the
previous frame's focus list, which prevents navigation to hidden widgets.
Style overrides use a balanced stack, and each queued drawing command
captures the appearance it needs before `PopStyle` restores the base style.
`UICornerRadiusUnit::Percent` converts the requested radius using the shortest
side of each resolved widget; pixel mode follows canvas scaling instead.

The root `VERSION` identifies the engine release; a game's own version does
not change when the engine is rebuilt. Follow the
[versioning and compatibility guide](../../VERSIONING.md) for release policy.
