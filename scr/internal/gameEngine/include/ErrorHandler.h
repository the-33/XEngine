#pragma once

#include <string>

#ifdef _WIN32
// Evita incluir partes innecesarias de Windows.h
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// Evita conflictos con min/max
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#endif

enum class ErrorLogResult
{
    OK = 1,
    Cancel = 2,
    Abort = 3,
    Retry = 4,
    Ignore = 5,
    Yes = 6,
    No = 7,
    Close = 8,
    Help = 9,
    TryAgain = 10,
    Continue = 11,
    Error = -1  // Usado cuando falla o no se muestra
};

// Log de errores multiplataforma
ErrorLogResult LogError(const std::string& caption,
    const std::string& error
#ifdef _WIN32
    , UINT msgBoxType = MB_OK | MB_ICONSTOP | MB_SETFOREGROUND
#endif
);

ErrorLogResult LogError(const std::string& caption,
    const std::string& error,
    const char* sdlError
#ifdef _WIN32
    , UINT msgBoxType = MB_OK | MB_ICONSTOP | MB_SETFOREGROUND
#endif
);