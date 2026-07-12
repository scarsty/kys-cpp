#include "BattleSetupFactory.h"
#include "BattleSceneUnitStore.h"
#include "ChessBattleMapCatalog.h"
#include "ChessBattlePlanner.h"
#include "ChessCombo.h"
#include "ChessGameSessionTestHelpers.h"
#include "HeadlessBattleRunner.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <format>
#include <set>
#include <tuple>

using namespace KysChess;

namespace
{

std::shared_ptr<const ChessGameContent> battleContent()
{
    ChessGameContentData data;
    data.difficulty = Difficulty::Normal;
    data.balance.minBattleSize = 1;
    data.balance.enemyTable = {{{1, 1}}};
    ChessRoleDefinition ally;
    ally.ID = 10;
    ally.Name = "我方";
    ally.Cost = 1;
    ally.MaxHP = 100;
    ally.Attack = 10;
    ally.Defence = 5;
    data.roles.emplace(ally.ID, ally);
    ChessRoleDefinition enemy = ally;
    enemy.ID = 20;
    enemy.Name = "敵方";
    data.roles.emplace(enemy.ID, enemy);
    data.poolRoleIds = {20};

    ChessBattleMapDefinition map;
    map.id = 7;
    map.battlefieldId = 3;
    map.teammateRoleIds = std::vector<int>(10, 0);
    map.enemyRoleIds = std::vector<int>(20, 0);
    for (int index = 0; index < 10; ++index)
    {
        map.teammateX.push_back(20 + index);
        map.teammateY.push_back(20);
    }
    for (int index = 0; index < 20; ++index)
    {
        map.enemyX.push_back(40 - index);
        map.enemyY.push_back(40);
    }
    data.battleMaps.emplace(map.id, map);
    ChessBattlefieldDefinition field;
    field.id = 3;
    field.layers.assign(2 * 64 * 64, 0);
    data.battlefields.emplace(field.id, std::move(field));
    return std::make_shared<const ChessGameContent>(std::move(data));
}

ChessSessionState deployedState()
{
    ChessSessionState state;
    ChessSessionPiece piece;
    piece.instanceId = 1;
    piece.roleId = 10;
    piece.deployed = true;
    state.roster.emplace(piece.instanceId, piece);
    state.nextChessInstanceId = 2;
    return state;
}

void addPlannerRole(ChessGameContentData& data, int roleId, int tier, std::string name)
{
    ChessRoleDefinition role;
    role.ID = roleId;
    role.Name = std::move(name);
    role.Cost = tier;
    role.MaxHP = 100;
    role.Attack = 10;
    role.Defence = 5;
    data.roles.emplace(role.ID, std::move(role));
}

void addPlannerMap(ChessGameContentData& data)
{
    ChessBattleMapDefinition map;
    map.id = 7;
    map.battlefieldId = 3;
    map.teammateRoleIds = std::vector<int>(10, -1);
    map.enemyRoleIds = std::vector<int>(20, -1);
    for (int index = 0; index < 10; ++index)
    {
        map.teammateX.push_back(20 + index);
        map.teammateY.push_back(20);
    }
    for (int index = 0; index < 20; ++index)
    {
        map.enemyX.push_back(40 - index);
        map.enemyY.push_back(40);
    }
    data.battleMaps.emplace(map.id, std::move(map));
    ChessBattlefieldDefinition field;
    field.id = 3;
    field.layers.assign(2 * 64 * 64, 0);
    data.battlefields.emplace(field.id, std::move(field));
}

std::shared_ptr<const ChessGameContent> enemySynergyContent(
    Difficulty difficulty,
    bool noSynergyFight,
    bool reversePool = false)
{
    ChessGameContentData data;
    data.difficulty = difficulty;
    data.balance.minBattleSize = 1;
    data.balance.enemyTable = {{
        {1, 1},
        {1, 1},
        {2, 1},
        {2, 1},
    }};
    if (noSynergyFight)
    {
        data.balance.noSynergyFights = {1};
    }
    addPlannerRole(data, 10, 1, "我方");
    for (const auto [roleId, tier] : std::vector<std::pair<int, int>>{
             {101, 1}, {102, 1}, {111, 1}, {112, 1},
             {201, 2}, {202, 2}, {211, 2}, {212, 2},
         })
    {
        addPlannerRole(data, roleId, tier, std::format("敵方{}", roleId));
        data.poolRoleIds.push_back(roleId);
    }
    if (reversePool)
    {
        std::ranges::reverse(data.poolRoleIds);
    }

    ComboDef combo;
    combo.id = 0;
    combo.name = "敵方組合";
    combo.memberRoleIds = {101, 102, 201, 202};
    combo.thresholds.push_back({3, "成形", {{EffectType::FlatATK, 10}}});
    data.combos.push_back(std::move(combo));
    addPlannerMap(data);
    return std::make_shared<const ChessGameContent>(std::move(data));
}

std::shared_ptr<const ChessGameContent> enemyEquipmentContent(bool equipBoth)
{
    ChessGameContentData data;
    data.difficulty = Difficulty::Easy;
    data.balance.minBattleSize = 1;
    data.balance.enemyTable = {{{1, 3}, {1, 3}, {1, 2}}};
    data.balance.enemyEquipmentLevels.push_back({1, 4, 1, equipBoth});
    addPlannerRole(data, 10, 1, "我方");
    for (const int roleId : {101, 102, 103, 104})
    {
        addPlannerRole(data, roleId, 1, std::format("敵方{}", roleId));
        data.poolRoleIds.push_back(roleId);
    }
    data.equipment.push_back({501, 1, 0});
    data.equipment.push_back({502, 1, 1});
    addPlannerMap(data);
    return std::make_shared<const ChessGameContent>(std::move(data));
}

std::vector<const PreparedChessBattleUnit*> enemyUnits(const PreparedChessBattle& prepared)
{
    std::vector<const PreparedChessBattleUnit*> result;
    for (const auto& unit : prepared.units)
    {
        if (unit.team == 1)
        {
            result.push_back(&unit);
        }
    }
    return result;
}

bool enemyComboActive(const PreparedChessBattle& prepared, const ChessGameContent& content)
{
    ChessSessionState state;
    int instanceId = 1;
    for (const auto* unit : enemyUnits(prepared))
    {
        ChessSessionPiece piece;
        piece.instanceId = instanceId++;
        piece.roleId = unit->roleId;
        piece.star = unit->star;
        piece.deployed = true;
        state.roster.emplace(piece.instanceId, piece);
    }
    return evaluateChessComboProgress(state, content, content.combos().front()).active;
}

}

