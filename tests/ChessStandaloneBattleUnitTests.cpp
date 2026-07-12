#include "BattleSetupFactory.h"
#include "ChessProgressionRules.h"
#include "ChessRuntimeConstants.h"
#include "ChessStandaloneBattle.h"

#include <catch2/catch_test_macros.hpp>

#include <format>
#include <memory>

using namespace KysChess;

namespace
{

std::shared_ptr<const ChessGameContent> standaloneContent()
{
    ChessGameContentData data;
    data.difficulty = Difficulty::Normal;
    data.balance.initialMoney = 20;
    data.balance.shopSlotCount = 1;
    data.balance.totalFights = 5;
    data.balance.minBattleSize = 1;
    data.balance.shopWeights[0] = {100, 0, 0, 0, 0};

    ChessRoleDefinition ally;
    ally.ID = 10;
    ally.Name = "獨立戰鬥我方";
    ally.Cost = 1;
    ally.MaxHP = 900;
    ally.MaxMP = 100;
    ally.Attack = 120;
    ally.Defence = 50;
    ally.Speed = 60;
    ally.MagicID[0] = 101;
    ally.MagicPower[0] = 100;
    data.roles.emplace(ally.ID, ally);

    ChessRoleDefinition enemy = ally;
    enemy.ID = 20;
    enemy.Name = "獨立戰鬥敵方";
    enemy.MaxHP = 300;
    enemy.Attack = 20;
    data.roles.emplace(enemy.ID, enemy);
    data.poolRoleIds = {ally.ID};

    for (const auto [id, power] : {std::pair{101, 100}, {102, 300}, {103, 200}})
    {
        ChessMagicDefinition magic;
        magic.ID = id;
        magic.Name = std::format("測試武學{}", id);
        magic.MagicType = 1;
        magic.AttackAreaType = 0;
        magic.SelectDistance = 4;
        data.magics.emplace(id, std::move(magic));
    }

    data.items.emplace(501, ChessItemDefinition{501, -1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, "測試武器"});
    data.items.emplace(502, ChessItemDefinition{502, -1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, "測試防具"});
    data.equipment.push_back({501, 1, 0});
    data.equipment.push_back({502, 2, 1});

    ComboDef combo;
    combo.id = 1;
    combo.name = "不應套用的羈絆";
    combo.memberRoleIds = {10};
    combo.thresholds.push_back({1, "啟用", {{EffectType::FlatATK, 999}}});
    data.combos.push_back(std::move(combo));

    ChessBattleMapDefinition map;
    map.id = 7;
    map.battlefieldId = 7;
    map.musicId = 12;
    for (int index = 0; index < 10; ++index)
    {
        map.teammateRoleIds.push_back(-1);
        map.teammateX.push_back(20 + index);
        map.teammateY.push_back(20);
    }
    for (int index = 0; index < 20; ++index)
    {
        map.enemyRoleIds.push_back(-1);
        map.enemyX.push_back(40 - index);
        map.enemyY.push_back(40);
    }
    data.battleMaps.emplace(map.id, std::move(map));
    ChessBattlefieldDefinition battlefield;
    battlefield.id = 7;
    battlefield.layers.assign(2 * 64 * 64, 0);
    data.battlefields.emplace(battlefield.id, std::move(battlefield));
    return std::make_shared<const ChessGameContent>(std::move(data));
}

ChessStandaloneBattleRequest basicRequest()
{
    ChessStandaloneBattleRequest request;
    request.rootSeed = 77;
    request.mapId = 7;
    request.battleSeed = 99;
    request.allies.push_back({10});
    request.enemies.push_back({20});
    return request;
}

}

TEST_CASE("ChessStandaloneBattle_ClassicProfileUsesCurrentRoleStatsWithoutAutoChessEffects", "[chess][standalone]")
{
    auto request = basicRequest();
    request.profile = ChessStandaloneBattleProfile::ClassicHades;
    RoleSave currentRole = *standaloneContent()->role(10);
    currentRole.MaxHP = 1337;
    currentRole.Attack = 246;
    currentRole.MagicID[0] = 101;
    currentRole.MagicPower[0] = 200;
    currentRole.MagicID[1] = 102;
    currentRole.MagicPower[1] = 50;
    currentRole.MagicID[2] = 103;
    currentRole.MagicPower[2] = 500;
    request.roleOverrides.emplace(10, currentRole);

    std::string error;
    auto built = ChessStandaloneBattle::prepare(standaloneContent(), request, error);
    REQUIRE(built);
    CHECK(error.empty());
    CHECK(built->preparedBattle.kind == PreparedChessBattleKind::Standalone);
    CHECK(built->preparedBattle.chosenMapId == 7);
    CHECK(built->preparedBattle.units[0].x == 20);
    CHECK(built->preparedBattle.units[1].x == 40);
    CHECK(built->content->combos().empty());
    CHECK(built->content->equipment().empty());

    const auto* overridden = built->content->role(10);
    REQUIRE(overridden);
    CHECK(overridden->MaxHP == 1337);
    CHECK(overridden->Attack == 246);
    CHECK(overridden->MagicID[0] == 102);
    CHECK(overridden->MagicPower[0] == 50);
    CHECK(overridden->MagicID[1] == 103);
    CHECK(overridden->MagicPower[1] == 500);

    const auto input = BattleSetupFactory::build(
        built->preparedBattle,
        *built->content,
        built->obtainedNeigongIds,
        kChessBattleFrameLimit);
    REQUIRE(input.units.size() == 2);
    CHECK(input.units[0].vitals.maxHp == 1337);
    CHECK(input.units[0].stats.attack == 246);
    CHECK(input.units[0].normalSkill.id == 102);
    CHECK(input.units[0].ultimateSkill.id == 103);
    CHECK(input.setup.comboDefinitions.empty());
}

TEST_CASE("ChessStandaloneBattle_SessionIsEphemeralAndHasNoCampaignProgression", "[chess][standalone]")
{
    std::string error;
    auto built = ChessStandaloneBattle::prepare(standaloneContent(), basicRequest(), error);
    REQUIRE(built);
    auto session = std::move(*built).createSession();
    CHECK(session->state().phase == ChessSessionPhase::BattlePreparation);
    CHECK(session->state().preparedBattle->kind == PreparedChessBattleKind::Standalone);
    CHECK_FALSE(session->exportReplay());

    ChessSessionState state;
    state.money = 44;
    state.fight = 3;
    state.phase = ChessSessionPhase::BattleResolution;
    state.preparedBattle = session->state().preparedBattle;
    HeadlessBattleResult battle;
    battle.summary.outcome = Battle::BattleOutcome::PlayerVictory;
    battle.summary.endFrame = 10;
    ChessRunRandom random(77);
    std::vector<ChessSemanticEvent> events;
    ChessProgressionRules::applyBattleResult(
        state,
        session->content(),
        random,
        battle,
        events);
    CHECK(state.phase == ChessSessionPhase::Management);
    CHECK_FALSE(state.preparedBattle);
    CHECK(state.money == 44);
    CHECK(state.fight == 3);
    REQUIRE(events.size() == 1);
    CHECK(events.front().type == ChessSemanticEventType::BattleEnded);
}
