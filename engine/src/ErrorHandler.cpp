#include "ErrorHandler.h"

#include "Engine.h"
#include "Platform.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

#if defined(XENGINE_PLATFORM_WINDOWS)
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {
enum class Level { Log, Warning, Error };

std::mutex gConsoleMutex;
std::thread gStdoutReader;
int gOriginalStdout = -1;
bool gOriginalUnitbuf = false;

int DuplicateFd(int fd) noexcept
{
#if defined(XENGINE_PLATFORM_WINDOWS)
    return _dup(fd);
#else
    return dup(fd);
#endif
}

int ReplaceFd(int from, int to) noexcept
{
#if defined(XENGINE_PLATFORM_WINDOWS)
    return _dup2(from, to);
#else
    return dup2(from, to);
#endif
}

void CloseFd(int fd) noexcept
{
#if defined(XENGINE_PLATFORM_WINDOWS)
    if (fd >= 0) (void)_close(fd);
#else
    if (fd >= 0) (void)close(fd);
#endif
}

int ReadFd(int fd, char* data, unsigned int size) noexcept
{
#if defined(XENGINE_PLATFORM_WINDOWS)
    return _read(fd, data, size);
#else
    return static_cast<int>(read(fd, data, size));
#endif
}

int WriteFd(int fd, const char* data, unsigned int size) noexcept
{
#if defined(XENGINE_PLATFORM_WINDOWS)
    return _write(fd, data, size);
#else
    return static_cast<int>(write(fd, data, size));
#endif
}

bool IsTerminal(int fd) noexcept
{
#if defined(XENGINE_PLATFORM_WINDOWS)
    return _isatty(fd) != 0;
#else
    return isatty(fd) != 0;
#endif
}

bool CreatePipe(int descriptors[2]) noexcept
{
#if defined(XENGINE_PLATFORM_WINDOWS)
    return _pipe(descriptors, 4096, _O_BINARY | _O_NOINHERIT) == 0;
#else
    return pipe(descriptors) == 0;
#endif
}

void WriteAll(int fd, std::string_view data) noexcept
{
    while (!data.empty()) {
        const auto chunk = static_cast<unsigned int>(std::min<size_t>(data.size(), 16384));
        const int written = WriteFd(fd, data.data(), chunk);
        if (written <= 0) return;
        data.remove_prefix(static_cast<size_t>(written));
    }
}

void Emit(Level level, std::string_view message, std::string_view title) noexcept
{
    std::lock_guard<std::mutex> lock(gConsoleMutex);
    const int target = gOriginalStdout >= 0 ? gOriginalStdout : 1;
    const bool colored = IsTerminal(target);
    const char* tint = level == Level::Error ? "\x1b[91m" :
                       level == Level::Warning ? "\x1b[93m" : "\x1b[96m";
    const char* prefix = level == Level::Error ? "✖ [ERROR] " :
                         level == Level::Warning ? "⚠ [WARNING] " : "● [LOG] ";

    if (colored) WriteAll(target, tint);
    WriteAll(target, prefix);
    WriteAll(target, title.empty() ? std::string_view("Game") : title);
    if (colored) WriteAll(target, "\x1b[0m");
    WriteAll(target, "\n");
    const auto writeLine = [&](std::string_view line) {
        WriteAll(target, "    ");
        WriteAll(target, line);
        WriteAll(target, "\n");
    };
    if (message.empty()) { writeLine({}); return; }
    while (!message.empty()) {
        const size_t lineEnd = message.find('\n');
        std::string_view line = message.substr(0, lineEnd);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        writeLine(line);
        if (lineEnd == std::string_view::npos) break;
        message.remove_prefix(lineEnd + 1);
        if (message.empty()) break;
    }
}

bool ErrorsEnabled() noexcept
{
    if (auto* engine = Engine::GetInstancePtr_NO_ERROR_MSG())
        return engine->logErrors;
    return true;
}

void ConsumeStdout(int readHandle) noexcept
{
    try {
        std::string pending;
        pending.reserve(512);
        char buffer[2048];
        for (;;) {
            const int readCount = ReadFd(readHandle, buffer, sizeof(buffer));
            if (readCount <= 0) break;
            for (int i = 0; i < readCount; ++i) {
                const char ch = buffer[i];
                if (ch == '\n' || ch == '\r') {
                    if (!pending.empty()) Debug::Log(pending);
                    pending.clear();
                } else {
                    pending.push_back(ch);
                    if (pending.size() >= 4096) {
                        Debug::Log(pending);
                        pending.clear();
                    }
                }
            }
        }
        if (!pending.empty()) Debug::Log(pending);
    } catch (...) {
        // Los errores en el lector no deben lanzar excepciones a través del hilo.
    }
    CloseFd(readHandle);
}
} // namespace

