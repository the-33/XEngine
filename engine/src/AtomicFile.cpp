#include "AtomicFile.h"

#include <algorithm>
#include <cerrno>
#include <filesystem>
#include <system_error>

#if defined(_WIN32)
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#else
# include <fcntl.h>
# include <unistd.h>
#endif

namespace {
FileError ErrorFromCode(const std::error_code& ec) noexcept {
    if (!ec) return FileError::None;
    if (ec == std::errc::no_such_file_or_directory) return FileError::NotFound;
    if (ec == std::errc::permission_denied) return FileError::PermissionDenied;
    if (ec == std::errc::file_exists) return FileError::AlreadyExists;
    return FileError::IoError;
}

std::filesystem::path DirectoryOf(const std::filesystem::path& path) {
    return path.parent_path().empty() ? std::filesystem::path(".") : path.parent_path();
}

FileError SyncDirectory(const std::filesystem::path& path) noexcept {
#if defined(_WIN32)
    // Windows does not expose portable directory fsync; file data is synced via FlushFileBuffers.
    (void)path;
    return FileError::None;
#else
    int fd = ::open(DirectoryOf(path).c_str(), O_RDONLY | O_DIRECTORY);
    if (fd < 0) return FileError::IoError;
    const bool ok = ::fsync(fd) == 0;
    ::close(fd);
    return ok ? FileError::None : FileError::IoError;
#endif
}

FileError SyncExistingFile(const std::filesystem::path& path) noexcept {
#if defined(_WIN32)
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return FileError::IoError;
    const bool ok = FlushFileBuffers(file) != 0;
    CloseHandle(file);
    return ok ? FileError::None : FileError::IoError;
#else
    int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) return FileError::IoError;
    const bool ok = ::fsync(fd) == 0;
    ::close(fd);
    return ok ? FileError::None : FileError::IoError;
#endif
}

FileError WriteNewAndSync(const std::filesystem::path& path,
                          std::span<const std::byte> data) noexcept {
#if defined(_WIN32)
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return GetLastError() == ERROR_FILE_EXISTS ? FileError::RecoveryRequired : FileError::IoError;
    bool ok = true;
    std::size_t offset = 0;
    while (offset < data.size()) {
        const DWORD count = static_cast<DWORD>(std::min<std::size_t>(data.size() - offset, 1u << 20));
        DWORD written = 0;
        if (!WriteFile(file, data.data() + offset, count, &written, nullptr) || !written) { ok = false; break; }
        offset += written;
    }
    if (ok) ok = FlushFileBuffers(file) != 0;
    if (!CloseHandle(file)) ok = false;
    return ok ? FileError::None : FileError::IoError;
#else
    int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) return errno == EEXIST ? FileError::RecoveryRequired : FileError::IoError;
    bool ok = true;
    std::size_t offset = 0;
    while (offset < data.size()) {
        ssize_t wrote = ::write(fd, data.data() + offset,
                                std::min<std::size_t>(data.size() - offset, 1u << 20));
        if (wrote < 0 && errno == EINTR) continue;
        if (wrote <= 0) { ok = false; break; }
        offset += static_cast<std::size_t>(wrote);
    }
    if (ok) ok = ::fsync(fd) == 0;
    if (::close(fd) != 0) ok = false;
    return ok ? FileError::None : FileError::IoError;
#endif
}

FileError PublishNew(const std::filesystem::path& temp,
                     const std::filesystem::path& target, bool hasPrimary,
                     const std::filesystem::path& backup, bool createBackup) noexcept {
#if defined(_WIN32)
    if (hasPrimary) {
        const wchar_t* backupName = createBackup ? backup.c_str() : nullptr;
        if (!ReplaceFileW(target.c_str(), temp.c_str(), backupName, 0, nullptr, nullptr))
            return FileError::IoError;
    } else if (!MoveFileExW(temp.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH)) {
        return FileError::IoError;
    }
#else
    (void)hasPrimary; (void)backup; (void)createBackup;
    std::error_code ec;
    std::filesystem::rename(temp, target, ec);
    if (ec) return ErrorFromCode(ec);
#endif
    return SyncDirectory(target);
}
}

