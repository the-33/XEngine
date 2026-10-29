# Platforms and conditional compilation {#xengine_game_build_macros}

XEngine provides compile-time macros so a game can select platform-specific
implementations and Debug/Release functionality without testing operating
system names at runtime. The official build pipeline currently supports
**Windows and Linux**, including cross-compilation between them.

Both headers are included by `<XEngine.h>`:

```cpp
#include <XEngine.h>
// Or, for focused includes:
#include <Platform.h>
#include <BuildConfig.h>
```

## Target platform macros

`Platform.h` derives platform macros from **the compiler target**, not the
machine running `tools/build.py`. Exactly one concrete operating-system macro
is selected; the category macros may be defined in addition.

| Macro | Meaning | Official build target? |
| --- | --- | --- |
| `XENGINE_PLATFORM_WINDOWS` | Windows executable | Yes |
| `XENGINE_PLATFORM_LINUX` | Linux executable | Yes |
| `XENGINE_PLATFORM_MACOS` | macOS compilation detected | No |
| `XENGINE_PLATFORM_ANDROID` | Android compilation detected | No |
| `XENGINE_PLATFORM_IOS` | iOS compilation detected | No |
| `XENGINE_PLATFORM_WEB` | Emscripten/Web compilation detected | No |
| `XENGINE_PLATFORM_DESKTOP` | Windows, Linux or macOS | Category, not target |
| `XENGINE_PLATFORM_MOBILE` | Android or iOS | Category, not target |

Use `defined(...)` to inspect platform macros because **unselected platforms
are not defined**:

```cpp
#if defined(XENGINE_PLATFORM_WINDOWS)
    // Windows-specific implementation.
#elif defined(XENGINE_PLATFORM_LINUX)
    // Linux-specific implementation.
#else
    // Code for another supported compiler target, if added later.
#endif
```

For example, a Windows-hosted cross-compiler targeting Linux defines
`XENGINE_PLATFORM_LINUX`, not `XENGINE_PLATFORM_WINDOWS`. XEngine's build tool
also sets `XENGINE_TARGET_WINDOWS=1` or `XENGINE_TARGET_LINUX=1` to verify
that the chosen compiler actually produces the requested platform's binary.
Do not define or override reserved `XENGINE_` macros in a game's `config.json`.

The other platform detection macros are present for source portability;
**their existence does not mean the engine can build/distribute on those
platforms yet**.

## Debug, Release and feature flags

`tools/build.py` assigns exactly one build mode:

| Macro or constant | Debug | Release |
| --- | --- | --- |
| `XENGINE_DEBUG` | Defined as `1` | Undefined |
| `XENGINE_RELEASE` | Undefined | Defined as `1` |
| `NDEBUG` | Undefined | Defined |
| `XENGINE_ENABLE_DEBUG_DRAW` | `1` | `0` |
| `XENGINE_ENABLE_PROFILER` | `1` | `0` |
| `XENGINE_ENABLE_STATS` | `1` | `0` |
| `XEngineBuild::IsDebug` | `true` | `false` |
| `XEngineBuild::IsRelease` | `false` | `true` |

The build flags are `-O0 -g3 -DXENGINE_DEBUG=1` for Debug and
`-O2 -DNDEBUG -DXENGINE_RELEASE=1` for Release. `BuildConfig.h` requires
exactly one XEngine build-mode macro, and Release must also define `NDEBUG`.
When compiling game code manually outside the supported build driver, you
must provide the same target/mode definitions consistently.

The optional build flag **`--no-doc`** affects *documentation generation*,
not C++ macro definitions or Debug/Release output. It skips local Doxygen
when the engine rebuilds and selects the public online API in the optional
VS Code hover integration until the next successful build without that flag.
See the [newcomer setup](../../DEVELOPER_GUIDE.md#first-checkout-optional-contributor-tools)
for Git hooks, VS Code and documentation options.

There are **two different ways to test macros**:

```cpp
#if defined(XENGINE_DEBUG)
    // This is compiled only in Debug.
#endif

#if XENGINE_ENABLE_PROFILER
    // This is compiled only when the profiler feature flag is 1.
#endif
```

The `XENGINE_ENABLE_*` feature flags are **always defined** and take values
`0` or `1`. Therefore `#ifdef XENGINE_ENABLE_PROFILER` would be wrong:
it would be true even in Release, where the macro is defined as `0`.

For C++20 code, the constexpr values are often clearer:

```cpp
if constexpr (XEngineBuild::IsDebug)
{
    // Debug-only C++ branch; compiled out of a Release translation unit.
}
```

`NDEBUG` also disables standard `assert` checks. Avoid putting state-changing
operations inside `assert`, because those operations disappear in Release.

## A platform-specific Behaviour

The following is a simplified pattern for a gameplay script; the platform
branch is selected at compile time, not every frame:

```cpp
#include <Behaviour.h>
#include <Platform.h>
#include <BuildConfig.h>

class PlatformMessage : public Behaviour
{
public:
    void Start() override
    {
#if defined(XENGINE_PLATFORM_WINDOWS)
        // Windows-specific initialization.
#elif defined(XENGINE_PLATFORM_LINUX)
        // Linux-specific initialization.
#endif

#if XENGINE_ENABLE_STATS
        // Optional development-only statistics initialization.
#endif
    }
};
```

Keep operating-system API calls behind small platform-specific functions
where practical. For data files, paths and user preferences, prefer XEngine's
portable `FileSystem`, `AtomicFile` and `PlayerPrefs` rather than implementing
separate save systems for Windows and Linux.

## Related manuals

- [Game project structure and includes](PROJECTS_AND_INCLUDES.md)
- [Full Developer Guide](../../DEVELOPER_GUIDE.md)
- [Cross-compilation setup](../../CROSS_COMPILATION.md)
- [Persistence and serialization](../../PERSISTENCE_AND_SERIALIZATION.md)
