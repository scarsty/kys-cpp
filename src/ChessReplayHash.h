#pragma once

#include <glaze/beve.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <tuple>

namespace KysChess
{

using ChessSha256 = std::array<std::uint8_t, 32>;
using ChessEvidenceHash = std::array<std::uint8_t, 16>;

ChessSha256 chessSha256(std::span<const std::uint8_t> bytes);
ChessSha256 chessSha256(std::string_view bytes);
std::string chessSha256Hex(const ChessSha256& hash);
ChessSha256 chessSha256FromHex(std::string_view hex);
std::string chessEvidenceHashHex(const ChessEvidenceHash& hash);
ChessEvidenceHash chessEvidenceHashFromHex(std::string_view hex);
ChessEvidenceHash chessEvidenceHash(const ChessSha256& hash);

template <typename... Values>
ChessSha256 chessBeveSha256(std::string_view domain, const Values&... values)
{
    constexpr std::uint16_t formatVersion = 1;
    const auto envelope = std::tuple{
        domain,
        formatVersion,
        std::tie(values...),
    };
    const auto encoded = glz::write_beve_untagged(envelope);
    return chessSha256(encoded.value());
}

template <typename... Values>
ChessEvidenceHash chessBeveEvidenceHash(std::string_view domain, const Values&... values)
{
    return chessEvidenceHash(chessBeveSha256(domain, values...));
}

}
