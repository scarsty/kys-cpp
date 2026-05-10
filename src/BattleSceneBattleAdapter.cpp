#include "BattleSceneBattleAdapter.h"

#include "BattleStatsView.h"
#include "ChessCombo.h"
#include "ChessEquipment.h"
#include "ChessNeigong.h"
#include "GameUtil.h"
#include "battle/BattleFind.h"

#include <algorithm>
#include <cassert>
#include <string>
#include <utility>

namespace KysChess::BattleSceneBattleAdapter
{

namespace
{
const Battle::BattleInitializationCloneIntent* findCloneIntent(
    const Battle::BattleInitializationResult& initialization,
    int cloneUnitId)
{
    return Battle::tryFindBy(initialization.cloneIntents, cloneUnitId, &Battle::BattleInitializationCloneIntent::cloneUnitId);
}

const BattleSetupUnitInput& requireSetupUnit(
    const BattleRuntimeCreationInput& input,
    int unitId)
{
    assert(unitId >= 0);
    assert(static_cast<std::size_t>(unitId) < input.units.size());
    assert(input.units[unitId].unitId == unitId);
    return input.units[unitId];
}

std::vector<Battle::BattleSetupComboDefinition> makeBattleSetupComboDefinitions()
{
    std::vector<Battle::BattleSetupComboDefinition> definitions;
    for (const auto& combo : KysChess::ChessCombo::getAllCombos())
    {
        Battle::BattleSetupComboDefinition definition;
        definition.id = combo.id;
        definition.name = combo.name;
        definition.memberRoleIds = combo.memberRoleIds;
        definition.isAntiCombo = combo.isAntiCombo;
        definition.starSynergyBonus = combo.starSynergyBonus;
        definition.thresholds.reserve(combo.thresholds.size());
        for (const auto& threshold : combo.thresholds)
        {
            definition.thresholds.push_back({ threshold.count, threshold.effects });
        }
        definitions.push_back(std::move(definition));
    }
    return definitions;
}

std::vector<Battle::BattleSetupEquipmentDefinition> makeBattleSetupEquipmentDefinitions()
{
    std::vector<Battle::BattleSetupEquipmentDefinition> definitions;
    for (const auto& equipment : KysChess::ChessEquipment::getAll())
    {
        definitions.push_back({
            equipment.itemId,
            equipment.equipType,
            equipment.effects,
            equipment.actAsComboNames,
        });
    }
    return definitions;
}

std::vector<Battle::BattleSetupEquipmentSynergyDefinition> makeBattleSetupEquipmentSynergyDefinitions()
{
    std::vector<Battle::BattleSetupEquipmentSynergyDefinition> definitions;
    for (const auto& synergy : KysChess::ChessEquipment::getAllSynergies())
    {
        definitions.push_back({
            synergy.roleIds,
            synergy.equipmentId,
            synergy.effects,
            synergy.actAsComboNames,
        });
    }
    return definitions;
}

std::vector<Battle::BattleSetupNeigongDefinition> makeBattleSetupNeigongDefinitions()
{
    std::vector<Battle::BattleSetupNeigongDefinition> definitions;
    for (const auto& neigong : KysChess::ChessNeigong::getPool())
    {
        definitions.push_back({ neigong.magicId, neigong.effects });
    }
    return definitions;
}

void populateBattleRuntimeSetupDefinitions(Battle::BattleRuntimeSetupSeed& setup)
{
    setup.comboDefinitions = makeBattleSetupComboDefinitions();
    setup.equipmentDefinitions = makeBattleSetupEquipmentDefinitions();
    setup.equipmentSynergies = makeBattleSetupEquipmentSynergyDefinitions();
    setup.neigongDefinitions = makeBattleSetupNeigongDefinitions();
}
}  // namespace

BattleSceneUnit makeInitializedSceneUnit(
    const Battle::BattleRuntimeUnit& runtimeUnit,
    const BattleSetupUnitInput& setup,
    const Battle::BattleInitializationCloneIntent* clone)
{
    BattleSceneUnit unit;
    unit.identity = {
        runtimeUnit.id,
        runtimeUnit.realRoleId,
        runtimeUnit.team,
        setup.headId,
        setup.name,
    };
    unit.unitId = runtimeUnit.id;
    unit.sourceUnitId = clone ? clone->sourceUnitId : runtimeUnit.id;
    unit.gridX = runtimeUnit.grid.x;
    unit.gridY = runtimeUnit.grid.y;
    unit.faceTowards = setup.faceTowards;
    unit.vitals = runtimeUnit.vitals;
    unit.stats = runtimeUnit.stats;
    unit.motion = runtimeUnit.motion;
    unit.animation = runtimeUnit.animation;
    unit.fightFrames = setup.fightFrames;
    unit.star = runtimeUnit.star;
    unit.chessInstanceId = clone ? -1 : setup.chessInstanceId;
    unit.weaponId = clone ? -1 : setup.weaponId;
    unit.armorId = clone ? -1 : setup.armorId;
    unit.cost = runtimeUnit.cost;
    unit.frozen = setup.frozen;
    unit.frozenMax = setup.frozenMax;
    unit.invincible = runtimeUnit.invincible;
    unit.alive = runtimeUnit.alive;
    unit.active = runtimeUnit.alive;
    unit.skillNames = setup.skillNames;
    return unit;
}

BattleInitializationSceneApplyResult makeBattleInitializationSceneApplyResult(
    const BattleRuntimeCreationInput& input,
    const Battle::BattleInitializationResult& initialization,
    const Battle::BattleRuntimeState& runtime)
{
    BattleInitializationSceneApplyResult result;
    result.comboStates = initialization.comboStates;
    result.logEvents = initialization.logEvents;
    result.visualEvents = initialization.visualEvents;

    result.units.reserve(runtime.units.units.size());
    for (const auto& runtimeUnit : runtime.units.units)
    {
        const auto* clone = findCloneIntent(initialization, runtimeUnit.id);
        const auto& setupUnit = clone
            ? requireSetupUnit(input, clone->sourceUnitId)
            : requireSetupUnit(input, runtimeUnit.id);
        result.units.push_back(makeInitializedSceneUnit(runtimeUnit, setupUnit, clone));
    }
    return result;
}

BattleRuntimeCreationResult createInitializedBattleRuntimeSession(const BattleRuntimeBuildContext& context)
{
    const auto& input = context.input;
    assert(input.comboStates);

    Battle::BattleRuntimeSessionCreationInput sessionInput;
    sessionInput.units = input.units;
    sessionInput.setup = input.runtimeSetupSeed;
    populateBattleRuntimeSetupDefinitions(sessionInput.setup);
    sessionInput.comboStates = *input.comboStates;
    sessionInput.terrainCells = input.terrainCells;
    sessionInput.rescueCells = input.rescueCells;
    sessionInput.actionPlanSeeds = input.actionPlanSeeds;
    sessionInput.rules = context.rules;
    sessionInput.randomSeed = context.randomSeed;
    sessionInput.battleFrame = input.battleFrame;
    sessionInput.rescueCounterAttackSkillId = input.rescueCounterAttackSkillId;

    BattleRuntimeCreationResult result{
        Battle::BattleRuntimeSession::createInitialized(std::move(sessionInput)),
        {},
    };
    auto initialization = result.session.releaseInitializationResult();
    result.sceneInitialization = makeBattleInitializationSceneApplyResult(
        input,
        initialization,
        result.session.runtime());
    return result;
}

}  // namespace KysChess::BattleSceneBattleAdapter
