#include "ChessReplayHash.h"

#include <picosha2.h>

#include <stdexcept>
#include <vector>

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
    throw std::invalid_argument("SHA-256 hex must be lowercase hexadecimal");
}

}

ChessSha256 chessSha256(std::span<const std::uint8_t> bytes)
{
    ChessSha256 result{};
    picosha2::hash256(bytes.begin(), bytes.end(), result.begin(), result.end());
    return result;
}

std::string chessSha256Hex(const ChessSha256& hash)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.resize(hash.size() * 2);
    for (std::size_t i = 0; i < hash.size(); ++i)
    {
        result[i * 2] = digits[hash[i] >> 4];
        result[i * 2 + 1] = digits[hash[i] & 0x0f];
    }
    return result;
}

ChessSha256 chessSha256FromHex(std::string_view hex)
{
    if (hex.size() != 64)
    {
        throw std::invalid_argument("SHA-256 hex must contain exactly 64 characters");
    }
    ChessSha256 result{};
    for (std::size_t i = 0; i < result.size(); ++i)
    {
        result[i] = static_cast<std::uint8_t>((hexValue(hex[i * 2]) << 4) | hexValue(hex[i * 2 + 1]));
    }
    return result;
}

ChessSha256 initialReplayChain(std::span<const std::uint8_t> canonicalHeader)
{
    constexpr std::string_view prefix = "KYS_CHESS_REPLAY_CHAIN_V1";
    std::vector<std::uint8_t> input(prefix.begin(), prefix.end());
    input.insert(input.end(), canonicalHeader.begin(), canonicalHeader.end());
    return chessSha256(input);
}

ChessSha256 extendReplayChain(
    const ChessSha256& previous,
    std::span<const std::uint8_t> canonicalAction,
    const ChessSha256& postStateHash,
    const ChessSha256& eventHash,
    const ChessSha256& rngDigest)
{
    std::vector<std::uint8_t> input;
    input.reserve(previous.size() + canonicalAction.size() + postStateHash.size() + eventHash.size() + rngDigest.size());
    input.insert(input.end(), previous.begin(), previous.end());
    input.insert(input.end(), canonicalAction.begin(), canonicalAction.end());
    input.insert(input.end(), postStateHash.begin(), postStateHash.end());
    input.insert(input.end(), eventHash.begin(), eventHash.end());
    input.insert(input.end(), rngDigest.begin(), rngDigest.end());
    return chessSha256(input);
}

}
