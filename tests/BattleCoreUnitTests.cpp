#include "battle/BattleCombatIntent.h"
#include "battle/BattleCore.h"
#include "battle/BattleFind.h"
#include "battle/BattleMovement.h"
#include "battle/BattlePresentationPlayback.h"
#include "battle/BattleRuntimeRules.h"
#include "BattleSceneBattleAdapter.h"
#include "ChessEftIds.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

using namespace KysChess::Battle;

namespace
{

constexpr double SceneTileWidth = 36.0;
constexpr double MaxEffectiveBattleReach = 480.0;
constexpr double SceneAttackHitRadius = SceneTileWidth * 2.0;
constexpr double SceneBounceSpawnDistance = SceneTileWidth * 1.5;
constexpr double SceneProjectileSpeed = SceneTileWidth / 3.0;
constexpr double LegacyMinimumVectorNorm = 0.0001;
constexpr int BattleCoordCount = 64;

TEST_CASE("BattleRuntimeRules_HadesRulesDeriveCurrentSceneValuesFromGrid")
{
    const auto rules = makeHadesBattleRuntimeRules(SceneTileWidth, BattleCoordCount);

    REQUIRE(rules.gridTransform.tileWidth == SceneTileWidth);
    REQUIRE(rules.gridTransform.coordCount == BattleCoordCount);
    REQUIRE(rules.projectileFollowUps.projectileSpeed == SceneProjectileSpeed);
    REQUIRE(rules.projectileFollowUps.minimumProjectileFrames == 20);
    REQUIRE(rules.projectileFollowUps.nearbyProjectileFramePadding == 18);
    REQUIRE(rules.projectileFollowUps.areaProjectileFramePadding == 15);
    REQUIRE(rules.projectileFollowUps.areaSpawnDistance == SceneTileWidth * 1.5);
    REQUIRE(rules.rescueCounterAttack.projectileSpeed == SceneProjectileSpeed);
    REQUIRE(rules.rescueCounterAttack.meleeAttackEffectOffset == SceneTileWidth * 2.0);
    REQUIRE(rules.action.actionRecoveryFrames == 4);
    REQUIRE(rules.action.dashRecoveryFrames == 5);
    REQUIRE(rules.movementPhysicsDashMomentumFrames == 5);
    REQUIRE(rules.action.projectileBounceRange == 90);
}

TEST_CASE("BattleHitResolver_UsesGroupedUnitSnapshotFields", "[battle][hit]")
{
    BattleHitUnitSnapshot attacker;
    attacker.id = 1;
    attacker.team = 0;
    attacker.alive = true;
    attacker.vitals = { 80, 100, 10, 20 };
    attacker.stats = { 30, 12, 9 };
    attacker.motion.position = { 1, 2, 0 };
    attacker.motion.facing = { 1, 0, 0 };
    attacker.animation = { 0, 5, 2, 1 };

    CHECK(attacker.vitals.hp == 80);
    CHECK(attacker.stats.attack == 30);
    CHECK(attacker.motion.position.x == 1);
    CHECK(attacker.animation.actType == 1);
}

TEST_CASE("BattleFind_RequireDenseByIdIndexesVectorByUnitId", "[battle][find][unit]")
{
    std::vector<BattleRuntimeUnit> units;
    BattleRuntimeUnit first;
    first.id = 0;
    first.vitals.hp = 10;
    units.push_back(first);
    BattleRuntimeUnit second;
    second.id = 1;
    second.vitals.hp = 20;
    units.push_back(second);

    CHECK(requireDenseById(units, 0).vitals.hp == 10);
    CHECK(requireDenseById(units, 1).vitals.hp == 20);

    requireDenseById(units, 1).vitals.hp = 25;
    CHECK(units[1].vitals.hp == 25);
}
TEST_CASE("BattleFind_TryDenseByIdReturnsNullForMissingDenseIndex", "[battle][find][unit]")
{
    std::vector<BattleRuntimeUnit> units;
    BattleRuntimeUnit only;
    only.id = 0;
    units.push_back(only);

    CHECK(tryDenseById(units, -1) == nullptr);
    CHECK(tryDenseById(units, 1) == nullptr);
}

BattleFrameResult runBattleFrame(BattleRuntimeState& state)
{
    if (state.action.castFrames.empty())
    {
        state.action.castFrames = { 6, 6, 6, 6 };
        state.action.actionRecoveryFrames = 4;
        state.action.dashRecoveryFrames = 5;
    }
    return BattleFrameRunner().runFrame(state);
}

BattleMovementConfig testConfig()
{
    BattleMovementGeometry geometry;
    geometry.tileWidth = SceneTileWidth;
    geometry.meleeAttackEffectOffset = SceneTileWidth * 2.0;
    geometry.meleeAttackHitRadius = SceneTileWidth * 2.0;
    geometry.dashFrames = 5;
    geometry.dashCooldownFrames = 18;
    geometry.maxRangedReach = MaxEffectiveBattleReach;
    return BattleGeometry(geometry).movementConfig();
}

BattleUnitState unit(int id, int team, Pointf position, CombatStyle style = CombatStyle::Melee)
{
    BattleUnitState state;
    state.id = id;
    state.realRoleId = id;
    state.name = std::to_string(id);
    state.team = team;
    state.position = position;
    state.speed = 5.0;
    state.reach = style == CombatStyle::Ranged ? 400.0 : 137.5;
    state.style = style;
    return state;
}

BattleWorldState worldWith(std::vector<BattleUnitState> units)
{
    BattleWorldState world;
    world.config = testConfig();
    world.units = std::move(units);
    return world;
}

BattleRuntimeUnit runtimeUnitSnapshot(int id, int team, int hp, Pointf position = {})
{
    BattleRuntimeUnit unit;
    unit.id = id;
    unit.team = team;
    unit.alive = hp > 0;
    unit.vitals = { hp, 100, 20, 50 };
    unit.stats = { 30, 5, 20 };
    unit.animation = { 10, 60, 0, 0 };
    unit.physicalPower = 3;
    unit.motion.position = position;
    unit.motion.facing = { 1, 0, 0 };
    return unit;
}

void seedRuntimeUnitsFromWorld(BattleRuntimeState& state, int hp = 100)
{
    if (state.units.gridTransform.tileWidth <= 0.0)
    {
        state.units.gridTransform = { SceneTileWidth, 64 };
    }
    state.units.units.clear();
    for (const auto& unit : state.world.units)
    {
        auto runtime = runtimeUnitSnapshot(unit.id, unit.team, hp, unit.position);
        runtime.alive = unit.alive;
        runtime.motion.velocity = unit.velocity;
        runtime.style = unit.style;
        runtime.grid = state.units.gridTransform.toGrid(runtime.motion.position);
        state.units.units.push_back(std::move(runtime));
    }
}

TEST_CASE("BattleUnitStore_RequiresAndMutatesCanonicalUnitValues", "[battle][core][runtime]")
{
    BattleUnitStore store;
    BattleRuntimeUnit unit;
    unit.id = 0;
    unit.team = 0;
    unit.alive = true;
    unit.vitals = { 80, 100, 10, 50 };
    unit.stats.attack = 20;
    unit.stats.defence = 7;
    unit.shield = 3;
    store.units.push_back(unit);

    BattleDamageUnitState damage;
    damage.id = 0;
    damage.vitals.hp = 25;
    damage.vitals.maxHp = 100;
    damage.alive = false;
    damage.vitals.mp = 18;
    damage.vitals.maxMp = 60;
    damage.attack = 31;
    damage.invincible = 4;
    damage.shield = 9;
    damage.mpBlocked = true;
    damage.mpRecoveryBonusPct = 25;
    store.writeDamageUnit(damage);

    const auto& updated = store.requireUnit(0);
    CHECK_FALSE(updated.alive);
    CHECK(updated.vitals.hp == 25);
    CHECK(updated.vitals.mp == 18);
    CHECK(updated.vitals.maxMp == 60);
    CHECK(updated.stats.attack == 31);
    CHECK(updated.invincible == 4);
    CHECK(updated.shield == 9);
    CHECK(updated.mpBlocked);
    CHECK(updated.mpRecoveryBonusPct == 25);
}

TEST_CASE("BattleUnitStore_UpdatesPositionAndGridWithCoreTransform", "[battle][core][runtime]")
{
    BattleUnitStore store;
    store.gridTransform.tileWidth = SceneTileWidth;
    store.gridTransform.coordCount = 64;
    BattleRuntimeUnit unit;
    unit.id = 0;
    unit.motion.position = { 64.0f * 36.0f, 0.0f, 0.0f };
    store.units.push_back(unit);

    store.setPosition(0, { 64.0f * 36.0f + 36.0f, 36.0f, 0.0f });

    const auto& updated = store.requireUnit(0);
    CHECK(updated.motion.position.x == 64.0f * 36.0f + 36.0f);
    CHECK(updated.motion.position.y == 36.0f);
    CHECK(updated.grid.x == 1);
    CHECK(updated.grid.y == 0);
}

TEST_CASE("BattleUnitStore_SelectsNearestAndFarthestLiveEnemyUnits", "[battle][core][runtime]")
{
    BattleUnitStore store;
    store.units = {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 100, { 130, 100, 0 }),
        runtimeUnitSnapshot(2, 1, 100, { 260, 100, 0 }),
        runtimeUnitSnapshot(3, 0, 100, { 110, 100, 0 }),
        runtimeUnitSnapshot(4, 1, 0, { 105, 100, 0 }),
    };

    CHECK(findNearestEnemyUnitId(store, 0) == 1);
    CHECK(findFarthestEnemyUnitId(store, 0) == 2);
}

TEST_CASE("BattleUnitStore_TargetSelectionReturnsNoUnitWithoutLiveEnemy", "[battle][core][runtime]")
{
    BattleUnitStore store;
    store.units = {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 0, 100, { 130, 100, 0 }),
        runtimeUnitSnapshot(2, 1, 0, { 260, 100, 0 }),
    };

    CHECK(findNearestEnemyUnitId(store, 0) == -1);
    CHECK(findFarthestEnemyUnitId(store, 0) == -1);
}

BattleAttackWorld attackWorld()
{
    BattleAttackWorld world;
    world.hitRadius = SceneAttackHitRadius;
    world.minimumVectorNorm = LegacyMinimumVectorNorm;
    world.bounceSpawnDistance = SceneBounceSpawnDistance;
    world.defaultProjectileSpeed = SceneProjectileSpeed;
    return world;
}

KysChess::AppliedEffectInstance triggeredEffect(KysChess::EffectType type,
                                                KysChess::Trigger trigger,
                                                int value,
                                                int triggerValue = 0,
                                                int duration = 0)
{
    KysChess::AppliedEffectInstance effect;
    effect.type = type;
    effect.trigger = trigger;
    effect.value = value;
    effect.triggerValue = triggerValue;
    effect.duration = duration;
    return effect;
}

BattleSkillState skill(int attackAreaType, double reach = 400.0, bool forceRanged = false)
{
    BattleSkillState state;
    state.id = 1;
    state.name = "test";
    state.attackAreaType = attackAreaType;
    state.magicType = 1;
    state.reach = reach;
    state.forceRanged = forceRanged;
    state.rangedStyle = forceRanged || attackAreaType == 1 || attackAreaType == 2 || attackAreaType == 3;
    return state;
}

BattleAttackSpawnRequest attackSpawnRequest()
{
    BattleAttackSpawnRequest request;
    request.initial.attackerUnitId = 0;
    request.initial.skillId = 101;
    request.initial.operationType = BattleOperationType::RangedProjectile;
    request.initial.visualEffectId = 44;
    request.initial.preferredTargetUnitId = 0;
    request.initial.position = { 100, 120, 0 };
    request.initial.velocity = { 6, 0, 0 };
    request.initial.totalFrame = 30;
    request.initial.track = true;
    request.initial.through = true;
    request.initial.sharedHitGroupId = 7;
    return request;
}

BattleStatusUnitState statusUnitSnapshot(int id, int hp)
{
    BattleStatusUnitState state;
    state.id = id;
    state.alive = true;
    state.hp = hp;
    state.maxHp = 100;
    return state;
}

BattleStatusRuntimeUnit statusRuntimeSnapshot(int id, int hp)
{
    return makeBattleStatusRuntimeUnit(statusUnitSnapshot(id, hp));
}

struct HitDamageFrameState
{
    BattleRuntimeState state;
};

HitDamageFrameState hitDamageFrameState(int resolvedBaseDamage, int defenderHp)
{
    HitDamageFrameState frame;
    auto& state = frame.state;
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 105, 100, 0 }),
    });
    state.attacks = attackWorld();
    state.attacks.units = {
        { 0, 0, true, false, false, { 100, 100, 0 } },
        { 1, 1, true, false, false, { 105, 100, 0 } },
    };
    seedRuntimeUnitsFromWorld(state);
    state.status.units = {
        statusRuntimeSnapshot(0, 100),
        statusRuntimeSnapshot(1, 100),
    };
    state.deathEffects.store.units = { { 0 }, { 1 } };
    state.status.units = {
        statusRuntimeSnapshot(0, 100),
        statusRuntimeSnapshot(1, 100),
    };
    state.deathEffects.store.units = { { 0 }, { 1 } };
    state.status.units = {
        statusRuntimeSnapshot(0, 100),
        statusRuntimeSnapshot(1, 100),
    };
    state.deathEffects.store.units = { { 0 }, { 1 } };

    BattleAttackInstance projectile;
    projectile.id = 10;
    projectile.state.attackerUnitId = 0;
    projectile.state.skillId = 101;
    projectile.state.skillMagicPower = resolvedBaseDamage * 12;
    projectile.state.totalFrame = 30;
    projectile.state.operationType = BattleOperationType::RangedProjectile;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };
    state.attacks.attacks.push_back(projectile);

    state.units.units = {
        runtimeUnitSnapshot(0, 0, 80, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, defenderHp, { 105, 100, 0 }),
    };
    state.deathEffects.store.units = { { 0 }, { 1 } };
    state.status.units = {
        statusRuntimeSnapshot(0, 80),
        statusRuntimeSnapshot(1, defenderHp),
    };
    return frame;
}

