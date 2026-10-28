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

/** @brief Available error log result modes for ErrorHandler.h.
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
/** @brief Reports an engine error through the configured logging mechanism.
 * @param caption Visible label on the widget.
 * @param error Error condition to report.
 */
ErrorLogResult LogError(const std::string& caption,
    const std::string& error
#ifdef _WIN32
    , UINT msgBoxType = MB_OK | MB_ICONSTOP | MB_SETFOREGROUND
#endif
);

/** @brief Reports an engine error through the configured logging mechanism.
 * @param caption Visible label on the widget.
 * @param error Error condition to report.
 * @param sdlError SDL error message associated with the failed operation.
 */
ErrorLogResult LogError(const std::string& caption,
    const std::string& error,
    const char* sdlError
#ifdef _WIN32
    , UINT msgBoxType = MB_OK | MB_ICONSTOP | MB_SETFOREGROUND
#endif
);
