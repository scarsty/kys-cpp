#include "ChessCombo.h"
#include "ChessGameContent.h"
#include "ChessGameSessionTestHelpers.h"
#include "ChessSessionTypes.h"
#include "battle/BattleInitialization.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <memory>
#include <utility>

using namespace KysChess;
using namespace KysChess::Battle;
using namespace KysChess::Test;

namespace
{

struct TestPiece
{
    int instanceId{};
    int roleId{};
    int star = 1;
    int cost{};
    int weaponItemId = -1;
    int armorItemId = -1;
};

ChessRoleDefinition testRole(int roleId, int cost)
{
    ChessRoleDefinition role;
    role.ID = roleId;
    role.Cost = cost;
    role.MaxHP = 100;
    return role;
}

ComboDef testCombo(
    int id,
    std::string name,
    std::vector<int> memberRoleIds,
    int threshold,
    ComboEffect effect,
    bool antiCombo = false,
    bool starSynergyBonus = false)
{
    ComboDef combo;
    combo.id = id;
    combo.name = std::move(name);
    combo.memberRoleIds = std::move(memberRoleIds);
    combo.thresholds.push_back({threshold, "測試門檻", {effect}});
    combo.isAntiCombo = antiCombo;
    combo.starSynergyBonus = starSynergyBonus;
    return combo;
}

std::shared_ptr<const ChessGameContent> testContent(
    const std::vector<TestPiece>& pieces,
    ComboDef combo,
    std::vector<EquipmentDef> equipment = {},
    std::vector<EquipmentSynergyDef> equipmentSynergies = {})
{
    ChessGameContentData data;
    for (const auto& piece : pieces)
    {
        data.roles.emplace(piece.roleId, testRole(piece.roleId, piece.cost));
    }
    data.combos.push_back(std::move(combo));
    data.equipment = std::move(equipment);
    data.equipmentSynergies = std::move(equipmentSynergies);
    return std::make_shared<const ChessGameContent>(std::move(data), "combo-parity-test");
}

ChessSessionState sessionState(const std::vector<TestPiece>& pieces)
{
    ChessSessionState state;
    int nextEquipmentInstanceId = 1;
    for (const auto& source : pieces)
    {
        ChessSessionPiece piece;
        piece.instanceId = source.instanceId;
        piece.roleId = source.roleId;
        piece.star = source.star;
        piece.deployed = true;
        if (source.weaponItemId >= 0)
        {
            ChessEquipmentInstance equipment;
            equipment.instanceId = nextEquipmentInstanceId++;
            equipment.itemId = source.weaponItemId;
            equipment.assignedChessInstanceId = source.instanceId;
            piece.weaponInstanceId = equipment.instanceId;
            state.equipmentInventory.emplace(equipment.instanceId, equipment);
        }
        if (source.armorItemId >= 0)
        {
            ChessEquipmentInstance equipment;
            equipment.instanceId = nextEquipmentInstanceId++;
            equipment.itemId = source.armorItemId;
            equipment.assignedChessInstanceId = source.instanceId;
            piece.armorInstanceId = equipment.instanceId;
            state.equipmentInventory.emplace(equipment.instanceId, equipment);
        }
        state.roster.emplace(piece.instanceId, piece);
    }
    return state;
}

BattleSetupComboDefinition battleCombo(const ComboDef& combo)
{
    BattleSetupComboDefinition result;
    result.id = combo.id;
    result.name = combo.name;
    result.memberRoleIds = combo.memberRoleIds;
    result.isAntiCombo = combo.isAntiCombo;
    result.starSynergyBonus = combo.starSynergyBonus;
    for (const auto& threshold : combo.thresholds)
    {
        result.thresholds.push_back({threshold.count, threshold.effects});
    }
    return result;
}

std::vector<BattleRuntimeUnitSpawn> initializedBattleSpawns(
    const std::vector<TestPiece>& pieces,
    const ComboDef& combo,
    const std::vector<EquipmentDef>& equipment = {},
    const std::vector<EquipmentSynergyDef>& equipmentSynergies = {})
{
    BattleRuntimeSetupSeed setup;
    setup.comboDefinitions.push_back(battleCombo(combo));
    for (const auto& definition : equipment)
    {
        setup.equipmentDefinitions.push_back({
            definition.itemId,
            definition.equipType,
            definition.effects,
            definition.actAsComboNames,
        });
    }
    for (const auto& synergy : equipmentSynergies)
    {
        setup.equipmentSynergies.push_back({
            synergy.roleIds,
            synergy.equipmentId,
            synergy.effects,
            synergy.actAsComboNames,
        });
    }

    std::vector<BattleRuntimeUnitSpawn> spawns;
    for (const auto& piece : pieces)
    {
        BattleInitializationUnitSeed seed;
        seed.unitId = piece.instanceId;
        seed.realRoleId = piece.roleId;
        seed.team = 0;
        seed.star = piece.star;
        seed.cost = piece.cost;
        seed.baseMaxHp = 100;
        setup.units.push_back(seed);

        BattleSetupRosterUnit roster;
        roster.unitId = piece.instanceId;
        roster.realRoleId = piece.roleId;
        roster.team = 0;
        roster.star = piece.star;
        roster.cost = piece.cost;
        roster.weaponId = piece.weaponItemId;
        roster.armorId = piece.armorItemId;
        roster.chessInstanceId = piece.instanceId;
        roster.sourceOrder = piece.instanceId;
        setup.allyRoster.push_back(roster);

        BattleRuntimeUnit unit;
        unit.id = piece.instanceId;
        unit.realRoleId = piece.roleId;
        unit.team = 0;
        unit.alive = true;
        unit.vitals = {100, 100, 0, 0};
        unit.star = piece.star;
        unit.cost = piece.cost;
        spawns.push_back(makeRuntimeUnitSpawn(std::move(unit), {}));
    }
    return BattleStartInitializer(
        std::move(spawns),
        setup,
        BattleInitializationContext{{36.0, 18}, 0}).initialize().spawns;
}

const BattleRuntimeUnitSpawn& requireSpawn(
    const std::vector<BattleRuntimeUnitSpawn>& spawns,
    int unitId)
{
    const auto found = std::ranges::find(spawns, unitId, [](const auto& spawn) {
        return spawn.unit.id;
    });
    REQUIRE(found != spawns.end());
    return *found;
}

}

