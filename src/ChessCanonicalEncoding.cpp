#include "ChessCanonicalEncoding.h"

#include <limits>
#include <stdexcept>

namespace KysChess
{
namespace
{

template <typename T>
void appendLittleEndian(std::vector<std::uint8_t>& output, T value)
{
    for (std::size_t i = 0; i < sizeof(T); ++i)
    {
        output.push_back(static_cast<std::uint8_t>(value & 0xffU));
        value >>= 8;
    }
}

}

bool isValidUtf8(std::string_view value)
{
    const auto* bytes = reinterpret_cast<const unsigned char*>(value.data());
    std::size_t index = 0;
    while (index < value.size())
    {
        const unsigned char lead = bytes[index++];
        if (lead <= 0x7f)
        {
            continue;
        }

        std::uint32_t codePoint = 0;
        std::size_t continuationCount = 0;
        std::uint32_t minimum = 0;
        if ((lead & 0xe0) == 0xc0)
        {
            codePoint = lead & 0x1f;
            continuationCount = 1;
            minimum = 0x80;
        }
        else if ((lead & 0xf0) == 0xe0)
        {
            codePoint = lead & 0x0f;
            continuationCount = 2;
            minimum = 0x800;
        }
        else if ((lead & 0xf8) == 0xf0)
        {
            codePoint = lead & 0x07;
            continuationCount = 3;
            minimum = 0x10000;
        }
        else
        {
            return false;
        }

        if (index + continuationCount > value.size())
        {
            return false;
        }
        for (std::size_t i = 0; i < continuationCount; ++i)
        {
            const unsigned char continuation = bytes[index++];
            if ((continuation & 0xc0) != 0x80)
            {
                return false;
            }
            codePoint = (codePoint << 6) | (continuation & 0x3f);
        }

        if (codePoint < minimum || codePoint > 0x10ffff
            || (codePoint >= 0xd800 && codePoint <= 0xdfff))
        {
            return false;
        }
    }
    return true;
}

ChessCanonicalWriter::ChessCanonicalWriter(std::string_view domainTag, std::uint16_t version)
{
    if (domainTag.size() != 4)
    {
        throw std::invalid_argument("canonical domain tag must contain exactly four bytes");
    }
    bytes_.insert(bytes_.end(), domainTag.begin(), domainTag.end());
    writeU16(version);
}

void ChessCanonicalWriter::writeBool(bool value)
{
    writeU8(value ? 1 : 0);
}

void ChessCanonicalWriter::writeU8(std::uint8_t value)
{
    bytes_.push_back(value);
}

void ChessCanonicalWriter::writeU16(std::uint16_t value)
{
    appendLittleEndian(bytes_, value);
}

void ChessCanonicalWriter::writeU32(std::uint32_t value)
{
    appendLittleEndian(bytes_, value);
}

void ChessCanonicalWriter::writeI32(std::int32_t value)
{
    writeU32(static_cast<std::uint32_t>(value));
}

void ChessCanonicalWriter::writeU64(std::uint64_t value)
{
    appendLittleEndian(bytes_, value);
}

void ChessCanonicalWriter::writeString(std::string_view value)
{
    if (!isValidUtf8(value))
    {
        throw std::invalid_argument("canonical string is not valid UTF-8");
    }
    writeCollectionSize(value.size());
    bytes_.insert(bytes_.end(), value.begin(), value.end());
}

void ChessCanonicalWriter::writeHash(const ChessSha256& value)
{
    bytes_.insert(bytes_.end(), value.begin(), value.end());
}

void ChessCanonicalWriter::writeBytes(std::span<const std::uint8_t> value)
{
    bytes_.insert(bytes_.end(), value.begin(), value.end());
}

void ChessCanonicalWriter::writeOptionalPresence(bool present)
{
    writeBool(present);
}

void ChessCanonicalWriter::writeCollectionSize(std::size_t size)
{
    if (size > std::numeric_limits<std::uint32_t>::max())
    {
        throw std::length_error("canonical collection exceeds uint32 length");
    }
    writeU32(static_cast<std::uint32_t>(size));
}

}
