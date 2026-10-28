#pragma once
/** @file FileSystem.h
 * @brief Direct filesystem operations and persistent user data paths.
 */

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/** Errors returned by file operations. File content is never interpreted here. */
enum class FileError {
    None,             ///< Operation completed successfully.
    NotFound,         ///< Requested path does not exist.
    PermissionDenied, ///< The operating system denied access.
    TooLarge,         ///< Read exceeds the caller's maximum permitted size.
    InvalidPath,      ///< Empty or otherwise invalid path.
    AlreadyExists,    ///< A destination already exists and overwrite was disabled.
    RecoveryRequired, ///< An existing temporary save must be inspected before a new atomic write.
    IoError           ///< Other filesystem or device failure.
};

/** @brief File error status and opaque byte content returned by a bounded read.
 */
struct FileReadResult {
    /** @brief Filesystem status of the read operation; `None` means the I/O succeeded.
     */
    FileError error = FileError::None;
    /** @brief Raw, uninterpreted contents returned by a successful read.
     */
    std::vector<std::byte> data;
    /** @brief Reports I/O success, not application-format validity.
     */
    bool Ok() const noexcept { return error == FileError::None; }
};

/** Low-level public filesystem API. These writes are NOT atomic; use AtomicFile if needed. */
/** @brief Raw file and directory API; direct writes are not crash-atomic.
 * @ingroup xengine_storage
 */
class FileSystem final {
public:
    /** @brief Default maximum raw read size (64 MiB), overridable per read call.
     */
    static constexpr std::uint64_t DefaultMaxReadBytes = 64ull * 1024 * 1024;

    /** @brief Tests whether the filesystem path exists, regardless of its type.
     * @param path Target filesystem path; file extension is application-defined.
     */
    static bool Exists(const std::filesystem::path& path) noexcept;
    /** @brief Reports whether the path points to an ordinary file.
     * @param path Target filesystem path; file extension is application-defined.
     */
    static bool IsRegularFile(const std::filesystem::path& path) noexcept;
    /** @brief Reports whether the path points to a directory.
     * @param path Target filesystem path; file extension is application-defined.
     */
    static bool IsDirectory(const std::filesystem::path& path) noexcept;
    /** @brief Recursively creates any missing directories at the requested path.
     * @param path Target filesystem path; file extension is application-defined.
     */
    static FileError CreateDirectories(const std::filesystem::path& path) noexcept;
    /** @brief Removes one file or empty directory; does not recursively delete trees.
     * @param path Target filesystem path; file extension is application-defined.
     */
    static FileError Remove(const std::filesystem::path& path) noexcept;
    /** @brief Copies a file; destination replacement requires explicit opt-in.
     * @param source Candidate or source location selected by the caller.
     * @param destination Destination path chosen by the caller.
     * @param overwrite Whether an existing destination is permitted to be replaced.
     */
    static FileError Copy(const std::filesystem::path& source, const std::filesystem::path& destination,
                          bool overwrite = false) noexcept;
    /** @brief Moves a filesystem entry to a new name or location.
     * @param source Candidate or source location selected by the caller.
     * @param destination Destination path chosen by the caller.
     * @param overwrite Whether an existing destination is permitted to be replaced.
     */
    static FileError Move(const std::filesystem::path& source, const std::filesystem::path& destination,
                          bool overwrite = false) noexcept;
    /** @brief Renames or moves an entry using the same semantics as Move().
     * @param source Candidate or source location selected by the caller.
     * @param destination Destination path chosen by the caller.
     * @param overwrite Whether an existing destination is permitted to be replaced.
     */
    static FileError Rename(const std::filesystem::path& source, const std::filesystem::path& destination,
                            bool overwrite = false) noexcept { return Move(source, destination, overwrite); }
    /** @brief Returns the filesystem modification timestamp, or `nullopt` on failure.
     * @param path Target filesystem path; file extension is application-defined.
     */
    static std::optional<std::filesystem::file_time_type> LastWriteTime(
        const std::filesystem::path& path) noexcept;

    /** @brief Reads an entire binary file with an explicit maximum byte count; never interprets the bytes.
     * @param path Target filesystem path; file extension is application-defined.
     * @param maxBytes Maximum number of bytes the caller permits reading.
     */
    static FileReadResult ReadAllBytes(const std::filesystem::path& path,
                                      std::uint64_t maxBytes = DefaultMaxReadBytes) noexcept;
    /** @brief Writes raw bytes directly, truncating any existing file; this operation is not atomic.
     * @param path Target filesystem path; file extension is application-defined.
     * @param bytes Uninterpreted binary byte sequence.
     */
    static FileError WriteAllBytes(const std::filesystem::path& path,
                                   std::span<const std::byte> bytes) noexcept;
    /** @brief Reads file contents as text with a caller-configurable byte-size limit.
     * @param path Target filesystem path; file extension is application-defined.
     * @param text Output string that receives the complete file contents on success.
     * @param maxBytes Maximum number of bytes the caller permits reading.
     */
    static FileError ReadAllText(const std::filesystem::path& path, std::string& text,
                                 std::uint64_t maxBytes = DefaultMaxReadBytes) noexcept;
    /** @brief Writes the supplied bytes of a text string directly; it does not perform an atomic publish.
     * @param path Target filesystem path; file extension is application-defined.
     * @param text Text to render or serialize.
     */
    static FileError WriteAllText(const std::filesystem::path& path,
                                  std::string_view text) noexcept;

    /** @brief Returns the SDL_GetPrefPath user-writable directory for the app identity.
     * @param organization Stable company or developer identifier.
     * @param application Stable game/application identifier.
     * @return Directory for persistent game data; not inside `assets/` or the current working directory.
     */
    static std::filesystem::path GetPersistentDataPath(const std::string& organization,
                                                       const std::string& application) noexcept;
};