TEST_CASE("combo resolution preserves last-instance star ordering for duplicate roles in both paths",
          "[chess][combo][parity][battle]")
{
    const std::vector<TestPiece> pieces{
        {1, 10, 2, 2},
        {2, 10, 1, 2},
    };
    const auto combo = testCombo(
        7,
        "同門",
        {10},
        2,
        {EffectType::FlatATK, 9},
        false,
        true);
    const auto content = testContent(pieces, combo);
    const auto state = sessionState(pieces);

    const auto progress = evaluateChessComboProgress(state, *content, content->combos().front());
    CHECK(progress.memberRoleIds == std::set<int>{10});
    CHECK(progress.physicalCount == 1);
    CHECK(progress.effectiveCount == 1);
    CHECK(progress.activeThresholdIndex == -1);

    const auto spawns = initializedBattleSpawns(pieces, combo);
    CHECK(requireSpawn(spawns, 1).combo.sumAlways(EffectType::FlatATK) == 0);
    CHECK(requireSpawn(spawns, 2).combo.sumAlways(EffectType::FlatATK) == 0);
}

TEST_CASE("equipment combo substitution resolves the same role-restricted members in both paths",
          "[chess][combo][parity][battle][equipment]")
{
    const std::vector<TestPiece> pieces{
        {1, 10, 1, 1},
        {2, 20, 1, 1, 500},
        {3, 30, 1, 1, 500},
    };
    const auto combo = testCombo(8, "劍客", {10}, 2, {EffectType::FlatDEF, 11});
    const std::vector<EquipmentDef> equipment{{500, 1, 0}};
    const std::vector<EquipmentSynergyDef> synergies{{{20}, 500, {}, {"劍客"}}};
    const auto content = testContent(pieces, combo, equipment, synergies);
    const auto state = sessionState(pieces);

    const auto progress = evaluateChessComboProgress(state, *content, content->combos().front());
    CHECK(progress.memberRoleIds == std::set<int>{10, 20});
    CHECK(progress.physicalCount == 2);
    CHECK(progress.effectiveCount == 2);
    CHECK(progress.activeThresholdIndex == 0);

    const auto spawns = initializedBattleSpawns(pieces, combo, equipment, synergies);
    CHECK(requireSpawn(spawns, 1).combo.sumAlways(EffectType::FlatDEF) == 11);
    CHECK(requireSpawn(spawns, 2).combo.sumAlways(EffectType::FlatDEF) == 11);
    CHECK(requireSpawn(spawns, 3).combo.sumAlways(EffectType::FlatDEF) == 0);
}

