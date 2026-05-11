#include "BattleRuntimeSession.h"

#include "../ChessCombo.h"
#include "BattleFind.h"

#include <algorithm>
#include <cassert>
#include <ranges>
#include <utility>

namespace KysChess::Battle
{
namespace
{
constexpr double RoleMoveSpeedDivisor = 22.0;
constexpr int ProjectileGraceFrames = 5;
constexpr int NormalDamageTextSize = 30;
constexpr int UltDamageTextSize = 44;

Pointf setupPlacementPosition(const BattleGridTransform& gridTransform, int x, int y)
{
    assert(gridTransform.tileWidth > 0.0);
    assert(gridTransform.coordCount > 0);
    return {
        static_cast<float>(-y * gridTransform.tileWidth + x * gridTransform.tileWidth + gridTransform.coordCount * gridTransform.tileWidth),
        static_cast<float>(y * gridTransform.tileWidth + x * gridTransform.tileWidth),
        0.0f,
    };
}

Pointf setupPlacementFacing(int faceTowards)
{
    switch (faceTowards)
    {
    case 0:
        return { 1.0f, 0.0f, 0.0f };
    case 1:
        return { 0.0f, 1.0f, 0.0f };
    case 2:
        return { -1.0f, 0.0f, 0.0f };
    case 3:
        return { 0.0f, -1.0f, 0.0f };
    default:
        assert(false && "invalid setup facing");
        return { 1.0f, 0.0f, 0.0f };
    }
}

BattlePresentationColor damageTextColor(int team, bool emphasized)
{
    if (team == 0)
    {
        return emphasized
            ? BattlePresentationColor{ 255, 45, 85, 255 }
            : BattlePresentationColor{ 255, 90, 79, 255 };
    }
    return emphasized
        ? BattlePresentationColor{ 47, 128, 255, 255 }
        : BattlePresentationColor{ 102, 207, 255, 255 };
}

BattleDamagePresentationStyle makeDamagePresentationStyle(int team)
{
    BattleDamagePresentationStyle style;
    style.normalDamageColor = damageTextColor(team, false);
    style.emphasizedDamageColor = damageTextColor(team, true);
    style.executeTextColor = { 255, 136, 48, 255 };
    style.normalDamageTextSize = NormalDamageTextSize;
    style.emphasizedDamageTextSize = UltDamageTextSize;
    style.executeTextSize = UltDamageTextSize;
    return style;
}

int getComboLookupId(int realRoleId, const RoleComboState& state)
{
    if (state.isSummonedClone)
    {
        return -1;
    }
    return realRoleId;
}

void configureAttackWorld(
    BattleAttackWorld& world,
    const BattleRuntimeRulesConfig& rules)
{
    world.hitRadius = rules.action.meleeAttackHitRadius;
    world.minimumVectorNorm = rules.minimumVectorNorm;
    world.projectileGraceFrames = ProjectileGraceFrames;
    world.bounceSpawnDistance = rules.projectileFollowUps.areaSpawnDistance;
    world.defaultProjectileSpeed = rules.projectileFollowUps.projectileSpeed;
    world.spendNonThroughOnHit = true;
}

BattleRuntimeUnit makeRuntimeUnit(
    const BattleSetupUnitInput& setup,
    const RoleComboState* combo,
    const BattleGridTransform& gridTransform)
{
    assert(setup.unitId >= 0);

    BattleRuntimeUnit unit;
    unit.id = setup.unitId;
    unit.realRoleId = setup.realRoleId;
    unit.name = setup.name;
    unit.team = setup.team;
    unit.alive = setup.alive;
    unit.vitals = setup.vitals;
    unit.stats = setup.stats;
    unit.motion = setup.motion;
    unit.animation = setup.animation;
    unit.haveAction = setup.haveAction;
    unit.operationType = setup.operationType;
    unit.operationCount = setup.operationCount;
    unit.physicalPower = setup.physicalPower;
    unit.invincible = setup.invincible;
    unit.hurtFrame = setup.hurtFrame;
    for (int magicType = 0; magicType <= 4; ++magicType)
    {
        unit.actPropertiesByMagicType.emplace(magicType, setup.actPropertiesByMagicType[magicType]);
    }
    unit.grid = gridTransform.toGrid(setup.motion.position);
    if (combo)
    {
        unit.shield = combo->shield;
        unit.mpBlocked = combo->mpBlockTimer > 0;
        unit.mpRecoveryBonusPct = combo->mpRecoveryBonusPct;
    }
    return unit;
}

BattleUnitState makeInitializedWorldUnit(
    const BattleRuntimeUnit& runtimeUnit,
    const std::map<int, RoleComboState>& comboStates,
    const BattleWorldState& existingWorld,
    const std::map<int, BattleMovementPhysicsState>& movementRuntime,
    const BattleActionRulesConfig& actionRules)
{
    const auto comboIt = comboStates.find(runtimeUnit.id);
    const bool dashAttackEnabled = comboIt != comboStates.end() && comboIt->second.dashAttack;

    auto unit = makeBattleWorldUnitState(runtimeUnit, RoleMoveSpeedDivisor);
    unit.reach = actionRules.meleeAttackReach;
    unit.style = CombatStyle::Melee;
    unit.taXue = dashAttackEnabled;
    unit.dashAttack = dashAttackEnabled;

    const auto existingMovement = std::ranges::find(existingWorld.units, runtimeUnit.id, &BattleUnitState::id);
    if (existingMovement != existingWorld.units.end())
    {
        unit.assignedSlot = existingMovement->assignedSlot;
        unit.slotSwitchCooldownRemaining = existingMovement->slotSwitchCooldownRemaining;
    }

    const auto movementIt = movementRuntime.find(runtimeUnit.id);
    if (movementIt != movementRuntime.end())
    {
        unit.dashFramesRemaining = movementIt->second.movementDashFrames;
        unit.dashCooldownRemaining = movementIt->second.movementDashCooldown;
        unit.postDashRetreatFramesRemaining = movementIt->second.postDashRetreatFrames;
        unit.postDashChaosFramesRemaining = movementIt->second.postDashChaosFrames;
    }

    return unit;
}

BattleAttackUnit makeAttackUnit(const BattleRuntimeUnit& runtimeUnit)
{
    BattleAttackUnit unit;
    unit.id = runtimeUnit.id;
    unit.team = runtimeUnit.team;
    unit.alive = runtimeUnit.alive;
    unit.invincible = runtimeUnit.invincible > 0;
    unit.hurtFrame = runtimeUnit.hurtFrame > 0;
    unit.position = runtimeUnit.motion.position;
    return unit;
}

BattleDeathEffectStore makeDeathEffectStore(
    const BattleUnitStore& units,
    const std::map<int, RoleComboState>& states)
{
    BattleDeathEffectStore store;
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

        BattleDeathEffectExtras extras;
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

void buildCanonicalRuntime(BattleRuntimeInit& init, const BattleRuntimeSessionCreationInput& input)
{
    init.runtime.units.gridTransform = input.rules.gridTransform;
    init.runtime.random = BattleRuntimeRandom(input.randomSeed);
    init.runtime.combo.units = input.comboStates;
    init.runtime.units.units.reserve(input.units.size());
    init.runtime.status.units.reserve(input.units.size());
    init.runtime.world.units.reserve(input.units.size());
    init.setup = input.setup;

    for (const auto& setup : input.units)
    {
        const auto comboIt = input.comboStates.find(setup.unitId);
        auto runtimeUnit = makeRuntimeUnit(
            setup,
            comboIt != input.comboStates.end() ? &comboIt->second : nullptr,
            input.rules.gridTransform);
        if (comboIt != input.comboStates.end())
        {
            auto statusUnit = makeBattleStatusUnitState(runtimeUnit, comboIt->second);
            statusUnit.effects.frozenTimer = setup.frozen;
            statusUnit.effects.frozenMaxTimer = setup.frozenMax;
            init.runtime.status.units.push_back(makeBattleStatusRuntimeUnit(statusUnit));
        }
        else
        {
            init.runtime.status.units.push_back(BattleStatusRuntimeUnit{ .id = setup.unitId });
        }
        init.runtime.world.units.push_back(makeBattleWorldUnitState(runtimeUnit, RoleMoveSpeedDivisor));
        init.runtime.units.units.push_back(std::move(runtimeUnit));
    }
}

void deriveRuntimeStores(BattleRuntimeState& runtime, BattleRuntimeSessionCreationInput input)
{
    const auto existingWorld = runtime.world;

    runtime.world.frame = input.battleFrame;
    runtime.world.config = input.rules.movementConfig;
    runtime.world.terrainCells = std::move(input.terrainCells);
    runtime.world.units.clear();
    runtime.world.units.reserve(runtime.units.units.size());
    for (const auto& unit : runtime.units.units)
    {
        if (!unit.alive)
        {
            continue;
        }
        runtime.world.units.push_back(makeInitializedWorldUnit(
            unit,
            runtime.combo.units,
            existingWorld,
            runtime.movementRuntime,
            input.rules.action));
    }

    configureAttackWorld(runtime.attacks, input.rules);
    runtime.attacks.units.clear();
    runtime.attacks.units.reserve(runtime.units.units.size());
    for (const auto& unit : runtime.units.units)
    {
        if (unit.alive)
        {
            runtime.attacks.units.push_back(makeAttackUnit(unit));
        }
    }

    runtime.teamEffects.healAuraRadius = input.rules.teamEffectHealAuraRadius;
    runtime.deathEffects.store = makeDeathEffectStore(runtime.units, runtime.combo.units);

    runtime.rescue.cells = std::move(input.rescueCells);
    runtime.rescue.executeUnattendedRadius = input.rules.rescueExecuteUnattendedRadius;
    runtime.rescue.counterAttack = input.rules.rescueCounterAttack;
    runtime.rescue.counterAttack.skillId = input.rescueCounterAttackSkillId;

    runtime.movementPhysics.config = input.rules.movementPhysicsConfig;
    runtime.movementPhysics.collision = input.rules.movementCollisionWorld;
    runtime.movementPhysics.actionCastFrames.assign(
        input.rules.castConfig.castFrames.begin(),
        input.rules.castConfig.castFrames.end());
    runtime.movementPhysics.dashMomentumFrames = input.rules.movementPhysicsDashMomentumFrames;

    runtime.action.castFrames.assign(
        input.rules.castConfig.castFrames.begin(),
        input.rules.castConfig.castFrames.end());
    runtime.action.actionRecoveryFrames = input.rules.action.actionRecoveryFrames;
    runtime.action.dashRecoveryFrames = input.rules.action.dashRecoveryFrames;
    runtime.action.blinkWeakTargetDefWeight = input.rules.action.blinkWeakTargetDefWeight;
    runtime.action.strengthenedMeleeOperationCountThreshold = input.rules.action.strengthenedMeleeOperationCountThreshold;
    runtime.action.projectileBounceRange = input.rules.action.projectileBounceRange;
    runtime.action.planSeeds.clear();
    for (const auto& seed : input.actionPlanSeeds)
    {
        runtime.action.planSeeds.emplace(seed.unitId, seed);
    }
    runtime.action.castConfig = input.rules.castConfig;
    runtime.action.castGeometry = input.rules.castGeometry;
    runtime.action.actionRules = input.rules.action;

    runtime.projectileFollowUps = input.rules.projectileFollowUps;

    runtime.damage.sortPendingDamageByDefenderMagnitude = true;
    runtime.damage.unitExtras.clear();
    runtime.damage.presentationStylesByDefender.clear();
    runtime.damage.unitEffects.clear();
    for (const auto& unit : runtime.units.units)
    {
        const auto stateIt = runtime.combo.units.find(unit.id);
        runtime.damage.unitExtras.push_back(makeBattleDamageRuntimeUnit(
            makeBattleDamageUnitState(
                unit,
                stateIt != runtime.combo.units.end() ? &stateIt->second : nullptr)));
        runtime.damage.presentationStylesByDefender.emplace(unit.id, makeDamagePresentationStyle(unit.team));
        if (stateIt != runtime.combo.units.end() && stateIt->second.deathAOEPct > 0)
        {
            runtime.damage.unitEffects.emplace(
                unit.id,
                BattleDamageApplicationUnitEffects{
                    stateIt->second.deathAOEPct,
                    stateIt->second.deathAOEStunFrames,
                    stateIt->second.deathAOEMaxTargets,
                });
        }
    }
}

}  // namespace

BattleRuntimeSession::BattleRuntimeSession(BattleRuntimeInit init)
    : runtime_(std::move(init.runtime))
{
    initialization_result_ = BattleInitializationSystem().initialize(runtime_, init.setup);
}

BattleRuntimeSession BattleRuntimeSession::createInitialized(BattleRuntimeSessionCreationInput input)
{
    BattleRuntimeInit init;
    buildCanonicalRuntime(init, input);

    BattleRuntimeSession session(std::move(init));
    deriveRuntimeStores(session.runtime_, std::move(input));
    return session;
}

BattleFrameResult BattleRuntimeSession::runFrame()
{
    frameStarted_ = true;
    return runner_.runFrame(runtime_);
}

void BattleRuntimeSession::commitSetupPlacement(const BattleSetupPlacementInput& input)
{
    assert(!setupPlacementCommitted_);
    assert(!frameStarted_);
    setupPlacementCommitted_ = true;

    for (const auto& unitPlacement : input.units)
    {
        const auto position = setupPlacementPosition(runtime_.units.gridTransform, unitPlacement.x, unitPlacement.y);
        runtime_.units.setPosition(unitPlacement.unitId, position);
        auto& unit = runtime_.units.requireUnit(unitPlacement.unitId);
        unit.motion.facing = setupPlacementFacing(unitPlacement.faceTowards);

        auto& worldUnit = requireById(runtime_.world.units, unitPlacement.unitId);
        worldUnit.position = position;
        worldUnit.velocity = { 0, 0, 0 };
    }
}

BattleInitializationResult BattleRuntimeSession::releaseInitializationResult()
{
    assert(initialization_result_.has_value());
    BattleInitializationResult result = std::move(*initialization_result_);
    initialization_result_.reset();
    return result;
}

const BattleRuntimeState& BattleRuntimeSession::runtime() const
{
    return runtime_;
}

}  // namespace KysChess::Battle
