#include "ChessGameSessionTestHelpers.h"
#include "ChessTournament.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <ranges>

using namespace KysChess;

namespace
{

ChessTournamentSubmission submission(
    std::string name,
    int roleId,
    int chessInstanceId)
{
    ChessTournamentSubmission result;
    result.name = std::move(name);
    result.source = result.name + ".json";
    result.payloadHash = chessSha256(result.source);
    result.composition.pieces.push_back({
        chessInstanceId,
        roleId,
        1,
        -1,
        -1,
        0,
    });
    result.composition.formationSlots.assign(kChessFormationSlotCount, -1);
    result.composition.formationSlots.front() = chessInstanceId;
    return result;
}

} // namespace

TEST_CASE("tournament battle seeds are deterministic and support stable offsets",
    "[chess][tournament][seed]")
{
    const auto all = ChessTournamentRunner::deriveBattleSeeds(0x12345678, 6);
    const auto first = ChessTournamentRunner::deriveBattleSeeds(0x12345678, 3);
    const auto second = ChessTournamentRunner::deriveBattleSeeds(0x12345678, 3, 3);

    REQUIRE(all.size() == 6);
    CHECK(std::ranges::equal(first, all | std::views::take(3)));
    CHECK(std::ranges::equal(second, all | std::views::drop(3)));
    CHECK(ChessTournamentRunner::deriveBattleSeeds(0x12345678, 6) == all);
    CHECK(ChessTournamentRunner::deriveBattleSeeds(0x12345679, 6) != all);
}

TEST_CASE("tournament scoring applies the player-one timeout and simultaneous-wipe rules",
    "[chess][tournament][scoring]")
{
    BattleSummary summary;
    summary.outcome = Battle::BattleOutcome::Timeout;
    CHECK(chessTournamentBattleOutcome(summary) == ChessTournamentBattleOutcome::TeamOneVictory);

    summary.outcome = Battle::BattleOutcome::PlayerVictory;
    CHECK(chessTournamentBattleOutcome(summary) == ChessTournamentBattleOutcome::TeamZeroVictory);

    summary.survivors.push_back({ 1, 1, 1, 0, 10, 0, false });
    CHECK(chessTournamentBattleOutcome(summary)
        == ChessTournamentBattleOutcome::TeamZeroVictory);

    summary.outcome = Battle::BattleOutcome::PlayerDefeat;
    CHECK(chessTournamentBattleOutcome(summary)
        == ChessTournamentBattleOutcome::TeamOneVictory);
}

TEST_CASE("round robin shares every seed and mirrors both sides for every pair",
    "[chess][tournament][round_robin]")
{
    const auto content = Test::syntheticContent(Difficulty::Hard);
    REQUIRE(content);
    REQUIRE(content->roles().size() >= 3);
    auto role = content->roles().begin();
    const int firstRoleId = role++->first;
    const int secondRoleId = role++->first;
    const int thirdRoleId = role->first;

    std::vector submissions{
        submission("甲", firstRoleId, 1001),
        submission("乙", secondRoleId, 2001),
        submission("丙", thirdRoleId, 3001),
    };
    ChessTournamentOptions options;
    options.seed = 987654321;
    options.battleSeedCount = 2;
    std::string error;
    const auto result = ChessTournamentRunner::run(
        content,
        submissions,
        options,
        error);

    INFO(error);
    REQUIRE(result);
    const auto& roundRobin = result->roundRobin;
    CHECK(roundRobin.battleSeeds
        == ChessTournamentRunner::deriveBattleSeeds(options.seed, 2));
    REQUIRE(roundRobin.pairings.size() == 3);
    REQUIRE(roundRobin.battles.size() == 12);

    for (int pairingIndex = 0; pairingIndex < 3; ++pairingIndex)
    {
        const auto& pairing = roundRobin.pairings[pairingIndex];
        for (int seedOffset = 0; seedOffset < 2; ++seedOffset)
        {
            std::vector<ChessTournamentBattleRecord> legs;
            for (const auto& battle : roundRobin.battles)
            {
                if (battle.pairingIndex == pairingIndex
                    && battle.seedSequenceIndex == seedOffset)
                {
                    legs.push_back(battle);
                }
            }
            REQUIRE(legs.size() == 2);
            CHECK(legs[0].battleSeed == roundRobin.battleSeeds[seedOffset]);
            CHECK(legs[1].battleSeed == roundRobin.battleSeeds[seedOffset]);
            CHECK(legs[0].teamZeroEntrantIndex == pairing.firstEntrantIndex);
            CHECK(legs[0].teamOneEntrantIndex == pairing.secondEntrantIndex);
            CHECK(legs[1].teamZeroEntrantIndex == pairing.secondEntrantIndex);
            CHECK(legs[1].teamOneEntrantIndex == pairing.firstEntrantIndex);
        }
    }

    const auto repeated = ChessTournamentRunner::run(
        content,
        std::move(submissions),
        options,
        error);
    REQUIRE(repeated);
    CHECK(repeated->roundRobin.battles == roundRobin.battles);
    const auto json = serializeChessTournamentResultJson(*result);
    CHECK(json.find('\n') != std::string::npos);
    CHECK(json.find("\"entrant_order\": \"utf8_filename_ascending\"") != std::string::npos);
    CHECK(json.find("\"side_assignment\": \"leg_0_first_entrant_is_team_zero; leg_1_swaps_sides\"") != std::string::npos);
}

TEST_CASE("tied leaders are co-champions without additional battles",
    "[chess][tournament][co_champion]")
{
    const auto content = Test::syntheticContent(Difficulty::Hard);
    REQUIRE(content);
    REQUIRE_FALSE(content->roles().empty());
    const int roleId = content->roles().begin()->first;

    std::vector submissions{
        submission("甲", roleId, 1001),
        submission("乙", roleId, 1001),
    };
    ChessTournamentOptions options;
    options.seed = 246813579;
    options.battleSeedCount = 1;
    std::string error;
    const auto result = ChessTournamentRunner::run(
        content,
        std::move(submissions),
        options,
        error);

    INFO(error);
    REQUIRE(result);
    REQUIRE(result->roundRobin.battles.size() == 2);
    REQUIRE(result->standings.size() == 2);
    CHECK(result->standings[0].rank == 1);
    CHECK(result->standings[1].rank == 1);
    CHECK(result->standings[0].halfPoints == result->standings[1].halfPoints);
    CHECK(result->championEntrantIndices == std::vector{0, 1});
}
