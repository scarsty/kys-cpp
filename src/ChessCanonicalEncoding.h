#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <utility>
#include <string>
#include <string_view>
#include <vector>

namespace KysChess
{

using ChessSha256 = std::array<std::uint8_t, 32>;

class ChessCanonicalWriter
{
public:
    ChessCanonicalWriter(std::string_view domainTag, std::uint16_t version = 1);

    void writeBool(bool value);
    void writeU8(std::uint8_t value);
    void writeU16(std::uint16_t value);
    void writeU32(std::uint32_t value);
    void writeI32(std::int32_t value);
    void writeU64(std::uint64_t value);
    void writeString(std::string_view value);
    void writeHash(const ChessSha256& value);
    void writeBytes(std::span<const std::uint8_t> value);
    void writeOptionalPresence(bool present);
    void writeCollectionSize(std::size_t size);

    const std::vector<std::uint8_t>& bytes() const { return bytes_; }
    std::vector<std::uint8_t> takeBytes() && { return std::move(bytes_); }

private:
    std::vector<std::uint8_t> bytes_;
};

bool isValidUtf8(std::string_view value);

}