TEST_CASE("battle planning captures the complete pre-draw checkpoint", "[chess][planner][determinism]")
{
    const auto content = battleContent();
    const auto state = deployedState();
    ChessRunRandom random(123);
    const auto checkpoint = random.checkpointPreparation();

    const auto prepared = ChessBattlePlanner::prepareCampaign(state, *content, random);

    CHECK(prepared.preparationCheckpoint == checkpoint);
    REQUIRE(prepared.units.size() == 2);
    CHECK(prepared.units[0].unitId == 1);
    CHECK(prepared.units[1].unitId == 2);
    CHECK(prepared.units[0].x == 20);
    CHECK(prepared.units[0].y == 20);
    CHECK(prepared.units[1].x == 40);
    CHECK(prepared.units[1].y == 40);
    CHECK(prepared.mapCandidates == std::vector<int>{7});
    CHECK(prepared.chosenMapId == 7);
    CHECK(random.streamState(ChessRngStream::EnemyLineup).rawDrawCount > 0);
    CHECK(random.streamState(ChessRngStream::MapSelection).rawDrawCount == 1);
    CHECK(random.streamState(ChessRngStream::BattleSeed).rawDrawCount > 0);
}

TEST_CASE("configured map choice effect controls whether preparation requires a decision", "[chess][planner][map][config]")
{
    const auto content = Test::configuredMapChoiceContent();

    SECTION("inactive configured threshold selects a fitting map automatically")
    {
        ChessSessionState state;
        state.roster.emplace(1, ChessSessionPiece{1, 10, 1, true});
        ChessRunRandom random(100);

        const auto prepared = ChessBattlePlanner::prepareCampaign(state, *content, random);

        CHECK(prepared.mapCandidates == std::vector<int>{7, 8});
        CHECK(std::ranges::contains(prepared.mapCandidates, prepared.chosenMapId));
        CHECK(random.streamState(ChessRngStream::MapSelection).rawDrawCount == 1);
    }

    SECTION("active configured threshold leaves the explicit decision pending")
    {
        ChessSessionState state;
        state.roster.emplace(1, ChessSessionPiece{1, 10, 2, true});
        ChessRunRandom random(100);

        const auto prepared = ChessBattlePlanner::prepareCampaign(state, *content, random);

        CHECK(prepared.mapCandidates == std::vector<int>{7, 8});
        CHECK(prepared.chosenMapId == -1);
        CHECK(random.streamState(ChessRngStream::MapSelection).rawDrawCount == 0);
    }

    SECTION("configured equipment substitution contributes to the threshold")
    {
        ChessSessionState state;
        state.roster.emplace(1, ChessSessionPiece{1, 10, 1, true});
        ChessSessionPiece proxy{2, 30, 1, true};
        proxy.weaponInstanceId = 1;
        state.roster.emplace(proxy.instanceId, proxy);
        state.equipmentInventory.emplace(1, ChessEquipmentInstance{1, 500, proxy.instanceId});

        CHECK(chessRosterHasActiveComboEffect(
            state,
            *content,
            EffectType::BattleMapChoice));

        ChessRunRandom random(100);
        const auto prepared = ChessBattlePlanner::prepareCampaign(state, *content, random);

        CHECK(prepared.mapCandidates == std::vector<int>{7, 8});
        CHECK(prepared.chosenMapId == -1);
        CHECK(random.streamState(ChessRngStream::MapSelection).rawDrawCount == 0);
    }
}

