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
#include <set>
#include <string>
#include <utility>

namespace KysChess::BattleSceneBattleAdapter
{

void configureBattleAttackWorld(
    Battle::BattleAttackWorld& world,
    const Battle::BattleRuntimeRulesConfig& rules);
Battle::BattleRuntimeUnit makeBattleRuntimeUnit(
    const BattleSetupRoleSnapshot& snapshot,
    const RoleComboState* state,
    const Battle::BattleGridTransform& gridTransform);
Battle::BattleStatusUnitState makeBattleStatusUnit(const BattleSetupRoleSnapshot& snapshot, const RoleComboState& state);
Battle::BattleDamageUnitState makeBattleDamageUnit(const BattleSetupRoleSnapshot& snapshot, const RoleComboState* state);
Battle::BattleDamagePresentationStyle makeBattleDamagePresentationStyle(int team);

namespace
{
constexpr double ROLE_MOVE_SPEED_DIVISOR = 22.0;

struct BattleActionPlanInputContext
{
    const std::vector<BattleSetupRoleSnapshot>* setupRoles = nullptr;
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
    explicit BattleSetupRoleProjection(const BattleSetupRoleSnapshot& snapshot)
        : snapshot_(snapshot)
    {
        assert(snapshot_.unitId >= 0);
    }

