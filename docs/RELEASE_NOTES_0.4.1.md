# XEngine 0.4.1 — Documentation build hotfix

Previous development release: **0.4.0**.

## Fixed

- Named the unused `UIWidgetEvents& events` argument in five decorative
  `UIWidget::DrawContent` implementations, matching their existing Doxygen
  `@param events` documentation. This resolves the reported Doxygen
  parameter-name warnings (which stop builds under `WARN_AS_ERROR`).
- Added a regression check comparing every `DrawContent` Doxygen `@param`
  with the corresponding C++ function argument name.

## Compatibility and release notes

No new UI APIs, rendering changes, serialized-data changes or game-version
updates. This is a backwards-compatible engine **PATCH** release. It retains
all keyed canvas and fluent widget behavior introduced in 0.4.0.

Focused widget tests compile and pass with a C++20 compiler. The full Python
test suite reports **535 tests, 6 skipped, 0 failures**. The development host
does not provide Doxygen or the native SDL2 SDK; regenerate both Doxygen sites
and run Windows/Linux Debug/Release validation before final distribution.
