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
Battle::BattleStatusUnitState makeBattleStatusUnit(const BattleSetupUnitInput& snapshot, const RoleComboState& state);
Battle::BattleDamageUnitState makeBattleDamageUnit(const BattleSetupUnitInput& snapshot, const RoleComboState* state);
Battle::BattleDamageUnitState makeBattleDamageUnit(const Battle::BattleRuntimeUnit& runtimeUnit, const RoleComboState* state);
Battle::BattleDamagePresentationStyle makeBattleDamagePresentationStyle(int team);

namespace
{
constexpr double ROLE_MOVE_SPEED_DIVISOR = 22.0;

struct BattleActionPlanInputContext
{
    const std::vector<BattleSetupUnitInput>* setupUnits = nullptr;
    Battle::BattleActionRulesConfig actionRules;
    Battle::BattleCastConfig castConfig;
    Battle::BattleCastGeometry castGeometry;
};

void initializeBattleActionPlanInputs(
    Battle::BattleRuntimeState::ActionState& action,
    BattleActionPlanInputContext& context);

class BattleSetupRoleProjection
{
public:
    explicit BattleSetupRoleProjection(const BattleSetupUnitInput& snapshot)
        : snapshot_(snapshot)
    {
        assert(snapshot_.unitId >= 0);
    }

    int id() const { return snapshot_.unitId; }
    int realRoleId() const { return snapshot_.realRoleId; }
    const std::string& name() const { return snapshot_.name; }
    int team() const { return snapshot_.team; }
    bool alive() const { return snapshot_.alive; }
    int hp() const { return snapshot_.vitals.hp; }
    int maxHp() const { return snapshot_.vitals.maxHp; }
    int mp() const { return snapshot_.vitals.mp; }
    int maxMp() const { return snapshot_.vitals.maxMp; }
    int attack() const { return snapshot_.stats.attack; }
    int defence() const { return snapshot_.stats.defence; }
    int speed() const { return snapshot_.stats.speed; }
    int star() const { return snapshot_.star; }
    int invincible() const { return snapshot_.invincible; }
    int hurtFrame() const { return snapshot_.hurtFrame; }
    int frozen() const { return snapshot_.frozen; }
    int frozenMax() const { return snapshot_.frozenMax; }
    Pointf position() const { return snapshot_.motion.position; }
    Pointf velocity() const { return snapshot_.motion.velocity; }
    Pointf acceleration() const { return snapshot_.motion.acceleration; }
    Pointf facing() const { return snapshot_.motion.facing; }

