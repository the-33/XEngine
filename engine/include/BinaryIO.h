#pragma once
/** @file BinaryIO.h
 * @brief Bounded binary primitive, byte-stream and bit-stream I/O.
 */

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

/** Bits inside a byte; independent of numeric little-endian layout. */
enum class BitOrder {
    LSBFirst, ///< First bit written/read becomes the least significant bit of its byte.
    MSBFirst  ///< First bit written/read becomes the most significant bit of its byte.
};

/** @brief Caller-defined upper bounds for binary file size, strings, collections and recursion.
 */
struct BinaryLimits {
    /** @brief Maximum total input size accepted by a BinaryReader, in bytes.
     */
    std::size_t maxBytes = 64u * 1024u * 1024u;
    /** @brief Maximum UTF-8 byte length of one decoded string.
     */
    std::size_t maxStringBytes = 1024u * 1024u;
    /** @brief Maximum number of elements accepted in one container.
     */
    std::uint32_t maxContainerElements = 1024u * 1024u;
    /** @brief Maximum nested serialization depth when visiting custom types.
     */
    std::uint32_t maxDepth = 64;
};

/** @brief Exception raised for malformed, misaligned or truncated binary input.
 */
class BinaryReadError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/** In-memory, bounds-aware BinaryWriter compatible with .NET's numeric and string layout.
 *  Little-endian IEEE 754 numbers and 7-bit UTF-8 byte length for strings. No object execution.
 */
/** @brief Writes explicit little-endian primitives, UTF-8 strings, bytes and packed bits.
 * @ingroup xengine_serialization
 */
class BinaryWriter final {
    std::vector<std::byte> bytes_;
    unsigned bitOffset_ = 0;
    BitOrder bitOrder_ = BitOrder::LSBFirst;

    void RequireAligned() const {
        if (bitOffset_) throw std::logic_error("Call AlignToByte() before writing byte-aligned values");
    }
public:
    /** @brief Constructs a BinaryWriter instance.
     * @param order Bit order within each byte.
     */
    explicit BinaryWriter(BitOrder order = BitOrder::LSBFirst) : bitOrder_(order) {}

    /** @brief Returns a read-only view of the serialized byte buffer.
     */
    const std::vector<std::byte>& Data() const noexcept { return bytes_; }
    /** @brief Moves the accumulated byte buffer out of the writer.
     */
    std::vector<std::byte> TakeData() { AlignToByte(); return std::move(bytes_); }
    /** @brief Returns the number of bytes currently written to the buffer.
     */
    std::size_t Size() const noexcept { return bytes_.size(); }
    /** @brief Reports whether the next write starts at a byte boundary.
     */
    bool IsByteAligned() const noexcept { return bitOffset_ == 0; }
    /** @brief Pads the current bit field to the next byte before writing byte-aligned primitives.
     */
    void AlignToByte() noexcept { bitOffset_ = 0; }

