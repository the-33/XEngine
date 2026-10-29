#pragma once
/** @file ErrorHandler.h
 * @brief Runtime diagnostic logging functions.
 */

#include <string>

#ifdef _WIN32
// Avoid including unnecessary parts of Windows.h
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// Avoid conflicts with min/max
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#endif

/** @brief Dialog response values, or `Error` when no response was returned.
 * @note On Linux, the current implementation shows an SDL error dialog when
 *       video is initialized, logs through SDL and returns `Error`.
 */
enum class ErrorLogResult
{
    OK = 1,      ///< User selected OK in the dialog.
    Cancel = 2,  ///< User cancelled the dialog.
    Abort = 3,   ///< User selected Abort.
    Retry = 4,   ///< User selected Retry.
    Ignore = 5,  ///< User selected Ignore.
    Yes = 6,     ///< User confirmed Yes.
    No = 7,      ///< User selected No.
    Close = 8,   ///< User closed the dialog via Close.
    Help = 9,    ///< User selected Help.
    TryAgain = 10, ///< User selected Try Again.
    Continue = 11, ///< User selected Continue.
    Error = -1   ///< No dialog result is available or the operation failed.
};

// Cross-platform error logging
/** @brief Reports an engine error in a platform dialog and/or SDL log.
 * @details On Windows, MessageBoxA is used first; otherwise SDL displays a
 * message box if video is initialized. The message is also sent to SDL_Log
 * unless the Windows dialog returned a result first. When `Engine::logErrors`
 * is false, the call is suppressed and returns `OK`.
 * @param caption Title of the error dialog and prefix of the log entry.
 * @param error Error condition to report.
 * @return A Windows dialog selection, or `Error` if no response is available;
 *         `OK` if engine error logging has been disabled.
 */
ErrorLogResult LogError(const std::string& caption,
    const std::string& error
#ifdef _WIN32
    , UINT msgBoxType = MB_OK | MB_ICONSTOP | MB_SETFOREGROUND
#endif
);

/** @brief As LogError(caption, error), appending an SDL error string.
 * @param caption Title of the error dialog and prefix of the log entry.
 * @param error Error condition to report.
 * @param sdlError Non-null, null-terminated SDL error message; pass SDL_GetError()
 *                 when reporting an SDL failure.
 * @return A Windows dialog selection, `Error`, or suppressed-logging `OK`.
 */
ErrorLogResult LogError(const std::string& caption,
    const std::string& error,
    const char* sdlError
#ifdef _WIN32
    , UINT msgBoxType = MB_OK | MB_ICONSTOP | MB_SETFOREGROUND
#endif
);
