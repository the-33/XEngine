#include "FileSystem.h"

#include "SDL.h"

#include <cerrno>
#include <fstream>
#include <limits>
#include <system_error>
#if defined(_WIN32)
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#endif

namespace {
FileError MapError(const std::error_code& error) noexcept {
    if (!error) return FileError::None;
    if (error == std::errc::no_such_file_or_directory) return FileError::NotFound;
    if (error == std::errc::permission_denied) return FileError::PermissionDenied;
    if (error == std::errc::file_exists) return FileError::AlreadyExists;
    return FileError::IoError;
}

bool Invalid(const std::filesystem::path& path) noexcept { return path.empty(); }
}

bool FileSystem::Exists(const std::filesystem::path& path) noexcept {
    std::error_code ec;
    return !Invalid(path) && std::filesystem::exists(path, ec) && !ec;
}
bool FileSystem::IsRegularFile(const std::filesystem::path& path) noexcept {
    std::error_code ec;
    return !Invalid(path) && std::filesystem::is_regular_file(path, ec) && !ec;
}
bool FileSystem::IsDirectory(const std::filesystem::path& path) noexcept {
    std::error_code ec;
    return !Invalid(path) && std::filesystem::is_directory(path, ec) && !ec;
}
FileError FileSystem::CreateDirectories(const std::filesystem::path& path) noexcept {
    if (Invalid(path)) return FileError::InvalidPath;
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    return MapError(ec);
}
FileError FileSystem::Remove(const std::filesystem::path& path) noexcept {
    if (Invalid(path)) return FileError::InvalidPath;
    std::error_code ec;
    if (!std::filesystem::remove(path, ec) && !ec) return FileError::NotFound;
    return MapError(ec);
}
FileError FileSystem::Copy(const std::filesystem::path& source,
                           const std::filesystem::path& destination, bool overwrite) noexcept {
    if (Invalid(source) || Invalid(destination)) return FileError::InvalidPath;
    std::error_code ec;
    const auto mode = overwrite ? std::filesystem::copy_options::overwrite_existing
                                : std::filesystem::copy_options::none;
    if (!std::filesystem::copy_file(source, destination, mode, ec) && !ec)
        return FileError::AlreadyExists;
    return MapError(ec);
}
FileError FileSystem::Move(const std::filesystem::path& source,
                           const std::filesystem::path& destination, bool overwrite) noexcept {
    if (Invalid(source) || Invalid(destination)) return FileError::InvalidPath;
    if (!overwrite && Exists(destination)) return FileError::AlreadyExists;
#if defined(_WIN32)
    const DWORD flags = MOVEFILE_COPY_ALLOWED | (overwrite ? MOVEFILE_REPLACE_EXISTING : 0);
    if (!MoveFileExW(source.c_str(), destination.c_str(), flags)) return FileError::IoError;
    return FileError::None;
#else
    std::error_code ec;
    std::filesystem::rename(source, destination, ec);
    return MapError(ec);
#endif
}
std::optional<std::filesystem::file_time_type> FileSystem::LastWriteTime(
    const std::filesystem::path& path) noexcept {
    if (Invalid(path)) return std::nullopt;
    std::error_code ec;
    auto time = std::filesystem::last_write_time(path, ec);
    return ec ? std::nullopt : std::optional{time};
}

FileReadResult FileSystem::ReadAllBytes(const std::filesystem::path& path,
                                        std::uint64_t maxBytes) noexcept {
    FileReadResult result;
    if (Invalid(path)) { result.error = FileError::InvalidPath; return result; }
    std::error_code ec;
    auto size = std::filesystem::file_size(path, ec);
    if (ec) { result.error = MapError(ec); return result; }
    if (size > maxBytes || size > static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max())) {
        result.error = FileError::TooLarge;
        return result;
    }
    try {
        std::ifstream stream(path, std::ios::binary);
        if (!stream) { result.error = FileError::IoError; return result; }
        result.data.resize(static_cast<std::size_t>(size));
        if (size && !stream.read(reinterpret_cast<char*>(result.data.data()),
                                 static_cast<std::streamsize>(size))) {
            result.data.clear();
            result.error = FileError::IoError;
        }
    } catch (...) { result.data.clear(); result.error = FileError::IoError; }
    return result;
}
FileError FileSystem::WriteAllBytes(const std::filesystem::path& path,
                                    std::span<const std::byte> bytes) noexcept {
    if (Invalid(path) || bytes.size() > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()))
        return FileError::InvalidPath;
    try {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream) return FileError::IoError;
        if (!bytes.empty()) stream.write(reinterpret_cast<const char*>(bytes.data()),
                                         static_cast<std::streamsize>(bytes.size()));
        stream.flush();
        return stream ? FileError::None : FileError::IoError;
    } catch (...) { return FileError::IoError; }
}
FileError FileSystem::ReadAllText(const std::filesystem::path& path, std::string& text,
                                  std::uint64_t maxBytes) noexcept {
    auto result = ReadAllBytes(path, maxBytes);
    if (!result.Ok()) return result.error;
    if (result.data.empty()) { text.clear(); return FileError::None; }
    try { text.assign(reinterpret_cast<const char*>(result.data.data()), result.data.size()); }
    catch (...) { return FileError::IoError; }
    return FileError::None;
}
FileError FileSystem::WriteAllText(const std::filesystem::path& path,
                                   std::string_view text) noexcept {
    return WriteAllBytes(path, {reinterpret_cast<const std::byte*>(text.data()), text.size()});
}
std::filesystem::path FileSystem::GetPersistentDataPath(const std::string& organization,
                                                        const std::string& application) noexcept {
    if (organization.empty() || application.empty()) return {};
    char* path = SDL_GetPrefPath(organization.c_str(), application.c_str());
    if (!path) return {};
    try {
        std::filesystem::path result(std::u8string(reinterpret_cast<const char8_t*>(path)));
        SDL_free(path);
        return result;
    } catch (...) { SDL_free(path); return {}; }
}