BattleCastConfig frameCastConfig()
{
    BattleCastConfig config;
    config.castFrames = { 25, 30, 20, 25 };
    config.baseCooldownFrames = { 105, 185, 115, 45 };
    config.minimumCooldownFrames = { 60, 70, 70, 45 };
    config.cooldownActPropertyDivisors = { 2, 1, 2, 0 };
    config.recoveryFrames = { 4, 4, 4, 5 };
    config.maxCooldownSpeed = 150;
    config.speedCooldownReductionRatio = 0.5;
    config.minimumCooldownAfterCastPadding = 2;
    config.normalCastMpDelta = 5;
    config.minimumFacingNorm = LegacyMinimumVectorNorm;
    config.meleeHitTotalFrame = 10;
    config.strengthenedMeleeTotalFrame = 30;
    config.strengthenedMeleeSelectDistanceDivisor = 2.0;
    config.strengthenedMeleeMultiplier = 2.0f;
    config.meleeSplashTotalFrame = 60;
    config.meleeSplashInitialFrame = 5;
    config.meleeSplashStrengthMultiplier = 0.5f;
    config.trackingProjectileTotalFrame = 120;
    config.dashHitTotalFrame = 30;
    config.strengthenedMeleeOperationCountThreshold = 2;
    return config;
}

BattleCastInput frameCastInput(int sourceUnitId, int targetUnitId)
{
    BattleCastInput input;
    input.config = frameCastConfig();
    input.geometry.meleeAttackEffectOffset = SceneTileWidth * 2.0;
    input.geometry.projectileSpeed = SceneProjectileSpeed;
    input.geometry.projectileSpawnOffset = SceneTileWidth * 2.0;
    input.geometry.projectileBaseTravel = SceneTileWidth * 5.0;
    input.geometry.projectileTravelPerSelectDistance = SceneTileWidth;
    input.geometry.meleeSplashProjectileSpeed = 3.0;
    input.geometry.dashHitPositionSpacing = 2.0;
    input.geometry.dashVelocityMagnitude = SceneTileWidth * 2.0 / 5.0;
    input.geometry.dashHitFrameStep = 3;
    input.unit.id = sourceUnitId;
    input.unit.position = { 10.0f, 20.0f, 0.0f };
    input.unit.facing = { 1.0f, 0.0f, 0.0f };
    input.unit.alive = true;
    input.unit.canStartAttack = true;
    input.unit.mp = 20;
    input.unit.maxMp = 100;
    input.unit.meleeAttackReach = 137.5;
    input.targetUnitId = targetUnitId;
    input.targetPosition = { 82.0f, 20.0f, 0.0f };
    input.targetDistance = 100.0;
    input.normalSkill.id = 301;
    input.normalSkill.name = "框架招式";
    input.normalSkill.attackAreaType = 0;
    input.normalSkill.magicType = 1;
    input.normalSkill.visualEffectId = 77;
    input.normalSkill.reach = 137.5;
    input.ultimateSkill.id = 401;
    input.ultimateSkill.name = "絕招";
    input.ultimateSkill.soundId = 55;
    input.ultimateSkill.attackAreaType = 1;
    input.ultimateSkill.magicType = 1;
    input.ultimateSkill.visualEffectId = 88;
    input.ultimateSkill.reach = 400.0;
    input.ultimateSkill.rangedStyle = true;
    return input;
}

BattleActionSkillSeed actionSkillSeedFromCastSkill(const BattleCastSkillState& skill)
{
    BattleActionSkillSeed seed;
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

BattleActionPlanSeed actionPlanSeedFromCastInput(const BattleCastInput& input)
{
    BattleActionPlanSeed seed;
    seed.unitId = input.unit.id;
    seed.hasEquippedSkill = input.unit.hasEquippedSkill;
    seed.normalSkill = actionSkillSeedFromCastSkill(input.normalSkill);
    seed.ultimateSkill = actionSkillSeedFromCastSkill(input.ultimateSkill);
    return seed;
}

void configureRuntimeActionPlan(BattleRuntimeState& state, BattleCastInput input)
{
    state.action.castConfig = input.config;
    state.action.castGeometry = input.geometry;
    state.action.actionRules.tileWidth = SceneTileWidth;
    state.action.actionRules.maxEffectiveBattleReach = MaxEffectiveBattleReach;
    state.action.actionRules.meleeAttackHitRadius = SceneAttackHitRadius;
    state.action.actionRules.meleeAttackReach = input.unit.meleeAttackReach;
    state.action.actionRules.dashAttackMeleeReach = input.unit.dashAttackReach > 0.0
        ? input.unit.dashAttackReach
        : 375.0;
    state.action.actionRules.dashMomentumFrames = 5;
    state.action.actionRules.actionRecoveryFrames = 4;
    state.action.actionRules.dashRecoveryFrames = 5;
    state.action.actionRules.strengthenedMeleeOperationCountThreshold = 2;
    state.action.actionRules.projectileBounceRange = 90;
    state.action.actionRules.coordCount = 64;
    state.action.actionRecoveryFrames = 4;
    state.action.dashRecoveryFrames = 5;
    state.action.strengthenedMeleeOperationCountThreshold = 2;
    state.action.projectileBounceRange = 90;
    state.action.planSeeds[input.unit.id] = actionPlanSeedFromCastInput(input);
}

BattleActionCommitInput frameActionCommitInput()
{
    BattleActionCommitInput input;
    input.unit.id = 0;
    input.unit.team = 0;
    input.unit.position = { 100, 100, 0 };
    input.unit.facing = { 1, 0, 0 };
    input.unit.operationCount = 0;
    input.strengthenedMeleeOperationCountThreshold = 2;
    return input;
}

void configureAutoUltimateActionRuntime(BattleRuntimeState& state, int unitId, int targetUnitId)
{
    configureRuntimeActionPlan(state, frameCastInput(unitId, targetUnitId));
    state.action.strengthenedMeleeOperationCountThreshold = 2;
    state.action.projectileBounceRange = 90;
}

BattleCastResult committedFrameCast()
{
    BattleCastResult result;
    result.decision.canCast = true;
    result.decision.unitId = 0;
    result.decision.targetUnitId = 1;
    result.decision.skillId = 101;
    result.decision.operationType = BattleOperationType::RangedProjectile;
    BattleAttackSpawnRequest request;
    request.initial.attackerUnitId = 0;
    request.initial.skillId = 101;
    request.initial.preferredTargetUnitId = 1;
    request.initial.operationType = BattleOperationType::RangedProjectile;
    request.initial.position = { 100, 100, 0 };
    request.initial.velocity = { 5, 0, 0 };
    request.initial.totalFrame = 30;
    result.attackSpawnRequests.push_back(request);
    return result;
}

BattleDamageTransactionInput lethalDamageInput(int attackerUnitId, int defenderUnitId)
{
    BattleDamageTransactionInput input;
    input.request.attackerUnitId = attackerUnitId;
    input.request.defenderUnitId = defenderUnitId;
    input.request.baseDamage = 20;
    input.attacker.id = attackerUnitId;
    input.attacker.alive = true;
    input.attacker.vitals = { 100, 100, 0, 0 };
    input.defender.id = defenderUnitId;
    input.defender.alive = true;
    input.defender.vitals = { 10, 100, 0, 0 };
    input.defenderStatus.id = defenderUnitId;
    input.defenderStatus.alive = true;
    input.defenderStatus.hp = 10;
    input.defenderStatus.maxHp = 100;
    return input;
}

BattleDamageTransactionInput preResolvedDamageInput(int attackerUnitId, int defenderUnitId, int hpBefore, int damage)
{
    BattleDamageTransactionInput input;
    input.request.attackerUnitId = attackerUnitId;
    input.request.defenderUnitId = defenderUnitId;
    input.request.baseDamage = damage;
    input.request.preResolvedDamage = true;
    input.attacker.id = attackerUnitId;
    input.attacker.alive = true;
    input.attacker.vitals = { 100, 100, 0, 0 };
    input.defender.id = defenderUnitId;
    input.defender.alive = true;
    input.defender.vitals = { hpBefore, 100, 0, 0 };
    input.defenderStatus.id = defenderUnitId;
    input.defenderStatus.alive = true;
    input.defenderStatus.hp = hpBefore;
    input.defenderStatus.maxHp = 100;
    return input;
}

TEST_CASE("BattleFrameRunner_RoutesDamageTransactionsThroughCanonicalUnitStore", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    state.units.gridTransform = { SceneTileWidth, 64 };
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 120, 100, 0 }),
    });
    state.attacks = attackWorld();
    state.units.units = {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 10, { 120, 100, 0 }),
    };
    state.deathEffects.store.units = { { 0 }, { 1 } };
    state.damage.pendingTransactions.push_back(lethalDamageInput(0, 1));
    state.status.units = {
        statusRuntimeSnapshot(0, 100),
        statusRuntimeSnapshot(1, 10),
    };

    runBattleFrame(state);

    const auto& defender = state.units.requireUnit(1);
    CHECK(defender.vitals.hp == 0);
    CHECK_FALSE(defender.alive);
}

TEST_CASE("BattleFrameRunner_RoutesStatusTicksThroughCanonicalUnitStore", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    state.world = worldWith({ unit(0, 0, { 100, 100, 0 }) });
    state.attacks = attackWorld();
    auto runtime = runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 });
    runtime.mpBlocked = true;
    state.units.units.push_back(runtime);
    auto status = statusUnitSnapshot(0, 100);
    status.effects.mpBlockTimer = 1;
    status.effects.damageImmunityAfterFrames = 1;
    status.effects.damageImmunityDuration = 5;
    status.effects.damageImmunityTimer = 0;
    state.status.units.push_back(makeBattleStatusRuntimeUnit(status));

    runBattleFrame(state);

    const auto& unit = state.units.requireUnit(0);
    CHECK_FALSE(unit.mpBlocked);
    CHECK(unit.invincible == 5);
}

TEST_CASE("BattleStatusSystem_CopiesStatusEffectsAsACluster", "[battle][status]")
{
    BattleStatusUnitState source;
    source.id = 7;
    source.effects.poisonTimer = 9;
    source.effects.poisonTickPct = 5;
    source.effects.poisonSourceId = 3;
    source.effects.bleedStacks = 2;
    source.effects.frozenTimer = 4;
    source.effects.mpBlockTimer = 6;
    source.effects.tempAttackBuffs.push_back({ 11, 12 });
    source.effects.damageReduceDebuffs.push_back({ 13, 14 });

    auto runtime = makeBattleStatusRuntimeUnit(source);

    CHECK(runtime.id == 7);
    CHECK(runtime.effects.poisonTimer == 9);
    CHECK(runtime.effects.poisonTickPct == 5);
    CHECK(runtime.effects.poisonSourceId == 3);
    CHECK(runtime.effects.bleedStacks == 2);
    CHECK(runtime.effects.frozenTimer == 4);
    CHECK(runtime.effects.mpBlockTimer == 6);
    REQUIRE(runtime.effects.tempAttackBuffs.size() == 1);
    CHECK(runtime.effects.tempAttackBuffs[0].attackBonus == 11);
    REQUIRE(runtime.effects.damageReduceDebuffs.size() == 1);
    CHECK(runtime.effects.damageReduceDebuffs[0].pct == 14);
}

TEST_CASE("BattleFrameRunner_RoutesTeamEffectEventsThroughCanonicalUnitStore", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    state.units.gridTransform = { SceneTileWidth, 64 };
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 0, { 120, 100, 0 }),
    });
    state.attacks = attackWorld();
    state.units.units = {
        runtimeUnitSnapshot(0, 0, 40, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 0, 80, { 120, 100, 0 }),
    };
    state.status.units = {
        statusRuntimeSnapshot(0, 40),
        statusRuntimeSnapshot(1, 80),
    };
    state.teamEffects.pendingCommands.push_back(BattleTeamHealCommand{ 0, 10, 0, "技能群療" });

    runBattleFrame(state);

    CHECK(state.units.requireUnit(0).vitals.hp == 50);
    CHECK(state.units.requireUnit(1).vitals.hp == 90);
}

TEST_CASE("BattleFrameRunner_RoutesMovementPhysicsThroughCanonicalUnitStore", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    state.world = worldWith({ unit(0, 0, { 100, 100, 0 }) });
    state.attacks = attackWorld();
    state.units.gridTransform = { SceneTileWidth, 64 };
    state.units.units.push_back(runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }));
    state.units.requireUnit(0).motion.velocity = { 5, 0, 0 };
    state.units.requireUnit(0).motion.acceleration = { 0, 0, 0 };
    state.movementRuntime[0].position = { 100, 100, 0 };
    state.movementRuntime[0].velocity = { 5, 0, 0 };
    state.movementRuntime[0].acceleration = { 0, 0, 0 };
    state.movementRuntime[0].movementDashFrames = 1;
    state.movementPhysics.config.gravity = 0.0f;
    state.movementPhysics.config.friction = 0.0f;
    state.movementPhysics.config.postDashSpreadFrames = 6;
    state.movementPhysics.collision.tileWidth = SceneTileWidth;
    state.movementPhysics.collision.coordCount = 64;
    state.movementPhysics.collision.defaultSeparationDistance = SceneTileWidth;
    state.movementPhysics.collision.units = { { 0, true, { 100, 100, 0 } } };
    for (int x = -64; x < 64; ++x)
    {
        for (int y = -64; y < 64; ++y)
        {
            state.movementPhysics.collision.cells.push_back({ x, y, true });
        }
    }

    auto result = runBattleFrame(state);

    REQUIRE(result.movementPhysicsResults.size() == 1);
    CHECK(result.movementPhysicsResults[0].physicsAdvanced);
    REQUIRE(result.movementPresentationResults.size() == result.movementPhysicsResults.size());
    CHECK(result.movementPresentationResults[0].unitId == result.movementPhysicsResults[0].unitId);
    CHECK(result.movementPresentationResults[0].position.x == result.movementPhysicsResults[0].state.position.x);
    CHECK(result.movementPresentationResults[0].velocity.x == result.movementPhysicsResults[0].state.velocity.x);
    REQUIRE(result.movementPresentationResults.size() == 1);
    CHECK(result.movementPresentationResults[0].unitId == 0);
    CHECK(result.movementPresentationResults[0].position.x == 105.0f);
    CHECK(result.movementPresentationResults[0].velocity.x == 5.0f);
    CHECK(result.movementPresentationResults[0].acceleration.x == 0.0f);
    CHECK(result.movementPresentationResults[0].frozenFrames == 0);
    const auto& unit = state.units.requireUnit(0);
    CHECK(unit.motion.position.x == 105.0f);
    CHECK(unit.motion.velocity.x == 5.0f);
    CHECK(state.movementRuntime.at(0).movementDashFrames == 0);
    CHECK(state.movementRuntime.at(0).movementDashSpreadFrames == 6);
}

