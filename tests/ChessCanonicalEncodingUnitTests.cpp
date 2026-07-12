#include "ChessCanonicalEncoding.h"
#include "ChessReplayHash.h"
#include "ChessReplayJournal.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <stdexcept>
#include <vector>

using namespace KysChess;

namespace
{

std::vector<std::uint8_t> canonicalGoldenBytes()
{
    return {
        0x48, 0x44, 0x52, 0x31, 0x01, 0x00,
        0x01,
        0x34, 0x12,
        0xef, 0xcd, 0xab, 0x89,
        0xff, 0xff, 0xff, 0xff,
        0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
        0x03, 0x00, 0x00, 0x00, 0xe4, 0xb8, 0xad,
    };
}

std::string byteHex(std::span<const std::uint8_t> bytes)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string result(bytes.size() * 2, '0');
    for (std::size_t index = 0; index < bytes.size(); ++index)
    {
        result[index * 2] = digits[bytes[index] >> 4];
        result[index * 2 + 1] = digits[bytes[index] & 0x0f];
    }
    return result;
}

}

TEST_CASE("canonical writer matches the version-one byte layout", "[chess][determinism][canonical]")
{
    ChessCanonicalWriter writer("HDR1");
    writer.writeBool(true);
    writer.writeU16(0x1234);
    writer.writeU32(0x89abcdef);
    writer.writeI32(-1);
    writer.writeU64(0x0102030405060708ULL);
    writer.writeString("\xe4\xb8\xad");

    CHECK(writer.bytes() == canonicalGoldenBytes());
    CHECK(chessSha256Hex(chessSha256(writer.bytes())) ==
        "a8e2cbd49596803dae63f9fae94c7fc6e5ff42429b9b3f8886177e20f503a267");
}

TEST_CASE("canonical empty session state matches the frozen STA1 byte layout", "[chess][determinism][canonical][state]")
{
    ChessSessionState state;
    ChessRunRandom random(0);

    CHECK(byteHex(canonicalChessState(state, random)) ==
        "535441310100000000000000000000000000000000000000000000ffffffff0100000001000000000001a08c00000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000524e4731010000000000000000000100000000"
        "000000000000000000000002000000000000000000000000000000030000000000000000000000000000000400000000"
        "000000000000000000000005000000000000000000000000000000060000000000000000000000000000000700000000"
        "0000000000000000000000");
}

TEST_CASE("prepared RNG cache words are not canonical authority", "[chess][determinism][canonical][state]")
{
    ChessRunRandom random(11);
    ChessSessionState state;
    PreparedChessBattle prepared;
    prepared.preparationCheckpoint = random.checkpointPreparation();
    state.preparedBattle = prepared;
    const auto expected = canonicalChessStateHash(state, random);

    state.preparedBattle->preparationCheckpoint.preparationStreams[0].words[0] ^= 1;
    CHECK(canonicalChessStateHash(state, random) == expected);

    ++state.preparedBattle->preparationCheckpoint.preparationStreams[0].rawDrawCount;
    CHECK(canonicalChessStateHash(state, random) != expected);
}

TEST_CASE("difficulty and deployment are explicit canonical state", "[chess][determinism][canonical][state]")
{
    ChessRunRandom random(12);
    ChessSessionState state;
    state.roster.emplace(1, ChessSessionPiece{1, 10, 1, false});
    const auto undeployed = canonicalChessStateHash(state, random);

    state.roster.at(1).deployed = true;
    const auto deployed = canonicalChessStateHash(state, random);
    CHECK(deployed != undeployed);

    state.difficulty = Difficulty::Hard;
    CHECK(canonicalChessStateHash(state, random) != deployed);
}

TEST_CASE("canonical writer rejects malformed values", "[chess][determinism][canonical]")
{
    CHECK_THROWS_AS(ChessCanonicalWriter("BAD"), std::invalid_argument);

    ChessCanonicalWriter writer("STA1");
    const std::string overlongUtf8{"\xc0\x80", 2};
    const std::string surrogateUtf8{"\xed\xa0\x80", 3};
    const std::string truncatedUtf8{"\xe4\xb8", 2};
    CHECK_THROWS_AS(writer.writeString(overlongUtf8), std::invalid_argument);
    CHECK_THROWS_AS(writer.writeString(surrogateUtf8), std::invalid_argument);
    CHECK_THROWS_AS(writer.writeString(truncatedUtf8), std::invalid_argument);
}

TEST_CASE("SHA-256 hexadecimal conversion is strict and reversible", "[chess][determinism][canonical]")
{
    const auto hash = chessSha256(canonicalGoldenBytes());
    const auto hex = chessSha256Hex(hash);

    CHECK(chessSha256FromHex(hex) == hash);
    CHECK_THROWS_AS(chessSha256FromHex("abc"), std::invalid_argument);
    CHECK_THROWS_AS(
        chessSha256FromHex("A8e2cbd49596803dae63f9fae94c7fc6e5ff42429b9b3f8886177e20f503a267"),
        std::invalid_argument);
}

TEST_CASE("replay chain matches fixed version-one hashes", "[chess][determinism][canonical]")
{
    const auto previous = initialReplayChain(canonicalGoldenBytes());
    CHECK(chessSha256Hex(previous) ==
        "51f11baa43e60e120005c827398213eaa0fba479276fe95a8fb4fef6c1352c26");

    ChessCanonicalWriter action("ACT1");
    action.writeU16(2);
    const std::array<std::uint8_t, 4> postText{'p', 'o', 's', 't'};
    const std::array<std::uint8_t, 6> eventText{'e', 'v', 'e', 'n', 't', 's'};
    const std::array<std::uint8_t, 3> rngText{'r', 'n', 'g'};
    const auto chain = extendReplayChain(
        previous,
        action.bytes(),
        chessSha256(postText),
        chessSha256(eventText),
        chessSha256(rngText));

    CHECK(chessSha256Hex(chain) ==
        "ec1179d338fa470efd5d3e823508c16a5df30e1649d7a149b62440dc9e200229");
}
