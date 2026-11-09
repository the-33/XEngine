# XEngine 0.4.4 — Doxygen friend contract hotfix

Previous development release: **0.4.3**.

## Fixed

- Documented the `friend class UIWidgetCanvas` access declaration in
  `engine/include/UIWidgets.h`.
- Updated `tools/check_doxygen_docs.py` so Doxygen XML entries for friend
  declarations are not misclassified as undocumented public API members.
  Regular public functions still require descriptions, named parameter
  contracts and return-value descriptions.
- Added a synthetic XML regression test for the formerly reported
  `UIWidget::UIWidgetCanvas` false positive.

## Compatibility

This is a backwards-compatible **PATCH** snapshot with no public C++ API,
runtime behavior, save/asset format or game version changes.

Full Python regression suite: **538 tests, 6 skipped, 0 failures**. The
local development environment does not include the native Doxygen or
SDL2 SDK tools, so actual documentation generation and Windows/Linux
Debug/Release engine builds must be validated on maintainer hardware.
