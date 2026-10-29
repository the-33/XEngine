#pragma once
/** @file Serialization.h
 * @brief Recursive JSON/binary archives and opt-in serializable fields.
 */

#include "BinaryIO.h"
#include "BaseTypes.h"

#include <nlohmann/json.hpp>

#include <array>
#include <concepts>
#include <cstdint>
#include <list>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

/**
 * Register plain named fields inside a class or struct (C++20 has no standard reflection).
 * Fields are recursively serialized, including nested registered classes and containers.
 * Names are used by JSON; binary saves only values in declared order.
 * For nontrivial types implement const and mutable Serialize(Archive&) overloads manually.
 */
#define XENGINE_SERIALIZABLE(...) \
    template<class Archive> void Serialize(Archive& archive) { archive.Fields(#__VA_ARGS__, __VA_ARGS__); } \
    template<class Archive> void Serialize(Archive& archive) const { archive.Fields(#__VA_ARGS__, __VA_ARGS__); }

class JsonWriteArchive;
class JsonReadArchive;
class BinaryWriteArchive;
class BinaryReadArchive;

namespace xengine::detail::serialization {
/** @brief Stores the k false setting or data field of XSerializationDetail.
 */
template<class T> inline constexpr bool kFalse = false;
template<class T> struct Vector : std::false_type {};
template<class T, class A> struct Vector<std::vector<T,A>> : std::true_type { using Value = T; };
template<class T> struct List : std::false_type {};
template<class T, class A> struct List<std::list<T,A>> : std::true_type { using Value = T; };
template<class T> struct Array : std::false_type {};
template<class T, std::size_t N> struct Array<std::array<T,N>> : std::true_type { using Value = T; };
template<class T> struct Map : std::false_type {};
template<class K, class V, class C, class A> struct Map<std::map<K,V,C,A>> : std::true_type {
    using Key = K; using Value = V;
};
template<class K, class V, class H, class E, class A>
struct Map<std::unordered_map<K,V,H,E,A>> : std::true_type { using Key = K; using Value = V; };
template<class T> struct Optional : std::false_type {};
template<class T> struct Optional<std::optional<T>> : std::true_type { using Value = T; };
template<class T> struct Vec2Type : std::false_type {};
template<math::Arithmetic T> struct Vec2Type<TVec2<T>> : std::true_type {};
template<class T> struct Vec3Type : std::false_type {};
template<math::Arithmetic T> struct Vec3Type<TVec3<T>> : std::true_type {};

template<class T, class A>
concept RegisteredConst = requires(const T& value, A& archive) { value.Serialize(archive); };
template<class T, class A>
concept RegisteredMutable = requires(T& value, A& archive) { value.Serialize(archive); };

/** @brief Splits the serialized field-name list used by the archive visitor.
 * @param csv Comma-separated field names.
 */
inline std::vector<std::string> SplitNames(std::string_view csv) {
    std::vector<std::string> names;
    while (!csv.empty()) {
        const auto comma = csv.find(',');
        auto piece = csv.substr(0, comma);
        const auto first = piece.find_first_not_of(" \t\r\n");
        const auto last = piece.find_last_not_of(" \t\r\n");
        if (first == std::string_view::npos) throw std::invalid_argument("Empty serializable field");
        names.emplace_back(piece.substr(first, last - first + 1));
        if (comma == std::string_view::npos) break;
        csv.remove_prefix(comma + 1);
    }
    return names;
}

template<class Archive, class... Values>
/** @brief Visits or converts developer-selected data fields in the active archive.
 * @param archive Serialization archive receiving or providing field values.
 * @param namesCsv Comma-separated serialized field names.
 * @param values User-selected fields or values visited by the archive.
 */
void Fields(Archive& archive, std::string_view namesCsv, Values&&... values) {
    auto names = SplitNames(namesCsv);
    if (names.size() != sizeof...(Values)) throw std::invalid_argument("Serializable field count mismatch");
    std::size_t index = 0;
    (archive.Field(names[index++], std::forward<Values>(values)), ...);
}

template<class T> nlohmann::json ToJson(const T& value, unsigned depth = 0);
template<class T> void FromJson(const nlohmann::json& json, T& value, unsigned depth = 0);
template<class T> void WriteBinary(BinaryWriter& writer, const T& value, unsigned depth = 0);
template<class T> void ReadBinary(BinaryReader& reader, T& value, unsigned depth = 0);
}

/** @brief Visits named fields of a registered object and constructs a JSON object.
 * @ingroup xengine_serialization
 */
class JsonWriteArchive final {
    nlohmann::json object_ = nlohmann::json::object();
    unsigned depth_ = 0;
public:
    /** @brief Constructs a JsonWriteArchive instance.
     * @param depth Current nesting depth used for bounded recursive serialization.
     */
    explicit JsonWriteArchive(unsigned depth = 0) : depth_(depth) {}
    /** @brief Visits or converts developer-selected data fields in the active archive.
     * @param name Name used to identify the resource or entity.
     * @param value Value to store, convert, draw or compare.
     */
    template<class T> void Field(std::string_view name, const T& value) {
        object_[std::string(name)] = xengine::detail::serialization::ToJson(value, depth_ + 1);
    }
    /** @brief Visits or converts developer-selected data fields in the active archive.
     * @param csv Comma-separated field names.
     * @param values User-selected fields or values visited by the archive.
     */
    template<class... Values> void Fields(std::string_view csv, const Values&... values) {
        xengine::detail::serialization::Fields(*this, csv, values...);
    }
    /** @brief Transfers the serialized value out of the archive.
     */
    nlohmann::json Take() && { return std::move(object_); }
};

/** @brief Reads named fields from a JSON object into a registered C++ type.
 * @ingroup xengine_serialization
 */
class JsonReadArchive final {
    const nlohmann::json& object_;
    unsigned depth_ = 0;
public:
    /** @brief Constructs a JsonReadArchive instance.
     * @param value Value to store, convert, draw or compare.
     * @param depth Current nesting depth used for bounded recursive serialization.
     */
    explicit JsonReadArchive(const nlohmann::json& value, unsigned depth = 0)
        : object_(value), depth_(depth) {
        if (!value.is_object()) throw std::invalid_argument("Expected JSON object");
    }
    /** @brief Visits or converts developer-selected data fields in the active archive.
     * @param name Name used to identify the resource or entity.
     * @param value Value to store, convert, draw or compare.
     */
    template<class T> void Field(std::string_view name, T& value) {
        xengine::detail::serialization::FromJson(object_.at(std::string(name)), value, depth_ + 1);
    }
    /** @brief Visits or converts developer-selected data fields in the active archive.
     * @param csv Comma-separated field names.
     * @param values User-selected fields or values visited by the archive.
     */
    template<class... Values> void Fields(std::string_view csv, Values&... values) {
        xengine::detail::serialization::Fields(*this, csv, values...);
    }
};

/** @brief Visits registered fields in their declared order when writing binary data.
 * @ingroup xengine_serialization
 */
class BinaryWriteArchive final {
    BinaryWriter& writer_;
    unsigned depth_ = 0;
public:
    /** @brief Constructs a BinaryWriteArchive instance.
     * @param writer Output binary archive.
     * @param depth Current nesting depth used for bounded recursive serialization.
     */
    explicit BinaryWriteArchive(BinaryWriter& writer, unsigned depth = 0)
        : writer_(writer), depth_(depth) {}
    /** @brief Visits or converts developer-selected data fields in the active archive.
     * @param value Value to store, convert, draw or compare.
     */
    template<class T> void Field(std::string_view, const T& value) {
        xengine::detail::serialization::WriteBinary(writer_, value, depth_ + 1);
    }
    /** @brief Visits or converts developer-selected data fields in the active archive.
     * @param csv Comma-separated field names.
     * @param values User-selected fields or values visited by the archive.
     */
    template<class... Values> void Fields(std::string_view csv, const Values&... values) {
        xengine::detail::serialization::Fields(*this, csv, values...);
    }
};

/** @brief Reads registered fields in their declared order from binary data.
 * @ingroup xengine_serialization
 */
class BinaryReadArchive final {
    BinaryReader& reader_;
    unsigned depth_ = 0;
public:
    /** @brief Constructs a BinaryReadArchive instance.
     * @param reader Input binary archive.
     * @param depth Current nesting depth used for bounded recursive serialization.
     */
    explicit BinaryReadArchive(BinaryReader& reader, unsigned depth = 0)
        : reader_(reader), depth_(depth) {}
    /** @brief Visits or converts developer-selected data fields in the active archive.
     * @param value Value to store, convert, draw or compare.
     */
    template<class T> void Field(std::string_view, T& value) {
        xengine::detail::serialization::ReadBinary(reader_, value, depth_ + 1);
    }
    /** @brief Visits or converts developer-selected data fields in the active archive.
     * @param csv Comma-separated field names.
     * @param values User-selected fields or values visited by the archive.
     */
    template<class... Values> void Fields(std::string_view csv, Values&... values) {
        xengine::detail::serialization::Fields(*this, csv, values...);
    }
};

namespace xengine::detail::serialization {
template<class T>
/** @brief Converts the selected data into a JSON value.
 * @param value Value to store, convert, draw or compare.
 * @param depth Current nesting depth used for bounded recursive serialization.
 */
nlohmann::json ToJson(const T& value, unsigned depth) {
    if (depth > 64) throw std::length_error("Serialization exceeds maximum depth");
    if constexpr (RegisteredConst<T, JsonWriteArchive>) {
        JsonWriteArchive archive(depth);
        value.Serialize(archive);
        return std::move(archive).Take();
    } else if constexpr (Vec2Type<T>::value) {
        return {{"x", value.x}, {"y", value.y}};
    } else if constexpr (Vec3Type<T>::value) {
        return {{"x", value.x}, {"y", value.y}, {"z", value.z}};
    } else if constexpr (Optional<T>::value) {
        return value ? ToJson(*value, depth + 1) : nlohmann::json(nullptr);
    } else if constexpr (Vector<T>::value || List<T>::value || Array<T>::value) {
        nlohmann::json result = nlohmann::json::array();
        for (const auto& item : value) result.push_back(ToJson(item, depth + 1));
        return result;
    } else if constexpr (Map<T>::value) {
        if constexpr (std::is_same_v<typename Map<T>::Key, std::string>) {
            nlohmann::json result = nlohmann::json::object();
            for (const auto& [key, item] : value) result[key] = ToJson(item, depth + 1);
            return result;
        } else {
            nlohmann::json result = nlohmann::json::array();
            for (const auto& [key, item] : value)
                result.push_back(nlohmann::json::array({ToJson(key, depth + 1), ToJson(item, depth + 1)}));
            return result;
        }
    } else {
        // Also supports user-supplied nlohmann::to_json overloads.
        return nlohmann::json(value);
    }
}

template<class T>
/** @brief Reads the selected data from a JSON value.
 * @param json JSON value to read or modify.
 * @param value Value to store, convert, draw or compare.
 * @param depth Current nesting depth used for bounded recursive serialization.
 */
void FromJson(const nlohmann::json& json, T& value, unsigned depth) {
    if (depth > 64) throw std::length_error("Deserialization exceeds maximum depth");
    if constexpr (RegisteredMutable<T, JsonReadArchive>) {
        JsonReadArchive archive(json, depth);
        value.Serialize(archive);
    } else if constexpr (Vec2Type<T>::value) {
        FromJson(json.at("x"), value.x, depth + 1);
        FromJson(json.at("y"), value.y, depth + 1);
    } else if constexpr (Vec3Type<T>::value) {
        FromJson(json.at("x"), value.x, depth + 1);
        FromJson(json.at("y"), value.y, depth + 1);
        FromJson(json.at("z"), value.z, depth + 1);
    } else if constexpr (Optional<T>::value) {
        if (json.is_null()) value.reset();
        else { typename Optional<T>::Value item{}; FromJson(json, item, depth + 1); value = std::move(item); }
    } else if constexpr (Vector<T>::value || List<T>::value) {
        if (!json.is_array()) throw std::invalid_argument("Expected JSON array");
        if (json.size() > 1024u * 1024u) throw std::length_error("JSON array exceeds limit");
        T result;
        for (const auto& element : json) {
            typename T::value_type item{};
            FromJson(element, item, depth + 1);
            if constexpr (Vector<T>::value) result.push_back(std::move(item));
            else result.push_back(std::move(item));
        }
        value = std::move(result);
    } else if constexpr (Array<T>::value) {
        if (!json.is_array() || json.size() != value.size())
            throw std::invalid_argument("Unexpected fixed-size JSON array length");
        for (std::size_t i = 0; i < value.size(); ++i) FromJson(json.at(i), value[i], depth + 1);
    } else if constexpr (Map<T>::value) {
        T result;
        if constexpr (std::is_same_v<typename Map<T>::Key, std::string>) {
            if (!json.is_object()) throw std::invalid_argument("Expected JSON object dictionary");
            if (json.size() > 1024u * 1024u) throw std::length_error("JSON dictionary exceeds limit");
            for (auto it = json.begin(); it != json.end(); ++it) {
                typename Map<T>::Value item{};
                FromJson(it.value(), item, depth + 1);
                result.emplace(it.key(), std::move(item));
            }
        } else {
            if (!json.is_array()) throw std::invalid_argument("Expected JSON dictionary pairs");
            if (json.size() > 1024u * 1024u) throw std::length_error("JSON dictionary exceeds limit");
            for (const auto& entry : json) {
                if (!entry.is_array() || entry.size() != 2)
                    throw std::invalid_argument("Expected key/value pair");
                typename Map<T>::Key key{};
                typename Map<T>::Value item{};
                FromJson(entry.at(0), key, depth + 1);
                FromJson(entry.at(1), item, depth + 1);
                if (!result.emplace(std::move(key), std::move(item)).second)
                    throw std::invalid_argument("Duplicate dictionary key");
            }
        }
        value = std::move(result);
    } else {
        value = json.get<T>();
    }
}

template<class T>
/** @brief Writes binary using the specified format and bounds.
 * @param writer Output binary archive.
 * @param value Value to store, convert, draw or compare.
 * @param depth Current nesting depth used for bounded recursive serialization.
 */
void WriteBinary(BinaryWriter& writer, const T& value, unsigned depth) {
    if (depth > 64) throw std::length_error("Serialization exceeds maximum depth");
    if constexpr (RegisteredConst<T, BinaryWriteArchive>) {
        BinaryWriteArchive archive(writer, depth);
        value.Serialize(archive);
    } else if constexpr (std::is_same_v<T, bool>) writer.WriteBool(value);
    else if constexpr (std::is_enum_v<T>) WriteBinary(writer, static_cast<std::underlying_type_t<T>>(value), depth+1);
    else if constexpr (std::is_integral_v<T> && sizeof(T) == 1) writer.WriteByte(static_cast<std::uint8_t>(value));
    else if constexpr (std::is_integral_v<T> && sizeof(T) == 2) writer.WriteUInt16(static_cast<std::uint16_t>(value));
    else if constexpr (std::is_integral_v<T> && sizeof(T) == 4) writer.WriteUInt32(static_cast<std::uint32_t>(value));
    else if constexpr (std::is_integral_v<T> && sizeof(T) == 8) writer.WriteUInt64(static_cast<std::uint64_t>(value));
    else if constexpr (std::is_same_v<T, float>) writer.WriteFloat32(value);
    else if constexpr (std::is_same_v<T, double>) writer.WriteFloat64(value);
    else if constexpr (std::is_same_v<T, std::string>) writer.WriteString(value);
    else if constexpr (Vec2Type<T>::value) {
        WriteBinary(writer, value.x, depth+1); WriteBinary(writer, value.y, depth+1);
    } else if constexpr (Vec3Type<T>::value) {
        WriteBinary(writer, value.x, depth+1); WriteBinary(writer, value.y, depth+1);
        WriteBinary(writer, value.z, depth+1);
    } else if constexpr (Optional<T>::value) {
        writer.WriteBool(value.has_value());
        if (value) WriteBinary(writer, *value, depth+1);
    } else if constexpr (Array<T>::value) {
        for (const auto& item : value) WriteBinary(writer, item, depth+1);
    } else if constexpr (Vector<T>::value || List<T>::value || Map<T>::value) {
        if (value.size() > UINT32_MAX) throw std::length_error("Container too large");
        writer.WriteUInt32(static_cast<std::uint32_t>(value.size()));
        if constexpr (Map<T>::value) {
            for (const auto& [key, item] : value) {
                WriteBinary(writer, key, depth+1); WriteBinary(writer, item, depth+1);
            }
        } else for (const auto& item : value) WriteBinary(writer, item, depth+1);
    } else static_assert(kFalse<T>, "Register serializable fields or write this type manually");
}

template<class T>
/** @brief Reads binary using the specified format and bounds.
 * @param reader Input binary archive.
 * @param value Value to store, convert, draw or compare.
 * @param depth Current nesting depth used for bounded recursive serialization.
 */
void ReadBinary(BinaryReader& reader, T& value, unsigned depth) {
    if (depth > reader.Limits().maxDepth) throw BinaryReadError("Deserialization exceeds maximum depth");
    if constexpr (RegisteredMutable<T, BinaryReadArchive>) {
        BinaryReadArchive archive(reader, depth); value.Serialize(archive);
    } else if constexpr (std::is_same_v<T, bool>) value = reader.ReadBool();
    else if constexpr (std::is_enum_v<T>) {
        std::underlying_type_t<T> underlying{}; ReadBinary(reader, underlying, depth+1);
        value = static_cast<T>(underlying); // Game must validate allowed enum members.
    } else if constexpr (std::is_integral_v<T> && sizeof(T) == 1) value = static_cast<T>(reader.ReadByte());
    else if constexpr (std::is_integral_v<T> && sizeof(T) == 2) {
        const auto raw = reader.ReadUInt16();
        if constexpr (std::is_signed_v<T>) value = std::bit_cast<T>(raw);
        else value = raw;
    } else if constexpr (std::is_integral_v<T> && sizeof(T) == 4) {
        const auto raw = reader.ReadUInt32();
        if constexpr (std::is_signed_v<T>) value = std::bit_cast<T>(raw);
        else value = raw;
    } else if constexpr (std::is_integral_v<T> && sizeof(T) == 8) {
        const auto raw = reader.ReadUInt64();
        if constexpr (std::is_signed_v<T>) value = std::bit_cast<T>(raw);
        else value = raw;
    } else if constexpr (std::is_same_v<T, float>) value = reader.ReadFloat32();
    else if constexpr (std::is_same_v<T, double>) value = reader.ReadFloat64();
    else if constexpr (std::is_same_v<T, std::string>) value = reader.ReadString();
    else if constexpr (Vec2Type<T>::value) {
        ReadBinary(reader, value.x, depth+1); ReadBinary(reader, value.y, depth+1);
    } else if constexpr (Vec3Type<T>::value) {
        ReadBinary(reader, value.x, depth+1); ReadBinary(reader, value.y, depth+1);
        ReadBinary(reader, value.z, depth+1);
    } else if constexpr (Optional<T>::value) {
        if (!reader.ReadBool()) value.reset();
        else { typename Optional<T>::Value item{}; ReadBinary(reader, item, depth+1); value=std::move(item); }
    } else if constexpr (Array<T>::value) {
        for (auto& item : value) ReadBinary(reader, item, depth+1);
    } else if constexpr (Vector<T>::value || List<T>::value || Map<T>::value) {
        const std::uint32_t count = reader.ReadUInt32();
        if (count > reader.Limits().maxContainerElements)
            throw BinaryReadError("Container exceeds configured limit");
        T result;
        for (std::uint32_t i=0; i<count; ++i) {
            if constexpr (Map<T>::value) {
                typename Map<T>::Key key{}; typename Map<T>::Value item{};
                ReadBinary(reader, key, depth+1); ReadBinary(reader, item, depth+1);
                if (!result.emplace(std::move(key), std::move(item)).second)
                    throw BinaryReadError("Duplicate dictionary key");
            } else {
                typename T::value_type item{}; ReadBinary(reader, item, depth+1);
                result.push_back(std::move(item));
            }
        }
        value = std::move(result);
    } else static_assert(kFalse<T>, "Register serializable fields or read this type manually");
}
}

/** @brief Converts supported value types and registered classes into a deterministic binary field stream.
 * @ingroup xengine_serialization
 */
class BinarySerializer final {
public:
    /** @brief Writes a supported value or registered class into a binary field stream, without a file header.
     * @param value Value to store, convert, draw or compare.
     * @param order Bit order within each byte.
     */
    template<class T> static std::vector<std::byte> Serialize(const T& value,
                                                              BitOrder order = BitOrder::LSBFirst) {
        BinaryWriter writer(order);
        xengine::detail::serialization::WriteBinary(writer, value);
        return writer.TakeData();
    }
    /** @brief Reads a binary field stream into a registered or supported value type with configured safety limits.
     * @param input Input sequence to parse or consume.
     * @param limits Safety bounds applied while parsing the input.
     * @param order Bit order within each byte.
     */
    template<class T> static T Deserialize(std::span<const std::byte> input,
                                            BinaryLimits limits = {}, BitOrder order = BitOrder::LSBFirst) {
        BinaryReader reader(input, limits, order);
        T value{};
        xengine::detail::serialization::ReadBinary(reader, value);
        if (reader.RemainingBytes() != 0 || !reader.IsByteAligned())
            throw BinaryReadError("Unconsumed bytes after deserialization");
        return value;
    }
};