TEST_CASE("reroll rekeys every preparation result deterministically", "[chess][planner][determinism]")
{
    const auto content = battleContent();
    const auto state = deployedState();
    ChessRunRandom first(456);
    ChessRunRandom second(456);
    first.rerollEnemySeed();
    second.rerollEnemySeed();

    const auto a = ChessBattlePlanner::prepareCampaign(state, *content, first);
    const auto b = ChessBattlePlanner::prepareCampaign(state, *content, second);

    CHECK(a == b);
    CHECK(first.enemyPlanKey() == second.enemyPlanKey());
}

TEST_CASE("normal and hard enemy planning restores synergy bias and configured anti-synergy fights", "[chess][planner][enemy][synergy]")
{
    for (const auto difficulty : {Difficulty::Normal, Difficulty::Hard})
    {
        const auto regular = enemySynergyContent(difficulty, false);
        const auto antiSynergy = enemySynergyContent(difficulty, true);
        int regularActive = 0;
        int antiSynergyActive = 0;
        for (std::uint64_t seed = 1; seed <= 64; ++seed)
        {
            ChessRunRandom regularRandom(seed);
            ChessRunRandom antiRandom(seed);
            const auto regularPrepared = ChessBattlePlanner::prepareCampaign(
                deployedState(),
                *regular,
                regularRandom);
            const auto antiPrepared = ChessBattlePlanner::prepareCampaign(
                deployedState(),
                *antiSynergy,
                antiRandom);
            regularActive += enemyComboActive(regularPrepared, *regular) ? 1 : 0;
            antiSynergyActive += enemyComboActive(antiPrepared, *antiSynergy) ? 1 : 0;

            const auto enemies = enemyUnits(regularPrepared);
            std::set<int> uniqueRoleIds;
            for (const auto* enemy : enemies)
            {
                uniqueRoleIds.insert(enemy->roleId);
            }
            CHECK(uniqueRoleIds.size() == enemies.size());
        }
        CHECK(regularActive > antiSynergyActive);
    }
}

