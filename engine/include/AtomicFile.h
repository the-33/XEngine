#pragma once
/** @file AtomicFile.h
 * @brief Single-file atomic publishing and explicit backup/recovery states.
 */

#include "FileSystem.h"

/** Where to read from. The engine never chooses a candidate based on its contents. */
enum class AtomicFileSource {
    Primary,   ///< Confirmed filename; validate the payload at game level.
    Backup,    ///< Previous confirmed bytes; validate before offering restoration.
    Temporary  ///< Interrupted write candidate; it may be incomplete or newer.
};

/** Presence-only status. No state implies that any bytes are semantically valid. */
enum class AtomicReadSituation {
    NoFiles,             ///< No candidate exists; the game may offer a fresh start.
    PrimaryOnly,         ///< Check the primary's format, version and integrity.
    BackupOnly,          ///< A prior save may be restorable after game-level validation.
    PrimaryAndBackup,    ///< Prefer a valid primary; offer backup if primary is invalid.
    TemporaryOnly,       ///< A write may have been interrupted; validate before considering it.
    PrimaryAndTemporary, ///< A temp remains; do not silently promote it over a valid primary.
    BackupAndTemporary,  ///< Inspect both independently; let the developer select a candidate.
    AllPresent,          ///< Inspect and validate all three; never auto-delete a candidate.
    Inaccessible         ///< Presence check failed (e.g. permissions); do not assume files absent.
};

/** @brief Reports which primary, backup and temporary candidates exist; never validates payloads.
 */
struct AtomicFileInspection {
    /** @brief Combination of candidates detected at the requested save path; presence is not validation.
     */
    AtomicReadSituation situation = AtomicReadSituation::NoFiles;
    /** @brief True if the committed target file exists.
     */
    bool primary = false;
    /** @brief True if the target plus `.bak` exists.
     */
    bool backup = false;
    /** @brief True if the target plus `.tmp` exists.
     */
    bool temporary = false;
    /** @brief Filesystem error encountered while inspecting candidates, if any.
     */
    FileError error = FileError::None;
    /** @brief The exact committed filename requested by the caller.
     */
    std::filesystem::path primaryPath;
    /** @brief Derived `.bak` filename; not necessarily a valid game save.
     */
    std::filesystem::path backupPath;
    /** @brief Derived `.tmp` filename; may contain partial or recoverable bytes.
     */
    std::filesystem::path temporaryPath;
};

/** @brief Selected candidate bytes with file status and overall recovery situation.
 */
struct AtomicReadResult {
    /** @brief Presence-only status of the candidates at read time.
     */
    AtomicReadSituation situation = AtomicReadSituation::NoFiles;
    /** @brief Candidate explicitly selected by the caller.
     */
    AtomicFileSource source = AtomicFileSource::Primary;
    /** @brief Status of the selected file read; not a game-data validation result.
     */
    FileError error = FileError::None;
    /** @brief Opaque bytes of the selected candidate, subject to the configured size limit.
     */
    std::vector<std::byte> data;
    /** @brief Returns true if the selected file could be read without a filesystem error.
     */
    bool Ok() const noexcept { return error == FileError::None; }
};

/** Atomic single-file publish with optional preserved previous version.
 *  File extension, internal framing, integrity and recovery decisions belong to the game.
 *  This class neither parses nor validates payloads, and it does not coordinate multi-file transactions.
 */
/** @brief Stages and publishes one opaque file with optional backup and explicit recovery inspection.
 * @ingroup xengine_storage
 */
class AtomicFile final {
public:
    /** Appended suffixes; e.g. score.dat -> score.dat.tmp / score.dat.bak. */
    static std::filesystem::path TemporaryPath(const std::filesystem::path& target);
    /** @brief Returns the target filename with a .bak suffix appended, regardless of its existing extension.
     * @param target Target filesystem path; file extension is application-defined.
     */
    static std::filesystem::path BackupPath(const std::filesystem::path& target);

    /** @brief Reports candidate presence and inspection errors without deciding which save is valid.
     * @param target Target filesystem path; file extension is application-defined.
     */
    static AtomicFileInspection Inspect(const std::filesystem::path& target) noexcept;
    /** @brief Reads exactly the selected primary/backup/temporary candidate with a maximum byte count.
     * @param target Target filesystem path; file extension is application-defined.
     * @param source Candidate or source location selected by the caller.
     * @param maxBytes Maximum number of bytes the caller permits reading.
     */
    static AtomicReadResult Read(const std::filesystem::path& target,
                                 AtomicFileSource source = AtomicFileSource::Primary,
                                 std::uint64_t maxBytes = FileSystem::DefaultMaxReadBytes) noexcept;

    /**
     * Write and durable-sync .tmp before publishing it. If a primary exists, keep its first
     * .bak; an existing .bak is NEVER overwritten. Returns RecoveryRequired if .tmp exists.
     * A failed operation may leave candidates; inspect and validate before the next write.
     * Only a single cooperative writer per path is supported.
     */
    static FileError Write(const std::filesystem::path& target, std::span<const std::byte> data,
                           bool keepBackup = true) noexcept;
};
