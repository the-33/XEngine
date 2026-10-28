#pragma once
/** @file Platform.h
 * @brief Public compile-time target-platform detection macros.
 * @details Selected macros describe the compiled executable, not the host
 * running the build tool. Windows and Linux are the current supported targets.
 * Each `XENGINE_PLATFORM_*` macro is defined only when applicable.
 * @see xengine_game_build_macros
 */
// Real compilation target; not controlled by runtime JSON.
#if defined(__EMSCRIPTEN__)
/// @brief Defined as 1 when compiling for Emscripten (not an official target).
# define XENGINE_PLATFORM_WEB 1
#elif defined(__ANDROID__)
/// @brief Defined as 1 when compiling for Android (not an official target).
# define XENGINE_PLATFORM_ANDROID 1
#elif defined(_WIN32)
/// @brief Defined as 1 for the Windows target, including cross-compilation.
# define XENGINE_PLATFORM_WINDOWS 1
#elif defined(__APPLE__)
# include <TargetConditionals.h>
# if TARGET_OS_IPHONE
/// @brief Defined as 1 for iOS targets (not an official build target).
#  define XENGINE_PLATFORM_IOS 1
# else
/// @brief Defined as 1 for macOS targets (not an official build target).
#  define XENGINE_PLATFORM_MACOS 1
# endif
#elif defined(__linux__)
/// @brief Defined as 1 for Linux targets, including cross-compilation.
# define XENGINE_PLATFORM_LINUX 1
#else
# error "Unsupported XEngine platform"
#endif

#if defined(XENGINE_PLATFORM_WINDOWS) || defined(XENGINE_PLATFORM_LINUX) || defined(XENGINE_PLATFORM_MACOS)
/// @brief Defined as 1 on desktop targets; currently Windows/Linux supported.
# define XENGINE_PLATFORM_DESKTOP 1
#endif
#if defined(XENGINE_PLATFORM_ANDROID) || defined(XENGINE_PLATFORM_IOS)
/// @brief Defined as 1 for Android or iOS targets (unsupported in build.py).
# define XENGINE_PLATFORM_MOBILE 1
#endif

#if defined(XENGINE_TARGET_WINDOWS) && !defined(XENGINE_PLATFORM_WINDOWS)
# error "Requested Windows but toolchain is not targeting Windows"
#endif
#if defined(XENGINE_TARGET_LINUX) && !defined(XENGINE_PLATFORM_LINUX)
# error "Requested Linux but toolchain is not targeting Linux"
#endif
#if defined(XENGINE_TARGET_ANDROID) && !defined(XENGINE_PLATFORM_ANDROID)
# error "Requested Android but toolchain is not targeting Android"
#endif