TEST_CASE("easy enemy planning ignores no-synergy tuning while configured pool ordering stays canonical", "[chess][planner][enemy][determinism]")
{
    const auto easy = enemySynergyContent(Difficulty::Easy, false);
    const auto easyNoSynergy = enemySynergyContent(Difficulty::Easy, true);
    const auto reversedPool = enemySynergyContent(Difficulty::Easy, false, true);
    ChessRunRandom first(314159);
    ChessRunRandom second(314159);
    ChessRunRandom third(314159);

    const auto normalOrder = ChessBattlePlanner::prepareCampaign(deployedState(), *easy, first);
    const auto ignoredTuning = ChessBattlePlanner::prepareCampaign(deployedState(), *easyNoSynergy, second);
    const auto reversed = ChessBattlePlanner::prepareCampaign(deployedState(), *reversedPool, third);

    const auto roleIds = [](const PreparedChessBattle& prepared) {
        std::vector<int> result;
        for (const auto* unit : enemyUnits(prepared))
        {
            result.push_back(unit->roleId);
        }
        return result;
    };
    CHECK(roleIds(normalOrder) == roleIds(ignoredTuning));
    CHECK(roleIds(normalOrder) == roleIds(reversed));
}

TEST_CASE("enemy equipment restores single-slot type sampling and source-order targeting", "[chess][planner][enemy][equipment]")
{
    const auto content = enemyEquipmentContent(false);
    bool sawWeapon = false;
    bool sawArmor = false;
    for (std::uint64_t seed = 1; seed <= 64; ++seed)
    {
        ChessRunRandom random(seed);
        const auto prepared = ChessBattlePlanner::prepareCampaign(deployedState(), *content, random);
        const auto enemies = enemyUnits(prepared);
        REQUIRE(enemies.size() == 3);

        int equippedCount = 0;
        for (const auto* enemy : enemies)
        {
            const int itemCount = (enemy->weaponItemId >= 0 ? 1 : 0)
                + (enemy->armorItemId >= 0 ? 1 : 0);
            CHECK(itemCount <= 1);
            equippedCount += itemCount;
        }
        CHECK(equippedCount == 1);
        CHECK(enemies[0]->unitId < enemies[1]->unitId);
        CHECK((enemies[0]->weaponItemId >= 0) != (enemies[0]->armorItemId >= 0));
        CHECK(enemies[1]->weaponItemId == -1);
        CHECK(enemies[1]->armorItemId == -1);
        sawWeapon = sawWeapon || enemies[0]->weaponItemId == 501;
        sawArmor = sawArmor || enemies[0]->armorItemId == 502;
    }
    CHECK(sawWeapon);
    CHECK(sawArmor);
}

TEST_CASE("enemy dual-equipment levels equip both slots on the highest-priority enemy", "[chess][planner][enemy][equipment]")
{
    const auto content = enemyEquipmentContent(true);
    ChessRunRandom random(7);
    const auto prepared = ChessBattlePlanner::prepareCampaign(deployedState(), *content, random);
    const auto enemies = enemyUnits(prepared);

    REQUIRE(enemies.size() == 3);
    CHECK(enemies[0]->weaponItemId == 501);
    CHECK(enemies[0]->armorItemId == 502);
    CHECK(enemies[1]->weaponItemId == -1);
    CHECK(enemies[1]->armorItemId == -1);
}

TEST_CASE("setup factory applies formation before runtime initialization", "[chess][planner][battle][determinism]")
{
    const auto content = battleContent();
    ChessRunRandom random(789);
    auto prepared = ChessBattlePlanner::prepareCampaign(deployedState(), *content, random);
    prepared.formationSwaps.emplace_back(1, 1);
    prepared.formationSwaps.clear();

    const auto first = BattleSetupFactory::build(prepared, *content, {}, 1);
    const auto second = BattleSetupFactory::build(prepared, *content, {}, 1);

    REQUIRE(first.units.size() == second.units.size());
    CHECK(first.units[0].unitId == second.units[0].unitId);
    CHECK(first.units[0].gridX == second.units[0].gridX);
    CHECK(first.units[1].gridY == second.units[1].gridY);
    CHECK(first.units[0].faceTowards == Towards_LeftDown);
    CHECK(first.units[0].motion.facing.x == -1);
    CHECK(first.units[0].motion.facing.y == 1);
    CHECK(first.units[1].faceTowards == Towards_RightUp);
    CHECK(first.units[1].motion.facing.x == 1);
    CHECK(first.units[1].motion.facing.y == -1);
    const auto firstResult = HeadlessBattleRunner::run(first);
    const auto secondResult = HeadlessBattleRunner::run(second);
    CHECK(firstResult.digest == secondResult.digest);
    CHECK(firstResult.summary.outcome == Battle::BattleOutcome::Timeout);
}

