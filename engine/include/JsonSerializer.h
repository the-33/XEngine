#pragma once
/** @file JsonSerializer.h
 * @brief JSON conversion facade and explicit file-write mode.
 */

#include "AtomicFile.h"
#include "Serialization.h"

#include <filesystem>
#include <string>
#include <string_view>

/** @brief Choose a direct truncating write or an atomic publish with optional backup.
 */
enum class JsonWriteMode {
    Direct, ///< Truncate/write using FileSystem; NOT crash-atomic.
    Atomic  ///< Publish through AtomicFile; caller owns backup/recovery decisions.
};

/** Thin JSON facade: nlohmann/json handles parsing; persistence is an explicit choice. */
/** @brief Converts values through nlohmann/json and optionally saves through FileSystem or AtomicFile.
 * @ingroup xengine_serialization
 */
class JsonSerializer final {
public:
    /** @brief Converts a supported value or registered class into JSON text without touching the filesystem.
     * @param value Object to convert; custom types need serialization support.
     * @param indent Pretty-printing indentation width.
     */
    template<class T> static std::string Serialize(const T& value, int indent = 2) {
        return xengine::detail::serialization::ToJson(value).dump(indent);
    }
    /** @brief Parses bounded JSON text and recursively fills registered fields; throws on malformed input.
     * @param input UTF-8 JSON text (not a filesystem path).
     * @param maxBytes Maximum number of bytes the caller permits reading.
     */
    template<class T> static T Deserialize(std::string_view input,
                                            std::size_t maxBytes = 64u * 1024u * 1024u) {
        if (input.size() > maxBytes) throw std::length_error("JSON input exceeds limit");
        const auto json = nlohmann::json::parse(input.begin(), input.end());
        T result{};
        xengine::detail::serialization::FromJson(json, result);
        return result;
    }
    /** @brief Attempts to parse JSON into a value, reporting failure without propagating parse errors.
     * @param input UTF-8 JSON text (not a filesystem path).
     * @param result Replaced only if parsing and conversion succeed.
     * @param maxBytes Maximum number of bytes the caller permits reading.
     */
    template<class T> static bool TryDeserialize(std::string_view input, T& result,
                                                  std::size_t maxBytes = 64u * 1024u * 1024u) noexcept {
        try {
            auto copy = Deserialize<T>(input, maxBytes);
            result = std::move(copy);
            return true;
        } catch (...) { return false; }
    }
    /** @brief Serializes to JSON then chooses direct or atomic file publication according to JsonWriteMode.
     * @param path Target filesystem path; file extension is application-defined.
     * @param value Object to serialize and write to disk.
     * @param mode Direct overwrites normally; Atomic publishes via a temporary
     *             file and preserves a backup according to AtomicFile defaults.
     * @param indent Pretty-printing indentation width.
     */
    template<class T> static FileError SaveToFile(const std::filesystem::path& path,
                                                  const T& value,
                                                  JsonWriteMode mode = JsonWriteMode::Atomic,
                                                  int indent = 2) noexcept {
        try {
            const std::string serialized = Serialize(value, indent);
            if (mode == JsonWriteMode::Direct) return FileSystem::WriteAllText(path, serialized);
            return AtomicFile::Write(path, {reinterpret_cast<const std::byte*>(serialized.data()),
                                            serialized.size()});
        } catch (...) { return FileError::IoError; }
    }
};
