#include "ChessReplayHash.h"
#include "ChessReplayJournal.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

using namespace KysChess;

TEST_CASE("SHA-256 hexadecimal conversion is strict and reversible", "[chess][determinism][hash]")
{
    const std::array<std::uint8_t, 3> bytes{'a', 'b', 'c'};
    const auto hash = chessSha256(bytes);
    const auto hex = chessSha256Hex(hash);

    CHECK(hex == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(chessSha256FromHex(hex) == hash);
    CHECK_THROWS_AS(chessSha256FromHex("abc"), std::invalid_argument);
    CHECK_THROWS_AS(
        chessSha256FromHex("BA7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"),
        std::invalid_argument);
}

TEST_CASE("replay evidence uses the leading 128 bits of SHA-256", "[chess][determinism][hash]")
{
    const auto full = chessBeveSha256("TEST_EVIDENCE", 7, std::string{"payload"});
    const auto evidence = chessBeveEvidenceHash("TEST_EVIDENCE", 7, std::string{"payload"});
    const auto hex = chessEvidenceHashHex(evidence);

    STATIC_CHECK(ChessEvidenceHash{}.size() == 16);
    CHECK(std::equal(evidence.begin(), evidence.end(), full.begin()));
    CHECK(hex.size() == 32);
    CHECK(chessEvidenceHashFromHex(hex) == evidence);
    CHECK_THROWS_AS(chessEvidenceHashFromHex("abc"), std::invalid_argument);
    CHECK_THROWS_AS(chessEvidenceHashFromHex(std::string(64, '0')), std::invalid_argument);
}

TEST_CASE("BEVE hashing is domain separated", "[chess][determinism][hash]")
{
    CHECK(chessBeveSha256("DOMAIN_A", 42) == chessBeveSha256("DOMAIN_A", 42));
    CHECK(chessBeveSha256("DOMAIN_A", 42) != chessBeveSha256("DOMAIN_B", 42));
}

TEST_CASE("ordered containers make session hashing independent of insertion history", "[chess][determinism][hash][state]")
{
    ChessSessionState first;
    first.roster.emplace(2, ChessSessionPiece{2, 20, 1, false});
    first.roster.emplace(1, ChessSessionPiece{1, 10, 1, true});

    ChessSessionState second;
    second.roster.emplace(1, ChessSessionPiece{1, 10, 1, true});
    second.roster.emplace(2, ChessSessionPiece{2, 20, 1, false});

    ChessRunRandom random(11);
    CHECK(chessStateHash(first, random) == chessStateHash(second, random));
}

TEST_CASE("stored vector order and exact action payload remain authoritative", "[chess][determinism][hash][state]")
{
    ChessSessionState first;
    first.preparedBattle.emplace();
    first.preparedBattle->mapCandidates = {2, 1};
    auto second = first;
    second.preparedBattle->mapCandidates = {1, 2};

    ChessRunRandom random(12);
    CHECK(chessStateHash(first, random) != chessStateHash(second, random));

    ChessReplayHeader header{"1", "normal", ChessTalentId::DivineArms, 12, {}, {}};
    ChessReplayJournal firstJournal(header);
    ChessReplayJournal secondJournal(header);
    ChessAction firstAction;
    firstAction.type = ChessActionType::SetDeployment;
    firstAction.chessInstanceIds = {2, 1};
    auto secondAction = firstAction;
    secondAction.chessInstanceIds = {1, 2};
    const ChessSha256 componentHash{};
    firstJournal.append(
        ChessSessionPhase::Management,
        firstAction,
        componentHash,
        componentHash,
        componentHash,
        componentHash);
    secondJournal.append(
        ChessSessionPhase::Management,
        secondAction,
        componentHash,
        componentHash,
        componentHash,
        componentHash);
    CHECK(firstJournal.evidenceHash() != secondJournal.evidenceHash());
}

TEST_CASE("all stored state and RNG words participate in direct hashing", "[chess][determinism][hash][state]")
{
    ChessSessionState state;
    ChessRunRandom random(13);
    const auto initial = chessStateHash(state, random);

    state.selectedForcedBanCount = 1;
    CHECK(chessStateHash(state, random) != initial);
    state.selectedForcedBanCount = 0;

    state.options.battleFrameLimit += 1;
    CHECK(chessStateHash(state, random) != initial);
    state.options.battleFrameLimit -= 1;

    state.difficulty = Difficulty::Hard;
    const auto difficult = chessStateHash(state, random);
    CHECK(difficult != initial);

    auto randomState = random.state();
    randomState.streams[0].words[0] ^= 1;
    random.restore(randomState);
    CHECK(chessStateHash(state, random) != difficult);
    CHECK(chessRngDigest(random) != chessRngDigest(ChessRunRandom(13)));
}

TEST_CASE("semantic event detail participates in direct hashing", "[chess][determinism][hash][events]")
{
    ChessSemanticEvent event;
    event.type = ChessSemanticEventType::ChessMerged;
    event.primaryId = 3;
    event.merge.emplace();
    event.merge->consumedInstanceIds = {1, 2};
    const auto initial = chessEventHash({event});

    event.merge->recursiveMergeFollowed = true;
    CHECK(chessEventHash({event}) != initial);
}
