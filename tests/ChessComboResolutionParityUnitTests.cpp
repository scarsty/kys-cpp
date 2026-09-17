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

EffectRule attributeRule(BattleAttribute attribute, int amount)
{
    EffectRule rule;
    rule.event = EffectEvent::BattleInitialized;
    ModifyAttributeAction action;
    action.attribute = attribute;
    action.amount.flat = amount;
    action.operation = AttributeOperation::FlatAdd;
    rule.actions.push_back({ EffectActionValue{ action } });
    return rule;
}

ComboDef testCombo(
    int id,
    std::string name,
    std::vector<int> memberRoleIds,
    int threshold,
    EffectRule rule,
    bool antiCombo = false,
    bool starSynergyBonus = false)
{
    ComboDef combo;
    combo.id = id;
    combo.name = std::move(name);
    combo.memberRoleIds = std::move(memberRoleIds);
    combo.thresholds.push_back({ threshold, "測試門檻", { std::move(rule) } });
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
        result.thresholds.push_back({
            threshold.count,
            threshold.rules,
            sumFightWinGrowth(threshold.managementRules),
        });
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
            countsAsComboNames(definition.managementRules),
            definition.rules,
        });
    }
    for (const auto& synergy : equipmentSynergies)
    {
        setup.equipmentSynergies.push_back({
            synergy.roleIds,
            synergy.equipmentId,
            countsAsComboNames(synergy.managementRules),
            synergy.rules,
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
        attributeRule(BattleAttribute::Attack, 9),
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
    CHECK(requireSpawn(spawns, 1).unit.stats.attack == 15);
    CHECK(requireSpawn(spawns, 2).unit.stats.attack == 0);
}

TEST_CASE("equipment combo substitution resolves the same role-restricted members in both paths",
          "[chess][combo][parity][battle][equipment]")
{
    const std::vector<TestPiece> pieces{
        {1, 10, 1, 1},
        {2, 20, 1, 1, 500},
        {3, 30, 1, 1, 500},
    };
    const auto combo = testCombo(
        8,
        "劍客",
        { 10 },
        2,
        attributeRule(BattleAttribute::Defence, 11));
    const std::vector<EquipmentDef> equipment{{500, 1, 0}};
    const std::vector<EquipmentSynergyDef> synergies{
        {{20}, 500, {}, {CountsAsComboRule{"劍客"}}},
    };
    const auto content = testContent(pieces, combo, equipment, synergies);
    const auto state = sessionState(pieces);

    const auto progress = evaluateChessComboProgress(state, *content, content->combos().front());
    CHECK(progress.memberRoleIds == std::set<int>{10, 20});
    CHECK(progress.physicalCount == 2);
    CHECK(progress.effectiveCount == 2);
    CHECK(progress.activeThresholdIndex == 0);
    REQUIRE(progress.contributions.size() == 2);
    const auto equippedContribution = std::ranges::find(
        progress.contributions,
        20,
        &ResolvedChessComboContribution::roleId);
    REQUIRE(equippedContribution != progress.contributions.end());
    CHECK_FALSE(equippedContribution->naturalMember);
    CHECK(equippedContribution->equipmentItemIds == std::vector<int>{500});
    CHECK(equippedContribution->physicalPoints == 1);
    CHECK(equippedContribution->starBonusPoints == 0);

    const auto spawns = initializedBattleSpawns(pieces, combo, equipment, synergies);
    CHECK(requireSpawn(spawns, 1).unit.stats.defence == 11);
    CHECK(requireSpawn(spawns, 2).unit.stats.defence == 11);
    CHECK(requireSpawn(spawns, 3).unit.stats.defence == 0);
}

TEST_CASE("equipment combo substitution does not double-count an existing member",
          "[chess][combo][equipment][provenance]")
{
    const std::vector<TestPiece> pieces{{1, 10, 1, 1, 500}};
    const auto combo = testCombo(
        11,
        "真武七截陣",
        { 10 },
        2,
        attributeRule(BattleAttribute::Defence, 11));
    EquipmentDef equipment{500, 3, 0};
    equipment.managementRules = {CountsAsComboRule{"真武七截陣"}};
    const auto content = testContent(pieces, combo, {equipment});
    const auto state = sessionState(pieces);

    const auto progress = evaluateChessComboProgress(state, *content, content->combos().front());

    CHECK(progress.physicalCount == 1);
    CHECK(progress.effectiveCount == 1);
    REQUIRE(progress.contributions.size() == 1);
    CHECK(progress.contributions.front().naturalMember);
    CHECK(progress.contributions.front().equipmentItemIds == std::vector<int>{500});
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
        attributeRule(BattleAttribute::MaxHp, 17),
        true);
    const auto content = testContent(pieces, combo);
    const auto state = sessionState(pieces);

    const auto progress = evaluateChessComboProgress(state, *content, content->combos().front());
    CHECK(progress.memberRoleIds == std::set<int>{20});
    CHECK(progress.physicalCount == 1);
    CHECK(progress.effectiveCount == 1);
    CHECK(progress.activeThresholdIndex == 0);

    const auto spawns = initializedBattleSpawns(pieces, combo);
    CHECK(requireSpawn(spawns, 1).unit.vitals.maxHp == 100);
    CHECK(requireSpawn(spawns, 2).unit.vitals.maxHp == 117);
    CHECK(requireSpawn(spawns, 3).unit.vitals.maxHp == 100);
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
    ComboDef combo;
    combo.id = 10;
    combo.name = "獎勵羈絆";
    combo.memberRoleIds = {10, 20, 30, 40};
    combo.thresholds.push_back({2, "測試門檻", {}, {VictoryGoldRule{1}}});
    combo.thresholds.push_back({4, "高階獎勵", {}, {VictoryGoldRule{2}}});
    const auto content = testContent(pieces, combo);
    const auto state = sessionState(pieces);

    const ChessComboGoldBonus expectedBonus{6, 10};
    CHECK(resolveChessComboGoldBonus(state, *content, {1, 5}) == expectedBonus);
    CHECK(calculateChessComboGoldBonus(state, *content, {1, 5}) == 6);
    CHECK(calculateChessComboGoldBonus(state, *content, {5}) == 0);
}