BattleRescueCellSnapshot rescueCell(int x, int y, bool walkable = true, bool occupied = false)
{
    return { x, y, walkable, occupied, occupied ? 99 : -1 };
}

BattleRuntimeState rescueDamageFrameState(int defenderHp, int damage)
{
    BattleRuntimeState state;
    state.units.gridTransform = { SceneTileWidth, 64 };
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 180, 180, 0 }),
        unit(2, 1, { 72, 72, 0 }),
    });
    state.attacks = attackWorld();
    state.status.units = {
        statusRuntimeSnapshot(0, 100),
        statusRuntimeSnapshot(1, defenderHp),
        statusRuntimeSnapshot(2, 100),
    };
    state.damage.pendingTransactions.push_back(preResolvedDamageInput(0, 1, defenderHp, damage));
    state.units.units = {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, defenderHp, { 180, 180, 0 }),
        runtimeUnitSnapshot(2, 1, 100, { 72, 72, 0 }),
    };
    state.units.units[0].grid = { 10, 10 };
    state.units.units[1].grid = { 5, 5 };
    state.units.units[2].grid = { 3, 2 };
    state.deathEffects.store.units = { { 0 }, { 1 }, { 2 } };
    state.rescue.cells = {
        rescueCell(2, 3),
        rescueCell(3, 2),
        rescueCell(5, 5),
    };
    state.combo.units[2].forcePullProtect = true;
    state.combo.units[2].forcePullProtectRemaining = 1;
    state.rescue.executeUnattendedRadius = SceneTileWidth * 3.0;
    state.rescue.counterAttack.skillId = 1;
    state.rescue.counterAttack.visualEffectId = 11;
    state.rescue.counterAttack.projectileSpeed = SceneProjectileSpeed;
    state.rescue.counterAttack.meleeAttackEffectOffset = SceneTileWidth * 2.0;
    state.rescue.counterAttack.minimumTotalFrames = 20;
    state.rescue.counterAttack.totalFramePadding = 15;
    return state;
}

}  // namespace

TEST_CASE("BattleMovementGeometryAndConfig_MaxRangedReachStartsEmptyUntilSupplied", "[battle][core]")
{
    BattleMovementGeometry geometry;
    BattleMovementConfig config;

    CHECK(geometry.maxRangedReach == 0.0);
    CHECK(config.maxRangedReach == 0.0);
}

TEST_CASE("BattleCombatIntent_OperationTypeMapping_MatchesSceneAnimationTypes", "[battle][intent]")
{
    BattleCombatIntentPlanner planner;
    CHECK(planner.operationTypeForAttackArea(0) == BattleOperationType::Melee);
    CHECK(planner.operationTypeForAttackArea(1) == BattleOperationType::RangedProjectile);
    CHECK(planner.operationTypeForAttackArea(2) == BattleOperationType::RangedProjectile);
    CHECK(planner.operationTypeForAttackArea(3) == BattleOperationType::TrackingProjectile);
    CHECK(planner.operationTypeForAttackArea(99) == BattleOperationType::None);
}

TEST_CASE("BattleCombatIntent_UltimateEquipsOnlyWhenReadyInputIsSet", "[battle][intent]")
{
    CombatIntentInput input;
    input.canStartAttack = true;
    input.hasEquippedSkill = false;
    input.ultimateReady = true;
    input.targetDistance = 100.0;
    input.meleeAttackReach = 137.5;
    input.dashAttackReach = 375.0;
    input.plannedSkill = skill(0, 137.5);

    BattleCombatIntentPlanner planner;
    auto intent = planner.select(input);
    CHECK(intent.equipPlannedSkill);
    CHECK(intent.announceUltimate);
    CHECK(intent.startAttack);
    CHECK(intent.operationType == BattleOperationType::Melee);

    input.ultimateReady = false;
    intent = planner.select(input);
    CHECK(intent.equipPlannedSkill);
    CHECK_FALSE(intent.announceUltimate);
}

TEST_CASE("BattleCombatIntent_PreservesMeleeBasicForcedRangedAndDashAttackRules", "[battle][intent]")
{
    CombatIntentInput forcedRanged;
    forcedRanged.canStartAttack = true;
    forcedRanged.hasEquippedSkill = true;
    forcedRanged.targetDistance = 300.0;
    forcedRanged.meleeAttackReach = 137.5;
    forcedRanged.dashAttackReach = 375.0;
    forcedRanged.plannedSkill = skill(0, 425.0, true);

    BattleCombatIntentPlanner planner;
    auto intent = planner.select(forcedRanged);
    CHECK(intent.startAttack);
    CHECK(intent.operationType == BattleOperationType::RangedProjectile);

    CombatIntentInput meleeDash = forcedRanged;
    meleeDash.dashAttackEnabled = true;
    meleeDash.plannedSkill = skill(0, 137.5, false);
    intent = planner.select(meleeDash);
    CHECK(intent.startAttack);
    CHECK(intent.operationType == BattleOperationType::Dash);
}

TEST_CASE("BattleCombatIntent_BlocksAttackWhileMovementDashContinues", "[battle][intent]")
{
    CombatIntentInput input;
    input.canStartAttack = true;
    input.hasEquippedSkill = false;
    input.ultimateReady = true;
    input.movementDashActive = true;
    input.targetDistance = 100.0;
    input.meleeAttackReach = 137.5;
    input.dashAttackReach = 375.0;
    input.plannedSkill = skill(0, 137.5);

    auto intent = BattleCombatIntentPlanner().select(input);
    CHECK(intent.equipPlannedSkill);
    CHECK(intent.announceUltimate);
    CHECK_FALSE(intent.startAttack);
}

TEST_CASE("BattleCore_MovementConfig_DerivesSharedGeometry", "[battle][core]")
{
    auto config = testConfig();
    CHECK(config.engagementDeadband == 18.0);
    CHECK(config.engagementArriveDistance == 27.0);
    CHECK(config.meleeAttackReach == 99.0);
    CHECK(config.meleeLocalTargetRadius == 171.0);
    CHECK(config.bodyRadius == 54.0);
    CHECK(config.movementDashDistanceMultiplier == 2.0);
}

TEST_CASE("BattleCore_ProbeMove_SeparatesWallsUnitsAndReservations", "[battle][core]")
{
    auto world = worldWith({
        unit(1, 0, { 100, 100, 0 }),
        unit(2, 0, { 160, 100, 0 }),
        unit(3, 1, { 300, 100, 0 }),
    });
    world.terrainCells = {
        { { -5, 100, 0 }, false },
        { { 135, 135, 0 }, true },
        { { 150, 100, 0 }, true },
        { { 290, 100, 0 }, true },
    };

    BattleMovementPlanner planner(world);
    CHECK(planner.probeMove(world.units[0], { -5, 100, 0 }, false).reason == MoveBlockReason::Wall);
    CHECK(planner.probeMove(world.units[0], { 150, 100, 0 }, false).reason == MoveBlockReason::Ally);
    CHECK(planner.probeMove(world.units[0], { 290, 100, 0 }, false).reason == MoveBlockReason::Enemy);

    std::map<int, Pointf> reservations = { { 2, { 130, 130, 0 } } };
    CHECK(planner.probeMove(world.units[0], { 135, 135, 0 }, false, reservations).reason == MoveBlockReason::Reservation);
    CHECK(planner.probeMove(world.units[0], { 150, 100, 0 }, true).canMove);
}

TEST_CASE("BattleCore_AttackReady_HoldsWhenAlreadyInRange", "[battle][core]")
{
    auto world = worldWith({
        unit(1, 0, { 100, 100, 0 }),
        unit(2, 1, { 210, 100, 0 }),
    });

    auto result = BattleMovementPlanner(world).tick();
    auto decision = result.decisions.at(1);
    CHECK(decision.action == MovementAction::AttackReady);
    CHECK(world.units[0].position.x == 100.0f);
    CHECK(world.units[0].velocity.norm() == 0.0f);
}

TEST_CASE("BattleCore_PlannedDash_UsesSpeedScaledDistanceForNormalUnits", "[battle][core]")
{
    auto world = worldWith({
        unit(1, 0, { 100, 100, 0 }),
        unit(2, 1, { 600, 100, 0 }),
    });

    auto result = BattleMovementPlanner(world).tick();
    auto decision = result.decisions.at(1);
    CHECK(decision.action == MovementAction::Dash);
    CHECK(decision.dashDistance == 50.0);
    CHECK(world.units[0].dashFramesRemaining == world.config.dashFrames);
}

TEST_CASE("BattleCore_MovementStats_CountsDashStartsOnly", "[battle][core]")
{
    auto run = BattleMovementSimulator(worldWith({
        unit(1, 0, { 100, 100, 0 }),
        unit(2, 1, { 600, 100, 0 }),
    })).run(6, 1);

    CHECK(run.stats.at(1).dashCount == 1);
    CHECK(run.stats.at(1).lastDashDistance == 50.0);
}

TEST_CASE("BattleCore_PlannedDash_IgnoresUnitsButRespectsTerrainSegment", "[battle][core]")
{
    auto world = worldWith({
        unit(1, 0, { 100, 100, 0 }),
        unit(2, 0, { 130, 100, 0 }),
        unit(3, 1, { 600, 100, 0 }),
    });

    auto openResult = BattleMovementPlanner(world).tick();
    CHECK(openResult.decisions.at(1).action == MovementAction::Dash);

    auto blockedWorld = worldWith({
        unit(1, 0, { 100, 100, 0 }),
        unit(2, 0, { 130, 100, 0 }),
        unit(3, 1, { 600, 100, 0 }),
    });
    blockedWorld.terrainCells = {
        { { 100, 100, 0 }, true },
        { { 125, 100, 0 }, false },
        { { 150, 100, 0 }, false },
        { { 180, 100, 0 }, true },
    };

    auto blockedResult = BattleMovementPlanner(blockedWorld).tick();
    CHECK(blockedResult.decisions.at(1).action != MovementAction::Dash);
}

TEST_CASE("BattleCore_TaXue_AllowsLongDash", "[battle][core]")
{
    auto chaser = unit(1, 0, { 100, 100, 0 });
    chaser.taXue = true;
    chaser.dashAttack = true;
    auto world = worldWith({
        chaser,
        unit(2, 1, { 600, 100, 0 }),
    });

    auto result = BattleMovementPlanner(world).tick();
    auto decision = result.decisions.at(1);
    CHECK(decision.action == MovementAction::Dash);
    CHECK(decision.dashDistance == world.config.maxDashDistance);
}

TEST_CASE("BattleCore_RangedHold_DoesNotBackIntoOccupiedRingWhenInRange", "[battle][core]")
{
    auto ranged = unit(1, 0, { 100, 100, 0 }, CombatStyle::Ranged);
    ranged.canAttack = false;
    auto world = worldWith({
        ranged,
        unit(2, 0, { 60, 100, 0 }),
        unit(3, 1, { 300, 100, 0 }),
    });

    auto result = BattleMovementPlanner(world).tick();
    auto decision = result.decisions.at(1);
    CHECK(decision.action == MovementAction::AttackReady);
    CHECK(world.units[0].position.x == 100.0f);
    CHECK(world.units[0].velocity.norm() == 0.0f);
}

TEST_CASE("BattleCore_MeleeOutsideReach_MovesInsteadOfReportingReady", "[battle][core]")
{
    auto world = worldWith({
        unit(1, 0, { 100, 100, 0 }),
        unit(2, 1, { 260, 100, 0 }),
    });

    auto result = BattleMovementPlanner(world).tick();
    CHECK(result.decisions.at(1).action == MovementAction::Move);
}

TEST_CASE("BattleCore_SlotSwitchCooldown_BoundsRepeatedReplans", "[battle][core]")
{
    auto world = worldWith({
        unit(1, 0, { 100, 100, 0 }),
        unit(2, 0, { 125, 100, 0 }),
        unit(3, 1, { 500, 100, 0 }),
    });
    world.units[0].dashCooldownRemaining = 999;

    auto first = BattleMovementPlanner(world).tick();
    CHECK((first.decisions.at(1).blockReason == MoveBlockReason::Ally
        || first.decisions.at(1).blockReason == MoveBlockReason::Reservation));
    CHECK(world.units[0].slotSwitchCooldownRemaining > 0);
    int slotAfterFirst = world.units[0].assignedSlot;

    auto second = BattleMovementPlanner(world).tick();
    CHECK(world.units[0].assignedSlot == slotAfterFirst);
    CHECK(second.decisions.at(1).slot == slotAfterFirst);
}

