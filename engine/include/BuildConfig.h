#pragma once
/** @file BuildConfig.h
 * @brief Public compile-time build-mode flags and constexpr helpers.
 * @details XEngine's build driver sets exactly one of XENGINE_DEBUG and
 * XENGINE_RELEASE; games must not override these reserved definitions.
 * @par Build-driver definitions
 * - `XENGINE_DEBUG`: defined as `1` in Debug, absent in Release.
 * - `XENGINE_RELEASE`: defined as `1` in Release, absent in Debug.
 * - `NDEBUG`: defined in Release, absent in Debug.
 * @see xengine_game_build_macros
 */

// Defined only by the build scripts. Never override these in game config.json.
#if defined(XENGINE_DEBUG) == defined(XENGINE_RELEASE)
# error "Exactly one of XENGINE_DEBUG and XENGINE_RELEASE must be defined"
#endif
#if defined(XENGINE_RELEASE) && !defined(NDEBUG)
# error "Release builds must define NDEBUG"
#endif

namespace XEngineBuild {
#if defined(XENGINE_DEBUG)
/// @brief True if the translation unit was compiled in Debug mode.
inline constexpr bool IsDebug = true;
/// @brief True if the translation unit was compiled in Release mode.
inline constexpr bool IsRelease = false;
#else
/// @brief True if the translation unit was compiled in Debug mode.
inline constexpr bool IsDebug = false;
/// @brief True if the translation unit was compiled in Release mode.
inline constexpr bool IsRelease = true;
#endif
}

#if defined(XENGINE_DEBUG)
/// @brief Enables debug-only rendering helpers when its value is 1.
# define XENGINE_ENABLE_DEBUG_DRAW 1
/// @brief Enables profiling instrumentation when its value is 1.
# define XENGINE_ENABLE_PROFILER 1
/// @brief Enables engine debug statistics when its value is 1.
# define XENGINE_ENABLE_STATS 1
#else
/// @brief Debug drawing is disabled in Release (value 0).
# define XENGINE_ENABLE_DEBUG_DRAW 0
/// @brief Profiling instrumentation is disabled in Release (value 0).
# define XENGINE_ENABLE_PROFILER 0
/// @brief Engine debug statistics are disabled in Release (value 0).
# define XENGINE_ENABLE_STATS 0
#endif
