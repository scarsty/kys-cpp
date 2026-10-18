#include "BattleSetupFactory.h"
#include "ChessGameSessionTestHelpers.h"
#include "ChessPreparedBattleAnalysis.h"
#include "ChessBattleResultModel.h"
#include "BattleLogTestHelpers.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess;
using namespace KysChess::Test;

TEST_CASE("battle result retains both dense rosters and ranks skill contributions", "[chess][result]")
{
    const auto content = configuredMapChoiceContent();
    PreparedChessBattle prepared;
    prepared.chosenMapId = 7;
    prepared.units = {{1, 1, 10, 0, 2, 500}, {2, 2, 20, 1}};
    BattlePostBattleSummary summary;
    for (int side = 0; side < 2; ++side)
        for (int index = 0; index < 13; ++index)
        {
            BattlePostBattleUnitSummary unit;
            unit.identity = {side * 100 + index, side == 0 ? 10 : 20, side, 0, "測試棋子"};
            unit.star = 3;
            unit.weaponId = 500;
            unit.dead = index != 0;
            unit.hpRemaining = index == 0 ? 77 : 0;
            unit.maxHpRemaining = 200;
            unit.cancelDmg = index;
            (side == 0 ? summary.allies : summary.enemies).push_back(unit);
        }
    auto attacker = BattleLogTest::reportUnit(1, 10, 0, 0, "測試棋子");
    auto defender = BattleLogTest::reportUnit(100, 20, 1, 0, "測試棋子");
    BattleReportBuilder builder;
    builder.recordDamage(&attacker, &defender, 10, "第一招", 30, {}, 1);
    builder.recordDamage(&attacker, &defender, 30, "第二招", 60, {}, 2);
    builder.recordDamage(&attacker, &defender, 20, "第三招", 90, {}, 3);
    builder.recordDamage(&attacker, &defender, 25, "第一招", 120, {}, 1);
    builder.recordKill(&attacker, &defender, 120);
    builder.recordDeath(&attacker, 180);
    builder.recordBattleEnd(600, 0);
    const auto teams = buildChessBattleResultTeams(summary, builder.report(), prepared, *content,
        {ChessTalentId::Backbone, ChessTalentId::Alliance});
    CHECK(teams[0].talent == ChessTalentId::Backbone);
    CHECK(teams[1].talent == ChessTalentId::Alliance);
    for (const auto& team : teams)
    {
        REQUIRE(team.units.size() == 13);
        CHECK(team.survivors == 1);
        CHECK(team.units.back().weaponName == "配置選圖劍");
    }
    const auto& first = teams[0].units.front();
    CHECK(first.unit.identity.battleId == 1);
    CHECK(first.unit.dead);
    CHECK(first.unit.cancelDmg == 1);
    CHECK(first.unit.star == 3);
    CHECK(first.damageDealt == 85);
    CHECK(first.dps == 28);
    CHECK(first.kills == 1);
    CHECK(first.skillNames[0] == "快速測試武學");
    CHECK(first.skillDamage == std::array<int, 2>{35, 30});
    CHECK(teams[0].damageDealt == 85);
    CHECK(teams[1].units.front().damageTaken == 85);
    REQUIRE(teams[0].synergies.size() == 1);
    CHECK(teams[0].synergies[0].active);
    CHECK(teams[0].synergies[0].effectiveCount == 2);
}

TEST_CASE("battle result omits synergies below the first threshold", "[chess][result]")
{
    const auto content = configuredMapChoiceContent();
    PreparedChessBattle prepared;
    prepared.chosenMapId = 7;
    prepared.units = {{1, 1, 10, 0}, {2, 2, 20, 1}};
    BattlePostBattleSummary summary;
    BattlePostBattleUnitSummary dead;
    dead.identity = {1, 10, 0, 0, "測試棋子"};
    dead.dead = true;
    summary.allies.push_back(dead);
    const auto teams = buildChessBattleResultTeams(summary, {}, prepared, *content, {});
    CHECK_FALSE(teams[0].talent);
    CHECK_FALSE(teams[1].talent);
    CHECK(teams[0].survivors == 0);
    CHECK(teams[0].synergies.empty());
    CHECK(teams[0].units[0].dps == 0);
    CHECK(teams[0].units[0].skillNames[0].empty());
}

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
    const auto input = BattleSetupFactory::build(prepared, *content, kChessBattleFrameLimit);

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
    const auto analysis = analyzePreparedChessBattle(prepared, *content, kChessBattleFrameLimit);

    REQUIRE(analysis.combatInitialized);
    CHECK(analysis.identity.stableBattleId == "campaign:1");
    CHECK(analysis.identity.chosenMapName == chessBattleMapDisplayName(*content, 7));
    REQUIRE(analysis.units.size() == 3);
    REQUIRE(analysis.allySynergies.size() == 1);
    CHECK(analysis.allySynergies.front().name == "配置選圖羈絆");
    CHECK(analysis.allySynergies.front().progressCount == "1+1/2 ✓");

    auto input = BattleSetupFactory::build(prepared, *content, kChessBattleFrameLimit);
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

TEST_CASE("prepared battle analysis can omit unconsumed effect descriptions",
          "[chess][prepared-analysis][projection][description]")
{
    const auto content = configuredMapChoiceContent();
    const auto analysis = analyzePreparedChessBattle(
        analysisBattle(),
        *content,
        kChessBattleFrameLimit,
        std::nullopt);

    REQUIRE(analysis.combatInitialized);
    REQUIRE_FALSE(analysis.allySynergies.empty());
    CHECK(std::ranges::all_of(analysis.units, [](const auto& unit) {
        return unit.abilities.empty();
    }));
    for (const auto& synergy : analysis.allySynergies)
    {
        REQUIRE_FALSE(synergy.thresholds.empty());
        CHECK(std::ranges::all_of(synergy.thresholds, [](const auto& threshold) {
            return threshold.effects.sections.empty();
        }));
    }
}

TEST_CASE("prepared battle analysis before map choice exposes identity without inventing setup",
          "[chess][prepared-analysis]")
{
    const auto content = configuredMapChoiceContent();
    auto prepared = analysisBattle();
    prepared.chosenMapId = -1;
    prepared.formationSwaps.clear();
    const auto analysis = analyzePreparedChessBattle(prepared, *content, kChessBattleFrameLimit);

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
    const auto evidenceBefore = session.journal().evidenceHash();
    const auto hashBefore = session.observe().stateHash;
    const auto analysis = analyzePreparedChessBattle(
        *session.state().preparedBattle,
        session.content(),
        session.state().options.battleFrameLimit);

    REQUIRE(analysis.combatInitialized);
    CHECK(session.state() == stateBefore);
    CHECK(session.random().state() == randomBefore);
    CHECK(session.journal().decisions().size() == decisionCountBefore);
    CHECK(session.journal().evidenceHash() == evidenceBefore);
    CHECK(session.observe().stateHash == hashBefore);
}