void Debug::Log(std::string_view message, std::string_view title) noexcept
{
    Emit(Level::Log, message, title);
}

void Debug::LogWarning(std::string_view message, std::string_view title) noexcept
{
    Emit(Level::Warning, message, title);
}

void Debug::LogError(std::string_view message, std::string_view title) noexcept
{
    if (ErrorsEnabled()) Emit(Level::Error, message, title);
}

void LogWarning(const std::string& caption, const std::string& warning) noexcept
{
    try {
        Debug::LogWarning(warning, caption);
    } catch (...) {}
}

ErrorLogResult LogError(const std::string& caption, const std::string& error
#if defined(XENGINE_PLATFORM_WINDOWS)
    , UINT
#endif
)
{
    if (!ErrorsEnabled()) return ErrorLogResult::OK;
    try {
        Debug::LogError(error, caption);
    } catch (...) {}
    return ErrorLogResult::Error;
}

ErrorLogResult LogError(const std::string& caption, const std::string& error,
    const char* sdlError
#if defined(XENGINE_PLATFORM_WINDOWS)
    , UINT
#endif
)
{
    if (!ErrorsEnabled()) return ErrorLogResult::OK;
    try {
        std::string detail = error;
        if (sdlError && *sdlError) detail += "\n" + std::string(sdlError);
        Debug::LogError(detail, caption);
    } catch (...) {}
    return ErrorLogResult::Error;
}

bool XEngineConsole::BeginStdoutCapture() noexcept
{
    if (gOriginalStdout >= 0) return true;
    std::fflush(stdout);
    std::cout.flush();
    int descriptors[2]{-1, -1};
    if (!CreatePipe(descriptors)) return false;
    const int original = DuplicateFd(1);
    if (original < 0) {
        CloseFd(descriptors[0]);
        CloseFd(descriptors[1]);
        return false;
    }

#if defined(XENGINE_PLATFORM_WINDOWS)
    // Windows necesita habilitar secuencias ANSI y UTF-8 para los iconos.
    if (IsTerminal(original)) {
        HANDLE output = reinterpret_cast<HANDLE>(_get_osfhandle(original));
        DWORD mode = 0;
        if (GetConsoleMode(output, &mode))
            (void)SetConsoleMode(output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        (void)SetConsoleOutputCP(CP_UTF8);
    }
#endif

    if (ReplaceFd(descriptors[1], 1) < 0) {
        CloseFd(original);
        CloseFd(descriptors[0]);
        CloseFd(descriptors[1]);
        return false;
    }
    CloseFd(descriptors[1]);
    gOriginalStdout = original;
    gOriginalUnitbuf = (std::cout.flags() & std::ios::unitbuf) != 0;
    std::cout.setf(std::ios::unitbuf);
    try {
        gStdoutReader = std::thread([reader = descriptors[0]] { ConsumeStdout(reader); });
    } catch (...) {
        (void)ReplaceFd(original, 1);
        CloseFd(descriptors[0]);
        CloseFd(original);
        gOriginalStdout = -1;
        if (!gOriginalUnitbuf) std::cout.unsetf(std::ios::unitbuf);
        return false;
    }
    return true;
}

void XEngineConsole::EndStdoutCapture() noexcept
{
    if (gOriginalStdout < 0) return;
    std::cout.flush();
    std::fflush(stdout);
    const int original = gOriginalStdout;
    (void)ReplaceFd(original, 1);
    if (gStdoutReader.joinable()) gStdoutReader.join();
    {
        std::lock_guard<std::mutex> lock(gConsoleMutex);
        CloseFd(original);
        gOriginalStdout = -1;
    }
    if (!gOriginalUnitbuf) std::cout.unsetf(std::ios::unitbuf);
}
