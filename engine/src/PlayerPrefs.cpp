#include "PlayerPrefs.h"

#include "AtomicFile.h"
#include "FileSystem.h"

#include <nlohmann/json.hpp>

#include <bit>
#include <climits>
#include <cstring>
#include <cstdlib>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#if defined(_WIN32)
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#endif

namespace {
using Preference = std::variant<std::int32_t, float, std::string>;
struct State {
    std::mutex mutex;
    bool initialized = false;
    std::filesystem::path path;
#if defined(_WIN32)
    std::wstring registryPath;
#else
    std::unordered_map<std::string, Preference> values;
#endif
};
State& Storage() { static State state; return state; }

bool ValidPart(std::string_view part) {
    return !part.empty() && part.size() < 160 && part != "." && part != ".." &&
           part.find('/') == std::string_view::npos && part.find('\\') == std::string_view::npos &&
           part.find('\0') == std::string_view::npos;
}
bool ValidKey(std::string_view key) {
    return !key.empty() && key.size() < 512 && key.find('\0') == std::string_view::npos;
}

#if defined(_WIN32)
std::wstring ToWide(std::string_view utf8) {
    if (utf8.empty() || utf8.size() > static_cast<std::size_t>(INT_MAX)) return {};
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(),
                                    static_cast<int>(utf8.size()), nullptr, 0);
    if (!count) return {};
    std::wstring result(count, L'\0');
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(),
                             static_cast<int>(utf8.size()), result.data(), count)) return {};
    return result;
}
std::string ToUtf8(const std::wstring& wide) {
    if (wide.empty()) return {};
    int count = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                                    nullptr, 0, nullptr, nullptr);
    if (!count) return {};
    std::string result(count, '\0');
    if (!WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                             result.data(), count, nullptr, nullptr)) return {};
    return result;
}
struct RegKey {
    HKEY handle = nullptr;
    ~RegKey() { if (handle) RegCloseKey(handle); }
    bool Open(const State& state, bool create) {
        const LONG status = create ?
            RegCreateKeyExW(HKEY_CURRENT_USER, state.registryPath.c_str(), 0, nullptr, 0,
                             KEY_READ | KEY_WRITE, nullptr, &handle, nullptr) :
            RegOpenKeyExW(HKEY_CURRENT_USER, state.registryPath.c_str(), 0,
                          KEY_READ | KEY_WRITE, &handle);
        return status == ERROR_SUCCESS;
    }
};
bool WriteRegistry(State& state, std::string_view key, DWORD type,
                   const void* bytes, DWORD byteCount) {
    if (!state.initialized || !ValidKey(key)) return false;
    const auto name = ToWide(key);
    if (name.empty()) return false;
    RegKey handle;
    return handle.Open(state, true) &&
           RegSetValueExW(handle.handle, name.c_str(), 0, type,
                          reinterpret_cast<const BYTE*>(bytes), byteCount) == ERROR_SUCCESS;
}
bool ReadRegistry(State& state, std::string_view key, DWORD type,
                  std::vector<std::byte>& result) {
    if (!state.initialized || !ValidKey(key)) return false;
    const auto name = ToWide(key);
    if (name.empty()) return false;
    RegKey handle;
    if (!handle.Open(state, false)) return false;
    DWORD length = 0, foundType = 0;
    if (RegQueryValueExW(handle.handle, name.c_str(), nullptr, &foundType, nullptr, &length) != ERROR_SUCCESS ||
        foundType != type || length > 1024 * 1024) return false;
    result.resize(length);
    return RegQueryValueExW(handle.handle, name.c_str(), nullptr, &foundType,
                            reinterpret_cast<BYTE*>(result.data()), &length) == ERROR_SUCCESS &&
           foundType == type;
}
#else
std::filesystem::path LinuxConfig(std::string_view company, std::string_view product) {
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    const char* home = std::getenv("HOME");
    std::filesystem::path root;
    if (xdg && *xdg && std::filesystem::path(xdg).is_absolute()) root = xdg;
    else if (home && *home) root = std::filesystem::path(home) / ".config";
    else return {};
    return root / "unity3d" / std::string(company) / std::string(product) / "xengine-playerprefs.json";
}
bool ReadLinux(State& state) {
    state.values.clear();
    if (!FileSystem::Exists(state.path)) return true;
    std::string text;
    if (FileSystem::ReadAllText(state.path, text, 4 * 1024 * 1024) != FileError::None) return false;
    try {
        auto json = nlohmann::json::parse(text);
        if (!json.is_object()) return false;
        for (auto it = json.begin(); it != json.end(); ++it) {
            const auto& item = it.value();
            const auto type = item.at("type").get<std::string>();
            if (type == "int") state.values[it.key()] = item.at("value").get<std::int32_t>();
            else if (type == "float") state.values[it.key()] = item.at("value").get<float>();
            else if (type == "string") state.values[it.key()] = item.at("value").get<std::string>();
            else return false;
        }
    } catch (...) { state.values.clear(); return false; }
    return true;
}
#endif
}

