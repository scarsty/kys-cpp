#include "BattleSceneBattleAdapter.h"

#include "Scene.h"
#include "Save.h"
#include "BattleScenePresentationConstants.h"
#include "BattleStatsView.h"
#include "ChessCombo.h"
#include "ChessEquipment.h"
#include "ChessEftIds.h"
#include "ChessNeigong.h"
#include "GameUtil.h"
#include "battle/BattleCombatIntent.h"
#include "battle/BattleDamageSystem.h"
#include "battle/BattleFind.h"
#include "battle/BattleProjectileTargetingSystem.h"

#include <algorithm>
#include <cassert>
#include <ranges>
#include <set>
#include <string>
#include <utility>

namespace KysChess::BattleSceneBattleAdapter
{

void configureBattleAttackWorld(
    Battle::BattleAttackWorld& world,
    const Battle::BattleRuntimeRulesConfig& rules);
Battle::BattleRuntimeUnit makeBattleRuntimeUnit(
    const BattleSetupUnitInput& snapshot,
    const RoleComboState* state,
    const Battle::BattleGridTransform& gridTransform);
Battle::BattleDamagePresentationStyle makeBattleDamagePresentationStyle(int team);

namespace
{
constexpr double ROLE_MOVE_SPEED_DIVISOR = 22.0;

Color damageTextColor(int team, bool emphasized)
{
    if (team == 0)
    {
        return emphasized ? Color{ 255, 45, 85, 255 } : Color{ 255, 90, 79, 255 };
    }
    return emphasized ? Color{ 47, 128, 255, 255 } : Color{ 102, 207, 255, 255 };
}

int getComboLookupId(int realRoleId, const RoleComboState& state)
{
    if (state.isSummonedClone)
    {
        return -1;
    }
    return realRoleId;
}

Battle::BattleUnitState makeInitializedBattleWorldUnit(
    const Battle::BattleRuntimeUnit& runtimeUnit,
    const std::map<int, RoleComboState>& comboStates,
    const Battle::BattleWorldState& world,
    const std::map<int, Battle::BattleMovementPhysicsState>& movementRuntime,
    const Battle::BattleActionRulesConfig& actionRules)
{
    const auto comboIt = comboStates.find(runtimeUnit.id);
    const bool dashAttackEnabled = comboIt != comboStates.end() && comboIt->second.dashAttack;

    auto unit = Battle::makeBattleWorldUnitState(runtimeUnit, ROLE_MOVE_SPEED_DIVISOR);
    unit.reach = actionRules.meleeAttackReach;
    unit.style = Battle::CombatStyle::Melee;
    unit.taXue = dashAttackEnabled;
    unit.dashAttack = dashAttackEnabled;

    const auto existingMovement = std::ranges::find(world.units, runtimeUnit.id, &Battle::BattleUnitState::id);
    if (existingMovement != world.units.end())
    {
        unit.assignedSlot = existingMovement->assignedSlot;
        unit.slotSwitchCooldownRemaining = existingMovement->slotSwitchCooldownRemaining;
    }

    const auto movementIt = movementRuntime.find(runtimeUnit.id);
    if (movementIt != movementRuntime.end())
    {
        unit.dashFramesRemaining = movementIt->second.movementDashFrames;
        unit.dashCooldownRemaining = movementIt->second.movementDashCooldown;
    }

    return unit;
}

Battle::BattleDeathEffectStore makeBattleDeathEffectStore(
    const Battle::BattleUnitStore& units,
    const std::map<int, RoleComboState>& states)
{
    Battle::BattleDeathEffectStore store;
    const auto& allCombos = KysChess::ChessCombo::getAllCombos();
    for (int comboId = 0; comboId < static_cast<int>(allCombos.size()); ++comboId)
    {
        if (!allCombos[comboId].isAntiCombo)
        {
            store.regularSynergyComboIds.insert(comboId);
        }
    }

    for (const auto& unit : units.units)
    {
        const auto stateIt = states.find(unit.id);
        assert(stateIt != states.end());

        Battle::BattleDeathEffectExtras extras;
        extras.id = unit.id;
        extras.shieldPctMaxHp = stateIt->second.shieldPctMaxHP;
        extras.shieldOnAllyDeathTracker = stateIt->second.shieldOnAllyDeathTracker;
        extras.appliedEffects = stateIt->second.appliedEffects;

        const int comboLookupId = getComboLookupId(unit.realRoleId, stateIt->second);
        if (comboLookupId >= 0)
        {
            extras.comboIds = KysChess::ChessCombo::getCombosForRole(comboLookupId);
        }

        store.units.push_back(std::move(extras));
    }

    return store;
}

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

Battle::BattleRuntimeSetupConfiguration makeInitializedBattleRuntimeConfiguration(
    const BattleRuntimeBuildContext& context,
    const Battle::BattleRuntimeState& runtime)
{
    Battle::BattleRuntimeSetupConfiguration config;

    config.world = runtime.world;
    config.world.frame = context.input.battleFrame;
    config.world.config = context.rules.movementConfig;
    config.world.terrainCells = context.input.terrainCells;

    const auto existingWorld = runtime.world;
    config.world.units.clear();
    config.world.units.reserve(runtime.units.units.size());
    for (const auto& unit : runtime.units.units)
    {
        if (!unit.alive)
        {
            continue;
        }
        config.world.units.push_back(makeInitializedBattleWorldUnit(
            unit,
            runtime.combo.units,
            existingWorld,
            runtime.movementRuntime,
            context.rules.action));
    }

    config.attacks = runtime.attacks;
    configureBattleAttackWorld(config.attacks, context.rules);

    config.teamEffects.healAuraRadius = context.rules.teamEffectHealAuraRadius;
    config.deathEffects.store = makeBattleDeathEffectStore(runtime.units, runtime.combo.units);

    config.rescue.cells = context.input.rescueCells;
    config.rescue.executeUnattendedRadius = context.rules.rescueExecuteUnattendedRadius;
    config.rescue.counterAttack = context.rules.rescueCounterAttack;
    config.rescue.counterAttack.skillId = context.input.rescueCounterAttackSkillId;

    config.movementPhysics.config = context.rules.movementPhysicsConfig;
    config.movementPhysics.collision = context.rules.movementCollisionWorld;
    config.movementPhysics.actionCastFrames.assign(
        context.rules.castConfig.castFrames.begin(),
        context.rules.castConfig.castFrames.end());
    config.movementPhysics.dashMomentumFrames = context.rules.movementPhysicsDashMomentumFrames;

    config.action.castFrames.assign(
        context.rules.castConfig.castFrames.begin(),
        context.rules.castConfig.castFrames.end());
    config.action.actionRecoveryFrames = context.rules.action.actionRecoveryFrames;
    config.action.dashRecoveryFrames = context.rules.action.dashRecoveryFrames;
    config.action.blinkWeakTargetDefWeight = context.rules.action.blinkWeakTargetDefWeight;
    config.action.strengthenedMeleeOperationCountThreshold = context.rules.action.strengthenedMeleeOperationCountThreshold;
    config.action.projectileBounceRange = context.rules.action.projectileBounceRange;

    config.projectileFollowUps = context.rules.projectileFollowUps;

    config.damage.aggregatePendingTransactionsByDefender = true;
    for (const auto& unit : runtime.units.units)
    {
        const auto stateIt = runtime.combo.units.find(unit.id);
        config.damage.unitExtras.push_back(Battle::makeBattleDamageRuntimeUnit(
            Battle::makeBattleDamageUnitState(
                unit,
                stateIt != runtime.combo.units.end() ? &stateIt->second : nullptr)));
        config.damage.presentationStylesByDefender.emplace(unit.id, makeBattleDamagePresentationStyle(unit.team));
        if (stateIt != runtime.combo.units.end() && stateIt->second.deathAOEPct > 0)
        {
            config.damage.unitEffects.emplace(
                unit.id,
                Battle::BattleDamageApplicationUnitEffects{
                    stateIt->second.deathAOEPct,
                    stateIt->second.deathAOEStunFrames,
                    stateIt->second.deathAOEMaxTargets,
                });
        }
    }

    config.action.planSeeds.clear();
    for (const auto& seed : context.input.actionPlanSeeds)
    {
        config.action.planSeeds.emplace(seed.unitId, seed);
    }
    config.action.castConfig = context.rules.castConfig;
    config.action.castGeometry = context.rules.castGeometry;
    config.action.actionRules = context.rules.action;

    return config;
}

BattleRuntimeCreationResult createInitializedBattleRuntimeSession(const BattleRuntimeBuildContext& context)
{
    const auto& input = context.input;
    assert(input.comboStates);

    Battle::BattleRuntimeInit init;
    init.runtime.units.gridTransform = context.rules.gridTransform;
    init.runtime.random = Battle::BattleRuntimeRandom(context.randomSeed);
    init.runtime.combo.units = *input.comboStates;
    init.runtime.units.units.reserve(input.units.size());
    init.runtime.status.units.reserve(input.units.size());
    init.runtime.world.units.reserve(input.units.size());
    init.setup = input.runtimeSetupSeed;
    populateBattleRuntimeSetupDefinitions(init.setup);

    for (const auto& role : input.units)
    {
        assert(role.unitId >= 0);
        auto stateIt = input.comboStates->find(role.unitId);
        auto runtimeUnit = makeBattleRuntimeUnit(
            role,
            stateIt != input.comboStates->end() ? &stateIt->second : nullptr,
            context.rules.gridTransform);
        if (stateIt != input.comboStates->end())
        {
            auto statusUnit = Battle::makeBattleStatusUnitState(runtimeUnit, stateIt->second);
            statusUnit.effects.frozenTimer = role.frozen;
            statusUnit.effects.frozenMaxTimer = role.frozenMax;
            init.runtime.status.units.push_back(Battle::makeBattleStatusRuntimeUnit(
                statusUnit));
        }
        else
        {
            init.runtime.status.units.push_back(Battle::BattleStatusRuntimeUnit{ .id = role.unitId });
        }
        init.runtime.world.units.push_back(Battle::makeBattleWorldUnitState(runtimeUnit, ROLE_MOVE_SPEED_DIVISOR));
        init.runtime.units.units.push_back(std::move(runtimeUnit));
    }

    BattleRuntimeCreationResult result{
        Battle::BattleRuntimeSession::createConfigured(
            std::move(init),
            [&context](const Battle::BattleRuntimeState& runtime)
            {
                return makeInitializedBattleRuntimeConfiguration(context, runtime);
            }),
        {},
    };
    auto initialization = result.session.releaseInitializationResult();
    result.sceneInitialization = makeBattleInitializationSceneApplyResult(
        input,
        initialization,
        result.session.runtime());
    return result;
}

Battle::BattlePresentationColor makeBattlePresentationColor(Color color)
{
    return { color.r, color.g, color.b, color.a };
}

void configureBattleAttackWorld(Battle::BattleAttackWorld& world, const Battle::BattleRuntimeRulesConfig& rules)
{
    world.hitRadius = rules.action.meleeAttackHitRadius;
    world.minimumVectorNorm = rules.minimumVectorNorm;
    world.projectileGraceFrames = 5;
    world.bounceSpawnDistance = rules.projectileFollowUps.areaSpawnDistance;
    world.defaultProjectileSpeed = rules.projectileFollowUps.projectileSpeed;
    world.spendNonThroughOnHit = false;
}

Battle::BattleRuntimeUnit makeBattleRuntimeUnit(
    const BattleSetupUnitInput& snapshot,
    const RoleComboState* state,
    const Battle::BattleGridTransform& gridTransform)
{
    assert(snapshot.unitId >= 0);

    Battle::BattleRuntimeUnit unit;
    unit.id = snapshot.unitId;
    unit.realRoleId = snapshot.realRoleId;
    unit.name = snapshot.name;
    unit.team = snapshot.team;
    unit.alive = snapshot.alive;
    unit.vitals = snapshot.vitals;
    unit.stats = snapshot.stats;
    unit.motion = snapshot.motion;
    unit.animation = snapshot.animation;
    unit.haveAction = snapshot.haveAction;
    unit.operationType = snapshot.operationType;
    unit.operationCount = snapshot.operationCount;
    unit.physicalPower = snapshot.physicalPower;
    unit.invincible = snapshot.invincible;
    unit.hurtFrame = snapshot.hurtFrame;
    for (int magicType = 0; magicType <= 4; ++magicType)
    {
        unit.actPropertiesByMagicType.emplace(magicType, snapshot.actPropertiesByMagicType[magicType]);
    }
    unit.grid = gridTransform.toGrid(snapshot.motion.position);
    if (state)
    {
        unit.shield = state->shield;
        unit.mpBlocked = state->mpBlockTimer > 0;
        unit.mpRecoveryBonusPct = state->mpRecoveryBonusPct;
    }
    return unit;
}

Battle::BattleDamagePresentationStyle makeBattleDamagePresentationStyle(int team)
{
    Battle::BattleDamagePresentationStyle style;
    style.normalDamageColor = makeBattlePresentationColor(damageTextColor(team, false));
    style.emphasizedDamageColor = makeBattlePresentationColor(damageTextColor(team, true));
    style.executeTextColor = makeBattlePresentationColor({ 255, 136, 48, 255 });
    style.normalDamageTextSize = NORMAL_DAMAGE_TEXT_SIZE;
    style.emphasizedDamageTextSize = ULT_DAMAGE_TEXT_SIZE;
    style.executeTextSize = ULT_DAMAGE_TEXT_SIZE;
    return style;
}

}  // namespace KysChess::BattleSceneBattleAdapter
