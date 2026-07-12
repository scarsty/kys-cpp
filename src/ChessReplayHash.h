#pragma once

#include "ChessCanonicalEncoding.h"

#include <span>
#include <string>
#include <string_view>

namespace KysChess
{

ChessSha256 chessSha256(std::span<const std::uint8_t> bytes);
std::string chessSha256Hex(const ChessSha256& hash);
ChessSha256 chessSha256FromHex(std::string_view hex);

ChessSha256 initialReplayChain(std::span<const std::uint8_t> canonicalHeader);
ChessSha256 extendReplayChain(
    const ChessSha256& previous,
    std::span<const std::uint8_t> canonicalAction,
    const ChessSha256& postStateHash,
    const ChessSha256& eventHash,
    const ChessSha256& rngDigest);

}