bool PlayerPrefs::Initialize(std::string_view organization, std::string_view application) noexcept {
    if (!ValidPart(organization) || !ValidPart(application)) return false;
    try {
        State& state = Storage();
        std::scoped_lock lock(state.mutex);
        state.initialized = false;
#if defined(_WIN32)
        const auto company = ToWide(organization), product = ToWide(application);
        if (company.empty() || product.empty()) return false;
        state.registryPath = L"Software\\" + company + L"\\" + product + L"\\XEngine";
        RegKey key;
        state.initialized = key.Open(state, true);
#else
        state.path = LinuxConfig(organization, application);
        if (state.path.empty() || !ReadLinux(state)) return false;
        state.initialized = true;
#endif
        return state.initialized;
    } catch (...) { return false; }
}
bool PlayerPrefs::SetInt(std::string_view key, std::int32_t value) noexcept {
    if (!ValidKey(key)) return false;
    try {
        State& state = Storage(); std::scoped_lock lock(state.mutex);
#if defined(_WIN32)
        const DWORD data = std::bit_cast<std::uint32_t>(value);
        return WriteRegistry(state, key, REG_DWORD, &data, sizeof(data));
#else
        if (!state.initialized) return false;
        state.values[std::string(key)] = value; return true;
#endif
    } catch (...) { return false; }
}
bool PlayerPrefs::SetFloat(std::string_view key, float value) noexcept {
    if (!ValidKey(key)) return false;
    try {
        State& state = Storage(); std::scoped_lock lock(state.mutex);
#if defined(_WIN32)
        const auto bits = std::bit_cast<std::uint32_t>(value);
        return WriteRegistry(state, key, REG_BINARY, &bits, sizeof(bits));
#else
        if (!state.initialized) return false;
        state.values[std::string(key)] = value; return true;
#endif
    } catch (...) { return false; }
}
bool PlayerPrefs::SetString(std::string_view key, std::string_view value) noexcept {
    if (!ValidKey(key) || value.size() > 1024 * 1024) return false;
    try {
        State& state = Storage(); std::scoped_lock lock(state.mutex);
#if defined(_WIN32)
        auto wide = value.empty() ? std::wstring{} : ToWide(value);
        if (!value.empty() && wide.empty()) return false;
        wide.push_back(L'\0');
        return WriteRegistry(state, key, REG_SZ, wide.c_str(),
                             static_cast<DWORD>(wide.size() * sizeof(wchar_t)));
#else
        if (!state.initialized) return false;
        state.values[std::string(key)] = std::string(value); return true;
#endif
    } catch (...) { return false; }
}
std::int32_t PlayerPrefs::GetInt(std::string_view key, std::int32_t fallback) noexcept {
    try {
        State& state = Storage(); std::scoped_lock lock(state.mutex);
#if defined(_WIN32)
        std::vector<std::byte> bytes;
        if (!ReadRegistry(state, key, REG_DWORD, bytes) || bytes.size() != 4) return fallback;
        DWORD raw = 0; std::memcpy(&raw, bytes.data(), 4);
        return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(raw));
#else
        const auto it = state.values.find(std::string(key));
        if (!state.initialized || it == state.values.end()) return fallback;
        if (auto v = std::get_if<std::int32_t>(&it->second)) return *v;
        return fallback;
#endif
    } catch (...) { return fallback; }
}
float PlayerPrefs::GetFloat(std::string_view key, float fallback) noexcept {
    try {
        State& state = Storage(); std::scoped_lock lock(state.mutex);
#if defined(_WIN32)
        std::vector<std::byte> bytes;
        if (!ReadRegistry(state, key, REG_BINARY, bytes) || bytes.size() != 4) return fallback;
        std::uint32_t raw = 0; std::memcpy(&raw, bytes.data(), 4);
        return std::bit_cast<float>(raw);
#else
        const auto it = state.values.find(std::string(key));
        if (!state.initialized || it == state.values.end()) return fallback;
        if (auto v = std::get_if<float>(&it->second)) return *v;
        return fallback;
#endif
    } catch (...) { return fallback; }
}
std::string PlayerPrefs::GetString(std::string_view key, std::string_view fallback) noexcept {
    try {
        State& state = Storage(); std::scoped_lock lock(state.mutex);
#if defined(_WIN32)
        std::vector<std::byte> bytes;
        if (!ReadRegistry(state, key, REG_SZ, bytes) || bytes.size() % sizeof(wchar_t)) return std::string(fallback);
        std::wstring wide(bytes.size()/sizeof(wchar_t), L'\0');
        std::memcpy(wide.data(), bytes.data(), bytes.size());
        if (!wide.empty() && wide.back() == L'\0') wide.pop_back();
        return ToUtf8(wide);
#else
        const auto it = state.values.find(std::string(key));
        if (!state.initialized || it == state.values.end()) return std::string(fallback);
        if (auto v = std::get_if<std::string>(&it->second)) return *v;
        return std::string(fallback);
#endif
    } catch (...) { return std::string(fallback); }
}
bool PlayerPrefs::HasKey(std::string_view key) noexcept {
    if (!ValidKey(key)) return false;
    try {
        State& state = Storage(); std::scoped_lock lock(state.mutex);
#if defined(_WIN32)
        if (!state.initialized) return false;
        RegKey handle; if (!handle.Open(state, false)) return false;
        const auto name = ToWide(key);
        return !name.empty() && RegQueryValueExW(handle.handle, name.c_str(), nullptr,
                                                  nullptr, nullptr, nullptr) == ERROR_SUCCESS;
#else
        return state.initialized && state.values.contains(std::string(key));
#endif
    } catch (...) { return false; }
}
bool PlayerPrefs::DeleteKey(std::string_view key) noexcept {
    if (!ValidKey(key)) return false;
    try {
        State& state = Storage(); std::scoped_lock lock(state.mutex);
#if defined(_WIN32)
        if (!state.initialized) return false;
        RegKey handle; if (!handle.Open(state, false)) return false;
        const auto name = ToWide(key);
        return !name.empty() && RegDeleteValueW(handle.handle, name.c_str()) == ERROR_SUCCESS;
#else
        return state.initialized && state.values.erase(std::string(key)) != 0;
#endif
    } catch (...) { return false; }
}
bool PlayerPrefs::DeleteAll() noexcept {
    try {
        State& state = Storage(); std::scoped_lock lock(state.mutex);
        if (!state.initialized) return false;
#if defined(_WIN32)
        return RegDeleteTreeW(HKEY_CURRENT_USER, state.registryPath.c_str()) == ERROR_SUCCESS;
#else
        state.values.clear(); return true; // Save() persists deletion, like other setters.
#endif
    } catch (...) { return false; }
}
bool PlayerPrefs::Save() noexcept {
    try {
        State& state = Storage(); std::scoped_lock lock(state.mutex);
        if (!state.initialized) return false;
#if defined(_WIN32)
        RegKey key; return key.Open(state, true) && RegFlushKey(key.handle) == ERROR_SUCCESS;
#else
        nlohmann::json json = nlohmann::json::object();
        for (const auto& [key, value] : state.values) {
            if (auto p = std::get_if<std::int32_t>(&value)) json[key] = {{"type", "int"}, {"value", *p}};
            else if (auto p = std::get_if<float>(&value)) json[key] = {{"type", "float"}, {"value", *p}};
            else json[key] = {{"type", "string"}, {"value", std::get<std::string>(value)}};
        }
        const auto text = json.dump(2);
        return AtomicFile::Write(state.path, {reinterpret_cast<const std::byte*>(text.data()),
                                               text.size()}) == FileError::None;
#endif
    } catch (...) { return false; }
}
std::filesystem::path PlayerPrefs::GetStoragePath() noexcept {
    try {
        State& state = Storage(); std::scoped_lock lock(state.mutex);
        return state.initialized ? state.path : std::filesystem::path{};
    } catch (...) { return {}; }
}
