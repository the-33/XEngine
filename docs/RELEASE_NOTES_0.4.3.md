# XEngine 0.4.3 — Doxygen dropdown geometry hotfix

Previous development release: **0.4.2**.

## Fixed

- Corrected `@param outerRadii` documentation for the `CornerRadii` overload
  of `xengine::detail::ui::DropdownRowClip` in
  `engine/include/XEngine/detail/UIVisualGeometry.h`.
- Introduced a regression test comparing the documented parameter names
  and concrete function signatures in `UIVisualGeometry.h` and
  `UICornerRadii.h`.

## Compatibility

The change is documentation-only, with no runtime behavior, C++ public API,
game version or serialized-data change. It is a backwards-compatible engine
**PATCH** snapshot.

The full Python suite passed (**537 tests, 6 skipped, 0 failures**). The
native Doxygen executable and SDL2 development SDK are not installed on
the analysis host, so regenerate both references and validate Windows/Linux
Debug/Release builds on developer hardware.
