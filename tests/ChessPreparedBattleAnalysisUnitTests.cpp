#include "BattleSetupFactory.h"
#include "ChessGameSessionTestHelpers.h"
#include "ChessPreparedBattleAnalysis.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess;
using namespace KysChess::Test;

namespace
{

PreparedChessBattle analysisBattle()
{
    PreparedChessBattle prepared;
    prepared.kind = PreparedChessBattleKind::Campaign;
    prepared.stableBattleId = "campaign:1";
    prepared.chosenMapId = 7;
    prepared.mapCandidates = {7, 8};
    prepared.battleSeed = 123;
    prepared.units = {
        {1, 41, 10, 0, 1, -1, -1, 0},
        {2, 42, 10, 0, 2, 500, -1, 3},
        {3, -1, 20, 1, 1, -1, -1, 0},
    };
    prepared.formationSwaps.emplace_back(1, 2);
    return prepared;
}

}  // namespace

TEST_CASE("prepared battle projection shares resolved runtime formation without initializing combat",
          "[chess][prepared-analysis][projection]")
{
    const auto content = configuredMapChoiceContent();
    auto prepared = analysisBattle();
    prepared.units[0].x = 2;
    prepared.units[0].y = 3;
    prepared.units[1].x = 7;
    prepared.units[1].y = 8;

    const auto resolved = BattleSetupFactory::resolvePreparedFormation(prepared, *content);
    const auto projection = projectPreparedChessBattle(prepared, *content);
    const auto input = BattleSetupFactory::build(prepared, *content, {}, 36000);

    CHECK_FALSE(projection.combatInitialized);
    CHECK(projection.allySynergies.empty());
    REQUIRE(projection.units.size() == prepared.units.size());
    for (const auto& unit : projection.units)
    {
        const auto expected = std::ranges::find(
            resolved,
            unit.unitId,
            &PreparedChessBattleUnit::unitId);
        const auto runtimeInput = std::ranges::find(
            input.units,
            unit.unitId,
            &Battle::BattleSetupUnitInput::unitId);
        REQUIRE(expected != resolved.end());
        REQUIRE(runtimeInput != input.units.end());
        CHECK(unit.x == expected->x);
        CHECK(unit.y == expected->y);
        CHECK(unit.x == runtimeInput->gridX);
        CHECK(unit.y == runtimeInput->gridY);
        CHECK_FALSE(unit.initializedCombatStats);
    }
}

TEST_CASE("prepared battle analysis uses final formation and initialized runtime stats",
          "[chess][prepared-analysis][parity]")
{
    const auto content = configuredMapChoiceContent();
    const auto prepared = analysisBattle();
    const auto analysis = analyzePreparedChessBattle(prepared, *content, {}, 36000);

    REQUIRE(analysis.combatInitialized);
    CHECK(analysis.identity.stableBattleId == "campaign:1");
    CHECK(analysis.identity.chosenMapName == chessBattleMapDisplayName(*content, 7));
    REQUIRE(analysis.units.size() == 3);
    REQUIRE(analysis.allySynergies.size() == 1);
    CHECK(analysis.allySynergies.front().name == "配置選圖羈絆");
    CHECK(analysis.allySynergies.front().progressCount == "1+1/2 ✓");

    auto input = BattleSetupFactory::build(prepared, *content, {}, 36000);
    const auto directCoordinates = input.units;
    auto creation = Battle::BattleRuntimeSession::createInitialized(std::move(input));
    for (const auto& unit : analysis.units)
    {
        const auto direct = std::ranges::find(
            directCoordinates,
            unit.unitId,
            &Battle::BattleSetupUnitInput::unitId);
        REQUIRE(direct != directCoordinates.end());
        CHECK(unit.x == direct->gridX);
        CHECK(unit.y == direct->gridY);
        const auto& runtime = creation.session.requireRuntimeUnit(unit.unitId);
        REQUIRE(unit.initializedCombatStats);
        CHECK(unit.initializedCombatStats->maxHp == runtime.vitals.maxHp);
        CHECK(unit.initializedCombatStats->attack == runtime.stats.attack);
        CHECK(unit.initializedCombatStats->defence == runtime.stats.defence);
        CHECK(unit.initializedCombatStats->speed == runtime.stats.speed);
    }
}

TEST_CASE("prepared battle analysis before map choice exposes identity without inventing setup",
          "[chess][prepared-analysis]")
{
    const auto content = configuredMapChoiceContent();
    auto prepared = analysisBattle();
    prepared.chosenMapId = -1;
    prepared.formationSwaps.clear();
    const auto analysis = analyzePreparedChessBattle(prepared, *content, {}, 36000);

    CHECK_FALSE(analysis.combatInitialized);
    CHECK(analysis.identity.chosenMapName.empty());
    CHECK(analysis.identity.mapCandidates.size() == 2);
    CHECK(analysis.allySynergies.empty());
    CHECK(std::ranges::all_of(analysis.units, [](const auto& unit) {
        return !unit.initializedCombatStats.has_value();
    }));
}

TEST_CASE("prepared battle analysis is a pure session query", "[chess][prepared-analysis][purity]")
{
    const auto content = configuredMapChoiceContent();
    ChessGameSession session(content, 0x1234);
    REQUIRE(session.submitAndDrain(buySlot(0)).accepted);
    REQUIRE(session.submitAndDrain(buySlot(1)).accepted);
    REQUIRE(session.submitAndDrain(buySlot(2)).accepted);
    ChessAction deploy;
    deploy.type = ChessActionType::SetDeployment;
    deploy.chessInstanceIds = {session.state().roster.begin()->first};
    REQUIRE(session.submitAndDrain(deploy).accepted);
    ChessAction prepare;
    prepare.type = ChessActionType::PrepareBattle;
    REQUIRE(session.submitAndDrain(prepare).accepted);
    ChessAction map;
    map.type = ChessActionType::ChooseMap;
    map.mapId = session.state().preparedBattle->mapCandidates.front();
    REQUIRE(session.submitAndDrain(map).accepted);

    const auto stateBefore = session.state();
    const auto randomBefore = session.random().state();
    const auto decisionCountBefore = session.journal().decisions().size();
    const auto chainBefore = session.journal().chainHash();
    const auto hashBefore = session.observe().stateHash;
    const auto analysis = analyzePreparedChessBattle(
        *session.state().preparedBattle,
        session.content(),
        session.state().obtainedNeigongIds,
        session.state().options.battleFrameLimit);

    REQUIRE(analysis.combatInitialized);
    CHECK(session.state() == stateBefore);
    CHECK(session.random().state() == randomBefore);
    CHECK(session.journal().decisions().size() == decisionCountBefore);
    CHECK(session.journal().chainHash() == chainBefore);
    CHECK(session.observe().stateHash == hashBefore);
}