TEST_CASE("BattleUnitFrameTickSystem_AdvancesCooldownAndIdleResourceTicks", "[battle][core]")
{
    BattleUnitFrameTickState state;
    state.cooldown = 1;
    state.actType = 2;
    state.operationType = BattleOperationType::Dash;
    state.haveAction = true;
    state.physicalPower = 9;

    BattleUnitFrameTickInput input;
    input.state = state;
    input.frame = 6;
    input.frozen = false;
    input.mpRegenIntervalFrames = 3;
    input.physicalPowerRegenIntervalFrames = 3;

    auto result = BattleUnitFrameTickSystem().advance(input);

    CHECK(result.state.cooldown == 0);
    CHECK(result.state.actFrame == 0);
    CHECK(result.state.actType == -1);
    CHECK(result.state.operationType == BattleOperationType::None);
    CHECK_FALSE(result.state.haveAction);
    CHECK(result.state.physicalPower == 10);
    CHECK(result.mpDelta == 1);
    CHECK(result.skillFinished);
    CHECK(result.resetDashVelocity);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_CommitsMovementBeforeProjectileEvents", "[battle][core]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 600, 100, 0 }),
    });
    state.attacks = attackWorld();

    BattleAttackInstance projectile;
    projectile.id = 10;
    projectile.state.attackerUnitId = 0;
    projectile.state.preferredTargetUnitId = 0;
    projectile.state.totalFrame = 30;
    projectile.state.visualEffectId = 33;
    projectile.state.operationType = BattleOperationType::RangedProjectile;
    projectile.state.position = { 0, 0, 0 };
    projectile.state.velocity = { 5, 0, 0 };

    state.attacks.units = {
        { 1, 0, true, false, false, { 100, 100, 0 } },
        { 2, 1, true, false, false, { 600, 100, 0 } },
    };
    state.attacks.attacks.push_back(projectile);

    auto result = runBattleFrame(state);

    CHECK(state.world.frame == 1);
    CHECK(result.movement.events[0].type == BattleEventType::DashStart);
    REQUIRE(result.frame.logEvents.size() == result.movement.events.size());
    REQUIRE(!result.frame.visualEvents.empty());
    CHECK(result.frame.snapshot.frame == 1);
    CHECK(result.frame.logEvents[0].type == BattleLogEventType::Status);
    CHECK(result.frame.logEvents[0].text == "dash-start");
    const auto& firstProjectileEvent = result.frame.visualEvents[0];
    CHECK(firstProjectileEvent.type == BattleVisualEventType::ProjectileMoved);
    CHECK(firstProjectileEvent.effectId == 10);
    CHECK(firstProjectileEvent.sourceUnitId == 0);
    CHECK(firstProjectileEvent.targetUnitId == 0);
    CHECK(firstProjectileEvent.durationFrames == 30);
    CHECK(firstProjectileEvent.visualEffectId == 33);
    CHECK(firstProjectileEvent.position.x == 5.0f);
    CHECK(firstProjectileEvent.velocity.x == 5.0f);
    CHECK(firstProjectileEvent.operationKind == 2);
}

TEST_CASE("BattleRuntimeState_ComposesHeadlessRuntimeStateForFullFrameRunner", "[battle][core]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(1, 0, { 100, 100, 0 }),
        unit(2, 1, { 240, 100, 0 }),
    });
    state.attacks = attackWorld();

    BattleDamageTransactionInput damageInput;
    damageInput.request.attackerUnitId = 0;
    damageInput.request.defenderUnitId = 0;
    state.damage.pendingTransactions.push_back(damageInput);

    state.units.units.push_back(runtimeUnitSnapshot(0, 0, 80));
    state.status.units.push_back(statusRuntimeSnapshot(1, 80));

    KysChess::RoleComboState comboState;
    comboState.postSkillInvincFrames = 12;
    state.combo.units.emplace(1, comboState);

    BattleDeathEffectExtras deathEffectExtras;
    deathEffectExtras.id = 1;
    state.deathEffects.store.units.push_back(deathEffectExtras);

    CHECK(state.world.units.size() == 2);
    CHECK(state.damage.pendingTransactions[0].request.defenderUnitId == 0);
    CHECK(state.units.requireUnit(0).vitals.hp == 80);
    CHECK(state.combo.units.at(1).postSkillInvincFrames == 12);
    CHECK(state.deathEffects.store.units[0].id == 1);
    CHECK_FALSE(state.result.ended);
    CHECK(state.result.winningTeam == -1);
}

TEST_CASE("BattleCore_AppliesTeamEffectGameplayCommands", "[battle][core]")
{
    BattleUnitStore units;
    units.units = {
        runtimeUnitSnapshot(0, 0, 40),
        runtimeUnitSnapshot(1, 0, 90),
        runtimeUnitSnapshot(2, 1, 20),
    };
    units.units[0].vitals.mp = 10;
    units.units[1].vitals.mp = 45;
    units.units[1].shield = 3;
    units.units[2].vitals.mp = 5;

    auto heal = applyBattleTeamEffectCommand(
        units,
        BattleTeamHealCommand{ 0, 5, 10, "技能群療" });
    REQUIRE(heal.events.size() == 2);
    CHECK(heal.events[0].targetUnitId == 0);
    CHECK(heal.events[0].value == 15);
    CHECK(units.units[0].vitals.hp == 55);
    CHECK(units.units[1].vitals.hp == 100);
    REQUIRE(heal.logEvents.size() == 2);
    CHECK(heal.logEvents[0].type == BattleLogEventType::Heal);
    CHECK(heal.logEvents[0].text == "技能群療");

    auto mp = applyBattleTeamEffectCommand(
        units,
        BattleTeamMpRestoreCommand{ 0, 8, "全隊回內" });
    REQUIRE(mp.events.size() == 2);
    CHECK(units.units[0].vitals.mp == 18);
    CHECK(units.units[1].vitals.mp == 50);
    REQUIRE(mp.logEvents.size() == 2);
    CHECK(mp.logEvents[0].type == BattleLogEventType::Status);
    CHECK(mp.logEvents[0].text == "全隊回內+8MP");

    auto shield = applyBattleTeamEffectCommand(
        units,
        BattleTeamShieldCommand{ 0, 7, true, "全隊護盾" });
    REQUIRE(shield.events.size() == 2);
    CHECK(units.units[0].shield == 7);
    CHECK(units.units[1].shield == 7);
    REQUIRE(shield.logEvents.size() == 2);
    CHECK(shield.logEvents[0].type == BattleLogEventType::Status);
    CHECK(shield.logEvents[0].text == "全隊護盾（7護盾）");
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_SnapshotIncludesCommittedUnitState", "[battle][core]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 600, 100, 0 }),
    });
    state.attacks = attackWorld();
    state.units.units = {
        runtimeUnitSnapshot(0, 0, 80, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 100, { 600, 100, 0 }),
    };
    state.units.units[0].vitals.maxHp = 120;
    state.units.units[0].invincible = 6;
    state.status.units.push_back(statusRuntimeSnapshot(0, 80));

    auto result = runBattleFrame(state);

    REQUIRE(result.frame.snapshot.units.size() == state.world.units.size());
    CHECK(result.frame.snapshot.frame == state.world.frame);
    CHECK(result.frame.snapshot.units[0].id == state.world.units[0].id);
    CHECK(result.frame.snapshot.units[0].hp == state.units.units[0].vitals.hp);
    CHECK(result.frame.snapshot.units[0].maxHp == state.units.units[0].vitals.maxHp);
    CHECK(result.frame.snapshot.units[0].invincible == state.units.units[0].invincible);
    CHECK(result.frame.snapshot.units[0].position.x == state.world.units[0].position.x);
    CHECK(result.frame.snapshot.units[0].velocity.x == state.world.units[0].velocity.x);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_UsesGroupedRuntimeUnitState", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 600, 100, 0 }),
    });
    state.attacks = attackWorld();

    auto grouped = runtimeUnitSnapshot(0, 0, 80, { 100, 100, 0 });
    grouped.vitals = { 80, 120, 4, 50 };
    grouped.motion = { { 100, 100, 0 }, { 7, 8, 0 }, { 0, 0, 0 }, { 1, 0, 0 } };
    grouped.animation = { 2, 60, 5, 13 };
    grouped.operationType = BattleOperationType::Dash;
    grouped.haveAction = true;

    auto target = runtimeUnitSnapshot(1, 1, 100, { 600, 100, 0 });
    state.units.units = { grouped, target };
    state.status.units = {
        statusRuntimeSnapshot(0, 80),
        statusRuntimeSnapshot(1, 100),
    };

    const auto result = runBattleFrame(state);

    REQUIRE(result.unitApplications.size() == 2);
    CHECK(result.unitApplications[0].unitId == 0);
    CHECK(result.unitApplications[0].cooldown == 1);
    CHECK(result.unitApplications[0].actFrame == 5);
    CHECK(result.unitApplications[0].actType == 13);
    CHECK(result.unitApplications[0].finalMp == 5);

    const auto& updated = state.units.requireUnit(0);
    CHECK(updated.animation.cooldown == 1);
    CHECK(updated.animation.actFrame == 6);
    CHECK(updated.animation.actType == 13);
    CHECK(updated.vitals.mp == 5);
    CHECK(updated.motion.velocity.x == 7.0f);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_RecordsPendingAttackSpawnRequest", "[battle][core][presentation]")
{
    BattleRuntimeState state;
    state.world = worldWith({});
    state.attacks = attackWorld();
    state.attacks.hitRadius = 10.0;
    state.attacks.nextAttackId = 50;
    state.attacks.units = {
        { 0, 0, true, false, false, { 0, 0, 0 } },
        { 1, 1, true, false, false, { 106, 120, 0 } },
    };
    state.units.units = {
        runtimeUnitSnapshot(0, 0, 100, { 0, 0, 0 }),
        runtimeUnitSnapshot(1, 1, 100, { 106, 120, 0 }),
    };
    state.pendingAttackSpawns.push_back(attackSpawnRequest());

    auto result = runBattleFrame(state);

    REQUIRE(result.attackEvents.size() == 3);
    CHECK(result.attackEvents[0].type == BattleAttackEventType::AttackSpawned);
    CHECK(result.attackEvents[0].attackId == 50);
    CHECK(result.attackEvents[1].type == BattleAttackEventType::Moved);
    CHECK(result.attackEvents[1].attackId == 50);
    CHECK(result.attackEvents[2].type == BattleAttackEventType::Hit);
    CHECK(result.attackEvents[2].attackId == 50);
    CHECK(result.attackEvents[2].unitId == 1);
    CHECK(state.pendingAttackSpawns.empty());
    REQUIRE(state.attacks.attacks.size() == 1);
    CHECK(state.attacks.attacks[0].id == 50);
    CHECK(state.attacks.attacks[0].state.position.x == 106.0f);
    CHECK(state.attacks.nextAttackId == 51);

    REQUIRE(result.frame.gameplayEvents.size() >= 3);
    auto gameplayIt = std::find_if(
        result.frame.gameplayEvents.begin(),
        result.frame.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::AttackSpawned
                && event.effectId == 50;
        });
    REQUIRE(gameplayIt != result.frame.gameplayEvents.end());
    CHECK(gameplayIt->sourceUnitId == 0);
    CHECK(gameplayIt->targetUnitId == 0);
    CHECK(gameplayIt->position.x == 100.0f);
    CHECK(gameplayIt->position.y == 120.0f);

    REQUIRE(result.frame.visualEvents.size() >= 3);
    const auto& presentation = result.frame.visualEvents[0];
    CHECK(presentation.type == BattleVisualEventType::ProjectileSpawned);
    CHECK(presentation.effectId == 50);
    CHECK(presentation.sourceUnitId == 0);
    CHECK(presentation.targetUnitId == 0);
    CHECK(presentation.durationFrames == 30);
    CHECK(presentation.visualEffectId == 44);
    CHECK(presentation.position.x == 100.0f);
    CHECK(presentation.position.y == 120.0f);
    CHECK(presentation.velocity.x == 6.0f);
    CHECK(presentation.operationKind == 2);

    CHECK(result.frame.visualEvents[1].type == BattleVisualEventType::ProjectileMoved);
    CHECK(result.frame.visualEvents[1].position.x == 106.0f);
    CHECK(result.frame.visualEvents[2].type == BattleVisualEventType::ProjectileHit);
    auto hitGameplayIt = std::find_if(
        result.frame.gameplayEvents.begin(),
        result.frame.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::ProjectileHit
                && event.targetUnitId == 1;
        });
    CHECK(hitGameplayIt != result.frame.gameplayEvents.end());

    auto plan = BattlePresentationPlaybackPlanner().build(result.frame);
    REQUIRE(plan.commands.size() == 3);
    CHECK(plan.commands[0].type == BattlePresentationCommandType::SpawnProjectile);
    CHECK(plan.commands[0].projectileAttackId == 50);
    CHECK(plan.commands[0].visualEffectId == 44);
    CHECK(plan.commands[0].projectilePosition.x == 100.0f);
    CHECK(plan.commands[0].projectileVelocity.x == 6.0f);
    CHECK(plan.commands[0].projectileDurationFrames == 30);
    CHECK(plan.commands[0].projectileOperationKind == 2);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_RunsStatusBeforeCastPlanning", "[battle][core]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 210, 100, 0 }),
    });
    state.attacks = attackWorld();

    state.units.units = {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 100, { 210, 100, 0 }),
    };
    state.units.units[0].stats.attack = 15;
    BattleStatusRuntimeUnit statusUnit;
    statusUnit.id = 0;
    statusUnit.effects.tempAttackBuffs.push_back({ 5, 1 });
    state.status.units.push_back(statusUnit);

    auto cast = frameCastInput(0, 1);
    configureRuntimeActionPlan(state, cast);
    state.units.requireUnit(0).animation.cooldown = 0;
    state.units.requireUnit(0).hurtFrame = 3;

    auto result = runBattleFrame(state);

    CHECK(state.units.requireUnit(0).stats.attack == 10);
    CHECK(result.actionResults.empty());
    CHECK(result.frame.gameplayEvents.empty());
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_CastPlanningRecordsStartWithoutSpawningAttack", "[battle][core]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(0, 0, { 10, 20, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 82, 20, 0 }),
    });
    state.attacks = attackWorld();
    state.attacks.hitRadius = 10.0;
    state.attacks.nextAttackId = 90;
    state.attacks.units = {
        { 0, 0, true, false, false, { 10, 20, 0 } },
        { 1, 1, true, false, false, { 82, 20, 0 } },
    };
    seedRuntimeUnitsFromWorld(state);
    auto cast = frameCastInput(0, 1);
    cast.normalSkill.attackAreaType = 1;
    cast.normalSkill.rangedStyle = true;
    cast.normalSkill.reach = 400.0;
    configureRuntimeActionPlan(state, cast);
    state.units.requireUnit(0).animation.cooldown = 0;

    auto result = runBattleFrame(state);

    REQUIRE(result.actionResults.size() == 1);
    REQUIRE(result.actionResults[0].castResult.attackSpawnRequests.size() == 1);
    CHECK(result.attackEvents.empty());
    REQUIRE(result.frame.gameplayEvents.size() == 1);
    CHECK(result.frame.gameplayEvents[0].type == BattleGameplayEventType::CastStarted);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_SelectsCastTargetFromRuntimeUnits", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(0, 0, { 10, 20, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 260, 20, 0 }),
        unit(2, 1, { 82, 20, 0 }),
    });
    state.units.units = {
        runtimeUnitSnapshot(0, 0, 100, { 10, 20, 0 }),
        runtimeUnitSnapshot(1, 1, 100, { 260, 20, 0 }),
        runtimeUnitSnapshot(2, 1, 100, { 82, 20, 0 }),
    };
    state.attacks = attackWorld();
    auto cast = frameCastInput(0, -1);
    cast.targetPosition = {};
    cast.targetDistance = 0.0;
    cast.normalSkill.attackAreaType = 1;
    cast.normalSkill.rangedStyle = true;
    cast.normalSkill.reach = 400.0;
    configureRuntimeActionPlan(state, cast);
    state.units.requireUnit(0).animation.cooldown = 0;

    auto result = runBattleFrame(state);

    REQUIRE(result.actionResults.size() == 1);
    const auto& castResult = result.actionResults[0].castResult;
    CHECK(castResult.decision.canCast);
    CHECK(castResult.decision.targetUnitId == 2);
    REQUIRE(castResult.attackSpawnRequests.size() == 1);
    CHECK(castResult.attackSpawnRequests[0].initial.preferredTargetUnitId == 2);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_CastInputUsesCommittedFrameState", "[battle][core]")
{
    BattleRuntimeState state;
    auto caster = unit(0, 0, { 10, 20, 0 }, CombatStyle::Ranged);
    caster.reach = 400.0;
    state.world = worldWith({
        caster,
        unit(1, 1, { 82, 20, 0 }),
    });
    state.attacks = attackWorld();
    state.attacks.nextAttackId = 70;
    state.attacks.units = {
        { 0, 0, true, false, false, { 10, 20, 0 } },
        { 1, 1, true, false, false, { 82, 20, 0 } },
    };
    state.units.units = {
        runtimeUnitSnapshot(0, 0, 100, { 10, 20, 0 }),
        runtimeUnitSnapshot(1, 1, 100, { 82, 20, 0 }),
    };
    BattleStatusUnitState frozenStatus;
    frozenStatus.id = 0;
    frozenStatus.alive = true;
    frozenStatus.effects.frozenTimer = 3;
    state.status.units.push_back(makeBattleStatusRuntimeUnit(frozenStatus));
    configureRuntimeActionPlan(state, frameCastInput(0, 1));
    state.units.requireUnit(0).animation.cooldown = 0;

    auto result = runBattleFrame(state);

    CHECK(result.actionResults.empty());
    CHECK(result.attackEvents.empty());
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_CastOriginUsesPostMovementPosition", "[battle][core]")
{
    BattleRuntimeState state;
    auto caster = unit(0, 0, { 10, 20, 0 }, CombatStyle::Ranged);
    caster.reach = 400.0;
    state.world = worldWith({
        caster,
        unit(1, 1, { 82, 20, 0 }),
    });
    state.attacks = attackWorld();
    state.attacks.nextAttackId = 80;
    state.attacks.units = {
        { 0, 0, true, false, false, { 10, 20, 0 } },
        { 1, 1, true, false, false, { 82, 20, 0 } },
    };
    seedRuntimeUnitsFromWorld(state);
    auto cast = frameCastInput(0, 1);
    cast.unit.position = { -500, -500, 0 };
    cast.targetPosition = { -250, -250, 0 };
    cast.normalSkill.attackAreaType = 1;
    cast.normalSkill.rangedStyle = true;
    cast.normalSkill.reach = 400.0;
    configureRuntimeActionPlan(state, cast);
    state.units.requireUnit(0).animation.cooldown = 0;

    auto result = runBattleFrame(state);

    REQUIRE(result.actionResults.size() == 1);
    REQUIRE(result.actionResults[0].castResult.attackSpawnRequests.size() == 1);
    const auto& spawn = result.actionResults[0].castResult.attackSpawnRequests[0];
    CHECK(spawn.initial.position.x == state.world.units[0].position.x + SceneTileWidth * 2.0);
    CHECK(spawn.initial.position.y == state.world.units[0].position.y);
    CHECK(result.attackEvents.empty());
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_CommitsActionInputsBeforeAttackTick", "[battle][core]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 160, 100, 0 }),
    });
    state.attacks = attackWorld();
    state.attacks.units = {
        { 0, 0, true, false, false, { 100, 100, 0 } },
        { 1, 1, true, false, false, { 160, 100, 0 } },
    };
    seedRuntimeUnitsFromWorld(state);
    auto& unit = state.units.requireUnit(0);
    unit.haveAction = true;
    unit.animation.actFrame = 6;
    unit.operationType = BattleOperationType::RangedProjectile;
    unit.animation.actType = 1;
    unit.animation.cooldown = 10;

    auto action = frameActionCommitInput();
    action.hasCast = true;
    action.cast = committedFrameCast();
    state.action.pendingCommitInputs.emplace(0, action);

    auto result = runBattleFrame(state);

    REQUIRE(result.actionResults.size() == 1);
    CHECK(result.actionResults[0].unitId == 0);
    CHECK(result.actionResults[0].actionInput.hasCast);
    CHECK(result.actionResults[0].actionInput.cast.decision.skillId == 101);
    REQUIRE(result.actionResults[0].actionResult.attackSpawnRequests.size() == 1);
    REQUIRE(result.attackEvents.size() >= 1);
    CHECK(result.attackEvents.front().type == BattleAttackEventType::AttackSpawned);
    CHECK(result.attackEvents.front().attackId == 0);
    CHECK(result.attackEvents.front().sourceUnitId == 0);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ConsumesRuntimeOwnedActionDirectives", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 160, 100, 0 }),
    });
    state.attacks = attackWorld();
    state.attacks.units = {
        { 0, 0, true, false, false, { 100, 100, 0 } },
        { 1, 1, true, false, false, { 160, 100, 0 } },
    };
    seedRuntimeUnitsFromWorld(state);
    auto& unit = state.units.requireUnit(0);
    unit.haveAction = true;
    unit.animation.actFrame = 6;
    unit.operationType = BattleOperationType::RangedProjectile;
    unit.animation.actType = 1;
    unit.animation.cooldown = 10;

    auto action = frameActionCommitInput();
    action.hasCast = true;
    action.cast = committedFrameCast();
    state.action.pendingCommitInputs.emplace(0, action);

    auto result = runBattleFrame(state);

    REQUIRE(result.actionResults.size() == 1);
    CHECK(result.actionResults[0].actionCommitted);
    REQUIRE(result.attackEvents.size() >= 1);
    CHECK(result.attackEvents.front().type == BattleAttackEventType::AttackSpawned);
}