    Battle::BattleUnitState worldUnit() const;
    Battle::BattleUnitState initializedWorldUnit(
        const std::map<int, RoleComboState>& comboStates,
        const Battle::BattleWorldState& world,
        const std::map<int, Battle::BattleMovementPhysicsState>& movementRuntime,
        const Battle::BattleActionRulesConfig& actionRules) const;
    Battle::BattleRuntimeUnit runtimeUnit(
        const RoleComboState* state,
        const Battle::BattleGridTransform& gridTransform) const;
    Battle::BattleStatusUnitState statusUnit(const RoleComboState& state) const;
    Battle::BattleDamageUnitState damageUnit(const RoleComboState* state) const;

private:
    const BattleSetupUnitInput& snapshot_;
};

Color damageTextColor(int team, bool emphasized)
{
    if (team == 0)
    {
        return emphasized ? Color{ 255, 45, 85, 255 } : Color{ 255, 90, 79, 255 };
    }
    return emphasized ? Color{ 47, 128, 255, 255 } : Color{ 102, 207, 255, 255 };
}

Battle::BattleUnitState BattleSetupRoleProjection::worldUnit() const
{
    Battle::BattleUnitState unit;
    unit.id = id();
    unit.realRoleId = realRoleId();
    unit.name = name();
    unit.team = team();
    unit.alive = alive();
    unit.position = position();
    unit.velocity = velocity();
    unit.speed = speed() / ROLE_MOVE_SPEED_DIVISOR;
    unit.star = star();
    unit.canAttack = snapshot_.animation.cooldown == 0;
    return unit;
}

int getComboLookupId(int realRoleId, const RoleComboState& state)
{
    if (state.isSummonedClone)
    {
        return -1;
    }
    return realRoleId;
}

Battle::BattleUnitState BattleSetupRoleProjection::initializedWorldUnit(
    const std::map<int, RoleComboState>& comboStates,
    const Battle::BattleWorldState& world,
    const std::map<int, Battle::BattleMovementPhysicsState>& movementRuntime,
    const Battle::BattleActionRulesConfig& actionRules) const
{
    const auto comboIt = comboStates.find(id());
    const bool dashAttackEnabled = comboIt != comboStates.end() && comboIt->second.dashAttack;

    auto unit = worldUnit();
    unit.reach = actionRules.meleeAttackReach;
    unit.style = Battle::CombatStyle::Melee;
    unit.taXue = dashAttackEnabled;
    unit.dashAttack = dashAttackEnabled;

    const auto existingMovement = std::find_if(
        world.units.begin(),
        world.units.end(),
        [unitId = id()](const Battle::BattleUnitState& existing)
        {
            return existing.id == unitId;
        });
    if (existingMovement != world.units.end())
    {
        unit.assignedSlot = existingMovement->assignedSlot;
        unit.slotSwitchCooldownRemaining = existingMovement->slotSwitchCooldownRemaining;
    }

    const auto movementIt = movementRuntime.find(id());
    if (movementIt != movementRuntime.end())
    {
        unit.dashFramesRemaining = movementIt->second.movementDashFrames;
        unit.dashCooldownRemaining = movementIt->second.movementDashCooldown;
    }

    return unit;
}

Battle::BattleUnitState makeBattleWorldUnit(const BattleSetupUnitInput& snapshot)
{
    return BattleSetupRoleProjection(snapshot).worldUnit();
}

Battle::BattleUnitState makeBattleWorldUnit(const Battle::BattleRuntimeUnit& runtimeUnit)
{
    Battle::BattleUnitState unit;
    unit.id = runtimeUnit.id;
    unit.realRoleId = runtimeUnit.realRoleId;
    unit.name = runtimeUnit.name;
    unit.team = runtimeUnit.team;
    unit.alive = runtimeUnit.alive;
    unit.position = runtimeUnit.motion.position;
    unit.velocity = runtimeUnit.motion.velocity;
    unit.speed = runtimeUnit.stats.speed / ROLE_MOVE_SPEED_DIVISOR;
    unit.star = runtimeUnit.star;
    unit.canAttack = runtimeUnit.animation.cooldown == 0;
    return unit;
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

    auto unit = makeBattleWorldUnit(runtimeUnit);
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
    const BattleRuntimeSceneSetupInput& setup,
    int unitId)
{
    assert(unitId >= 0);
    assert(static_cast<std::size_t>(unitId) < setup.units.size());
    assert(setup.units[unitId].unitId == unitId);
    return setup.units[unitId];
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

Battle::BattleRuntimeSetupSeed makeBattleRuntimeSetupSeed(const BattleRuntimeSceneSetupInput& context)
{
    Battle::BattleRuntimeSetupSeed setup;
    setup.comboDefinitions = makeBattleSetupComboDefinitions();
    setup.equipmentDefinitions = makeBattleSetupEquipmentDefinitions();
    setup.equipmentSynergies = makeBattleSetupEquipmentSynergyDefinitions();
    setup.neigongDefinitions = makeBattleSetupNeigongDefinitions();
    setup.obtainedNeigongMagicIds = context.obtainedNeigongMagicIds;

    for (const auto& unit : context.units)
    {
        Battle::BattleSetupRosterUnit roster{
            unit.unitId,
            unit.realRoleId,
            unit.team,
            unit.star,
            unit.cost,
            unit.weaponId,
            unit.armorId,
            unit.chessInstanceId,
            unit.fightsWon,
            unit.sourceOrder,
        };
        if (unit.team == 0)
        {
            setup.allyRoster.push_back(roster);
        }
        else
        {
            setup.enemyRoster.push_back(roster);
        }

        setup.units.push_back({
            unit.unitId,
            unit.realRoleId,
            unit.team,
            unit.star,
            unit.cost,
            unit.vitals.maxHp,
            unit.stats.attack,
            unit.stats.defence,
            unit.stats.speed,
            unit.fist,
            unit.sword,
            unit.knife,
            unit.unusual,
            unit.hiddenWeapon,
            {},
        });
    }

    std::ranges::sort(setup.allyRoster, {}, &Battle::BattleSetupRosterUnit::sourceOrder);
    std::ranges::sort(setup.enemyRoster, {}, &Battle::BattleSetupRosterUnit::sourceOrder);

    for (const auto& unit : context.units)
    {
        if (unit.team != 0)
        {
            continue;
        }
        setup.cloneSources.push_back({
            unit.unitId,
            unit.realRoleId,
            unit.vitals.maxHp + unit.stats.attack + unit.stats.defence,
            unit.star,
            unit.chessInstanceId,
            unit.sourceOrder,
        });
    }
    for (const auto& [x, y] : context.cloneSpawnCells)
    {
        bool occupied = false;
        for (const auto& unit : context.units)
        {
            if (!unit.alive)
            {
                continue;
            }
            if (unit.gridX == x && unit.gridY == y)
            {
                occupied = true;
                break;
            }
        }
        setup.cloneCells.push_back({ x, y, true, occupied });
    }

    return setup;
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
    const BattleRuntimeSceneSetupInput& setup,
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
            ? requireSetupUnit(setup, clone->sourceUnitId)
            : requireSetupUnit(setup, runtimeUnit.id);
        result.units.push_back(makeInitializedSceneUnit(runtimeUnit, setupUnit, clone));
    }
    return result;
}

void commitInitializedBattleRuntimeConfiguration(
    Battle::BattleRuntimeSession& session,
    const BattleRuntimeBuildContext& context,
    const Battle::BattleInitializationResult& initialization)
{
    const auto& runtime = session.runtime();
    Battle::BattleRuntimeSetupConfiguration config;

    config.world = runtime.world;
    config.world.frame = context.setup.battleFrame;
    config.world.config = context.rules.movementConfig;
    config.world.terrainCells = context.setup.terrainCells;

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

    config.rescue.positionsByCell = context.setup.rescuePositionsByCell;
    config.rescue.cells = context.setup.rescueCells;
    config.rescue.executeUnattendedRadius = context.rules.rescueExecuteUnattendedRadius;
    config.rescue.counterAttack = context.rules.rescueCounterAttack;
    config.rescue.counterAttack.skillId = context.setup.rescueCounterAttackSkillId;

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
            makeBattleDamageUnit(
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

    BattleActionPlanInputContext actionContext;
    actionContext.setupUnits = &context.setup.units;
    actionContext.actionRules = context.rules.action;
    actionContext.castConfig = context.rules.castConfig;
    actionContext.castGeometry = context.rules.castGeometry;
    initializeBattleActionPlanInputs(config.action, actionContext);

    session.commitSetupConfiguration(std::move(config));
}

BattleRuntimeCreationResult createInitializedBattleRuntimeSession(const BattleRuntimeBuildContext& context)
{
    const auto& setup = context.setup;
    assert(setup.comboStates);

    Battle::BattleRuntimeInit init;
    init.runtime.units.gridTransform = context.rules.gridTransform;
    init.runtime.combo.units = *setup.comboStates;
    init.runtime.units.units.reserve(setup.units.size());
    init.runtime.status.units.reserve(setup.units.size());
    init.runtime.world.units.reserve(setup.units.size());
    init.setup = makeBattleRuntimeSetupSeed(setup);

    for (const auto& role : setup.units)
    {
        assert(role.unitId >= 0);
        auto stateIt = setup.comboStates->find(role.unitId);
        init.runtime.units.units.push_back(makeBattleRuntimeUnit(
            role,
            stateIt != setup.comboStates->end() ? &stateIt->second : nullptr,
            context.rules.gridTransform));
        if (stateIt != setup.comboStates->end())
        {
            init.runtime.status.units.push_back(Battle::makeBattleStatusRuntimeUnit(
                makeBattleStatusUnit(role, stateIt->second)));
        }
        else
        {
            init.runtime.status.units.push_back(Battle::BattleStatusRuntimeUnit{ .id = role.unitId });
        }
        init.runtime.world.units.push_back(makeBattleWorldUnit(role));
    }

    BattleRuntimeCreationResult result{
        Battle::BattleRuntimeSession(std::move(init)),
        {},
    };
    auto initialization = result.session.releaseInitializationResult();
    result.sceneInitialization = makeBattleInitializationSceneApplyResult(
        setup,
        initialization,
        result.session.runtime());
    commitInitializedBattleRuntimeConfiguration(result.session, context, initialization);
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

Battle::BattleRuntimeUnit BattleSetupRoleProjection::runtimeUnit(
    const RoleComboState* state,
    const Battle::BattleGridTransform& gridTransform) const
{
    Battle::BattleRuntimeUnit unit;
    unit.id = id();
    unit.realRoleId = realRoleId();
    unit.name = name();
    unit.team = team();
    unit.alive = alive();
    unit.vitals = snapshot_.vitals;
    unit.stats = snapshot_.stats;
    unit.motion = snapshot_.motion;
    unit.animation = snapshot_.animation;
    unit.haveAction = snapshot_.haveAction;
    unit.operationType = snapshot_.operationType;
    unit.operationCount = snapshot_.operationCount;
    unit.physicalPower = snapshot_.physicalPower;
    unit.invincible = invincible();
    unit.hurtFrame = hurtFrame();
    for (int magicType = 0; magicType <= 4; ++magicType)
    {
        unit.actPropertiesByMagicType.emplace(magicType, snapshot_.actPropertiesByMagicType[magicType]);
    }
    unit.grid = gridTransform.toGrid(position());
    if (state)
    {
        unit.shield = state->shield;
        unit.mpBlocked = state->mpBlockTimer > 0;
        unit.mpRecoveryBonusPct = state->mpRecoveryBonusPct;
    }
    return unit;
}

Battle::BattleRuntimeUnit makeBattleRuntimeUnit(
    const BattleSetupUnitInput& snapshot,
    const RoleComboState* state,
    const Battle::BattleGridTransform& gridTransform)
{
    return BattleSetupRoleProjection(snapshot).runtimeUnit(state, gridTransform);
}

Battle::BattleStatusUnitState BattleSetupRoleProjection::statusUnit(const RoleComboState& state) const
{
    Battle::BattleStatusUnitState unit;
    unit.id = id();
    unit.alive = alive();
    unit.hp = hp();
    unit.maxHp = maxHp();
    unit.attack = attack();
    unit.invincible = invincible();
    unit.effects.poisonTimer = state.poisonTimer;
    unit.effects.poisonTickPct = state.poisonTickDmg;
    unit.effects.poisonSourceId = state.poisonSourceId;
    unit.effects.bleedStacks = state.bleedStacks;
    unit.effects.bleedTimer = state.bleedTimer;
    unit.effects.bleedSourceId = state.bleedSourceId;
    unit.effects.frozenTimer = frozen();
    unit.effects.frozenMaxTimer = frozenMax();
    unit.effects.freezeReductionPct = state.freezeReductionPct;
    unit.effects.shieldFreezeResPct = state.shieldFreezeResPct;
    unit.effects.controlImmunityFrames = state.controlImmunityFrames;
    unit.effects.mpBlockTimer = state.mpBlockTimer;
    unit.effects.damageImmunityAfterFrames = state.damageImmunityAfterFrames;
    unit.effects.damageImmunityDuration = state.damageImmunityDuration;
    unit.effects.damageImmunityTimer = state.damageImmunityTimer;

    for (const auto& buff : state.tempAttackBuffs)
    {
        unit.effects.tempAttackBuffs.push_back({ buff.attackBonus, buff.remainingFrames });
    }
    for (const auto& debuff : state.dmgReduceDebuffs)
    {
        unit.effects.damageReduceDebuffs.push_back({ debuff.remainingFrames, debuff.pct });
    }
    return unit;
}

Battle::BattleStatusUnitState makeBattleStatusUnit(const BattleSetupUnitInput& snapshot, const RoleComboState& state)
{
    return BattleSetupRoleProjection(snapshot).statusUnit(state);
}

Battle::BattleDamageUnitState BattleSetupRoleProjection::damageUnit(const RoleComboState* state) const
{
    Battle::BattleDamageUnitState unit;
    unit.id = id();
    unit.alive = alive();
    unit.vitals = { hp(), maxHp(), mp(), maxMp() };
    unit.attack = attack();
    unit.invincible = invincible();
    if (!state)
    {
        return unit;
    }

    unit.hurtInvincFrames = state->hurtInvincFrames;
    unit.shield = state->shield;
    unit.blockFirstHitsRemaining = state->blockFirstHitsRemaining;
    unit.deathPrevention = state->deathPrevention;
    unit.deathPreventionUsed = state->deathPreventionUsed;
    unit.deathPreventionFrames = state->deathPreventionFrames;
    unit.killHealPct = state->killHealPct;
    unit.killInvincFrames = state->killInvincFrames;
    unit.bloodlustAttackPerKill = state->bloodlustATKPerKill;
    unit.mpBlocked = state->mpBlockTimer > 0;
    unit.mpRecoveryBonusPct = state->mpRecoveryBonusPct;
    return unit;
}

Battle::BattleDamageUnitState makeBattleDamageUnit(const BattleSetupUnitInput& snapshot, const RoleComboState* state)
{
    return BattleSetupRoleProjection(snapshot).damageUnit(state);
}

Battle::BattleDamageUnitState makeBattleDamageUnit(const Battle::BattleRuntimeUnit& runtimeUnit, const RoleComboState* state)
{
    Battle::BattleDamageUnitState unit;
    unit.id = runtimeUnit.id;
    unit.alive = runtimeUnit.alive;
    unit.vitals = runtimeUnit.vitals;
    unit.attack = runtimeUnit.stats.attack;
    unit.invincible = runtimeUnit.invincible;
    if (!state)
    {
        return unit;
    }

    unit.hurtInvincFrames = state->hurtInvincFrames;
    unit.shield = state->shield;
    unit.blockFirstHitsRemaining = state->blockFirstHitsRemaining;
    unit.deathPrevention = state->deathPrevention;
    unit.deathPreventionUsed = state->deathPreventionUsed;
    unit.deathPreventionFrames = state->deathPreventionFrames;
    unit.killHealPct = state->killHealPct;
    unit.killInvincFrames = state->killInvincFrames;
    unit.bloodlustAttackPerKill = state->bloodlustATKPerKill;
    unit.mpBlocked = state->mpBlockTimer > 0;
    unit.mpRecoveryBonusPct = state->mpRecoveryBonusPct;
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

Battle::BattleActionSkillSeed makeBattleActionSkillSeed(const BattleSetupSkillSnapshot& skill)
{
    Battle::BattleActionSkillSeed seed;
    if (skill.id < 0)
    {
        return seed;
    }
    seed.id = skill.id;
    seed.name = skill.name;
    seed.soundId = skill.soundId;
    seed.hurtType = skill.hurtType;
    seed.attackAreaType = skill.attackAreaType;
    seed.magicType = skill.magicType;
    seed.visualEffectId = skill.visualEffectId;
    seed.selectDistance = skill.selectDistance;
    seed.actProperty = skill.actProperty;
    seed.magicPower = skill.magicPower;
    return seed;
}

Battle::BattleActionPlanSeed makeBattleActionPlanSeed(const BattleSetupUnitInput& role)
{
    Battle::BattleActionPlanSeed seed;
    seed.unitId = role.unitId;
    seed.hasEquippedSkill = role.hasEquippedSkill;
    seed.normalSkill = makeBattleActionSkillSeed(role.normalSkill);
    seed.ultimateSkill = makeBattleActionSkillSeed(role.ultimateSkill);
    return seed;
}

namespace
{
void initializeBattleActionPlanInputs(
    Battle::BattleRuntimeState::ActionState& action,
    BattleActionPlanInputContext& context)
{
    assert(context.setupUnits);

    action.planSeeds.clear();
    action.castConfig = context.castConfig;
    action.castGeometry = context.castGeometry;
    action.actionRules = context.actionRules;
    for (const auto& role : *context.setupUnits)
    {
        action.planSeeds[role.unitId] = makeBattleActionPlanSeed(role);
    }
}
}  // namespace

}  // namespace KysChess::BattleSceneBattleAdapter