    /** @brief Appends one unsigned byte, requiring byte alignment.
     * @param value Byte value to append.
     */
    void WriteByte(std::uint8_t value) { RequireAligned(); bytes_.push_back(static_cast<std::byte>(value)); }
    /** @brief Writes a boolean as one byte (0 or 1), requiring alignment.
     * @param value Boolean value to append.
     */
    void WriteBool(bool value) { WriteByte(value ? 1 : 0); }
    /** @brief Writes exactly the supplied number of bytes, without an implicit prefix or padding.
     * @param bytes Uninterpreted binary byte sequence.
     */
    void WriteBytes(std::span<const std::byte> bytes) {
        RequireAligned(); bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
    }
    /** @brief Writes exactly the supplied number of bytes, without an implicit prefix or padding.
     * @param bytes Uninterpreted binary byte sequence.
     * @param length Requested number of elements or bytes.
     */
    void WriteBytes(const void* bytes, std::size_t length) {
        if (length && !bytes) throw std::invalid_argument("Null byte buffer");
        WriteBytes({static_cast<const std::byte*>(bytes), length});
    }
    /** @brief Writes uint16 using the specified format and bounds.
     * @param v Value, vector or input to this operation.
     */
    void WriteUInt16(std::uint16_t v) {
        WriteByte(static_cast<std::uint8_t>(v)); WriteByte(static_cast<std::uint8_t>(v >> 8));
    }
    /** @brief Writes int16 using the specified format and bounds.
     * @param v Value, vector or input to this operation.
     */
    void WriteInt16(std::int16_t v) { WriteUInt16(std::bit_cast<std::uint16_t>(v)); }
    /** @brief Writes uint32 using the specified format and bounds.
     * @param v Value, vector or input to this operation.
     */
    void WriteUInt32(std::uint32_t v) {
        for (unsigned i = 0; i < 4; ++i) WriteByte(static_cast<std::uint8_t>(v >> (8 * i)));
    }
    /** @brief Writes int32 using the specified format and bounds.
     * @param v Value, vector or input to this operation.
     */
    void WriteInt32(std::int32_t v) { WriteUInt32(std::bit_cast<std::uint32_t>(v)); }
    /** @brief Writes uint64 using the specified format and bounds.
     * @param v Value, vector or input to this operation.
     */
    void WriteUInt64(std::uint64_t v) {
        for (unsigned i = 0; i < 8; ++i) WriteByte(static_cast<std::uint8_t>(v >> (8 * i)));
    }
    /** @brief Writes int64 using the specified format and bounds.
     * @param v Value, vector or input to this operation.
     */
    void WriteInt64(std::int64_t v) { WriteUInt64(std::bit_cast<std::uint64_t>(v)); }
    /** @brief Writes float32 using the specified format and bounds.
     * @param v Value, vector or input to this operation.
     */
    void WriteFloat32(float v) {
        static_assert(sizeof(float) == 4);
        WriteUInt32(std::bit_cast<std::uint32_t>(v));
    }
    /** @brief Writes float64 using the specified format and bounds.
     * @param v Value, vector or input to this operation.
     */
    void WriteFloat64(double v) {
        static_assert(sizeof(double) == 8);
        WriteUInt64(std::bit_cast<std::uint64_t>(v));
    }
    /** @brief Writes 7 bit encoded uint32 using the specified format and bounds.
     * @param v Value, vector or input to this operation.
     */
    void Write7BitEncodedUInt32(std::uint32_t v) {
        while (v >= 0x80u) { WriteByte(static_cast<std::uint8_t>(v | 0x80u)); v >>= 7; }
        WriteByte(static_cast<std::uint8_t>(v));
    }
    /** @brief Writes a UTF-8 string prefixed with its byte length encoded as a .NET-compatible 7-bit integer.
     * @param utf8 UTF-8 encoded input text.
     */
    void WriteString(std::string_view utf8) {
        if (utf8.size() > UINT32_MAX) throw std::length_error("String exceeds uint32 length");
        Write7BitEncodedUInt32(static_cast<std::uint32_t>(utf8.size()));
        WriteBytes(utf8.data(), utf8.size());
    }
    /** @brief Writes a uint32 length followed by the exact binary bytes.
     * @param bytes Uninterpreted binary byte sequence.
     */
    void WriteByteArray(std::span<const std::byte> bytes) {
        if (bytes.size() > UINT32_MAX) throw std::length_error("Array exceeds uint32 length");
        WriteUInt32(static_cast<std::uint32_t>(bytes.size()));
        WriteBytes(bytes);
    }
    /** Write one bit into the current byte; use AlignToByte() before any primitive write. */
    void WriteBit(bool value) {
        if (bitOffset_ == 0) bytes_.push_back(std::byte{0});
        const unsigned position = bitOrder_ == BitOrder::LSBFirst ? bitOffset_ : 7 - bitOffset_;
        if (value) bytes_.back() |= static_cast<std::byte>(1u << position);
        bitOffset_ = (bitOffset_ + 1) % 8;
    }
    /** Write count bits (0..64), least or most significant first per BitOrder. */
    void WriteBits(std::uint64_t value, unsigned count) {
        if (count > 64) throw std::invalid_argument("WriteBits count must be <= 64");
        for (unsigned i = 0; i < count; ++i) {
            const unsigned shift = bitOrder_ == BitOrder::LSBFirst ? i : count - 1 - i;
            WriteBit(((value >> shift) & 1u) != 0);
        }
    }
    /** Write arbitrary number of bits, taking each byte's bits in the configured order. */
    void WriteBits(std::span<const std::byte> input, std::size_t count) {
        if (count > input.size() * 8) throw std::invalid_argument("Insufficient source bits");
        for (std::size_t i = 0; i < count; ++i) {
            const unsigned shift = bitOrder_ == BitOrder::LSBFirst ? (i % 8) : (7 - i % 8);
            WriteBit(((std::to_integer<unsigned>(input[i / 8]) >> shift) & 1u) != 0);
        }
    }
};