TEST_CASE("BattleFrameRunner_PlansCastFromRuntimeOwnedCastPlanInput", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    });
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    state.units.requireUnit(0).animation.cooldown = 0;
    auto cast = frameCastInput(0, 1);
    cast.normalSkill.attackAreaType = 1;
    cast.normalSkill.rangedStyle = true;
    cast.normalSkill.reach = 400.0;
    configureRuntimeActionPlan(state, cast);
    state.units.requireUnit(0).animation.cooldown = 0;

    auto result = runBattleFrame(state);

    REQUIRE(result.actionResults.size() == 1);
    CHECK(result.actionResults[0].castStarted);
    CHECK(result.actionResults[0].castResult.decision.skillId == 301);
}

TEST_CASE("BattleFrameRunner_ClearsDashSpreadWhenRuntimeCastStarts", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    });
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    state.units.requireUnit(0).animation.cooldown = 0;
    state.movementRuntime[0].movementDashSpreadFrames = 9;
    auto cast = frameCastInput(0, 1);
    cast.normalSkill.attackAreaType = 1;
    cast.normalSkill.rangedStyle = true;
    cast.normalSkill.reach = 400.0;
    configureRuntimeActionPlan(state, cast);
    state.units.requireUnit(0).animation.cooldown = 0;

    auto result = runBattleFrame(state);

    REQUIRE(result.actionResults.size() == 1);
    CHECK(result.actionResults[0].castStarted);
    CHECK(state.movementRuntime.at(0).movementDashSpreadFrames == 0);
}

TEST_CASE("BattleFrameRunner_RollsDashHitCountFromRuntimeStateWhenDashCastStarts", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Melee),
        unit(1, 1, { 300, 100, 0 }),
    });
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);

    auto& runtimeUnit = state.units.requireUnit(0);
    runtimeUnit.animation.cooldown = 0;
    runtimeUnit.stats.speed = 360;
    state.combo.units[0].dashAttack = true;

    auto cast = frameCastInput(0, 1);
    cast.unit.dashAttackEnabled = true;
    cast.unit.dashAttackReach = 350.0;
    cast.unit.dashHitCount = 1;
    cast.unit.dashVelocity = { 10.0f, 0.0f, 0.0f };
    cast.normalSkill.attackAreaType = 0;
    cast.normalSkill.rangedStyle = false;
    cast.normalSkill.reach = 350.0;
    cast.normalSkill.actProperty = 0;
    configureRuntimeActionPlan(state, cast);

    auto result = runBattleFrame(state);

    REQUIRE(result.actionResults.size() == 1);
    const auto& action = result.actionResults[0];
    REQUIRE(action.castStarted);
    REQUIRE(action.castResult.decision.operationType == BattleOperationType::Dash);
    const auto dashHitCount = std::count_if(
        action.castResult.attackSpawnRequests.begin(),
        action.castResult.attackSpawnRequests.end(),
        [](const BattleAttackSpawnRequest& request)
        {
            return request.initial.castSubrequestKind == BattleAttackCastSubrequestKind::DashHit;
        });
    CHECK(dashHitCount == 3);
    const auto pending = state.action.pendingCommitInputs.find(0);
    REQUIRE(pending != state.action.pendingCommitInputs.end());
    const auto pendingDashHitCount = std::count_if(
        pending->second.cast.attackSpawnRequests.begin(),
        pending->second.cast.attackSpawnRequests.end(),
        [](const BattleAttackSpawnRequest& request)
        {
            return request.initial.castSubrequestKind == BattleAttackCastSubrequestKind::DashHit;
        });
    CHECK(pendingDashHitCount == 3);
}

TEST_CASE("BattleFrameRunner_StoresPendingCastCommitInputWhenCastStarts", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    });
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);

    auto cast = frameCastInput(0, 1);
    cast.normalSkill.attackAreaType = 1;
    cast.normalSkill.rangedStyle = true;
    cast.normalSkill.reach = 400.0;
    configureRuntimeActionPlan(state, cast);
    state.units.requireUnit(0).animation.cooldown = 0;

    auto result = runBattleFrame(state);

    REQUIRE(result.actionResults.size() == 1);
    CHECK(result.actionResults[0].castStarted);
    auto pending = state.action.pendingCommitInputs.find(0);
    REQUIRE(pending != state.action.pendingCommitInputs.end());
    CHECK(pending->second.hasCast);
    CHECK(pending->second.cast.decision.skillId == 301);
}

TEST_CASE("BattleFrameRunner_CommitsRuntimeOwnedPendingCastInputWithoutSceneDirective", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    });
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    auto& unit = state.units.requireUnit(0);
    unit.haveAction = true;
    unit.animation.actFrame = 6;
    unit.operationType = BattleOperationType::RangedProjectile;
    unit.animation.actType = 1;
    unit.animation.cooldown = 10;

    auto action = frameActionCommitInput();
    action.hasCast = true;
    action.cast = committedFrameCast();
    state.action.pendingCommitInputs.emplace(0, action);

    auto result = runBattleFrame(state);

    CHECK(state.action.pendingCommitInputs.empty());
    REQUIRE(result.actionResults.size() == 1);
    CHECK(result.actionResults[0].actionCommitted);
    CHECK(result.actionResults[0].castCommitted);
    CHECK(result.actionResults[0].actionInput.hasCast);
    REQUIRE(result.actionResults[0].actionResult.attackSpawnRequests.size() == 1);
}

TEST_CASE("BattleFrameRunner_CommitsRuntimeOwnedPendingCastSound", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    });
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    auto& unit = state.units.requireUnit(0);
    unit.haveAction = true;
    unit.animation.actFrame = 6;
    unit.operationType = BattleOperationType::RangedProjectile;
    unit.animation.actType = 1;
    unit.animation.cooldown = 10;

    auto action = frameActionCommitInput();
    action.hasCast = true;
    action.cast = committedFrameCast();
    action.cast.decision.soundId = 55;
    state.action.pendingCommitInputs.emplace(0, action);

    auto result = runBattleFrame(state);

    REQUIRE(result.applications.attackSoundIds.size() == 1);
    CHECK(result.applications.attackSoundIds[0] == 55);
}

