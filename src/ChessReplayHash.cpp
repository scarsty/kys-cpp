#include "ChessReplayHash.h"

#include <picosha2.h>

#include <stdexcept>

namespace KysChess
{
namespace
{

std::uint8_t hexValue(char value)
{
    if (value >= '0' && value <= '9')
    {
        return static_cast<std::uint8_t>(value - '0');
    }
    if (value >= 'a' && value <= 'f')
    {
        return static_cast<std::uint8_t>(value - 'a' + 10);
    }
    throw std::invalid_argument("hash hex must be lowercase hexadecimal");
}

template <std::size_t Size>
std::string hashHex(const std::array<std::uint8_t, Size>& hash)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string result(hash.size() * 2, '0');
    for (std::size_t i = 0; i < hash.size(); ++i)
    {
        result[i * 2] = digits[hash[i] >> 4];
        result[i * 2 + 1] = digits[hash[i] & 0x0f];
    }
    return result;
}

template <std::size_t Size>
std::array<std::uint8_t, Size> hashFromHex(std::string_view hex)
{
    if (hex.size() != Size * 2)
    {
        throw std::invalid_argument("hash hex has an invalid length");
    }
    std::array<std::uint8_t, Size> result{};
    for (std::size_t i = 0; i < result.size(); ++i)
    {
        result[i] = static_cast<std::uint8_t>((hexValue(hex[i * 2]) << 4) | hexValue(hex[i * 2 + 1]));
    }
    return result;
}

}

ChessSha256 chessSha256(std::span<const std::uint8_t> bytes)
{
    ChessSha256 result{};
    picosha2::hash256(bytes.begin(), bytes.end(), result.begin(), result.end());
    return result;
}

ChessSha256 chessSha256(std::string_view bytes)
{
    ChessSha256 result{};
    picosha2::hash256(bytes.begin(), bytes.end(), result.begin(), result.end());
    return result;
}

std::string chessSha256Hex(const ChessSha256& hash)
{
    return hashHex(hash);
}

ChessSha256 chessSha256FromHex(std::string_view hex)
{
    return hashFromHex<ChessSha256{}.size()>(hex);
}

std::string chessEvidenceHashHex(const ChessEvidenceHash& hash)
{
    return hashHex(hash);
}

ChessEvidenceHash chessEvidenceHashFromHex(std::string_view hex)
{
    return hashFromHex<ChessEvidenceHash{}.size()>(hex);
}

ChessEvidenceHash chessEvidenceHash(const ChessSha256& hash)
{
    ChessEvidenceHash result{};
    std::ranges::copy_n(hash.begin(), result.size(), result.begin());
    return result;
}

}