/** @brief Reads bounded little-endian primitives, strings, bytes and packed bits.
 * @ingroup xengine_serialization
 */
class BinaryReader final {
    std::span<const std::byte> data_;
    std::size_t position_ = 0;
    unsigned bitOffset_ = 0;
    BitOrder bitOrder_ = BitOrder::LSBFirst;
    BinaryLimits limits_{};

    void RequireAligned() const {
        if (bitOffset_) throw BinaryReadError("Call AlignToByte() before reading byte-aligned values");
    }
    void Require(std::size_t count) const {
        if (position_ > data_.size() || count > data_.size() - position_)
            throw BinaryReadError("Unexpected end of binary input");
    }
public:
    /** @brief Constructs a BinaryReader instance.
     * @param input Input sequence to parse or consume.
     * @param limits Safety bounds applied while parsing the input.
     * @param order Bit order within each byte.
     */
    explicit BinaryReader(std::span<const std::byte> input,
                          BinaryLimits limits = {}, BitOrder order = BitOrder::LSBFirst)
        : data_(input), bitOrder_(order), limits_(limits) {
        if (input.size() > limits.maxBytes) throw BinaryReadError("Binary input exceeds configured limit");
    }
    /** @brief Returns the current read offset in bytes.
     */
    std::size_t Position() const noexcept { return position_; }
    /** @brief Returns the number of unread complete input bytes.
     */
    std::size_t RemainingBytes() const noexcept { return data_.size() - position_; }
    /** @brief Reports whether byte aligned is currently true.
     */
    bool IsByteAligned() const noexcept { return bitOffset_ == 0; }
    /** @brief Returns the configured bounds for binary input reads.
     */
    const BinaryLimits& Limits() const noexcept { return limits_; }
    /** @brief Skips unused bits of the current input byte before reading primitives.
     */
    void AlignToByte() noexcept {
        if (bitOffset_) { ++position_; bitOffset_ = 0; }
    }
    /** @brief Reads byte using the specified format and bounds.
     */
    std::uint8_t ReadByte() {
        RequireAligned(); Require(1);
        return std::to_integer<std::uint8_t>(data_[position_++]);
    }
    /** @brief Attempts to consume one byte without throwing on insufficient input.
     * @param output Destination buffer for decoded values.
     */
    bool TryReadByte(std::uint8_t& output) noexcept {
        if (bitOffset_ || position_ >= data_.size()) return false;
        output = std::to_integer<std::uint8_t>(data_[position_++]); return true;
    }
    /** @brief Reads bool using the specified format and bounds.
     */
    bool ReadBool() {
        auto b = ReadByte();
        if (b > 1) throw BinaryReadError("Invalid boolean encoding");
        return b == 1;
    }
    /** @brief Reads exactly the requested byte count and throws on truncated or misaligned input.
     * @param output Destination buffer for decoded values.
     */
    void ReadBytes(std::span<std::byte> output) {
        RequireAligned(); Require(output.size());
        std::copy_n(data_.begin() + position_, output.size(), output.begin());
        position_ += output.size();
    }
    /** @brief Attempts to consume the requested number of bytes.
     * @param output Destination buffer for decoded values.
     */
    bool TryReadBytes(std::span<std::byte> output) noexcept {
        if (bitOffset_ || output.size() > data_.size() - position_) return false;
        std::copy_n(data_.begin() + position_, output.size(), output.begin());
        position_ += output.size(); return true;
    }
    /** @brief Reads exactly the requested byte count and throws on truncated or misaligned input.
     * @param count Number of elements or bits to read/write, as stated by the method.
     */
    std::vector<std::byte> ReadBytes(std::size_t count) {
        RequireAligned(); Require(count);
        std::vector<std::byte> result(count);
        ReadBytes(std::span<std::byte>(result));
        return result;
    }
    /** @brief Reads uint16 using the specified format and bounds.
     */
    std::uint16_t ReadUInt16() {
        std::uint16_t result = 0;
        for (unsigned i = 0; i < 2; ++i) result |= std::uint16_t(ReadByte()) << (8 * i);
        return result;
    }
    /** @brief Reads int16 using the specified format and bounds.
     */
    std::int16_t ReadInt16() { return std::bit_cast<std::int16_t>(ReadUInt16()); }
    /** @brief Reads uint32 using the specified format and bounds.
     */
    std::uint32_t ReadUInt32() {
        std::uint32_t result = 0;
        for (unsigned i = 0; i < 4; ++i) result |= std::uint32_t(ReadByte()) << (8 * i);
        return result;
    }
    /** @brief Reads int32 using the specified format and bounds.
     */
    std::int32_t ReadInt32() { return std::bit_cast<std::int32_t>(ReadUInt32()); }
    /** @brief Reads uint64 using the specified format and bounds.
     */
    std::uint64_t ReadUInt64() {
        std::uint64_t result = 0;
        for (unsigned i = 0; i < 8; ++i) result |= std::uint64_t(ReadByte()) << (8 * i);
        return result;
    }
    /** @brief Reads int64 using the specified format and bounds.
     */
    std::int64_t ReadInt64() { return std::bit_cast<std::int64_t>(ReadUInt64()); }
    /** @brief Reads float32 using the specified format and bounds.
     */
    float ReadFloat32() { return std::bit_cast<float>(ReadUInt32()); }
    /** @brief Reads float64 using the specified format and bounds.
     */
    double ReadFloat64() { return std::bit_cast<double>(ReadUInt64()); }
    /** @brief Reads 7 bit encoded uint32 using the specified format and bounds.
     */
    std::uint32_t Read7BitEncodedUInt32() {
        std::uint32_t result = 0;
        for (unsigned i = 0; i < 5; ++i) {
            std::uint8_t byte = ReadByte();
            if (i == 4 && (byte & 0xf0u)) throw BinaryReadError("Invalid 7-bit length");
            result |= std::uint32_t(byte & 0x7fu) << (i * 7);
            if ((byte & 0x80u) == 0) return result;
        }
        throw BinaryReadError("Unterminated 7-bit length");
    }
    /** @brief Reads a .NET-style 7-bit UTF-8 length and bounded string bytes; rejects invalid prefixes and truncation.
     */
    std::string ReadString() {
        const std::size_t count = Read7BitEncodedUInt32();
        if (count > limits_.maxStringBytes) throw BinaryReadError("String exceeds configured limit");
        RequireAligned(); Require(count);
        const std::string result(reinterpret_cast<const char*>(data_.data() + position_), count);
        position_ += count;
        return result;
    }
    /** @brief Reads byte array using the specified format and bounds.
     */
    std::vector<std::byte> ReadByteArray() {
        auto count = ReadUInt32();
        if (count > limits_.maxContainerElements) throw BinaryReadError("Array exceeds configured limit");
        return ReadBytes(count);
    }
    /** @brief Reads bit using the specified format and bounds.
     */
    bool ReadBit() {
        Require(1);
        const unsigned shift = bitOrder_ == BitOrder::LSBFirst ? bitOffset_ : 7 - bitOffset_;
        const bool value = ((std::to_integer<unsigned>(data_[position_]) >> shift) & 1u) != 0;
        if (++bitOffset_ == 8) { bitOffset_ = 0; ++position_; }
        return value;
    }
    /** @brief Reads exactly count bits using the selected BitOrder and raises BinaryReadError if incomplete.
     * @param count Number of elements or bits to read/write, as stated by the method.
     */
    std::uint64_t ReadBits(unsigned count) {
        if (count > 64) throw BinaryReadError("ReadBits count must be <= 64");
        std::uint64_t result = 0;
        for (unsigned i = 0; i < count; ++i) {
            const unsigned shift = bitOrder_ == BitOrder::LSBFirst ? i : count - 1 - i;
            result |= std::uint64_t(ReadBit()) << shift;
        }
        return result;
    }
    /** @brief Reads exactly count bits using the selected BitOrder and raises BinaryReadError if incomplete.
     * @param output Destination buffer for decoded values.
     * @param count Number of elements or bits to read/write, as stated by the method.
     */
    void ReadBits(std::span<std::byte> output, std::size_t count) {
        if (count > output.size() * 8) throw BinaryReadError("Insufficient destination bits");
        std::fill(output.begin(), output.end(), std::byte{0});
        for (std::size_t i = 0; i < count; ++i) {
            const unsigned shift = bitOrder_ == BitOrder::LSBFirst ? i % 8 : 7 - i % 8;
            if (ReadBit()) output[i / 8] |= static_cast<std::byte>(1u << shift);
        }
    }
};