TEST_CASE("BattleFrameRunner_ConsumesUltimateCasterWhenRuntimeOwnedCastCommits", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    });
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    auto& unit = state.units.requireUnit(0);
    unit.haveAction = true;
    unit.animation.actFrame = 6;
    unit.operationType = BattleOperationType::RangedProjectile;
    unit.animation.actType = 1;
    unit.animation.cooldown = 10;
    state.ultimateCasters.insert(0);

    auto action = frameActionCommitInput();
    action.hasCast = true;
    action.cast = committedFrameCast();
    state.action.pendingCommitInputs.emplace(0, action);

    auto result = runBattleFrame(state);

    REQUIRE(result.actionResults.size() == 1);
    CHECK(result.actionResults[0].actionCommitted);
    CHECK(state.ultimateCasters.empty());
}

TEST_CASE("BattleFrameRunner_PrunesFinishedRuntimeAttacksAfterFrame", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    });
    state.attacks = attackWorld();
    BattleAttackInstance attack;
    attack.id = 77;
    attack.frame = 0;
    attack.state.attackerUnitId = 0;
    attack.state.preferredTargetUnitId = 0;
    attack.state.operationType = BattleOperationType::RangedProjectile;
    attack.state.position = { 100, 100, 0 };
    attack.state.velocity = { 0, 0, 0 };
    attack.state.totalFrame = 1;
    state.attacks.attacks.push_back(attack);

    auto result = runBattleFrame(state);

    REQUIRE_FALSE(result.attackEvents.empty());
    CHECK(std::any_of(
        result.attackEvents.begin(),
        result.attackEvents.end(),
        [](const BattleAttackEvent& event)
        {
            return event.type == BattleAttackEventType::Expired && event.attackId == 77;
        }));
    CHECK(state.attacks.attacks.empty());
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_CommitsRuntimePendingCastInput", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 160, 100, 0 }),
    });
    state.attacks = attackWorld();
    state.attacks.units = {
        { 0, 0, true, false, false, { 100, 100, 0 } },
        { 1, 1, true, false, false, { 160, 100, 0 } },
    };
    seedRuntimeUnitsFromWorld(state);
    auto& unit = state.units.requireUnit(0);
    unit.haveAction = true;
    unit.animation.actFrame = 6;
    unit.operationType = BattleOperationType::RangedProjectile;
    unit.animation.actType = 1;
    unit.animation.cooldown = 10;

    auto actionInput = frameActionCommitInput();
    actionInput.hasCast = true;
    actionInput.cast = committedFrameCast();
    state.action.pendingCommitInputs.emplace(0, actionInput);

    auto result = runBattleFrame(state);

    REQUIRE(result.actionResults.size() == 1);
    const auto& action = result.actionResults[0];
    CHECK(action.castCommitted);
    CHECK(action.actionInput.hasCast);
    CHECK(action.actionInput.cast.decision.skillId == 101);
    REQUIRE(action.actionResult.attackSpawnRequests.size() == 1);
    REQUIRE(result.attackEvents.size() >= 1);
    CHECK(result.attackEvents.front().type == BattleAttackEventType::AttackSpawned);
    CHECK(result.attackEvents.front().sourceUnitId == 0);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ClearsRecoveredActionFrameUnitState", "[battle][core]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
    });
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    auto& unit = state.units.requireUnit(0);
    unit.haveAction = true;
    unit.animation.actFrame = 11;
    unit.animation.actType = 2;
    unit.operationType = BattleOperationType::RangedProjectile;
    unit.animation.cooldown = 30;

    auto result = runBattleFrame(state);

    REQUIRE(result.actionResults.size() == 1);
    const auto& action = result.actionResults[0];
    CHECK(action.state.actFrame == 12);
    CHECK_FALSE(action.state.haveAction);
    CHECK(action.state.actType == -1);
    CHECK(action.state.operationType == BattleOperationType::None);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DamageDeathPrecedesBattleEndEvent", "[battle][core]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 210, 100, 0 }),
    });
    state.attacks = attackWorld();
    state.status.units = {
        statusRuntimeSnapshot(0, 100),
        statusRuntimeSnapshot(1, 10),
    };
    state.units.units = {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 10, { 210, 100, 0 }),
    };
    state.deathEffects.store.units = { { 0 }, { 1 } };
    state.damage.pendingTransactions.push_back(lethalDamageInput(0, 1));

    auto result = runBattleFrame(state);

    REQUIRE(result.damageTransactions.size() == 1);
    REQUIRE(result.damageRenderApplications.size() == 1);
    const auto& renderDamage = result.damageRenderApplications.front();
    const auto& transaction = result.damageTransactions.front();
    CHECK(renderDamage.defender.unitId == transaction.defender.id);
    CHECK(renderDamage.defender.hp == transaction.defender.vitals.hp);
    CHECK(renderDamage.defender.mp == transaction.defender.vitals.mp);
    CHECK(renderDamage.defender.alive == transaction.defender.alive);
    CHECK(renderDamage.frozenFrames == transaction.defenderStatus.effects.frozenTimer);
    CHECK(renderDamage.frozenMaxFrames == transaction.defenderStatus.effects.frozenMaxTimer);
    CHECK(renderDamage.cooldown == transaction.defenderCooldown.cooldown);
    CHECK(renderDamage.committedHpDamage == transaction.finalHpDamage);
    CHECK(state.world.units[1].alive == false);
    CHECK(state.units.requireUnit(1).alive == false);
    CHECK(state.result.ended);
    CHECK(state.result.winningTeam == 0);

    std::vector<BattleGameplayEventType> gameplayTypes;
    for (const auto& event : result.frame.gameplayEvents)
    {
        gameplayTypes.push_back(event.type);
    }
    REQUIRE(gameplayTypes.size() >= 3);
    CHECK(gameplayTypes[gameplayTypes.size() - 3] == BattleGameplayEventType::DamageApplied);
    CHECK(gameplayTypes[gameplayTypes.size() - 2] == BattleGameplayEventType::UnitDied);
    CHECK(gameplayTypes[gameplayTypes.size() - 1] == BattleGameplayEventType::BattleEnded);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_RunsMovementPhysicsInsideCore", "[battle][core][movement]")
{
    BattleRuntimeState state;
    state.units.gridTransform = { SceneTileWidth, 64 };
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 200, 100, 0 }),
    });
    state.attacks = attackWorld();
    state.units.units = {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 100, { 200, 100, 0 }),
    };
    state.units.requireUnit(0).motion.velocity = { 5, 0, 0 };
    state.units.requireUnit(0).motion.acceleration = { 0, 0, -4 };
    state.units.requireUnit(0).motion.velocity = { 5, 0, 0 };
    state.units.requireUnit(0).motion.acceleration = { 0, 0, -4 };
    state.status.units = {
        statusRuntimeSnapshot(0, 100),
        statusRuntimeSnapshot(1, 100),
    };
    state.status.units[1].effects.frozenTimer = 2;
    state.movementPhysics.config.gravity = -4.0f;
    state.movementPhysics.config.friction = 0.1f;
    state.movementPhysics.config.postDashSpreadFrames = 6;
    state.movementPhysics.collision.tileWidth = SceneTileWidth;
    state.movementPhysics.collision.coordCount = 64;
    state.movementPhysics.collision.defaultSeparationDistance = SceneTileWidth * 1.5;
    state.movementPhysics.collision.units = {
        { 0, true, { 100, 100, 0 } },
        { 1, true, { 200, 100, 0 } },
    };
    for (int x = -64; x < 64; ++x)
    {
        for (int y = -64; y < 64; ++y)
        {
            state.movementPhysics.collision.cells.push_back({ x, y, true });
        }
    }
    state.movementRuntime[0].position = { 100, 100, 0 };
    state.movementRuntime[0].velocity = { 5, 0, 0 };
    state.movementRuntime[0].acceleration = { 0, 0, -4 };
    state.movementRuntime[0].movementDashFrames = 1;
    state.movementRuntime[1].position = { 200, 100, 0 };
    state.movementRuntime[1].velocity = { 5, 0, 0 };
    state.movementRuntime[1].acceleration = { 0, 0, -4 };

    auto result = runBattleFrame(state);

    REQUIRE(result.movementPhysicsResults.size() == 2);
    const auto& moved = result.movementPhysicsResults[0];
    CHECK(moved.unitId == 0);
    CHECK(moved.physicsAdvanced);
    CHECK(moved.frozenFrames == 0);
    CHECK(moved.state.position.x == 105.0f);
    CHECK(moved.state.movementDashFrames == 0);
    CHECK(moved.state.movementDashSpreadFrames == 6);

    const auto& stopped = result.movementPhysicsResults[1];
    CHECK(stopped.unitId == 1);
    CHECK_FALSE(stopped.physicsAdvanced);
    CHECK(stopped.frozenFrames == 1);
    CHECK(state.status.units[1].effects.frozenTimer == 1);
    CHECK(stopped.state.position.x == 200.0f);
    CHECK(stopped.state.velocity.x == 0.0f);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_StoresDamageApplicationResultInFrameState", "[battle][core][breakthrough]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 210, 100, 0 }),
        unit(2, 0, { 120, 100, 0 }),
    });
    state.attacks = attackWorld();
    state.damage.aggregatePendingTransactionsByDefender = true;
    state.units.units = {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 10, { 210, 100, 0 }),
        runtimeUnitSnapshot(2, 0, 80, { 120, 100, 0 }),
    };
    state.deathEffects.store.units = { { 0 }, { 1 }, { 2 } };

    auto first = lethalDamageInput(0, 1);
    first.request.baseDamage = 3;
    first.defender.vitals.hp = 10;
    first.defenderStatus.hp = 10;
    auto second = lethalDamageInput(2, 1);
    second.request.baseDamage = 4;
    second.attacker.id = 2;
    second.attacker.vitals.hp = 80;
    second.attacker.vitals.maxHp = 100;
    second.defender.vitals.hp = 10;
    second.defenderStatus.hp = 10;
    state.damage.pendingTransactions.push_back(first);
    state.damage.pendingTransactions.push_back(second);

    BattleDamagePresentationInput firstPresentation;
    firstPresentation.enabled = true;
    firstPresentation.skillName = "先手";
    firstPresentation.detailText = "第一段";
    firstPresentation.normalDamageColor = { 10, 20, 30, 255 };
    firstPresentation.normalDamageTextSize = 22;

    BattleDamagePresentationInput secondPresentation;
    secondPresentation.enabled = true;
    secondPresentation.skillName = "終段";
    secondPresentation.detailText = "第二段";
    secondPresentation.critical = true;
    secondPresentation.emphasizedDamageColor = { 40, 50, 60, 255 };
    secondPresentation.emphasizedDamageTextSize = 33;
    state.damage.pendingPresentation.push_back(firstPresentation);
    state.damage.pendingPresentation.push_back(secondPresentation);

    auto result = runBattleFrame(state);

    REQUIRE(result.damageTransactions.size() == 1);
    CHECK(result.damageTransactions[0].attacker.id == 2);
    CHECK(result.damageTransactions[0].defender.vitals.hp == 3);
    CHECK(std::any_of(
        result.frame.visualEvents.begin(),
        result.frame.visualEvents.end(),
        [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::DamageNumber
                && event.targetUnitId == 1
                && event.amount == 7
                && event.textSize == 33
                && event.color.r == 40;
        }));
    CHECK(std::any_of(
        result.frame.logEvents.begin(),
        result.frame.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Damage
                && event.skillName == "終段"
                && event.detailText == "第二段";
        }));
    CHECK_FALSE(std::any_of(
        result.frame.gameplayEvents.begin(),
        result.frame.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::UnitDied;
        }));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ReducesTeamHealCommandInsideCore", "[battle][core][breakthrough]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 0, { 120, 100, 0 }),
        unit(2, 1, { 180, 100, 0 }),
    });
    state.attacks = attackWorld();
    state.units.units = {
        runtimeUnitSnapshot(0, 0, 50, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 0, 70, { 120, 100, 0 }),
        runtimeUnitSnapshot(2, 1, 80, { 180, 100, 0 }),
    };
    state.teamEffects.pendingCommands.push_back(BattleTeamHealCommand{ 0, 10, 0, "技能群療" });

    auto result = runBattleFrame(state);

    REQUIRE(result.teamEffectEvents.size() == 2);
    CHECK(state.units.requireUnit(0).vitals.hp == 60);
    CHECK(state.units.requireUnit(1).vitals.hp == 80);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AppliesPostSkillInvincibilityToUnitStore", "[battle][core][breakthrough]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 220, 100, 0 }),
    });
    state.world.frame = 1;
    state.attacks = attackWorld();
    state.status.units = {
        statusRuntimeSnapshot(0, 100),
        statusRuntimeSnapshot(1, 100),
    };
    state.units.units = {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 100, { 220, 100, 0 }),
    };
    state.units.units[0].invincible = 1;
    state.units.units[0].animation.cooldown = 1;
    state.units.units[0].haveAction = true;
    state.combo.units[0].postSkillInvincFrames = 5;

    auto result = runBattleFrame(state);

    CHECK(state.units.requireUnit(0).invincible == 5);
    REQUIRE(result.effectCommands.size() == 1);
    CHECK(result.effectCommands[0].type == BattleEffectCommandType::AddInvincibility);
    CHECK(result.effectCommands[0].value == 4);
    CHECK(state.units.requireUnit(0).invincible == 5);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ReducesDeathAoeToPendingProjectileSpawn", "[battle][core][breakthrough]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 120, 100, 0 }),
    });
    state.attacks = attackWorld();
    state.damage.pendingTransactions.push_back(lethalDamageInput(0, 1));
    state.damage.unitEffects[1] = { 50, 6, 1 };
    state.deathEffects.store.units = { { 0 }, { 1 } };
    state.projectileFollowUps.projectileSpeed = SceneProjectileSpeed;
    state.projectileFollowUps.minimumProjectileFrames = 20;
    state.projectileFollowUps.areaProjectileFramePadding = 15;
    state.projectileFollowUps.areaSpawnDistance = SceneTileWidth;
    state.units.units = {
        runtimeUnitSnapshot(0, 0, 100, { 100.0f, 100.0f, 0.0f }),
        runtimeUnitSnapshot(1, 1, 10, { 120.0f, 100.0f, 0.0f }),
    };

    auto result = runBattleFrame(state);

    REQUIRE(state.pendingAttackSpawns.size() == 1);
    CHECK(state.pendingAttackSpawns[0].initial.attackerUnitId == 1);
    CHECK(state.pendingAttackSpawns[0].initial.preferredTargetUnitId == 0);
    CHECK(state.pendingAttackSpawns[0].initial.scriptedStunFrames == 6);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ReducesTempAttackBuffInsideCore", "[battle][core][breakthrough]")
{
    auto frame = hitDamageFrameState(70, 100);
    auto& state = frame.state;
    KysChess::RoleComboState defenderCombo;
    defenderCombo.shield = 10;
    defenderCombo.triggeredEffects.push_back(
        triggeredEffect(KysChess::EffectType::TempFlatATK, KysChess::Trigger::OnShieldBreak, 14, 100, 45));
    state.combo.units.emplace(1, defenderCombo);

    runBattleFrame(state);

    CHECK(state.units.requireUnit(1).stats.attack == 44);
    REQUIRE(state.combo.units.at(1).tempAttackBuffs.size() == 1);
    CHECK(state.combo.units.at(1).tempAttackBuffs[0].attackBonus == 14);
    CHECK(state.combo.units.at(1).tempAttackBuffs[0].remainingFrames == 45);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ReducesAutoUltimateCommandInsideCore", "[battle][core][breakthrough]")
{
    auto frame = hitDamageFrameState(70, 100);
    auto& state = frame.state;
    configureAutoUltimateActionRuntime(state, 1, 0);
    KysChess::RoleComboState defenderCombo;
    defenderCombo.counterUltimateBlockChancePct = 100;
    state.combo.units.emplace(1, defenderCombo);

    auto result = runBattleFrame(state);

    REQUIRE(state.pendingAttackSpawns.size() == 1);
    CHECK(state.pendingAttackSpawns[0].initial.attackerUnitId == 1);
    CHECK(state.pendingAttackSpawns[0].initial.preferredTargetUnitId == 0);
    CHECK(state.pendingAttackSpawns[0].initial.skillId == 401);
    REQUIRE(result.applications.attackSoundIds.size() == 1);
    CHECK(result.applications.attackSoundIds[0] == 55);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_CommitsRuntimeAutoUltimateReadyInsideCore", "[battle][core][breakthrough]")
{
    auto frame = hitDamageFrameState(70, 100);
    auto& state = frame.state;
    configureAutoUltimateActionRuntime(state, 1, 0);
    KysChess::RoleComboState combo;
    combo.autoUltimateAfterFrames = 1;
    combo.autoUltimateTimer = 1;
    state.combo.units[1] = combo;

    auto result = runBattleFrame(state);

    REQUIRE(state.pendingAttackSpawns.size() == 1);
    CHECK(state.pendingAttackSpawns[0].initial.attackerUnitId == 1);
    CHECK(state.pendingAttackSpawns[0].initial.skillId == 401);
    CHECK(std::any_of(
        result.frame.logEvents.begin(),
        result.frame.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Status
                && event.sourceUnitId == 1
                && event.text == "自動絕招·絕招";
        }));
    REQUIRE(result.applications.attackSoundIds.size() == 1);
    CHECK(result.applications.attackSoundIds[0] == 55);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_RunsProtectRescueInsideDamageLifecycle", "[battle][core][breakthrough]")
{
    auto state = rescueDamageFrameState(50, 30);

    auto result = runBattleFrame(state);

    REQUIRE(result.damageTransactions.size() == 1);
    REQUIRE(result.rescueResults.size() == 1);
    const auto& rescue = result.rescueResults.front();
    REQUIRE(rescue.teleport.has_value());
    CHECK(rescue.teleport->unitId == 1);
    CHECK(rescue.teleport->pullerUnitId == 2);
    CHECK(rescue.teleport->destinationCell.x == 2);
    CHECK(rescue.teleport->destinationCell.y == 3);
    CHECK(rescue.counterDelta.unitId == 2);
    CHECK(rescue.counterDelta.protectRemainingDelta == -1);
    CHECK(rescue.heal.targetUnitId == 1);
    CHECK(rescue.heal.amount == 10);
    CHECK(rescue.invincibility.targetUnitId == 1);
    CHECK(rescue.invincibility.frames == 10);
    CHECK(std::any_of(
        result.frame.visualEvents.begin(),
        result.frame.visualEvents.end(),
        [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::RoleEffect
                && event.targetUnitId == 1
                && event.effectId == KysChess::EFT_HEAL;
        }));
    CHECK(state.units.requireUnit(1).vitals.hp == 30);
    CHECK(state.units.requireUnit(1).invincible == 10);
    CHECK(state.combo.units.at(2).forcePullProtectRemaining == 0);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_RunsExecuteRescueAndQueuesCounterAttackInsideDamageLifecycle", "[battle][core][breakthrough]")
{
    auto state = rescueDamageFrameState(20, 10);
    state.combo.units[2].forcePullProtect = false;
    state.combo.units[2].forcePullProtectRemaining = 0;
    state.combo.units[0].forcePullExecute = true;
    state.combo.units[0].forcePullExecuteRemaining = 2;
    state.units.requireUnit(0).grid = { 10, 10 };
    state.units.requireUnit(1).grid = { 5, 5 };
    state.rescue.cells = {
        rescueCell(9, 10),
        rescueCell(10, 9),
        rescueCell(10, 10),
    };

    auto result = runBattleFrame(state);

    REQUIRE(result.rescueResults.size() == 1);
    const auto& rescue = result.rescueResults.front();
    REQUIRE(rescue.teleport.has_value());
    CHECK(rescue.teleport->unitId == 1);
    CHECK(rescue.teleport->pullerUnitId == 0);
    CHECK(rescue.counterDelta.unitId == 0);
    CHECK(rescue.counterDelta.executeRemainingDelta == -1);
    REQUIRE(rescue.basicCounterAttack.has_value());
    CHECK(rescue.basicCounterAttack->attackerUnitId == 0);
    CHECK(rescue.basicCounterAttack->targetUnitId == 1);
    CHECK(state.combo.units.at(0).forcePullExecuteRemaining == 1);
    REQUIRE(state.pendingAttackSpawns.size() == 1);
    CHECK(state.pendingAttackSpawns.front().initial.attackerUnitId == 0);
    CHECK(state.pendingAttackSpawns.front().initial.preferredTargetUnitId == 1);
    CHECK(state.pendingAttackSpawns.front().initial.skillId == 1);
    CHECK(state.pendingAttackSpawns.front().initial.visualEffectId == 11);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DoesNotEmitRescueDeltaWithoutLegalCell", "[battle][core][breakthrough]")
{
    auto state = rescueDamageFrameState(50, 30);
    state.rescue.cells = {
        rescueCell(2, 3, false),
        rescueCell(5, 5),
    };

    auto result = runBattleFrame(state);

    REQUIRE(result.damageTransactions.size() == 1);
    CHECK(result.rescueResults.empty());
    CHECK(state.combo.units.at(2).forcePullProtectRemaining == 1);
    CHECK(state.units.requireUnit(1).vitals.hp == 20);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_CanonicalUnitsSeeCommittedDamageRewards", "[battle][core]")
{
    BattleRuntimeState state;
    state.world = worldWith({});
    state.attacks = attackWorld();
    state.units.units = {
        runtimeUnitSnapshot(0, 0, 40),
        runtimeUnitSnapshot(1, 1, 10),
    };
    state.units.units[0].stats.attack = 12;
    state.units.units[0].stats.defence = 8;
    state.units.units[1].stats.attack = 9;
    state.units.units[1].stats.defence = 6;
    auto input = lethalDamageInput(0, 1);
    input.attacker.vitals.hp = 40;
    input.attacker.vitals.maxHp = 100;
    input.attacker.attack = 12;
    input.attacker.killHealPct = 25;
    input.attacker.bloodlustAttackPerKill = 7;
    state.damage.pendingTransactions.push_back(input);
    state.deathEffects.store.units = {
        { 0 },
        { 1 },
    };

    auto result = runBattleFrame(state);

    REQUIRE(result.damageTransactions.size() == 1);
    REQUIRE(result.deathEffectTrackers.size() == 2);
    CHECK(result.deathEffectTrackers[0].unitId == 0);
    CHECK(result.deathEffectTrackers[1].unitId == 1);
    REQUIRE(state.deathEffects.store.units.size() == 2);
    CHECK(result.damageTransactions.front().attacker.vitals.hp == 65);
    CHECK(result.damageTransactions.front().attacker.attack == 19);
    CHECK(state.units.requireUnit(0).vitals.hp == 65);
    CHECK(state.units.requireUnit(0).stats.attack == 19);
    CHECK(state.units.requireUnit(1).alive == false);
    CHECK(state.units.requireUnit(1).vitals.hp == 0);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_BattleEndEventEmitsOnce", "[battle][core]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 210, 100, 0 }),
    });
    state.attacks = attackWorld();
    state.damage.pendingTransactions.push_back(lethalDamageInput(0, 1));
    state.units.units = {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 10, { 210, 100, 0 }),
    };
    state.deathEffects.store.units = { { 0 }, { 1 } };

    auto first = runBattleFrame(state);
    auto second = runBattleFrame(state);

    int firstEndEvents = 0;
    for (const auto& event : first.frame.gameplayEvents)
    {
        if (event.type == BattleGameplayEventType::BattleEnded)
        {
            ++firstEndEvents;
        }
    }
    int secondEndEvents = 0;
    for (const auto& event : second.frame.gameplayEvents)
    {
        if (event.type == BattleGameplayEventType::BattleEnded)
        {
            ++secondEndEvents;
        }
    }

    CHECK(firstEndEvents == 1);
    CHECK(secondEndEvents == 0);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_RecordsProjectileGameplayEventsSeparatelyFromPresentation", "[battle][core]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 105, 100, 0 }),
    });
    state.attacks = attackWorld();

    BattleAttackInstance projectile;
    projectile.id = 10;
    projectile.state.attackerUnitId = 0;
    projectile.state.totalFrame = 30;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };

    BattleAttackInstance expiringProjectile;
    expiringProjectile.id = 20;
    expiringProjectile.state.attackerUnitId = 0;
    expiringProjectile.state.totalFrame = 1;
    expiringProjectile.noHurt = true;
    expiringProjectile.state.position = { 300, 100, 0 };
    expiringProjectile.state.velocity = { 5, 0, 0 };

    state.attacks.units = {
        { 0, 0, true, false, false, { 100, 100, 0 } },
        { 1, 1, true, false, false, { 105, 100, 0 } },
    };
    seedRuntimeUnitsFromWorld(state);
    state.attacks.attacks.push_back(projectile);
    state.attacks.attacks.push_back(expiringProjectile);

    auto result = runBattleFrame(state);

    REQUIRE(result.attackEvents.size() == 4);
    REQUIRE(result.frame.gameplayEvents.size() == result.attackEvents.size());
    REQUIRE(result.frame.logEvents.size() == result.movement.events.size());
    REQUIRE(result.frame.visualEvents.size() == result.attackEvents.size());

    CHECK(result.attackEvents[0].type == BattleAttackEventType::Moved);
    CHECK(result.frame.gameplayEvents[0].type == BattleGameplayEventType::ProjectileMoved);
    CHECK(result.frame.gameplayEvents[0].effectId == 10);
    CHECK(result.frame.gameplayEvents[0].sourceUnitId == 0);
    CHECK(result.frame.gameplayEvents[0].position.x == 105.0f);
    CHECK(result.frame.visualEvents[0].type == BattleVisualEventType::ProjectileMoved);

    CHECK(result.attackEvents[1].type == BattleAttackEventType::Hit);
    CHECK(result.frame.gameplayEvents[1].type == BattleGameplayEventType::ProjectileHit);
    CHECK(result.frame.gameplayEvents[1].effectId == 10);
    CHECK(result.frame.gameplayEvents[1].sourceUnitId == 0);
    CHECK(result.frame.gameplayEvents[1].targetUnitId == 1);
    CHECK(result.frame.visualEvents[1].type == BattleVisualEventType::ProjectileHit);

    CHECK(result.attackEvents[2].type == BattleAttackEventType::Moved);
    CHECK(result.frame.gameplayEvents[2].type == BattleGameplayEventType::ProjectileMoved);
    CHECK(result.frame.gameplayEvents[2].effectId == 20);
    CHECK(result.frame.visualEvents[2].type == BattleVisualEventType::ProjectileMoved);

    CHECK(result.attackEvents[3].type == BattleAttackEventType::Expired);
    CHECK(result.frame.gameplayEvents[3].type == BattleGameplayEventType::ProjectileExpired);
    CHECK(result.frame.gameplayEvents[3].effectId == 20);
    CHECK(result.frame.visualEvents[3].type == BattleVisualEventType::ProjectileExpired);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ResolvesHitEventsWithFrameHitInputs", "[battle][core]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 105, 100, 0 }),
    });
    state.attacks = attackWorld();
    state.attacks.units = {
        { 0, 0, true, false, false, { 100, 100, 0 } },
        { 1, 1, true, false, false, { 105, 100, 0 } },
    };
    seedRuntimeUnitsFromWorld(state);

    BattleAttackInstance projectile;
    projectile.id = 10;
    projectile.state.attackerUnitId = 0;
    projectile.state.skillId = 101;
    projectile.state.skillMagicPower = 840;
    projectile.state.totalFrame = 30;
    projectile.state.operationType = BattleOperationType::RangedProjectile;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };
    state.attacks.attacks.push_back(projectile);

    auto result = runBattleFrame(state);

    REQUIRE(result.hitResults.size() == 1);
    CHECK(result.hitResults[0].attackerUnitId == 0);
    CHECK(result.hitResults[0].defenderUnitId == 1);
    CHECK(result.hitResults[0].finalHpDamage > 0);

    REQUIRE(result.damageTransactions.size() == 1);
    CHECK(result.damageTransactions.front().attacker.id == 0);
    CHECK(result.damageTransactions.front().defender.id == 1);
    CHECK(result.damageTransactions.front().finalHpDamage == result.hitResults[0].finalHpDamage);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ReducesHitDamageInsideSameFrame", "[battle][core][breakthrough]")
{
    auto frame = hitDamageFrameState(70, 100);
    auto& state = frame.state;

    auto result = runBattleFrame(state);

    REQUIRE(result.hitResults.size() == 1);
    REQUIRE(result.damageTransactions.size() == 1);
    CHECK(result.damageTransactions.front().defender.id == 1);
    CHECK(result.damageTransactions.front().finalHpDamage > 0);
    CHECK(result.damageTransactions.front().defender.vitals.hp < 100);
    CHECK(state.damage.pendingTransactions.empty());
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AppliesDamageTakenMpGainInsideRuntime", "[battle][core][breakthrough]")
{
    auto frame = hitDamageFrameState(70, 100);
    auto& state = frame.state;
    state.world.frame = 1;
    auto& defender = state.units.requireUnit(1);
    defender.vitals.mp = 5;
    defender.vitals.maxMp = 100;
    defender.mpRecoveryBonusPct = 50;

    auto result = runBattleFrame(state);

    REQUIRE(result.damageTransactions.size() == 1);
    const auto& damage = result.damageTransactions.front();
    const int baseGain = static_cast<int>(
        static_cast<double>(damage.finalHpDamage) / damage.defender.vitals.maxHp * 75.0);
    const int expectedGain = static_cast<int>(baseGain * 1.5);
    CHECK(damage.defender.vitals.mp == 5 + expectedGain);
    CHECK(damage.defenderDelta.mpDelta == expectedGain);
    CHECK(state.units.requireUnit(1).vitals.mp == 5 + expectedGain);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DamageTakenMpGainHonorsMpBlock", "[battle][core][breakthrough]")
{
    auto frame = hitDamageFrameState(70, 100);
    auto& state = frame.state;
    state.world.frame = 1;
    auto& defender = state.units.requireUnit(1);
    defender.vitals.mp = 5;
    defender.vitals.maxMp = 100;
    state.status.units[1].effects.mpBlockTimer = 2;

    auto result = runBattleFrame(state);

    REQUIRE(result.damageTransactions.size() == 1);
    const auto& damage = result.damageTransactions.front();
    CHECK(damage.defender.mpBlocked);
    CHECK(damage.defender.vitals.mp == 5);
    CHECK(damage.defenderDelta.mpDelta == 0);
    CHECK(state.units.requireUnit(1).vitals.mp == 5);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AppliesMainProjectileImpactFreezeInCore", "[battle][core][breakthrough]")
{
    auto frame = hitDamageFrameState(70, 100);
    auto& state = frame.state;

    auto result = runBattleFrame(state);

    REQUIRE(result.damageTransactions.size() == 1);
    CHECK(result.damageTransactions.front().defenderStatus.effects.frozenTimer == 5);
    CHECK(result.damageTransactions.front().defenderStatus.effects.frozenMaxTimer == 5);
    CHECK(state.status.units[1].effects.frozenTimer == 5);
    CHECK(state.status.units[1].effects.frozenMaxTimer == 5);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DoesNotApplyImpactFreezeForNonMainProjectile", "[battle][core][breakthrough]")
{
    auto frame = hitDamageFrameState(70, 100);
    auto& state = frame.state;
    state.attacks.attacks.front().state.mainProjectile = false;

    auto result = runBattleFrame(state);

    REQUIRE(result.damageTransactions.size() == 1);
    CHECK(result.damageTransactions.front().defenderStatus.effects.frozenTimer == 0);
    CHECK(state.status.units[1].effects.frozenTimer == 0);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ReducesLethalHitToDeathAndBattleEndInsideSameFrame", "[battle][core][breakthrough]")
{
    auto frame = hitDamageFrameState(120, 20);
    auto& state = frame.state;

    auto result = runBattleFrame(state);

    REQUIRE(result.damageTransactions.size() == 1);
    CHECK_FALSE(result.damageTransactions.front().defender.alive);
    CHECK(state.result.ended);
    CHECK(state.result.winningTeam == 0);
    CHECK(std::any_of(
        result.frame.gameplayEvents.begin(),
        result.frame.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::UnitDied
                && event.sourceUnitId == 0
                && event.targetUnitId == 1;
        }));
    CHECK(std::any_of(
        result.frame.gameplayEvents.begin(),
        result.frame.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::BattleEnded
                && event.amount == 0;
        }));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DodgeConsumesHitBeforeDamage", "[battle][core]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 105, 100, 0 }),
    });
    state.attacks = attackWorld();
    state.attacks.units = {
        { 0, 0, true, false, false, { 100, 100, 0 } },
        { 1, 1, true, false, false, { 105, 100, 0 } },
    };
    seedRuntimeUnitsFromWorld(state);

    BattleAttackInstance projectile;
    projectile.id = 10;
    projectile.state.attackerUnitId = 0;
    projectile.state.skillId = 101;
    projectile.state.skillMagicPower = 840;
    projectile.state.totalFrame = 30;
    projectile.state.operationType = BattleOperationType::RangedProjectile;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };
    state.attacks.attacks.push_back(projectile);

    KysChess::RoleComboState defenderCombo;
    defenderCombo.dodgeChancePct = 100;
    state.combo.units.emplace(1, defenderCombo);

    auto result = runBattleFrame(state);

    REQUIRE(result.hitResults.size() == 1);
    CHECK(result.hitResults[0].dodged);
    CHECK(state.combo.units.at(1).dodgedLast);
    CHECK(state.damage.pendingTransactions.empty());

    const auto dodgeLog = std::find_if(
        result.frame.logEvents.begin(),
        result.frame.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Status
                && event.sourceUnitId == 1
                && event.targetUnitId == 0
                && event.text == "閃避了來襲攻擊";
        });
    CHECK(dodgeLog != result.frame.logEvents.end());
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ResolvesScriptedHitEvents", "[battle][core]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 105, 100, 0 }),
    });
    state.attacks = attackWorld();
    state.attacks.units = {
        { 0, 0, true, false, false, { 100, 100, 0 } },
        { 1, 1, true, false, false, { 105, 100, 0 } },
    };
    seedRuntimeUnitsFromWorld(state);

    BattleAttackInstance projectile;
    projectile.id = 10;
    projectile.state.attackerUnitId = 0;
    projectile.state.scriptedDamage = 33;
    projectile.state.totalFrame = 30;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };
    state.attacks.attacks.push_back(projectile);

    auto result = runBattleFrame(state);

    REQUIRE(result.hitResults.size() == 1);
    CHECK(result.hitResults[0].finalHpDamage == 33);
    REQUIRE(result.damageTransactions.size() == 1);
    CHECK(result.damageTransactions.front().finalHpDamage == 33);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_RecordsTargetLostCancellationWithoutPairedAttack", "[battle][core]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(1, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(3, 1, { 700, 100, 0 }, CombatStyle::Ranged),
    });
    state.attacks = attackWorld();

    BattleAttackInstance projectile;
    projectile.id = 10;
    projectile.state.attackerUnitId = 0;
    projectile.state.preferredTargetUnitId = 0;
    projectile.state.requirePreferredTarget = true;
    projectile.state.totalFrame = 30;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };

    state.attacks.units = {
        { 1, 0, true, false, false, { 100, 100, 0 } },
        { 3, 1, true, false, false, { 700, 100, 0 } },
    };
    state.attacks.attacks.push_back(projectile);

    auto result = runBattleFrame(state);

    REQUIRE(result.attackEvents.size() == 2);
    REQUIRE(result.frame.gameplayEvents.size() == 2);
    REQUIRE(result.frame.logEvents.size() == result.movement.events.size());
    REQUIRE(result.frame.visualEvents.size() == result.attackEvents.size());

    CHECK(result.attackEvents[1].type == BattleAttackEventType::TargetLost);
    CHECK(result.frame.gameplayEvents[1].type == BattleGameplayEventType::ProjectileCancelled);
    CHECK(result.frame.gameplayEvents[1].effectId == 10);
    CHECK(result.frame.gameplayEvents[1].targetUnitId == -1);
    CHECK(result.frame.gameplayEvents[1].otherAttackId == -1);
    CHECK(result.frame.visualEvents[1].type == BattleVisualEventType::ProjectileTargetLost);
    CHECK(result.frame.visualEvents[1].amount == -1);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_RecordsProjectileCancelPairWithOtherAttackId", "[battle][core]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 900, 900, 0 }, CombatStyle::Ranged),
    });
    state.attacks = attackWorld();

    BattleAttackInstance first;
    first.id = 10;
    first.state.attackerUnitId = 0;
    first.frame = 5;
    first.state.totalFrame = 30;
    first.state.position = { 500, 500, 0 };
    first.state.operationType = BattleOperationType::TrackingProjectile;
    first.state.projectileCancelDamage = 11;

    BattleAttackInstance second;
    second.id = 20;
    second.state.attackerUnitId = 1;
    second.frame = 5;
    second.state.totalFrame = 30;
    second.state.position = { 500, 500, 0 };
    second.state.operationType = BattleOperationType::RangedProjectile;
    second.state.projectileCancelDamage = 10;

    state.attacks.units = {
        { 0, 0, true, false, false, { 100, 100, 0 } },
        { 1, 1, true, false, false, { 900, 900, 0 } },
    };
    state.attacks.attacks.push_back(first);
    state.attacks.attacks.push_back(second);

    auto result = runBattleFrame(state);

    REQUIRE(result.attackEvents.size() == 3);
    REQUIRE(result.frame.gameplayEvents.size() == 3);
    CHECK(result.attackEvents[2].type == BattleAttackEventType::ProjectileCancel);
    CHECK(result.attackEvents[2].sourceUnitId == 0);
    CHECK(result.attackEvents[2].otherSourceUnitId == 1);
    CHECK(result.attackEvents[2].projectileCancelDamage == 17);
    CHECK(result.attackEvents[2].otherProjectileCancelDamage == 10);
    CHECK(result.frame.gameplayEvents[2].type == BattleGameplayEventType::ProjectileCancelled);
    CHECK(result.frame.gameplayEvents[2].effectId == 10);
    CHECK(result.frame.gameplayEvents[2].otherAttackId == 20);
    CHECK(result.frame.gameplayEvents[2].amount == 0);
    CHECK(result.frame.visualEvents[2].type == BattleVisualEventType::ProjectileCancelled);
    CHECK(result.frame.visualEvents[2].amount == 20);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_RecordsBounceAsAttackSpawnedGameplay", "[battle][core]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 105, 100, 0 }),
        unit(2, 1, { 180, 100, 0 }),
    });
    state.attacks = attackWorld();

    BattleAttackInstance projectile;
    projectile.id = 10;
    projectile.state.attackerUnitId = 0;
    projectile.state.preferredTargetUnitId = 1;
    projectile.state.totalFrame = 30;
    projectile.state.visualEffectId = 44;
    projectile.state.operationType = BattleOperationType::RangedProjectile;
    projectile.state.bounceRemaining = 1;
    projectile.state.bounceRange = 500;
    projectile.state.bounceChancePct = 100;
    projectile.state.bounceRollPct = 0;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };

    state.attacks.nextAttackId = 30;
    state.attacks.units = {
        { 0, 0, true, false, false, { 100, 100, 0 } },
        { 1, 1, true, false, false, { 105, 100, 0 } },
        { 2, 1, true, false, false, { 180, 100, 0 } },
    };
    seedRuntimeUnitsFromWorld(state);
    state.attacks.attacks.push_back(projectile);

    auto result = runBattleFrame(state);

    REQUIRE(result.attackEvents.size() == 3);
    REQUIRE(result.frame.gameplayEvents.size() == 3);
    REQUIRE(result.frame.logEvents.size() == result.movement.events.size());
    REQUIRE(result.frame.visualEvents.size() == result.attackEvents.size() + 1);
    CHECK(result.attackEvents[2].type == BattleAttackEventType::Bounce);
    CHECK(result.frame.gameplayEvents[2].type == BattleGameplayEventType::AttackSpawned);
    CHECK(result.frame.gameplayEvents[2].effectId == 30);
    CHECK(result.frame.gameplayEvents[2].sourceUnitId == 0);
    CHECK(result.frame.gameplayEvents[2].targetUnitId == 2);
    CHECK(result.frame.visualEvents[2].type == BattleVisualEventType::ProjectileBounced);
    CHECK(result.frame.visualEvents[2].effectId == 10);
    CHECK(result.frame.visualEvents[2].amount == 30);
    const auto& spawnEvent = result.frame.visualEvents[3];
    CHECK(spawnEvent.type == BattleVisualEventType::ProjectileSpawned);
    CHECK(spawnEvent.effectId == 30);
    CHECK(spawnEvent.sourceUnitId == 0);
    CHECK(spawnEvent.targetUnitId == 2);
    CHECK(spawnEvent.durationFrames >= 20);
    CHECK(spawnEvent.visualEffectId == 44);
    CHECK(spawnEvent.position.x != 0.0f);
    CHECK(spawnEvent.velocity.x > 0.0f);
    CHECK(spawnEvent.operationKind == 2);
}