TEST_CASE("actual auto-chess map catalog preserves curated selectable battlefields", "[chess][planner][battle][map]")
{
    const auto content = Test::actualContent();
    REQUIRE(content);

    CHECK(ChessBattleMapCatalog::fittingMapIds(*content, 2, 2)
        == std::vector<int>{6, 13, 17, 21, 24, 26, 54, 56, 60, 80});
    CHECK(ChessBattleMapCatalog::fittingMapIds(*content, 10, 20)
        == std::vector<int>{13, 56});
    CHECK(ChessBattleMapCatalog::displayName(60) == "散陣遭遇");
}

TEST_CASE("non-curated map catalogs fall back to the largest combined capacity", "[chess][planner][battle][map]")
{
    ChessGameContentData data;
    for (const auto [id, allyCapacity, enemyCapacity] : std::vector<std::tuple<int, int, int>>{
             {701, 2, 3},
             {702, 4, 5},
             {703, 3, 4},
         })
    {
        ChessBattleMapDefinition map;
        map.id = id;
        map.teammateX.resize(allyCapacity);
        map.teammateY.resize(allyCapacity);
        map.enemyX.resize(enemyCapacity);
        map.enemyY.resize(enemyCapacity);
        data.battleMaps.emplace(id, std::move(map));
    }
    const ChessGameContent content(std::move(data));

    CHECK(ChessBattleMapCatalog::fittingMapIds(content, 8, 8) == std::vector<int>{702});
}

TEST_CASE("selected curated map owns terrain formation clone cells and moving runtime", "[chess][planner][battle][map][movement]")
{
    const auto content = Test::actualContent();
    REQUIRE(content);

    PreparedChessBattle prepared;
    prepared.chosenMapId = 60;
    prepared.battleSeed = 0x60;
    prepared.units = {
        {.unitId = 1, .chessInstanceId = 1, .roleId = 4, .team = 0, .star = 1},
        {.unitId = 2, .roleId = 160, .team = 1, .star = 1},
    };

    auto input = BattleSetupFactory::build(prepared, *content, {}, 600);
    REQUIRE(input.units.size() == 2);
    CHECK(input.units[0].gridX == 32);
    CHECK(input.units[0].gridY == 20);
    CHECK(input.units[1].gridX == 21);
    CHECK(input.units[1].gridY == 23);
    CHECK(input.rules.movementCollisionWorld.walkableByCell[20 * BattlefieldData::CoordinateCount + 32] == 1);
    CHECK(input.rules.movementCollisionWorld.walkableByCell[23 * BattlefieldData::CoordinateCount + 21] == 1);
    CHECK(std::ranges::any_of(input.setup.cloneCells, [](const auto& cell) {
        return cell.team == 0 && cell.x == 33 && cell.y == 20;
    }));

    auto creation = Battle::BattleRuntimeSession::createInitialized(std::move(input));
    auto& runtime = creation.session;
    BattleSceneUnitStore sceneUnits;
    sceneUnits.initialize(runtime);
    const auto allyStart = sceneUnits.requireRuntimeUnit(1).motion.position;
    const auto enemyStart = sceneUnits.requireRuntimeUnit(2).motion.position;
    bool moved = false;
    for (int frame = 0; frame < 180 && !runtime.runtime().result.ended; ++frame)
    {
        runtime.runFrame();
        const auto ally = sceneUnits.requireRuntimeUnit(1).motion.position;
        const auto enemy = sceneUnits.requireRuntimeUnit(2).motion.position;
        if ((ally - allyStart).norm() > 0.5 || (enemy - enemyStart).norm() > 0.5)
        {
            moved = true;
            break;
        }
    }
    CHECK(moved);
}
