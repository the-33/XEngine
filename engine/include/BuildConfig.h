#pragma once

// Defined only by the build scripts. Never override these in game config.json.
#if defined(XENGINE_DEBUG) == defined(XENGINE_RELEASE)
# error "Exactly one of XENGINE_DEBUG and XENGINE_RELEASE must be defined"
#endif
#if defined(XENGINE_RELEASE) && !defined(NDEBUG)
# error "Release builds must define NDEBUG"
#endif

namespace XEngineBuild {
#if defined(XENGINE_DEBUG)
inline constexpr bool IsDebug = true;
inline constexpr bool IsRelease = false;
#else
inline constexpr bool IsDebug = false;
inline constexpr bool IsRelease = true;
#endif
}

#if defined(XENGINE_DEBUG)
# define XENGINE_ENABLE_DEBUG_DRAW 1
# define XENGINE_ENABLE_PROFILER 1
# define XENGINE_ENABLE_STATS 1
#else
# define XENGINE_ENABLE_DEBUG_DRAW 0
# define XENGINE_ENABLE_PROFILER 0
# define XENGINE_ENABLE_STATS 0
#endif
