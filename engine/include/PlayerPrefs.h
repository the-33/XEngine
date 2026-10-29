#pragma once
/** @file PlayerPrefs.h
 * @brief Per-user integer, float and string preferences on Windows/Linux.
 */

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

/** Unity-style small per-user settings (not save-game storage).
 *  Initialize once using the developer's own organization and application names.
 *  Windows: HKCU registry; Linux: an atomic JSON file under the per-user config folder.
 */
/** @brief Stores small per-user settings in HKCU (Windows) or JSON (Linux).
 * @note Call Initialize with a stable organization/application identity first.
 * Windows writes keys immediately; Linux buffers modifications in memory until
 * Save() publishes the JSON file. Do not use this for large save-game data.
 * @ingroup xengine_storage
 */
class PlayerPrefs final {
public:
    /** @brief Selects application identity and the user's Registry/config storage location.
     * @param organization Organization name used to isolate persistent application storage.
     * @param application Application name used to isolate persistent storage.
     */
    static bool Initialize(std::string_view organization, std::string_view application) noexcept;
    /** @brief Associates a signed 32-bit integer with an application-specific key.
     * @param key Application-defined lookup key.
     * @param value Value to store, convert, draw or compare.
     */
    static bool SetInt(std::string_view key, std::int32_t value) noexcept;
    /** @brief Associates a 32-bit floating-point value with an application-specific key.
     * @param key Application-defined lookup key.
     * @param value Value to store, convert, draw or compare.
     */
    static bool SetFloat(std::string_view key, float value) noexcept;
    /** @brief Associates a UTF-8 string with an application-specific key.
     * @param key Application-defined lookup key.
     * @param value Value to store, convert, draw or compare.
     */
    static bool SetString(std::string_view key, std::string_view value) noexcept;
    /** @brief Gets an integer preference or the fallback if absent or not an integer.
     * @param key Application-defined lookup key.
     * @param fallback Value returned when the requested key is absent or has another type.
     */
    static std::int32_t GetInt(std::string_view key, std::int32_t fallback = 0) noexcept;
    /** @brief Gets a float preference or the fallback if absent or not a float.
     * @param key Application-defined lookup key.
     * @param fallback Value returned when the requested key is absent or has another type.
     */
    static float GetFloat(std::string_view key, float fallback = 0.f) noexcept;
    /** @brief Gets a string preference or the fallback if absent or not a string.
     * @param key Application-defined lookup key.
     * @param fallback Value returned when the requested key is absent or has another type.
     */
    static std::string GetString(std::string_view key, std::string_view fallback = {}) noexcept;
    /** @brief Reports whether key is present.
     * @param key Application-defined lookup key.
     */
    static bool HasKey(std::string_view key) noexcept;
    /** @brief Deletes one preference key; does not delete other settings.
     * @param key Application-defined lookup key.
     */
    static bool DeleteKey(std::string_view key) noexcept;
    /** @brief Deletes this application's preferences without removing unrelated Unity data.
     */
    static bool DeleteAll() noexcept;
    /** @brief Flushes HKCU Registry changes or atomically publishes Linux JSON settings.
     * @note Use `Save()` at controlled points, such as when applying menu settings.
     */
    static bool Save() noexcept;
    /** @brief Returns the Linux JSON settings filename; empty on Windows (Registry-backed).
     */
    static std::filesystem::path GetStoragePath() noexcept;
};
