# XEngine 0.4.2 — Doxygen Find documentation hotfix

Previous development release: **0.4.1**.

## Fixed

- Removed duplicate Doxygen documentation from both out-of-class template
  definitions of `UIManager::Find` in `engine/include/UIWidgets.h`.
  Their declarations in `engine/include/UIManager.h` remain the single
  documented source of `@brief`, `@tparam T`, `@param path`, and `@return`.
- Added regression coverage so the merged declaration/definition Doxygen
  parameter problem cannot silently recur.

## Compatibility

This is a backwards-compatible engine **PATCH** release. The changes are
limited to comments, regression tests and version/release metadata. The
public UI API, runtime behavior, game versions and save/asset formats have
not changed.

The full Python suite passed (**536 tests, 6 skipped, 0 failures**), including
SDL-independent widget C++20 compilation. A real Doxygen executable and the SDL2 development SDK
are unavailable on this host; the Doxygen sites and native Windows/Linux
Debug/Release builds still require maintainer verification.