    int id() const { return snapshot_.unitId; }
    int realRoleId() const { return snapshot_.realRoleId; }
    const std::string& name() const { return snapshot_.name; }
    int team() const { return snapshot_.team; }
    bool alive() const { return snapshot_.alive; }
    int hp() const { return snapshot_.hp; }
    int maxHp() const { return snapshot_.maxHp; }
    int mp() const { return snapshot_.mp; }
    int maxMp() const { return snapshot_.maxMp; }
    int attack() const { return snapshot_.attack; }
    int defence() const { return snapshot_.defence; }
    int speed() const { return snapshot_.speed; }
    int star() const { return snapshot_.star; }
    int invincible() const { return snapshot_.invincible; }
    int hurtFrame() const { return snapshot_.hurtFrame; }
    int frozen() const { return snapshot_.frozen; }
    int frozenMax() const { return snapshot_.frozenMax; }
    Pointf position() const { return snapshot_.position; }
    Pointf velocity() const { return snapshot_.velocity; }
    Pointf acceleration() const { return snapshot_.acceleration; }
    Pointf facing() const { return snapshot_.facing; }

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
    const BattleSetupRoleSnapshot& snapshot_;
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
    unit.canAttack = snapshot_.cooldown == 0;
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

Battle::BattleUnitState makeBattleWorldUnit(const BattleSetupRoleSnapshot& snapshot)
{
    return BattleSetupRoleProjection(snapshot).worldUnit();
}

Battle::BattleUnitState makeInitializedBattleWorldUnit(
    const BattleSetupRoleSnapshot& snapshot,
    const std::map<int, RoleComboState>& comboStates,
    const Battle::BattleWorldState& world,
    const std::map<int, Battle::BattleMovementPhysicsState>& movementRuntime,
    const Battle::BattleActionRulesConfig& actionRules)
{
    return BattleSetupRoleProjection(snapshot).initializedWorldUnit(
        comboStates,
        world,
        movementRuntime,
        actionRules);
}

Battle::BattleDeathEffectStore makeBattleDeathEffectStore(
    const std::vector<BattleSetupRoleSnapshot>& roles,
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

    for (const auto& role : roles)
    {
        const auto stateIt = states.find(role.unitId);
        assert(stateIt != states.end());

        Battle::BattleDeathEffectExtras extras;
        extras.id = role.unitId;
        extras.shieldPctMaxHp = stateIt->second.shieldPctMaxHP;
        extras.shieldOnAllyDeathTracker = stateIt->second.shieldOnAllyDeathTracker;
        extras.appliedEffects = stateIt->second.appliedEffects;

        const int comboLookupId = getComboLookupId(role.realRoleId, stateIt->second);
        if (comboLookupId >= 0)
        {
            extras.comboIds = KysChess::ChessCombo::getCombosForRole(comboLookupId);
        }

        store.units.push_back(std::move(extras));
    }

    return store;
}

const BattleSetupRoleSnapshot& requireSetupRoleSnapshot(
    const std::vector<BattleSetupRoleSnapshot>& roles,
    int unitId)
{
    return Battle::requireBy(roles, unitId, &BattleSetupRoleSnapshot::unitId);
}

const Battle::BattleInitializationCloneIntent* findCloneIntent(
    const Battle::BattleInitializationResult& initialization,
    int cloneUnitId)
{
    return Battle::tryFindBy(initialization.cloneIntents, cloneUnitId, &Battle::BattleInitializationCloneIntent::cloneUnitId);
}

BattleSetupRoleSnapshot makeRuntimeSetupRoleSnapshot(
    const Battle::BattleRuntimeUnit& unit,
    const BattleSetupRoleSnapshot& source)
{
    auto snapshot = source;
    snapshot.unitId = unit.id;
    snapshot.realRoleId = unit.realRoleId;
    snapshot.team = unit.team;
    snapshot.alive = unit.alive;
    snapshot.position = unit.position;
    snapshot.velocity = unit.velocity;
    snapshot.acceleration = unit.acceleration;
    snapshot.facing = unit.facing;
    snapshot.gridX = unit.grid.x;
    snapshot.gridY = unit.grid.y;
    snapshot.star = unit.star;
    snapshot.cost = unit.cost;
    snapshot.hp = unit.hp;
    snapshot.maxHp = unit.maxHp;
    snapshot.mp = unit.mp;
    snapshot.maxMp = unit.maxMp;
    snapshot.attack = unit.attack;
    snapshot.defence = unit.defence;
    snapshot.speed = unit.speed;
    snapshot.cooldown = unit.cooldown;
    snapshot.cooldownMax = unit.cooldownMax;
    snapshot.haveAction = unit.haveAction;
    snapshot.actFrame = unit.actFrame;
    snapshot.operationType = unit.operationType;
    snapshot.actType = unit.actType;
    snapshot.operationCount = unit.operationCount;
    snapshot.physicalPower = unit.physicalPower;
    snapshot.invincible = unit.invincible;
    snapshot.hurtFrame = unit.hurtFrame;
    return snapshot;
}

std::vector<BattleSetupRoleSnapshot> makeOrderedRuntimeRoleSnapshots(
    const Battle::BattleRuntimeState& runtime,
    const BattleRuntimeSceneSetupInput& setup,
    const Battle::BattleInitializationResult& initialization)
{
    std::vector<BattleSetupRoleSnapshot> roles;
    roles.reserve(runtime.units.units.size());
    for (const auto& unit : runtime.units.units)
    {
        if (const auto* source = Battle::tryFindBy(setup.roles, unit.id, &BattleSetupRoleSnapshot::unitId))
        {
            roles.push_back(makeRuntimeSetupRoleSnapshot(unit, *source));
            continue;
        }

        const auto* clone = findCloneIntent(initialization, unit.id);
        assert(clone);
        const auto& source = requireSetupRoleSnapshot(setup.roles, clone->sourceUnitId);
        roles.push_back(makeRuntimeSetupRoleSnapshot(unit, source));
    }
    return roles;
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
    setup.allyRoster = context.allyRoster;
    setup.enemyRoster = context.enemyRoster;
    setup.comboDefinitions = makeBattleSetupComboDefinitions();
    setup.equipmentDefinitions = makeBattleSetupEquipmentDefinitions();
    setup.equipmentSynergies = makeBattleSetupEquipmentSynergyDefinitions();
    setup.neigongDefinitions = makeBattleSetupNeigongDefinitions();
    setup.obtainedNeigongMagicIds = context.obtainedNeigongMagicIds;

    for (const auto& unit : context.allyRoster)
    {
        setup.units.push_back({
            unit.unitId,
            unit.realRoleId,
            unit.team,
            unit.star,
            unit.cost,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            {},
        });
    }
    for (const auto& unit : context.enemyRoster)
    {
        setup.units.push_back({
            unit.unitId,
            unit.realRoleId,
            unit.team,
            unit.star,
            unit.cost,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            {},
        });
    }

    for (const auto& roster : { &context.allyRoster, &context.enemyRoster })
    {
        for (const auto& unit : *roster)
        {
            const auto& role = requireSetupRoleSnapshot(context.roles, unit.unitId);

            const auto seedIt = std::find_if(
                setup.units.begin(),
                setup.units.end(),
                [&unit](const Battle::BattleInitializationUnitSeed& seed)
                {
                    return seed.unitId == unit.unitId;
                });
            if (seedIt == setup.units.end())
            {
                continue;
            }

            seedIt->baseMaxHp = role.maxHp;
            seedIt->baseAttack = role.attack;
            seedIt->baseDefence = role.defence;
            seedIt->baseSpeed = role.speed;
            seedIt->baseFist = role.fist;
            seedIt->baseSword = role.sword;
            seedIt->baseKnife = role.knife;
            seedIt->baseUnusual = role.unusual;
            seedIt->baseHiddenWeapon = role.hiddenWeapon;
        }
    }

    for (const auto& unit : context.allyRoster)
    {
        setup.cloneSources.push_back({
            unit.unitId,
            unit.realRoleId,
            0,
            unit.star,
            unit.chessInstanceId,
            unit.sourceOrder,
        });
    }
    for (auto& source : setup.cloneSources)
    {
        const auto& role = requireSetupRoleSnapshot(context.roles, source.sourceUnitId);
        source.power = role.maxHp + role.attack + role.defence;
    }
    for (const auto& [x, y] : context.cloneSpawnCells)
    {
        bool occupied = false;
        for (const auto& role : context.roles)
        {
            if (!role.alive)
            {
                continue;
            }
            if (role.gridX == x && role.gridY == y)
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

const Battle::BattleSetupRosterUnit* tryFindRosterUnit(
    const BattleRuntimeSceneSetupInput& setup,
    int unitId)
{
    if (const auto* ally = Battle::tryFindBy(setup.allyRoster, unitId, &Battle::BattleSetupRosterUnit::unitId))
    {
        return ally;
    }
    return Battle::tryFindBy(setup.enemyRoster, unitId, &Battle::BattleSetupRosterUnit::unitId);
}

BattleSceneUnit makeInitializedSceneUnit(
    const BattleSetupRoleSnapshot& role,
    const BattleRuntimeSceneSetupInput& setup,
    const Battle::BattleInitializationResult& initialization)
{
    const auto* clone = findCloneIntent(initialization, role.unitId);
    const auto* roster = clone ? nullptr : tryFindRosterUnit(setup, role.unitId);

    BattleSceneUnit unit;
    unit.identity = {
        role.unitId,
        role.realRoleId,
        role.team,
        role.headId,
        role.name,
    };
    unit.unitId = role.unitId;
    unit.sourceUnitId = clone ? clone->sourceUnitId : role.unitId;
    unit.gridX = role.gridX;
    unit.gridY = role.gridY;
    unit.faceTowards = role.faceTowards;
    unit.position = role.position;
    unit.velocity = role.velocity;
    unit.acceleration = role.acceleration;
    unit.realTowards = role.facing;
    unit.fightFrames = role.fightFrames;
    unit.star = role.star;
    unit.chessInstanceId = roster ? roster->chessInstanceId : -1;
    unit.weaponId = roster ? roster->weaponId : -1;
    unit.armorId = roster ? roster->armorId : -1;
    unit.cost = role.cost;
    unit.hp = role.hp;
    unit.maxHp = role.maxHp;
    unit.mp = role.mp;
    unit.maxMp = role.maxMp;
    unit.attack = role.attack;
    unit.defence = role.defence;
    unit.speed = role.speed;
    unit.cooldown = role.cooldown;
    unit.cooldownMax = role.cooldownMax;
    unit.frozen = role.frozen;
    unit.frozenMax = role.frozenMax;
    unit.invincible = role.invincible;
    unit.actType = role.actType;
    unit.actFrame = role.actFrame;
    unit.alive = role.alive;
    unit.active = role.alive;
    unit.skillNames = role.skillNames;
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

    const auto orderedRoles = makeOrderedRuntimeRoleSnapshots(runtime, setup, initialization);
    result.units.reserve(orderedRoles.size());
    for (const auto& role : orderedRoles)
    {
        result.units.push_back(makeInitializedSceneUnit(role, setup, initialization));
    }
    return result;
}

void commitInitializedBattleRuntimeConfiguration(
    Battle::BattleRuntimeSession& session,
    const BattleRuntimeBuildContext& context,
    const Battle::BattleInitializationResult& initialization)
{
    const auto& runtime = session.runtime();
    const auto orderedRoles = makeOrderedRuntimeRoleSnapshots(runtime, context.setup, initialization);
    Battle::BattleRuntimeSetupConfiguration config;

    config.world = runtime.world;
    config.world.frame = context.setup.battleFrame;
    config.world.config = context.rules.movementConfig;
    config.world.terrainCells = context.setup.terrainCells;

    const auto existingWorld = runtime.world;
    config.world.units.clear();
    config.world.units.reserve(orderedRoles.size());
    for (const auto& role : orderedRoles)
    {
        if (!role.alive)
        {
            continue;
        }
        config.world.units.push_back(makeInitializedBattleWorldUnit(
            role,
            runtime.combo.units,
            existingWorld,
            runtime.movementRuntime,
            context.rules.action));
    }

    config.attacks = runtime.attacks;
    configureBattleAttackWorld(config.attacks, context.rules);

    config.teamEffects.healAuraRadius = context.rules.teamEffectHealAuraRadius;
    config.deathEffects.store = makeBattleDeathEffectStore(orderedRoles, runtime.combo.units);

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
    for (const auto& role : orderedRoles)
    {
        const auto stateIt = runtime.combo.units.find(role.unitId);
        config.damage.unitExtras.push_back(Battle::makeBattleDamageRuntimeUnit(
            makeBattleDamageUnit(
                role,
                stateIt != runtime.combo.units.end() ? &stateIt->second : nullptr)));
        config.damage.presentationStylesByDefender.emplace(role.unitId, makeBattleDamagePresentationStyle(role.team));
        if (stateIt != runtime.combo.units.end() && stateIt->second.deathAOEPct > 0)
        {
            config.damage.unitEffects.emplace(
                role.unitId,
                Battle::BattleDamageApplicationUnitEffects{
                    stateIt->second.deathAOEPct,
                    stateIt->second.deathAOEStunFrames,
                    stateIt->second.deathAOEMaxTargets,
                });
        }
    }

    BattleActionPlanInputContext actionContext;
    actionContext.setupRoles = &orderedRoles;
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
    init.runtime.units.units.reserve(setup.roles.size());
    init.runtime.status.units.reserve(setup.roles.size());
    init.runtime.world.units.reserve(setup.roles.size());
    init.setup = makeBattleRuntimeSetupSeed(setup);

    for (const auto& role : setup.roles)
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
    unit.hp = hp();
    unit.maxHp = maxHp();
    unit.mp = mp();
    unit.maxMp = maxMp();
    unit.attack = attack();
    unit.defence = defence();
    unit.speed = speed();
    unit.cooldown = snapshot_.cooldown;
    unit.cooldownMax = snapshot_.cooldownMax;
    unit.haveAction = snapshot_.haveAction;
    unit.actFrame = snapshot_.actFrame;
    unit.operationType = snapshot_.operationType;
    unit.actType = snapshot_.actType;
    unit.operationCount = snapshot_.operationCount;
    unit.physicalPower = snapshot_.physicalPower;
    unit.invincible = invincible();
    unit.hurtFrame = hurtFrame();
    for (int magicType = 0; magicType <= 4; ++magicType)
    {
        unit.actPropertiesByMagicType.emplace(magicType, snapshot_.actPropertiesByMagicType[magicType]);
    }
    unit.position = position();
    unit.velocity = velocity();
    unit.acceleration = acceleration();
    unit.facing = facing();
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
    const BattleSetupRoleSnapshot& snapshot,
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
    unit.poisonTimer = state.poisonTimer;
    unit.poisonTickPct = state.poisonTickDmg;
    unit.poisonSourceId = state.poisonSourceId;
    unit.bleedStacks = state.bleedStacks;
    unit.bleedTimer = state.bleedTimer;
    unit.bleedSourceId = state.bleedSourceId;
    unit.frozenTimer = frozen();
    unit.frozenMaxTimer = frozenMax();
    unit.freezeReductionPct = state.freezeReductionPct;
    unit.shieldFreezeResPct = state.shieldFreezeResPct;
    unit.controlImmunityFrames = state.controlImmunityFrames;
    unit.mpBlockTimer = state.mpBlockTimer;
    unit.damageImmunityAfterFrames = state.damageImmunityAfterFrames;
    unit.damageImmunityDuration = state.damageImmunityDuration;
    unit.damageImmunityTimer = state.damageImmunityTimer;

    for (const auto& buff : state.tempAttackBuffs)
    {
        unit.tempAttackBuffs.push_back({ buff.attackBonus, buff.remainingFrames });
    }
    for (const auto& debuff : state.dmgReduceDebuffs)
    {
        unit.damageReduceDebuffs.push_back({ debuff.remainingFrames, debuff.pct });
    }
    return unit;
}

Battle::BattleStatusUnitState makeBattleStatusUnit(const BattleSetupRoleSnapshot& snapshot, const RoleComboState& state)
{
    return BattleSetupRoleProjection(snapshot).statusUnit(state);
}

Battle::BattleDamageUnitState BattleSetupRoleProjection::damageUnit(const RoleComboState* state) const
{
    Battle::BattleDamageUnitState unit;
    unit.id = id();
    unit.alive = alive();
    unit.hp = hp();
    unit.maxHp = maxHp();
    unit.mp = mp();
    unit.maxMp = maxMp();
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

Battle::BattleDamageUnitState makeBattleDamageUnit(const BattleSetupRoleSnapshot& snapshot, const RoleComboState* state)
{
    return BattleSetupRoleProjection(snapshot).damageUnit(state);
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

Battle::BattleActionPlanSeed makeBattleActionPlanSeed(const BattleSetupRoleSnapshot& role)
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
    assert(context.setupRoles);

    action.planSeeds.clear();
    action.castConfig = context.castConfig;
    action.castGeometry = context.castGeometry;
    action.actionRules = context.actionRules;
    for (const auto& role : *context.setupRoles)
    {
        action.planSeeds[role.unitId] = makeBattleActionPlanSeed(role);
    }
}
}  // namespace

}  // namespace KysChess::BattleSceneBattleAdapter