TEST_CASE("anti-combo resolution selects the same highest-cost deterministic member in both paths",
          "[chess][combo][parity][battle][anti]")
{
    const std::vector<TestPiece> pieces{
        {1, 10, 1, 2},
        {2, 20, 1, 4},
        {3, 30, 1, 4},
    };
    const auto combo = testCombo(
        9,
        "獨行",
        {10, 20, 30},
        1,
        {EffectType::FlatHP, 17},
        true);
    const auto content = testContent(pieces, combo);
    const auto state = sessionState(pieces);

    const auto progress = evaluateChessComboProgress(state, *content, content->combos().front());
    CHECK(progress.memberRoleIds == std::set<int>{20});
    CHECK(progress.physicalCount == 1);
    CHECK(progress.effectiveCount == 1);
    CHECK(progress.activeThresholdIndex == 0);

    const auto spawns = initializedBattleSpawns(pieces, combo);
    CHECK(requireSpawn(spawns, 1).combo.sumAlways(EffectType::FlatHP) == 0);
    CHECK(requireSpawn(spawns, 2).combo.sumAlways(EffectType::FlatHP) == 17);
    CHECK(requireSpawn(spawns, 3).combo.sumAlways(EffectType::FlatHP) == 0);
}

TEST_CASE("configured combo gold uses the highest active coefficient and surviving star",
          "[chess][combo][gold][progression]")
{
    const std::vector<TestPiece> pieces{
        {1, 10, 1, 1},
        {2, 20, 1, 1},
        {3, 30, 1, 1},
        {4, 40, 1, 1},
        {5, 50, 3, 1},
    };
    auto combo = testCombo(
        10,
        "獎勵羈絆",
        {10, 20, 30, 40},
        2,
        {EffectType::GoldCoefficient, 1});
    combo.thresholds.push_back({4, "高階獎勵", {{EffectType::GoldCoefficient, 2}}});
    const auto content = testContent(pieces, combo);
    const auto state = sessionState(pieces);

    const ChessComboGoldBonus expectedBonus{6, 10};
    CHECK(resolveChessComboGoldBonus(state, *content, {1, 5}) == expectedBonus);
    CHECK(calculateChessComboGoldBonus(state, *content, {1, 5}) == 6);
    CHECK(calculateChessComboGoldBonus(state, *content, {5}) == 0);
}

TEST_CASE("actual 丐幫 configuration drives its victory gold effect",
          "[chess][combo][gold][actual-config]")
{
    const auto content = actualContent();
    REQUIRE(content);
    const auto comboIt = std::ranges::find(content->combos(), std::string{"丐幫"}, &ComboDef::name);
    REQUIRE(comboIt != content->combos().end());
    REQUIRE_FALSE(comboIt->memberRoleIds.empty());

    const ComboThreshold* activeThreshold = nullptr;
    for (const auto& threshold : comboIt->thresholds)
    {
        if (threshold.count <= static_cast<int>(comboIt->memberRoleIds.size()))
        {
            activeThreshold = &threshold;
        }
    }
    REQUIRE(activeThreshold);
    const auto goldEffect = std::ranges::find(
        activeThreshold->effects,
        EffectType::GoldCoefficient,
        &ComboEffect::type);
    REQUIRE(goldEffect != activeThreshold->effects.end());

    std::vector<TestPiece> pieces;
    int instanceId = 1;
    for (const int roleId : comboIt->memberRoleIds)
    {
        const auto* role = content->role(roleId);
        REQUIRE(role);
        pieces.push_back({instanceId++, roleId, 1, role->Cost});
    }
    const auto outsider = std::ranges::find_if(content->roles(), [&](const auto& entry) {
        return !std::ranges::contains(comboIt->memberRoleIds, entry.first);
    });
    REQUIRE(outsider != content->roles().end());
    const int outsiderInstanceId = instanceId;
    pieces.push_back({outsiderInstanceId, outsider->first, 3, outsider->second.Cost});
    const auto state = sessionState(pieces);

    const ChessComboGoldBonus expectedBonus{3 * goldEffect->value, comboIt->id};
    CHECK(resolveChessComboGoldBonus(state, *content, {1, outsiderInstanceId}) == expectedBonus);
    CHECK(calculateChessComboGoldBonus(state, *content, {1, outsiderInstanceId})
          == 3 * goldEffect->value);
    CHECK(calculateChessComboGoldBonus(state, *content, {outsiderInstanceId}) == 0);
}