std::filesystem::path AtomicFile::TemporaryPath(const std::filesystem::path& target) {
    auto path = target;
    path += ".tmp";
    return path;
}
std::filesystem::path AtomicFile::BackupPath(const std::filesystem::path& target) {
    auto path = target;
    path += ".bak";
    return path;
}

AtomicFileInspection AtomicFile::Inspect(const std::filesystem::path& target) noexcept {
    AtomicFileInspection result;
    try {
        result.primaryPath = target;
        result.backupPath = BackupPath(target);
        result.temporaryPath = TemporaryPath(target);
        auto examine = [&](const std::filesystem::path& path) -> bool {
            std::error_code ec;
            const auto status = std::filesystem::symlink_status(path, ec);
            if (ec && ec != std::errc::no_such_file_or_directory) result.error = ErrorFromCode(ec);
            return !ec && std::filesystem::exists(status);
        };
        result.primary = examine(result.primaryPath);
        result.backup = examine(result.backupPath);
        result.temporary = examine(result.temporaryPath);
        const unsigned mask = (result.primary ? 1u : 0u) |
                              (result.backup ? 2u : 0u) |
                              (result.temporary ? 4u : 0u);
        // Enum ordering is deliberately the presence mask: 0..7.
        result.situation = result.error == FileError::None ?
            static_cast<AtomicReadSituation>(mask) : AtomicReadSituation::Inaccessible;
    } catch (...) {
        result.error = FileError::IoError;
        result.situation = AtomicReadSituation::Inaccessible;
    }
    return result;
}

AtomicReadResult AtomicFile::Read(const std::filesystem::path& target,
                                  AtomicFileSource source, std::uint64_t maxBytes) noexcept {
    AtomicReadResult result;
    try {
        auto files = Inspect(target);
        result.situation = files.situation;
        result.source = source;
        const auto& path = source == AtomicFileSource::Backup ? files.backupPath :
                           source == AtomicFileSource::Temporary ? files.temporaryPath : files.primaryPath;
        auto bytes = FileSystem::ReadAllBytes(path, maxBytes);
        result.error = bytes.error;
        result.data = std::move(bytes.data);
    } catch (...) { result.error = FileError::IoError; }
    return result;
}

FileError AtomicFile::Write(const std::filesystem::path& target,
                            std::span<const std::byte> data, bool keepBackup) noexcept {
    if (target.empty() || target.filename().empty()) return FileError::InvalidPath;
    try {
        const auto temp = TemporaryPath(target);
        const auto backup = BackupPath(target);
        if (FileSystem::Exists(temp)) return FileError::RecoveryRequired;
        const auto directory = DirectoryOf(target);
        FileError result = FileSystem::CreateDirectories(directory);
        if (result != FileError::None) return result;
        result = WriteNewAndSync(temp, data);
        if (result != FileError::None) return result;
        result = SyncDirectory(target);
        if (result != FileError::None) return result;

        const bool hasPrimary = FileSystem::Exists(target);
        const bool needsBackup = keepBackup && hasPrimary && !FileSystem::Exists(backup);
#if !defined(_WIN32)
        if (needsBackup) {
            // Try an instantaneous hardlink first; otherwise stage and sync a full copy.
            std::error_code ec;
            std::filesystem::create_hard_link(target, backup, ec);
            if (ec) {
                auto stagedBackup = backup;
                stagedBackup += ".tmp";
                if (FileSystem::Exists(stagedBackup)) return FileError::RecoveryRequired;
                result = FileSystem::Copy(target, stagedBackup);
                if (result != FileError::None) return result;
                result = SyncExistingFile(stagedBackup);
                if (result != FileError::None) return result;
                std::filesystem::rename(stagedBackup, backup, ec);
                if (ec) return ErrorFromCode(ec);
            }
            result = SyncDirectory(target);
            if (result != FileError::None) return result;
        }
#endif
        return PublishNew(temp, target, hasPrimary, backup, needsBackup);
    } catch (...) { return FileError::IoError; }
}
