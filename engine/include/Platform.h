#pragma once
// Real compilation target; not controlled by runtime JSON.
#if defined(__EMSCRIPTEN__)
# define XENGINE_PLATFORM_WEB 1
#elif defined(__ANDROID__)
# define XENGINE_PLATFORM_ANDROID 1
#elif defined(_WIN32)
# define XENGINE_PLATFORM_WINDOWS 1
#elif defined(__APPLE__)
# include <TargetConditionals.h>
# if TARGET_OS_IPHONE
#  define XENGINE_PLATFORM_IOS 1
# else
#  define XENGINE_PLATFORM_MACOS 1
# endif
#elif defined(__linux__)
# define XENGINE_PLATFORM_LINUX 1
#else
# error "Unsupported XEngine platform"
#endif

#if defined(XENGINE_PLATFORM_WINDOWS) || defined(XENGINE_PLATFORM_LINUX) || defined(XENGINE_PLATFORM_MACOS)
# define XENGINE_PLATFORM_DESKTOP 1
#endif
#if defined(XENGINE_PLATFORM_ANDROID) || defined(XENGINE_PLATFORM_IOS)
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
