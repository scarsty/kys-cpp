#include "battle/BattleCombatIntent.h"
#include "battle/BattleCore.h"
#include "battle/BattleLogSegments.h"
#include "battle/BattleMovement.h"
#include "battle/BattleRuntimeSession.h"
#include "battle/BattleRuntimeRules.h"
#include "battle/BattleRuntimeUnitSpawn.h"
#include "ChessEftIds.h"
#include "Find.h"
#include "BattleLogTestHelpers.h"
#include "BattleMovementTestHelpers.h"
#include "BattlePresentationTestHelpers.h"
#include "BattleRuntimeRecordTestHelpers.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <map>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace KysChess::Battle;
using namespace KysChess::Battle::Test;
using namespace KysChess;
using namespace BattlePresentationTest;

template <class T, class = void>
struct HasStatusRuntimeId : std::false_type
{
};

template <class T>
struct HasStatusRuntimeId<T, std::void_t<decltype(std::declval<T&>().id)>> : std::true_type
{
};

static_assert(!HasStatusRuntimeId<BattleStatusRuntimeUnit>::value);

namespace
{

constexpr double SceneTileWidth = 36.0;
constexpr double MaxEffectiveBattleReach = 480.0;
constexpr double SceneAttackHitRadius = SceneTileWidth * 2.0;
constexpr double SceneBounceSpawnDistance = SceneTileWidth * 1.5;
constexpr double SceneProjectileSpeed = SceneTileWidth / 3.0;
constexpr double TestMinimumVectorNorm = 0.0001;
constexpr int BattleCoordCount = 64;

bool hasVisualEvent(const BattlePresentationFrame& frame, BattleVisualEventType type)
{
    return std::any_of(
        frame.visualEvents.begin(),
        frame.visualEvents.end(),
        [type](const BattleVisualEvent& event)
        {
            return event.type == type;
        });
}

const BattleVisualEvent* findVisualEvent(const BattlePresentationFrame& frame, BattleVisualEventType type)
{
    const auto it = std::find_if(
        frame.visualEvents.begin(),
        frame.visualEvents.end(),
        [type](const BattleVisualEvent& event)
        {
            return event.type == type;
        });
    return it != frame.visualEvents.end() ? &*it : nullptr;
}

const BattleVisualEvent* findVisualEvent(
    const BattlePresentationFrame& frame,
    BattleVisualEventType type,
    int effectId)
{
    const auto it = std::find_if(
        frame.visualEvents.begin(),
        frame.visualEvents.end(),
        [type, effectId](const BattleVisualEvent& event)
        {
            return event.type == type && event.effectId == effectId;
        });
    return it != frame.visualEvents.end() ? &*it : nullptr;
}

bool hasGameplayEvent(const BattlePresentationFrame& frame, BattleGameplayEventType type)
{
    return std::any_of(
        frame.gameplayEvents.begin(),
        frame.gameplayEvents.end(),
        [type](const BattleGameplayEvent& event)
        {
            return event.type == type;
        });
}

bool hasProjectilePresentationEvent(const BattlePresentationFrame& frame)
{
    return hasVisualEvent(frame, BattleVisualEventType::ProjectileSpawned)
        || hasVisualEvent(frame, BattleVisualEventType::ProjectileMoved)
        || hasVisualEvent(frame, BattleVisualEventType::ProjectileHit)
        || hasVisualEvent(frame, BattleVisualEventType::ProjectileExpired)
        || hasVisualEvent(frame, BattleVisualEventType::ProjectileTargetLost)
        || hasVisualEvent(frame, BattleVisualEventType::ProjectileCancelled)
        || hasVisualEvent(frame, BattleVisualEventType::ProjectileBounced);
}

TEST_CASE("BattleRuntimeRandom_ReplaysFromSeed", "[battle][random]")
{
    BattleRuntimeRandom first(1234u);
    const int firstA = first.nextInt(1000);
    const int firstB = first.nextInt(1000);
    const double firstC = first.nextPercent();

    BattleRuntimeRandom second(1234u);
    CHECK(second.nextInt(1000) == firstA);
    CHECK(second.nextInt(1000) == firstB);
    CHECK(second.nextPercent() == firstC);

    BattleRuntimeRandom different(1235u);
    CHECK(different.nextInt(1000) != firstA);
}

TEST_CASE("BattleRuntimeRandom_ChanceHandlesGuaranteedOutcomes", "[battle][random]")
{
    BattleRuntimeRandom random(99u);

    CHECK_FALSE(random.chance(0));
    CHECK(random.chance(100));

    BattleRuntimeRandom first(99u);
    BattleRuntimeRandom second(99u);
    CHECK(first.chance(50) == second.chance(50));
}

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

BattlePresentationFrame runBattleFrame(BattleRuntimeState& state)
{
    if (state.action.castFrames.empty())
    {
        state.action.castFrames = { 6, 6, 6, 6 };
        state.action.actionRecoveryFrames = 4;
        state.action.dashRecoveryFrames = 5;
    }
    if (state.action.castConfig.minimumFacingNorm <= 0.0)
    {
        state.action.castConfig.minimumFacingNorm = TestMinimumVectorNorm;
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
    state.team = team;
    state.position = position;
    state.speed = 5.0;
    state.reach = style == CombatStyle::Ranged ? 400.0 : 137.5;
    state.style = style;
    return state;
}

BattleMovementPlanInput worldWith(std::vector<BattleUnitState> units)
{
    BattleMovementPlanInput world;
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

void seedRuntimeUnitsFromMovementUnits(
    BattleRuntimeState& state,
    const std::vector<BattleUnitState>& units,
    int hp = 100)
{
    if (state.gridTransform.tileWidth <= 0.0)
    {
        state.gridTransform = { SceneTileWidth, 64 };
    }
    state.units = {};
    state.damage.presentationStylesByDefender.clear();
    state.units = {};
    for (const auto& unit : units)
    {
        auto runtime = runtimeUnitSnapshot(unit.id, unit.team, hp, unit.position);
        runtime.alive = unit.alive;
        runtime.motion.velocity = unit.velocity;
        runtime.style = unit.style;
        runtime.grid = state.gridTransform.toGrid(runtime.motion.position);
        appendRuntimeUnit(
            state,
            makeRuntimeUnitSpawn(std::move(runtime), KysChess::RoleComboState{}));
    }
}

void seedRuntimeUnits(BattleRuntimeState& state, std::vector<BattleRuntimeUnit> units)
{
    state.units = {};
    state.damage.presentationStylesByDefender.clear();
    state.units = {};

    for (auto& unit : units)
    {
        appendRuntimeUnit(
            state,
            makeRuntimeUnitSpawn(std::move(unit), KysChess::RoleComboState{}));
    }
}

void configureRuntimeMovement(BattleRuntimeState& state, BattleMovementPlanInput input)
{
    state.movement.frame = input.frame;
    state.movement.config = input.config;
    state.movement.terrainCells = std::move(input.terrainCells);
    state.movement.movementReservations = std::move(input.movementReservations);
    seedRuntimeUnitsFromMovementUnits(state, input.units);
}

void seedRuntimeUnitsFromWorld(BattleRuntimeState& state, int hp = 100)
{
    std::vector<BattleUnitState> units;
    units.reserve(state.units.size());
    for (const auto& runtime : state.units.cores())
    {
        auto unit = makeBattleMovementPlanUnit(runtime, BattleRuntimeMoveSpeedDivisor);
        unit.reach = runtime.reach;
        unit.style = runtime.style;
        units.push_back(unit);
    }
    seedRuntimeUnitsFromMovementUnits(state, units, hp);
}

TEST_CASE("BattleRuntimeUnits_RequiresAndMutatesCanonicalUnitValues", "[battle][core][runtime]")
{
    BattleRuntimeUnits store;
    BattleRuntimeUnit unit;
    unit.id = 0;
    unit.team = 0;
    unit.alive = true;
    unit.vitals = { 80, 100, 10, 50 };
    unit.stats.attack = 20;
    unit.stats.defence = 7;
    unit.shield = 3;
    appendRuntimeRecord(store, unit);

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
    store.writeDamageUnit(damage);

    const auto& updated = store.requireCore(0);
    CHECK_FALSE(updated.alive);
    CHECK(updated.vitals.hp == 25);
    CHECK(updated.vitals.mp == 18);
    CHECK(updated.vitals.maxMp == 60);
    CHECK(updated.stats.attack == 31);
    CHECK(updated.invincible == 4);
    CHECK(updated.shield == 9);
}

TEST_CASE("BattleRuntimeUnits_UpdatesPositionAndGridWithCoreTransform", "[battle][core][runtime]")
{
    BattleRuntimeUnits store;
    BattleGridTransform gridTransform{ SceneTileWidth, 64 };
    BattleRuntimeUnit unit;
    unit.id = 0;
    unit.motion.position = { 64.0f * 36.0f, 0.0f, 0.0f };
    appendRuntimeRecord(store, unit);

    store.setPosition(0, { 64.0f * 36.0f + 36.0f, 36.0f, 0.0f }, gridTransform);

    const auto& updated = store.requireCore(0);
    CHECK(updated.motion.position.x == 64.0f * 36.0f + 36.0f);
    CHECK(updated.motion.position.y == 36.0f);
    CHECK(updated.grid.x == 1);
    CHECK(updated.grid.y == 0);
}

TEST_CASE("BattleRuntimeUnits_SelectsNearestAndFarthestLiveEnemyUnits", "[battle][core][runtime]")
{
    auto store = runtimeRecords({
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 100, { 130, 100, 0 }),
        runtimeUnitSnapshot(2, 1, 100, { 260, 100, 0 }),
        runtimeUnitSnapshot(3, 0, 100, { 110, 100, 0 }),
        runtimeUnitSnapshot(4, 1, 0, { 105, 100, 0 }),
    });

    CHECK(findNearestEnemyUnitId(store, 0) == 1);
    CHECK(findFarthestEnemyUnitId(store, 0) == 2);
}

TEST_CASE("BattleRuntimeUnits_TargetSelectionReturnsNoUnitWithoutLiveEnemy", "[battle][core][runtime]")
{
    auto store = runtimeRecords({
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 0, 100, { 130, 100, 0 }),
        runtimeUnitSnapshot(2, 1, 0, { 260, 100, 0 }),
    });

    CHECK(findNearestEnemyUnitId(store, 0) == -1);
    CHECK(findFarthestEnemyUnitId(store, 0) == -1);
}

BattleAttackState attackWorld()
{
    BattleAttackState world;
    world.hitRadius = SceneAttackHitRadius;
    world.minimumVectorNorm = TestMinimumVectorNorm;
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

BattleStatusRuntimeUnit statusRuntimeSnapshot(int unitIndex, int hp)
{
    return makeBattleStatusRuntimeUnit(statusUnitSnapshot(unitIndex, hp));
}

struct HitDamageFrameState
{
    BattleRuntimeState state;
};

HitDamageFrameState hitDamageFrameState(int resolvedBaseDamage, int defenderHp)
{
    HitDamageFrameState frame;
    auto& state = frame.state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 105, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    state.units.require(0).status = statusRuntimeSnapshot(0, 100);
    state.units.require(1).status = statusRuntimeSnapshot(1, 100);
    state.units.require(0).status = statusRuntimeSnapshot(0, 100);
    state.units.require(1).status = statusRuntimeSnapshot(1, 100);
    state.units.require(0).status = statusRuntimeSnapshot(0, 100);
    state.units.require(1).status = statusRuntimeSnapshot(1, 100);

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

    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 80, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, defenderHp, { 105, 100, 0 }),
});
    state.units.require(0).status = statusRuntimeSnapshot(0, 80);
    state.units.require(1).status = statusRuntimeSnapshot(1, defenderHp);
    for (const auto& unit : state.units.cores())
    {
        const auto damage = makeBattleDamageUnitState(
            unit,
            static_cast<const BattleDamageRuntimeUnit*>(nullptr));
        state.units.require(unit.id).damage = makeBattleDamageRuntimeUnit(damage);
        state.units.require(unit.id).combo = KysChess::RoleComboState{};
    }
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
    config.minimumFacingNorm = TestMinimumVectorNorm;
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
    if (!(state.units.find(input.unit.id) != nullptr))
    {
        state.units.require(input.unit.id).combo = {};
    }
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
    state.units.require(input.unit.id).setActionPlan(actionPlanSeedFromCastInput(input));
}

BattlePendingCastAction framePendingCastAction()
{
    auto cast = frameCastInput(0, 1);
    cast.normalSkill.id = 101;
    BattlePendingCastAction pending;
    pending.unitId = 0;
    pending.targetUnitId = 1;
    pending.ultimate = false;
    pending.operationType = BattleOperationType::RangedProjectile;
    pending.skill = cast.normalSkill;
    return pending;
}

void preparePendingCastCommitFrame(BattleRuntimeState& state,
                                   int unitId,
                                   BattleOperationType operationType = BattleOperationType::RangedProjectile,
                                   int actFrame = 6)
{
    auto& unit = state.units.requireCore(unitId);
    unit.haveAction = true;
    unit.animation.actFrame = actFrame;
    unit.operationType = operationType;
    unit.animation.actType = 1;
    unit.animation.cooldown = 10;
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

BattlePendingDamageIntent pendingDamageIntent(
    BattleDamageTransactionInput transaction,
    BattleDamagePresentationInput presentation = {})
{
    return {
        transaction.request,
        std::move(presentation),
    };
}

template <typename T>
T& ensureById(std::vector<T>& items, int id)
{
    if (auto* item = tryFindById(items, id))
    {
        return *item;
    }

    T added;
    added.id = id;
    items.push_back(added);
    return items.back();
}

void applyModifierEffects(RoleComboState& combo, const BattleDamageModifierState& modifier)
{
    if (modifier.flatDamageIncrease != 0)
    {
        ChessBattleEffects::applyEffect(combo, { EffectType::FlatDmgIncrease, modifier.flatDamageIncrease });
    }
    if (modifier.skillDamagePct != 0)
    {
        ChessBattleEffects::applyEffect(combo, { EffectType::SkillDmgPct, modifier.skillDamagePct });
    }
    if (modifier.poisonDamageAmpPct != 0)
    {
        ChessBattleEffects::applyEffect(combo, { EffectType::PoisonDmgAmp, modifier.poisonDamageAmpPct });
    }
    if (modifier.flatDamageReduction != 0)
    {
        ChessBattleEffects::applyEffect(combo, { EffectType::FlatDmgReduction, modifier.flatDamageReduction });
    }
    if (modifier.damageReductionPct != 0)
    {
        ChessBattleEffects::applyEffect(combo, { EffectType::DmgReductionPct, modifier.damageReductionPct });
    }
    if (modifier.maxHitPctMaxHp != 0)
    {
        ChessBattleEffects::applyEffect(combo, { EffectType::MaxHitPctCurrentHP, modifier.maxHitPctMaxHp });
    }
}

void queuePendingDamage(
    BattleRuntimeState& state,
    BattleDamageTransactionInput transaction,
    BattleDamagePresentationInput presentation = {})
{
    if (transaction.attacker.id >= 0)
    {
        auto& combo = state.units.require(transaction.attacker.id).combo;
        applyModifierEffects(combo, transaction.attackerModifiers);
        auto& status = state.units.require(transaction.attacker.id).status;
        status.effects.poisonTimer = transaction.attackerModifiers.poisonTimer;
        status.effects.damageReduceDebuffs.clear();
        for (const auto& debuff : transaction.attackerModifiers.outgoingDamageReduceDebuffs)
        {
            status.effects.damageReduceDebuffs.push_back({ debuff.remainingFrames, debuff.pct });
        }
    }

    if (transaction.attacker.id >= 0)
    {
        state.units.writeDamageUnit(transaction.attacker);
        writeBattleDamageRuntimeUnit(
            state.units.require(transaction.attacker.id).damage,
            transaction.attacker);
    }

    {
        auto& combo = state.units.require(transaction.defender.id).combo;
        applyModifierEffects(combo, transaction.defenderModifiers);
        auto& status = state.units.require(transaction.defender.id).status;
        status.effects.poisonTimer = transaction.defenderModifiers.poisonTimer;
        status.effects.damageReduceDebuffs.clear();
        for (const auto& debuff : transaction.defenderModifiers.outgoingDamageReduceDebuffs)
        {
            status.effects.damageReduceDebuffs.push_back({ debuff.remainingFrames, debuff.pct });
        }
    }
    state.units.writeDamageUnit(transaction.defender);
    writeBattleDamageRuntimeUnit(
        state.units.require(transaction.defender.id).damage,
        transaction.defender);
    writeBattleStatusRuntimeUnit(
        state.units.require(transaction.defenderStatus.id).status,
        transaction.defenderStatus);
    state.nextFrame.queueDamage(pendingDamageIntent(std::move(transaction), std::move(presentation)));
}

TEST_CASE("BattleFrameRunner_RoutesDamageTransactionsThroughRuntimeUnits", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    state.gridTransform = { SceneTileWidth, 64 };
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 120, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 10, { 120, 100, 0 }),
});
    queuePendingDamage(state, lethalDamageInput(0, 1));
    state.units.require(0).status = statusRuntimeSnapshot(0, 100);
    state.units.require(1).status = statusRuntimeSnapshot(1, 10);

    runBattleFrame(state);

    const auto& defender = state.units.requireCore(1);
    CHECK(defender.vitals.hp == 0);
    CHECK_FALSE(defender.alive);
}

TEST_CASE("BattleFrameRunner_RoutesStatusTicksThroughRuntimeUnits", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({ unit(0, 0, { 100, 100, 0 }) }));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
});
    auto status = statusUnitSnapshot(0, 100);
    status.effects.mpBlockTimer = 1;
    status.effects.damageImmunityAfterFrames = 1;
    status.effects.damageImmunityDuration = 5;
    status.effects.damageImmunityTimer = 0;
    state.units.require(0).status = makeBattleStatusRuntimeUnit(status);

    runBattleFrame(state);

    const auto& unit = state.units.requireCore(0);
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

TEST_CASE("BattleStatusSystem_UsesRecordCoreIdWithoutStatusRuntimeId", "[battle][status]")
{
    BattleRuntimeUnitRecord record;
    record.core.id = 7;
    record.core.alive = true;
    record.core.vitals.hp = 80;
    record.core.vitals.maxHp = 100;
    record.core.stats.attack = 15;
    record.status.effects.tempAttackBuffs.push_back({ 5, 1 });

    auto result = BattleStatusSystem({}).tick(record);
    auto damageState = record.statusDamageState();

    REQUIRE(result.events.size() == 1);
    CHECK(result.events.front().unitId == 7);
    CHECK(result.events.front().sourceUnitId == 7);
    CHECK(damageState.id == 7);
}

TEST_CASE("BattleFrameRunner_RoutesMovementPhysicsThroughRuntimeUnits", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({ unit(0, 0, { 100, 100, 0 }) }));
    state.attacks = attackWorld();
    state.gridTransform = { SceneTileWidth, 64 };
    state.units.requireCore(0).motion.velocity = { 5, 0, 0 };
    state.units.requireCore(0).motion.acceleration = { 0, 0, 0 };
    state.units.require(0).movement.physics.position = { 100, 100, 0 };
    state.units.require(0).movement.physics.velocity = { 5, 0, 0 };
    state.units.require(0).movement.physics.acceleration = { 0, 0, 0 };
    state.units.require(0).movement.physics.movementDashFrames = 1;
    state.movementPhysics.config.gravity = 0.0f;
    state.movementPhysics.config.friction = 0.0f;
    state.movementPhysics.config.postDashSpreadFrames = 6;
    state.movementPhysics.terrain.tileWidth = 100.0;
    state.movementPhysics.terrain.coordCount = 2;
    state.movementPhysics.terrain.defaultSeparationDistance = SceneTileWidth;
    state.movementPhysics.terrain.walkableByCell.assign(2 * 2, 1);

    runBattleFrame(state);

    const auto& unit = state.units.requireCore(0);
    CHECK(unit.motion.position.x == 105.0f);
    CHECK(unit.motion.velocity.x == 5.0f);
    CHECK(unit.motion.acceleration.x == 0.0f);
    CHECK(state.units.require(0).movement.physics.movementDashFrames == 0);
    CHECK(state.units.require(0).movement.physics.movementDashSpreadFrames == 6);
}

BattleRescueCellSnapshot rescueCell(int x, int y, bool walkable = true, bool occupied = false)
{
    return {
        x,
        y,
        walkable,
        occupied,
        occupied ? 99 : -1,
        { static_cast<float>(x * SceneTileWidth), static_cast<float>(y * SceneTileWidth), 0.0f },
    };
}

BattleRuntimeState rescueDamageFrameState(int defenderHp, int damage)
{
    BattleRuntimeState state;
    state.gridTransform = { SceneTileWidth, 64 };
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 180, 180, 0 }),
        unit(2, 1, { 72, 72, 0 }),
    }));
    state.attacks = attackWorld();
    state.units.require(0).status = statusRuntimeSnapshot(0, 100);
    state.units.require(1).status = statusRuntimeSnapshot(1, defenderHp);
    state.units.require(2).status = statusRuntimeSnapshot(2, 100);
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, defenderHp, { 180, 180, 0 }),
        runtimeUnitSnapshot(2, 1, 100, { 72, 72, 0 }),
});
    queuePendingDamage(state, preResolvedDamageInput(0, 1, defenderHp, damage));
    state.units.requireCore(0).grid = { 10, 10 };
    state.units.requireCore(1).grid = { 5, 5 };
    state.units.requireCore(2).grid = { 3, 2 };
    state.rescue.cells = {
        rescueCell(2, 3),
        rescueCell(3, 2),
        rescueCell(5, 5),
    };
    KysChess::ChessBattleEffects::applyEffect(
        state.units.require(2).combo,
        { KysChess::EffectType::ForcePullProtect, 1 });
    state.units.require(0).rescue = { 0, 0, 0 };
    state.units.require(1).rescue = { 1, 0, 0 };
    state.units.require(2).rescue = { 2, 1, 0 };
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

TEST_CASE("BattleCombatIntent_DashAttackDoesNotExtendRangedSkillReach", "[battle][intent]")
{
    CombatIntentInput input;
    input.canStartAttack = true;
    input.hasEquippedSkill = true;
    input.dashAttackEnabled = true;
    input.targetDistance = 320.0;
    input.meleeAttackReach = 137.5;
    input.dashAttackReach = 375.0;
    input.plannedSkill = skill(1, 240.0, false);

    auto intent = BattleCombatIntentPlanner().select(input);

    CHECK_FALSE(intent.startAttack);
    CHECK(intent.operationType == BattleOperationType::None);
}

TEST_CASE("BattleCombatIntent_RangedSkillIgnoresDashAttackWhenInReach", "[battle][intent]")
{
    CombatIntentInput input;
    input.canStartAttack = true;
    input.hasEquippedSkill = true;
    input.dashAttackEnabled = true;
    input.targetDistance = 160.0;
    input.meleeAttackReach = 137.5;
    input.dashAttackReach = 375.0;
    input.plannedSkill = skill(1, 240.0, false);

    auto intent = BattleCombatIntentPlanner().select(input);

    CHECK(intent.startAttack);
    CHECK(intent.operationType == BattleOperationType::RangedProjectile);
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
    CHECK(decision.dashFramesRemaining == world.config.dashFrames);
    CHECK(decision.dashCooldownRemaining == world.config.dashCooldownFrames);
}

TEST_CASE("BattleCore_MovementStats_CountsDashStartsOnly", "[battle][core]")
{
    auto run = runMovementPlanForFrames(worldWith({
        unit(1, 0, { 100, 100, 0 }),
        unit(2, 1, { 600, 100, 0 }),
    }), 6);

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
    world.units[1].speed = 0.0;

    auto openResult = BattleMovementPlanner(world).tick();
    CHECK(openResult.decisions.at(1).action == MovementAction::Dash);

    auto blockedWorld = worldWith({
        unit(1, 0, { 100, 100, 0 }),
        unit(2, 0, { 130, 100, 0 }),
        unit(3, 1, { 600, 100, 0 }),
    });
    blockedWorld.units[1].speed = 0.0;
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

    auto firstRun = runMovementPlanForFrames(world, 1);
    CHECK((firstRun.stats.at(1).lastBlockReason == MoveBlockReason::Ally
        || firstRun.stats.at(1).lastBlockReason == MoveBlockReason::Reservation));
    CHECK(firstRun.world.units[0].slotSwitchCooldownRemaining > 0);
    const int slotAfterFirst = firstRun.world.units[0].assignedSlot;

    auto secondRun = runMovementPlanForFrames(firstRun.world, 1);
    CHECK(secondRun.world.units[0].assignedSlot == slotAfterFirst);
    CHECK(secondRun.world.units[0].slotSwitchCooldownRemaining < firstRun.world.units[0].slotSwitchCooldownRemaining);
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
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 600, 100, 0 }),
    }));
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

    seedRuntimeUnitsFromWorld(state);
    state.attacks.attacks.push_back(projectile);

    auto result = runBattleFrame(state);

    CHECK(state.movement.frame == 1);
    REQUIRE(result.logEvents.empty());
    REQUIRE(!result.visualEvents.empty());
    CHECK(result.frame == 1);
    const auto& firstProjectileEvent = result.visualEvents[0];
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
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 240, 100, 0 }),
    }));
    state.attacks = attackWorld();

    BattleDamageTransactionInput damageInput;
    damageInput.request.attackerUnitId = 0;
    damageInput.request.defenderUnitId = 0;
    state.nextFrame.queueDamage(pendingDamageIntent(damageInput));

    state.units.requireCore(0).vitals.hp = 80;

    KysChess::RoleComboState comboState;
    KysChess::ChessBattleEffects::applyEffect(
        comboState,
        { KysChess::EffectType::PostSkillInvincFrames, 12 });
    state.units.require(1).combo = comboState;

    BattleDeathEffectExtras deathEffectExtras;
    deathEffectExtras.id = 1;
    state.units.require(deathEffectExtras.id).deathEffects = deathEffectExtras;

    CHECK(state.units.size() == 2);
    CHECK(state.nextFrame.queuedDamageForTest()[0].request.defenderUnitId == 0);
    CHECK(state.units.requireCore(0).vitals.hp == 80);
    CHECK(KysChess::maxAlwaysEffectValue(state.units.require(1).combo, KysChess::EffectType::PostSkillInvincFrames) == 12);
    CHECK(state.units.require(1).deathEffects.id == 1);
    CHECK_FALSE(state.result.ended);
    CHECK(state.result.winningTeam == -1);
}

TEST_CASE("BattleRuntimeUnitRecord_MpRecoveryBonusStaysSeparateFromMpBlock", "[battle][core][ownership]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
    }));
    auto& runtimeUnit = state.units.require(0);
    runtimeUnit.setMpBlockFrames(5);
    KysChess::ChessBattleEffects::applyEffect(
        state.units.require(0).combo,
        { KysChess::EffectType::MPRecoveryBonus, 40 });

    CHECK(runtimeUnit.mpBlocked());
    CHECK(runtimeUnit.sumAlways(KysChess::EffectType::MPRecoveryBonus) == 40);
}

TEST_CASE("BattleRuntimeUnitRecord_OwnsPerUnitRuntimeFacts", "[battle][core][ownership]")
{
    BattleRuntimeUnitRecord record;
    record.core.id = 7;
    record.core.alive = true;
    record.combo.onSkillTeamHealPending = true;
    record.damage.id = 7;
    record.movement.active = true;
    record.deathEffects.id = 7;
    record.rescue.unitId = 7;

    BattleActionPlanSeed plan;
    plan.unitId = 99;
    record.setActionPlan(plan);

    BattlePendingCastAction pending;
    pending.unitId = 99;
    pending.targetUnitId = 3;
    record.setPendingCast(pending);
    record.markUltimateCaster();

    CHECK(record.id() == 7);
    CHECK(record.alive());
    REQUIRE(record.actionPlan() != nullptr);
    CHECK(record.actionPlan()->unitId == 7);
    REQUIRE(record.pendingCast() != nullptr);
    CHECK(record.pendingCast()->unitId == 7);
    CHECK(record.pendingCast()->targetUnitId == 3);
    CHECK(record.isUltimateCaster());

    record.clearActionOwners();

    CHECK(record.pendingCast() == nullptr);
    CHECK_FALSE(record.isUltimateCaster());
}

TEST_CASE("BattleRuntimeUnitRecord_ActionOwnershipReplacesRuntimeActionMaps", "[battle][core][ownership]")
{
    BattleRuntimeState state;
    BattleRuntimeUnit unit;
    unit.id = 0;
    unit.alive = true;

    BattleActionPlanSeed seed;
    seed.unitId = 0;
    appendRuntimeUnit(state, makeRuntimeUnitSpawn(std::move(unit), KysChess::RoleComboState{}, seed));

    auto& record = state.units.require(0);
    REQUIRE(record.actionPlan() != nullptr);
    CHECK(record.actionPlan()->unitId == 0);

    BattlePendingCastAction pending;
    pending.targetUnitId = 1;
    record.setPendingCast(pending);
    record.markUltimateCaster();

    REQUIRE(record.pendingCast() != nullptr);
    CHECK(record.pendingCast()->unitId == 0);
    CHECK(record.isUltimateCaster());

    record.clearActionOwners();

    CHECK(record.pendingCast() == nullptr);
    CHECK_FALSE(record.isUltimateCaster());
}

TEST_CASE("BattleRuntimeUnitRecord_MovementAgentLivesOnRecord", "[battle][core][movement][ownership]")
{
    BattleRuntimeState state;
    BattleRuntimeUnit unit;
    unit.id = 1;
    unit.alive = true;
    unit.motion.position = { 12.0f, 18.0f, 0.0f };

    appendRuntimeUnit(state, makeRuntimeUnitSpawn(std::move(unit), KysChess::RoleComboState{}));

    auto& record = state.units.require(1);
    record.movement.targetId = 0;
    record.movement.assignedSlot = 2;

    CHECK(record.movement.active);
    CHECK(record.movement.physics.position.x == 12.0f);
    CHECK(record.movement.targetId == 0);
    CHECK(record.movement.assignedSlot == 2);
}

TEST_CASE("BattleRuntimeUnitRecord_StatusDomainMethodsMutateOwnedStatus", "[battle][core][ownership]")
{
    BattleRuntimeUnitRecord record;
    record.core.id = 2;
    record.core.vitals.hp = 50;
    record.core.vitals.maxHp = 100;
    record.status.effects.frozenTimer = 5;
    record.status.effects.frozenMaxTimer = 8;

    record.clearFrozen();
    record.setMpBlockFrames(3);
    record.addTempAttackBuff(7, 11);

    CHECK(record.status.effects.frozenTimer == 0);
    CHECK(record.status.effects.frozenMaxTimer == 0);
    CHECK(record.status.effects.mpBlockTimer == 3);
    REQUIRE(record.status.effects.tempAttackBuffs.size() == 1);
    CHECK(record.status.effects.tempAttackBuffs.front().attackBonus == 7);
    CHECK(record.status.effects.tempAttackBuffs.front().remainingFrames == 11);
}

TEST_CASE("BattleRuntimeUnitRecord_ComboDomainMethodsUseOwnedCombo", "[battle][core][ownership]")
{
    BattleRuntimeUnitRecord record;
    record.core.id = 1;
    record.combo.onSkillTeamHealPending = true;
    AppliedEffectInstance effect;
    effect.type = EffectType::MPRecoveryBonus;
    effect.trigger = Trigger::Always;
    effect.value = 25;
    effect.sourceComboId = -1;
    record.combo.appliedEffects.push_back(effect);

    CHECK(record.sumAlways(EffectType::MPRecoveryBonus) == 25);
    CHECK(record.hasAlways(EffectType::MPRecoveryBonus));

    record.clearPendingSkillHeal();

    CHECK_FALSE(record.combo.onSkillTeamHealPending);
}

TEST_CASE("BattleRuntimeUnitRecord_DamageStateComposesOwnedFacts", "[battle][core][ownership]")
{
    BattleRuntimeUnitRecord record;
    record.core.id = 3;
    record.core.alive = true;
    record.core.vitals.hp = 40;
    record.core.vitals.maxHp = 100;
    record.core.vitals.mp = 5;
    record.core.vitals.maxMp = 20;
    record.damage.id = 3;
    record.damage.blockFirstHitsRemaining = 2;
    record.status.effects.mpBlockTimer = 4;
    AppliedEffectInstance effect;
    effect.type = EffectType::MPRecoveryBonus;
    effect.trigger = Trigger::Always;
    effect.value = 30;
    effect.sourceComboId = -1;
    record.combo.appliedEffects.push_back(effect);

    const auto damage = record.damageState();

    CHECK(damage.id == 3);
    CHECK(damage.blockFirstHitsRemaining == 2);
    CHECK(damage.mpBlocked);
    CHECK(damage.mpRecoveryBonusPct == 30);
}

TEST_CASE("BattleRuntimeUnitRecord_DeathAndRescueFactsLiveOnRecord", "[battle][core][ownership]")
{
    BattleRuntimeUnitRecord record;
    record.core.id = 5;
    record.deathEffects.id = 5;
    record.rescue.unitId = 5;
    record.rescue.forcePullProtectRemaining = 2;
    record.rescue.forcePullExecuteRemaining = 3;

    AppliedEffectInstance effect;
    effect.sourceComboId = 11;
    record.transferDeathAppliedEffect(effect);

    BattleRescueCounterDelta delta;
    delta.unitId = 5;
    delta.protectRemainingDelta = -1;
    delta.executeRemainingDelta = 2;
    record.applyRescueCounterDelta(delta);

    REQUIRE(record.deathEffects.appliedEffects.size() == 1);
    CHECK(record.deathEffects.appliedEffects.front().sourceComboId == 11);
    CHECK(record.forcePullProtectRemaining() == 1);
    CHECK(record.forcePullExecuteRemaining() == 5);

    record.clearForcePullProtect();
    CHECK(record.forcePullProtectRemaining() == 0);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_UsesGroupedRuntimeUnitState", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 600, 100, 0 }),
    }));
    state.attacks = attackWorld();

    auto grouped = runtimeUnitSnapshot(0, 0, 80, { 100, 100, 0 });
    grouped.vitals = { 80, 120, 4, 50 };
    grouped.motion = { { 100, 100, 0 }, { 7, 8, 0 }, { 0, 0, 0 }, { 1, 0, 0 } };
    grouped.animation = { 2, 60, 5, 13 };
    grouped.operationType = BattleOperationType::Dash;
    grouped.haveAction = true;

    auto target = runtimeUnitSnapshot(1, 1, 100, { 600, 100, 0 });
    seedRuntimeUnits(state, { grouped, target });
    state.units.require(0).status = statusRuntimeSnapshot(0, 80);
    state.units.require(1).status = statusRuntimeSnapshot(1, 100);

    runBattleFrame(state);

    const auto& updated = state.units.requireCore(0);
    CHECK(updated.animation.cooldown == 1);
    CHECK(updated.animation.actFrame == 6);
    CHECK(updated.animation.actType == 13);
    CHECK(updated.vitals.mp == 5);
    CHECK(updated.motion.velocity.x == 7.0f);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_RecordsPendingAttackSpawnRequest", "[battle][core][presentation]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({}));
    state.attacks = attackWorld();
    state.attacks.hitRadius = 10.0;
    state.attacks.nextAttackId = 50;
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 0, 0, 0 }),
        runtimeUnitSnapshot(1, 1, 100, { 106, 120, 0 }),
    });
    state.nextFrame.queueAttack(attackSpawnRequest());

    auto result = runBattleFrame(state);

    CHECK(state.nextFrame.queuedAttacksForTest().empty());
    REQUIRE(state.attacks.attacks.size() == 1);
    CHECK(state.attacks.attacks[0].id == 50);
    CHECK(state.attacks.attacks[0].state.position.x == 106.0f);
    CHECK(state.attacks.nextAttackId == 51);

    REQUIRE(result.gameplayEvents.size() >= 3);
    auto gameplayIt = std::find_if(
        result.gameplayEvents.begin(),
        result.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::AttackSpawned
                && event.effectId == 50;
        });
    REQUIRE(gameplayIt != result.gameplayEvents.end());
    CHECK(gameplayIt->sourceUnitId == 0);
    CHECK(gameplayIt->targetUnitId == 0);
    CHECK(gameplayIt->position.x == 100.0f);
    CHECK(gameplayIt->position.y == 120.0f);

    const auto* presentation = findVisualEvent(result, BattleVisualEventType::ProjectileSpawned, 50);
    REQUIRE(presentation);
    CHECK(presentation->sourceUnitId == 0);
    CHECK(presentation->targetUnitId == 0);
    CHECK(presentation->durationFrames == 30);
    CHECK(presentation->visualEffectId == 44);
    CHECK(presentation->position.x == 100.0f);
    CHECK(presentation->position.y == 120.0f);
    CHECK(presentation->velocity.x == 6.0f);
    CHECK(presentation->operationKind == 2);

    const auto* moved = findVisualEvent(result, BattleVisualEventType::ProjectileMoved, 50);
    REQUIRE(moved);
    CHECK(moved->position.x == 106.0f);
    REQUIRE(findVisualEvent(result, BattleVisualEventType::ProjectileHit, 50));
    auto hitGameplayIt = std::find_if(
        result.gameplayEvents.begin(),
        result.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::ProjectileHit
                && event.targetUnitId == 1;
        });
    CHECK(hitGameplayIt != result.gameplayEvents.end());

    CHECK(presentation->effectId == 50);
    CHECK(presentation->visualEffectId == 44);
    CHECK(presentation->position.x == 100.0f);
    CHECK(presentation->velocity.x == 6.0f);
    CHECK(presentation->durationFrames == 30);
    CHECK(presentation->operationKind == 2);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_RunsStatusBeforeCastPlanning", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 210, 100, 0 }),
    }));
    state.attacks = attackWorld();

    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 100, { 210, 100, 0 }),
});
    state.units.requireCore(0).stats.attack = 15;
    auto& statusUnit = state.units.require(0).status;
    statusUnit.effects.tempAttackBuffs.push_back({ 5, 1 });
    statusUnit.effects.frozenTimer = 10;

    auto cast = frameCastInput(0, 1);
    configureRuntimeActionPlan(state, cast);
    state.units.requireCore(0).animation.cooldown = 0;

    auto result = runBattleFrame(state);

    CHECK(state.units.requireCore(0).stats.attack == 10);
    CHECK(state.units.pendingCastCount() == 0);
    CHECK_FALSE(state.units.requireCore(0).haveAction);
    CHECK(result.gameplayEvents.empty());
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_CastPlanningRecordsStartWithoutSpawningAttack", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 10, 20, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 82, 20, 0 }),
    }));
    state.attacks = attackWorld();
    state.attacks.hitRadius = 10.0;
    state.attacks.nextAttackId = 90;
    seedRuntimeUnitsFromWorld(state);
    auto cast = frameCastInput(0, 1);
    cast.normalSkill.attackAreaType = 0;
    cast.normalSkill.forceRanged = true;
    cast.normalSkill.rangedStyle = true;
    cast.normalSkill.reach = 400.0;
    configureRuntimeActionPlan(state, cast);
    state.units.requireCore(0).animation.cooldown = 0;

    auto result = runBattleFrame(state);

    auto pending = state.units.require(0).pendingCast();
    REQUIRE(pending != nullptr);
    CHECK(pending->skill.id == 301);
    CHECK_FALSE(hasProjectilePresentationEvent(result));
    REQUIRE(result.gameplayEvents.size() == 1);
    CHECK(result.gameplayEvents[0].type == BattleGameplayEventType::CastStarted);
    REQUIRE(result.logEvents.size() == 1);
    CHECK(result.logEvents[0].type == BattleLogEventType::Status);
    CHECK(result.logEvents[0].sourceUnitId == 0);
    CHECK(result.logEvents[0].targetUnitId == 1);
    CHECK(BattleLogTest::textOf(result.logEvents[0]) == "施放框架招式");
    CHECK(result.logEvents[0].skillName == "框架招式");
}

TEST_CASE("BattleFrameRunner_ForcedRangedMeleeUsesEffectiveProjectileSelectDistance", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 10, 20, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 260, 20, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    auto cast = frameCastInput(0, 1);
    cast.normalSkill.attackAreaType = 0;
    cast.normalSkill.selectDistance = 1;
    configureRuntimeActionPlan(state, cast);
    KysChess::ChessBattleEffects::applyEffect(
        state.units.require(0).combo,
        { KysChess::EffectType::ForceRangedAttack, 100 });
    state.units.requireCore(0).animation.cooldown = 0;

    auto start = runBattleFrame(state);

    auto pending = state.units.require(0).pendingCast();
    REQUIRE(pending != nullptr);
    CHECK(pending->operationType == BattleOperationType::RangedProjectile);
    CHECK_FALSE(hasProjectilePresentationEvent(start));

    auto& caster = state.units.requireCore(0);
    caster.haveAction = true;
    caster.operationType = BattleOperationType::RangedProjectile;
    caster.animation.actType = 1;
    caster.animation.actFrame = state.action.castFrames[battleOperationIndex(BattleOperationType::RangedProjectile)];
    caster.animation.cooldown = 20;

    auto result = runBattleFrame(state);

    auto projectile = std::find_if(
        result.visualEvents.begin(),
        result.visualEvents.end(),
        [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::ProjectileSpawned;
        });
    REQUIRE(projectile != result.visualEvents.end());
    CHECK(projectile->durationFrames == 30);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_SelectsCastTargetFromRuntimeUnits", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 10, 20, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 260, 20, 0 }),
        unit(2, 1, { 82, 20, 0 }),
    }));
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 10, 20, 0 }),
        runtimeUnitSnapshot(1, 1, 100, { 260, 20, 0 }),
        runtimeUnitSnapshot(2, 1, 100, { 82, 20, 0 }),
});
    state.attacks = attackWorld();
    auto cast = frameCastInput(0, -1);
    cast.targetPosition = {};
    cast.targetDistance = 0.0;
    cast.normalSkill.attackAreaType = 0;
    cast.normalSkill.forceRanged = true;
    cast.normalSkill.rangedStyle = true;
    cast.normalSkill.reach = 400.0;
    configureRuntimeActionPlan(state, cast);
    state.units.requireCore(0).animation.cooldown = 0;

    auto result = runBattleFrame(state);

    auto pending = state.units.require(0).pendingCast();
    REQUIRE(pending != nullptr);
    CHECK(pending->targetUnitId == 2);
    CHECK_FALSE(hasProjectilePresentationEvent(result));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_CastInputUsesCommittedFrameState", "[battle][core]")
{
    BattleRuntimeState state;
    auto caster = unit(0, 0, { 10, 20, 0 }, CombatStyle::Ranged);
    caster.reach = 400.0;
    configureRuntimeMovement(state, worldWith({
        caster,
        unit(1, 1, { 82, 20, 0 }),
    }));
    state.attacks = attackWorld();
    state.attacks.nextAttackId = 70;
    BattleStatusUnitState frozenStatus;
    frozenStatus.id = 0;
    frozenStatus.alive = true;
    frozenStatus.effects.frozenTimer = 3;
    state.units.require(0).status = makeBattleStatusRuntimeUnit(frozenStatus);
    configureRuntimeActionPlan(state, frameCastInput(0, 1));
    state.units.requireCore(0).animation.cooldown = 0;

    auto result = runBattleFrame(state);

    CHECK(state.units.pendingCastCount() == 0);
    CHECK_FALSE(state.units.requireCore(0).haveAction);
    CHECK_FALSE(hasProjectilePresentationEvent(result));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_CastOriginUsesPostMovementPosition", "[battle][core]")
{
    BattleRuntimeState state;
    auto caster = unit(0, 0, { 10, 20, 0 }, CombatStyle::Ranged);
    caster.reach = 400.0;
    configureRuntimeMovement(state, worldWith({
        caster,
        unit(1, 1, { 82, 20, 0 }),
    }));
    state.attacks = attackWorld();
    state.attacks.nextAttackId = 80;
    seedRuntimeUnitsFromWorld(state);
    auto cast = frameCastInput(0, 1);
    cast.unit.position = { -500, -500, 0 };
    cast.targetPosition = { -250, -250, 0 };
    cast.normalSkill.attackAreaType = 0;
    cast.normalSkill.forceRanged = true;
    cast.normalSkill.rangedStyle = true;
    cast.normalSkill.reach = 400.0;
    configureRuntimeActionPlan(state, cast);
    state.units.requireCore(0).animation.cooldown = 0;

    auto result = runBattleFrame(state);

    REQUIRE((state.units.require(0).pendingCast() != nullptr));
    CHECK_FALSE(hasProjectilePresentationEvent(result));
    auto& releaseUnit = state.units.requireCore(0);
    releaseUnit.haveAction = true;
    releaseUnit.operationType = BattleOperationType::RangedProjectile;
    releaseUnit.animation.actType = 1;
    releaseUnit.animation.actFrame = state.action.castFrames[battleOperationIndex(BattleOperationType::RangedProjectile)];
    releaseUnit.animation.cooldown = 20;

    auto release = runBattleFrame(state);

    auto spawn = std::find_if(
        release.visualEvents.begin(),
        release.visualEvents.end(),
        [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::ProjectileSpawned;
        });
    REQUIRE(spawn != release.visualEvents.end());
    CHECK(spawn->position.x == state.units.requireCore(0).motion.position.x + SceneTileWidth * 2.0);
    CHECK(spawn->position.y == state.units.requireCore(0).motion.position.y);
    CHECK_FALSE(hasProjectilePresentationEvent(result));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_CommitsActionInputsBeforeAttackTick", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 160, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    state.units.require(0).combo = {};
    auto& unit = state.units.requireCore(0);
    unit.haveAction = true;
    unit.animation.actFrame = 6;
    unit.operationType = BattleOperationType::RangedProjectile;
    unit.animation.actType = 1;
    unit.animation.cooldown = 10;

    configureRuntimeActionPlan(state, frameCastInput(0, 1));
    state.units.require(0).setPendingCast(framePendingCastAction());

    auto result = runBattleFrame(state);

    CHECK(state.units.pendingCastCount() == 0);
    const auto* spawn = findVisualEvent(result, BattleVisualEventType::ProjectileSpawned, 0);
    REQUIRE(spawn);
    CHECK(spawn->sourceUnitId == 0);
    REQUIRE(state.attacks.attacks.size() >= 1);
    CHECK(state.attacks.attacks.front().state.skillId == 101);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ConsumesPreAttackLocalSpawnsInsideFrame", "[battle][core][breakthrough]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 160, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    state.units.require(0).combo = {};
    preparePendingCastCommitFrame(state, 0);

    configureRuntimeActionPlan(state, frameCastInput(0, 1));
    state.units.require(0).setPendingCast(framePendingCastAction());

    auto result = runBattleFrame(state);

    CHECK(state.nextFrame.queuedAttacksForTest().empty());
    CHECK(state.nextFrame.queuedDamageForTest().empty());
    REQUIRE(state.attacks.attacks.size() == 1);
    CHECK(state.attacks.attacks.front().state.attackerUnitId == 0);
    CHECK(state.attacks.attacks.front().state.skillId == 101);
    CHECK(std::any_of(
        result.gameplayEvents.begin(),
        result.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::AttackSpawned
                && event.sourceUnitId == 0;
        }));

    result = runBattleFrame(state);

    CHECK(std::none_of(
        result.gameplayEvents.begin(),
        result.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::AttackSpawned
                && event.sourceUnitId == 0;
        }));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ConsumesRuntimeOwnedActionDirectives", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 160, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    state.units.require(0).combo = {};
    auto& unit = state.units.requireCore(0);
    unit.haveAction = true;
    unit.animation.actFrame = 6;
    unit.operationType = BattleOperationType::RangedProjectile;
    unit.animation.actType = 1;
    unit.animation.cooldown = 10;

    configureRuntimeActionPlan(state, frameCastInput(0, 1));
    state.units.require(0).setPendingCast(framePendingCastAction());

    auto result = runBattleFrame(state);

    CHECK(state.units.pendingCastCount() == 0);
    REQUIRE(hasVisualEvent(result, BattleVisualEventType::ProjectileSpawned));
}

TEST_CASE("BattleFrameRunner_PlansCastFromRuntimeOwnedCastPlanInput", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    state.units.requireCore(0).animation.cooldown = 0;
    auto cast = frameCastInput(0, 1);
    cast.normalSkill.attackAreaType = 1;
    cast.normalSkill.rangedStyle = true;
    cast.normalSkill.reach = 400.0;
    configureRuntimeActionPlan(state, cast);
    state.units.requireCore(0).animation.cooldown = 0;

    auto result = runBattleFrame(state);

    auto pending = state.units.require(0).pendingCast();
    REQUIRE(pending != nullptr);
    CHECK(pending->skill.id == 301);
}

TEST_CASE("BattleFrameRunner_RuntimeCastStartFacesTargetDirection", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 180, 180, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    state.units.requireCore(0).motion.facing = { 1, 0, 0 };
    state.units.requireCore(0).animation.cooldown = 0;
    auto cast = frameCastInput(0, 1);
    cast.normalSkill.attackAreaType = 1;
    cast.normalSkill.rangedStyle = true;
    cast.normalSkill.reach = 400.0;
    cast.targetPosition = { 180, 180, 0 };
    cast.targetDistance = static_cast<double>((cast.targetPosition - cast.unit.position).norm());
    configureRuntimeActionPlan(state, cast);

    runBattleFrame(state);

    const auto pending = state.units.require(0).pendingCast();
    REQUIRE(pending != nullptr);
    CHECK(pending->targetUnitId == 1);
    const auto& runtimeUnit = state.units.requireCore(0);
    CHECK(runtimeUnit.motion.facing.x > 0.6f);
    CHECK(runtimeUnit.motion.facing.y > 0.6f);
}

TEST_CASE("BattleFrameRunner_RuntimeCastPopulatesProjectileSpreadTargetsFromAliveEnemies", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
        unit(2, 1, { 220, 140, 0 }),
        unit(3, 1, { 220, 90, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    state.units.requireCore(0).animation.cooldown = 0;
    state.units.requireCore(0).vitals.mp = state.units.requireCore(0).vitals.maxMp;
    state.units.requireCore(3).alive = false;

    auto cast = frameCastInput(0, 1);
    cast.ultimateSkill.attackAreaType = 3;
    cast.ultimateSkill.rangedStyle = true;
    cast.ultimateSkill.reach = 400.0;
    cast.ultimateSkill.selectDistance = 4;
    configureRuntimeActionPlan(state, cast);
    state.units.requireCore(0).animation.cooldown = 0;

    auto start = runBattleFrame(state);
    REQUIRE((state.units.require(0).pendingCast() != nullptr));
    CHECK_FALSE(hasProjectilePresentationEvent(start));

    auto& caster = state.units.requireCore(0);
    caster.haveAction = true;
    caster.operationType = BattleOperationType::TrackingProjectile;
    caster.animation.actType = 1;
    caster.animation.actFrame = state.action.castFrames[battleOperationIndex(BattleOperationType::TrackingProjectile)];
    caster.animation.cooldown = 20;

    auto result = runBattleFrame(state);

    std::vector<BattleVisualEvent> spawned;
    std::copy_if(
        result.visualEvents.begin(),
        result.visualEvents.end(),
        std::back_inserter(spawned),
        [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::ProjectileSpawned;
        });
    REQUIRE(spawned.size() == 2);
    CHECK(spawned[0].targetUnitId == 1);
    CHECK(spawned[1].targetUnitId == 2);
}

TEST_CASE("BattleFrameRunner_ClearsDashSpreadWhenRuntimeCastStarts", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    state.units.requireCore(0).animation.cooldown = 0;
    state.units.require(0).movement.physics.movementDashSpreadFrames = 9;
    auto cast = frameCastInput(0, 1);
    cast.normalSkill.attackAreaType = 1;
    cast.normalSkill.rangedStyle = true;
    cast.normalSkill.reach = 400.0;
    configureRuntimeActionPlan(state, cast);
    state.units.requireCore(0).animation.cooldown = 0;

    auto result = runBattleFrame(state);

    REQUIRE((state.units.require(0).pendingCast() != nullptr));
    CHECK(state.units.require(0).movement.physics.movementDashSpreadFrames == 0);
}

TEST_CASE("BattleFrameRunner_RollsDashHitCountFromRuntimeStateWhenDashCastStarts", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Melee),
        unit(1, 1, { 300, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);

    auto& runtimeUnit = state.units.requireCore(0);
    runtimeUnit.animation.cooldown = 0;
    runtimeUnit.stats.speed = 360;
    KysChess::ChessBattleEffects::applyEffect(
        state.units.require(0).combo,
        { KysChess::EffectType::DashAttack, 1 });

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

    auto start = runBattleFrame(state);

    REQUIRE((state.units.require(0).pendingCast() != nullptr));
    CHECK_FALSE(hasProjectilePresentationEvent(start));
    const auto pending = state.units.require(0).pendingCast();
    REQUIRE(pending != nullptr);
    CHECK(pending->operationType == BattleOperationType::Dash);

    runtimeUnit.haveAction = true;
    runtimeUnit.operationType = BattleOperationType::Dash;
    runtimeUnit.animation.actType = 1;
    runtimeUnit.animation.actFrame = state.action.castFrames[battleOperationIndex(BattleOperationType::Dash)];
    runtimeUnit.animation.cooldown = 20;

    auto result = runBattleFrame(state);

    const auto dashHitCount = std::count_if(
        state.attacks.attacks.begin(),
        state.attacks.attacks.end(),
        [](const BattleAttackInstance& attack)
        {
            return attack.state.operationType == BattleOperationType::Dash;
        });
    CHECK(dashHitCount == 3);
}

TEST_CASE("BattleFrameRunner_RangedDashAttackCastsProjectileWithoutDashHits", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 180, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    state.units.requireCore(0).motion.facing = { 1, 0, 0 };
    state.units.requireCore(0).stats.speed = 180;
    state.units.requireCore(0).animation.cooldown = 0;
    KysChess::ChessBattleEffects::applyEffect(
        state.units.require(0).combo,
        { KysChess::EffectType::DashAttack, 1 });

    auto cast = frameCastInput(0, 1);
    cast.normalSkill.attackAreaType = 1;
    cast.normalSkill.rangedStyle = true;
    cast.normalSkill.reach = 400.0;
    cast.targetPosition = { 180, 100, 0 };
    cast.targetDistance = 80.0;
    configureRuntimeActionPlan(state, cast);

    auto start = runBattleFrame(state);

    auto pending = state.units.require(0).pendingCast();
    REQUIRE(pending != nullptr);
    CHECK(pending->operationType == BattleOperationType::RangedProjectile);
    CHECK_FALSE(hasProjectilePresentationEvent(start));
    auto& caster = state.units.requireCore(0);
    caster.haveAction = true;
    caster.operationType = BattleOperationType::RangedProjectile;
    caster.animation.actType = 1;
    caster.animation.actFrame = state.action.castFrames[battleOperationIndex(BattleOperationType::RangedProjectile)];
    caster.animation.cooldown = 20;

    auto result = runBattleFrame(state);

    const auto dashHitCount = std::count_if(
        state.attacks.attacks.begin(),
        state.attacks.attacks.end(),
        [](const BattleAttackInstance& attack)
        {
            return attack.state.operationType == BattleOperationType::Dash;
        });
    CHECK(dashHitCount == 0);
}

TEST_CASE("BattleFrameRunner_MeleeDashCommitSchedulesPostDashRetreat", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Melee),
        unit(1, 1, { 300, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    state.units.require(0).combo = {};
    auto& unit = state.units.requireCore(0);
    unit.haveAction = true;
    unit.animation.actFrame = 6;
    unit.operationType = BattleOperationType::Dash;
    unit.animation.actType = 1;
    unit.animation.cooldown = 10;

    auto cast = frameCastInput(0, 1);
    cast.normalSkill.id = 101;
    cast.normalSkill.attackAreaType = 0;
    cast.normalSkill.rangedStyle = false;
    cast.normalSkill.reach = 350.0;
    configureRuntimeActionPlan(state, cast);
    auto pending = framePendingCastAction();
    pending.operationType = BattleOperationType::Dash;
    pending.skill = cast.normalSkill;
    state.units.require(0).setPendingCast(pending);

    auto result = runBattleFrame(state);

    CHECK(state.units.pendingCastCount() == 0);
    CHECK(state.units.require(0).movement.physics.postDashRetreatVelocity.x < -3.0f);
    CHECK(std::abs(state.units.require(0).movement.physics.postDashRetreatVelocity.y) > 1.0f);
    CHECK(state.units.require(0).movement.physics.postDashRetreatFrames == 11);
    CHECK(state.units.require(0).movement.physics.postDashChaosFrames == 6);
}

TEST_CASE("BattleFrameRunner_PostDashRetreatYieldsMovementPlannerToPhysics", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Melee),
        unit(1, 1, { 210, 100, 0 }),
    }));
    state.attacks = attackWorld();
    state.gridTransform = { SceneTileWidth, 64 };
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 100, { 210, 100, 0 }),
});
    state.movementPhysics.config.gravity = 0.0f;
    state.movementPhysics.config.friction = 0.0f;
    state.movementPhysics.config.postDashSpreadFrames = 6;
    state.movementPhysics.terrain.tileWidth = 100.0;
    state.movementPhysics.terrain.coordCount = 2;
    state.movementPhysics.terrain.defaultSeparationDistance = SceneTileWidth;
    state.movementPhysics.terrain.walkableByCell.assign(2 * 2, 1);
    state.units.require(0).movement.physics.position = { 100, 100, 0 };
    state.units.require(0).movement.physics.velocity = { 0, 0, 0 };
    state.units.require(0).movement.physics.acceleration = { 0, 0, 0 };
    state.units.require(0).movement.physics.postDashRetreatVelocity = { -5, 0, 0 };
    state.units.require(0).movement.physics.postDashRetreatFrames = 2;
    state.units.require(1).movement.physics.position = { 210, 100, 0 };

    runBattleFrame(state);

    CHECK(state.units.requireCore(0).motion.position.x == Catch::Approx(95.0f));
    CHECK(state.units.requireCore(0).motion.velocity.x == Catch::Approx(-5.0f));
    CHECK(state.units.require(0).movement.physics.postDashRetreatFrames == 1);
}

TEST_CASE("BattleFrameRunner_StoresPendingCastIntentWhenCastStarts", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);

    auto cast = frameCastInput(0, 1);
    cast.normalSkill.attackAreaType = 1;
    cast.normalSkill.rangedStyle = true;
    cast.normalSkill.reach = 400.0;
    configureRuntimeActionPlan(state, cast);
    state.units.requireCore(0).animation.cooldown = 0;

    runBattleFrame(state);
    auto pending = state.units.require(0).pendingCast();
    REQUIRE(pending != nullptr);
    CHECK(pending->unitId == 0);
    CHECK(pending->targetUnitId == 1);
    CHECK(pending->skill.id == 301);
    CHECK(pending->operationType == BattleOperationType::RangedProjectile);
}

TEST_CASE("BattleFrameRunner_RefreshesRuntimeCastTargetAtCommitFrame", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }, CombatStyle::Ranged),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);

    auto cast = frameCastInput(0, 1);
    cast.unit.position = { 100, 100, 0 };
    cast.targetPosition = { 220, 100, 0 };
    cast.targetDistance = 120.0;
    cast.normalSkill.attackAreaType = 1;
    cast.normalSkill.rangedStyle = true;
    cast.normalSkill.reach = 400.0;
    configureRuntimeActionPlan(state, cast);
    state.units.requireCore(0).animation.cooldown = 0;

    auto start = runBattleFrame(state);
    REQUIRE((state.units.require(0).pendingCast() != nullptr));
    CHECK_FALSE(hasProjectilePresentationEvent(start));
    REQUIRE((state.units.require(0).pendingCast() != nullptr));

    auto& caster = state.units.requireCore(0);
    caster.haveAction = true;
    caster.operationType = BattleOperationType::RangedProjectile;
    caster.animation.actType = 1;
    caster.animation.actFrame = state.action.castFrames[battleOperationIndex(BattleOperationType::RangedProjectile)];
    caster.animation.cooldown = 20;
    state.units.requireCore(1).motion.position = { 220, 220, 0 };

    auto release = runBattleFrame(state);

    std::vector<BattleVisualEvent> spawned;
    std::copy_if(
        release.visualEvents.begin(),
        release.visualEvents.end(),
        std::back_inserter(spawned),
        [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::ProjectileSpawned;
        });
    REQUIRE(spawned.size() == 3);
    CHECK(spawned[0].velocity.x > 0.0f);
    CHECK(spawned[0].velocity.y > 0.0f);
}

TEST_CASE("BattleFrameRunner_CommitsRuntimeOwnedPendingCastInputWithoutSceneDirective", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    state.units.require(0).combo = {};
    auto& unit = state.units.requireCore(0);
    unit.haveAction = true;
    unit.animation.actFrame = 6;
    unit.operationType = BattleOperationType::RangedProjectile;
    unit.animation.actType = 1;
    unit.animation.cooldown = 10;

    configureRuntimeActionPlan(state, frameCastInput(0, 1));
    state.units.require(0).setPendingCast(framePendingCastAction());

    auto result = runBattleFrame(state);

    CHECK(state.units.pendingCastCount() == 0);
    REQUIRE(hasVisualEvent(result, BattleVisualEventType::ProjectileSpawned));
}

TEST_CASE("BattleFrameRunner_RetargetsPendingCastWhenOriginalTargetDiesBeforeCommit", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
        unit(2, 1, { 220, 220, 0 }),
    }));
    state.movement.frame = 1;
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    state.units.require(0).combo = {};
    preparePendingCastCommitFrame(state, 0);
    auto& caster = state.units.requireCore(0);
    caster.operationCount = 1;
    caster.vitals.mp = 20;
    caster.vitals.maxMp = 50;

    configureRuntimeActionPlan(state, frameCastInput(0, 1));
    state.units.require(0).setPendingCast(framePendingCastAction());
    auto& target = state.units.requireCore(1);
    target.alive = false;
    target.vitals.hp = 0;

    auto result = runBattleFrame(state);

    CHECK(state.units.pendingCastCount() == 0);
    CHECK(state.units.requireCore(0).operationCount == 1);
    CHECK(state.units.requireCore(0).vitals.mp == 25);
    auto request = std::find_if(
        result.visualEvents.begin(),
        result.visualEvents.end(),
        [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::ProjectileSpawned;
        });
    REQUIRE(request != result.visualEvents.end());
    CHECK(request->velocity.x > 0.0f);
    CHECK(request->velocity.y > 0.0f);
}

TEST_CASE("BattleFrameRunner_CancelsPendingCastWhenNoLiveEnemyRemainsBeforeCommit", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    }));
    state.movement.frame = 1;
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    state.units.require(0).combo = {};
    preparePendingCastCommitFrame(state, 0);
    auto& caster = state.units.requireCore(0);
    caster.operationCount = 1;
    caster.vitals.mp = 20;
    caster.vitals.maxMp = 50;

    configureRuntimeActionPlan(state, frameCastInput(0, 1));
    state.units.require(0).setPendingCast(framePendingCastAction());
    auto& target = state.units.requireCore(1);
    target.alive = false;
    target.vitals.hp = 0;

    auto result = runBattleFrame(state);

    CHECK(state.units.pendingCastCount() == 0);
    CHECK(state.units.requireCore(0).operationCount == 1);
    CHECK(state.units.requireCore(0).vitals.mp == 20);
    CHECK_FALSE(state.units.requireCore(0).haveAction);
    CHECK(state.units.requireCore(0).operationType == BattleOperationType::None);
    CHECK(state.units.requireCore(0).animation.actType == -1);
    CHECK_FALSE(hasProjectilePresentationEvent(result));
}

TEST_CASE("BattleFrameRunner_CancelsPendingCastWhenCasterDiesBeforeActionFrame", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    }));
    state.movement.frame = 1;
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    state.units.require(0).combo = {};
    preparePendingCastCommitFrame(state, 0);

    configureRuntimeActionPlan(state, frameCastInput(0, 1));
    state.units.require(0).setPendingCast(framePendingCastAction());
    auto& caster = state.units.requireCore(0);
    caster.alive = false;
    caster.vitals.hp = 0;

    auto result = runBattleFrame(state);

    CHECK(state.units.pendingCastCount() == 0);
    CHECK_FALSE(state.units.requireCore(0).haveAction);
    CHECK(state.units.requireCore(0).operationType == BattleOperationType::None);
    CHECK(state.units.requireCore(0).animation.actType == -1);
    CHECK_FALSE(hasProjectilePresentationEvent(result));
}

TEST_CASE("BattleFrameRunner_PrunesPendingCastWhenCasterDiesDuringDamageLifecycle", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 220, 100, 0 }, CombatStyle::Ranged),
    }));
    state.movement.frame = 1;
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    state.units.require(1).combo = {};
    preparePendingCastCommitFrame(state, 1, BattleOperationType::RangedProjectile, 0);
    configureRuntimeActionPlan(state, frameCastInput(1, 0));
    auto pending = framePendingCastAction();
    pending.unitId = 1;
    pending.targetUnitId = 0;
    state.units.require(1).setPendingCast(pending);

    queuePendingDamage(state, lethalDamageInput(0, 1));

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).size() == 1);
    CHECK_FALSE(state.units.requireCore(1).alive);
    CHECK(state.units.pendingCastCount() == 0);
    CHECK_FALSE(state.units.requireCore(1).haveAction);
    CHECK(state.units.requireCore(1).operationType == BattleOperationType::None);
    CHECK(state.units.requireCore(1).animation.actType == -1);
}

TEST_CASE("BattleFrameRunner_CommitsRuntimeOwnedPendingCastAgainstLiveComboState", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    state.units.require(0).combo = {};
    auto& unit = state.units.requireCore(0);
    unit.haveAction = true;
    unit.animation.actFrame = 6;
    unit.operationType = BattleOperationType::RangedProjectile;
    unit.animation.actType = 1;
    unit.animation.cooldown = 10;

    KysChess::RoleComboState liveCombo;
    liveCombo.dodgeAdaptations.push_back({ 5, 7 });
    liveCombo.dodgeAdaptationStacks.push_back({ { 1, 2 } });
    state.units.require(0).combo = liveCombo;

    configureRuntimeActionPlan(state, frameCastInput(0, 1));
    state.units.require(0).setPendingCast(framePendingCastAction());

    auto result = runBattleFrame(state);

    CHECK(state.units.pendingCastCount() == 0);
    REQUIRE((state.units.find(0) != nullptr));
    REQUIRE(state.units.require(0).combo.dodgeAdaptationStacks.size() == 1);
    REQUIRE(state.units.require(0).combo.dodgeAdaptationStacks[0].contains(1));
    CHECK(state.units.require(0).combo.dodgeAdaptationStacks[0].at(1) == 2);
}

TEST_CASE("BattleFrameRunner_CommitsCastScopedComboEffectsOnActionCommit", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    auto& unit = state.units.requireCore(0);
    unit.haveAction = true;
    unit.animation.actFrame = 6;
    unit.operationType = BattleOperationType::RangedProjectile;
    unit.animation.actType = 1;
    unit.animation.cooldown = 10;
    unit.vitals.mp = 5;
    unit.vitals.maxMp = 50;

    KysChess::RoleComboState combo;
    combo.triggeredEffects.push_back(
        triggeredEffect(KysChess::EffectType::TeamMPRestore, KysChess::Trigger::OnCast, 8, 100));
    combo.triggeredEffects.push_back(
        triggeredEffect(KysChess::EffectType::FlatShield, KysChess::Trigger::OnCast, 12, 100));
    KysChess::ChessBattleEffects::applyEffect(
        combo,
        { KysChess::EffectType::PostSkillInvincFrames, 12 });
    state.units.require(0).combo = combo;

    configureRuntimeActionPlan(state, frameCastInput(0, 1));
    state.units.require(0).setPendingCast(framePendingCastAction());

    auto result = runBattleFrame(state);

    CHECK(state.units.pendingCastCount() == 0);
    CHECK(state.units.requireCore(0).vitals.mp == 19);
    CHECK(state.units.requireCore(0).shield == 12);
    CHECK(state.units.requireCore(0).invincible == 12);
    CHECK(state.units.require(0).combo.effectActivationCounts.at(0) == 1);
    CHECK(state.units.require(0).combo.effectActivationCounts.at(1) == 1);

    const auto invincibilityLog = std::find_if(
        result.logEvents.begin(),
        result.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Status
                && event.sourceUnitId == 0
                && event.targetUnitId == 0
                && event.amount == 12;
        });
    REQUIRE(invincibilityLog != result.logEvents.end());
        CHECK(BattleLogTest::textOf(*invincibilityLog) == "技能後無敵（12幀）");
        CHECK(BattleLogTest::hasSegment(*invincibilityLog, "12", BattleLogTextTone::DurationValue));
        CHECK(BattleLogTest::hasSegment(*invincibilityLog, "幀", BattleLogTextTone::DurationValue));
}

TEST_CASE("BattleFrameRunner_AppliesCastScopedMpRestoreAfterUltimateSpend", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);

    auto& unit = state.units.requireCore(0);
    preparePendingCastCommitFrame(state, 0);
    unit.vitals.mp = 100;
    unit.vitals.maxMp = 100;

    KysChess::RoleComboState combo;
    combo.triggeredEffects.push_back(
        triggeredEffect(KysChess::EffectType::TeamMPRestore, KysChess::Trigger::OnCast, 8, 100));
    state.units.require(0).combo = combo;
    state.units.require(0).markUltimateCaster();

    auto cast = frameCastInput(0, 1);
    cast.normalSkill.id = 101;
    configureRuntimeActionPlan(state, cast);
    auto action = framePendingCastAction();
    action.ultimate = true;
    state.units.require(0).setPendingCast(action);

    runBattleFrame(state);

    CHECK(state.units.pendingCastCount() == 0);
    CHECK(state.units.requireCore(0).vitals.mp == 8);
}

TEST_CASE("BattleFrameRunner_CastScopedMpRestoreDoesNotChangeLaterSameFrameCastSelection", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 0, { 120, 100, 0 }, CombatStyle::Ranged),
        unit(2, 1, { 220, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);

    preparePendingCastCommitFrame(state, 0);
    state.units.requireCore(0).vitals.mp = 5;
    state.units.requireCore(0).vitals.maxMp = 100;
    state.units.requireCore(1).animation.cooldown = 0;
    state.units.requireCore(1).vitals.mp = 90;
    state.units.requireCore(1).vitals.maxMp = 100;

    KysChess::RoleComboState combo;
    combo.triggeredEffects.push_back(
        triggeredEffect(KysChess::EffectType::TeamMPRestore, KysChess::Trigger::OnCast, 10, 100));
    state.units.require(0).combo = combo;
    state.units.require(1).combo = {};

    configureRuntimeActionPlan(state, frameCastInput(0, 2));
    configureRuntimeActionPlan(state, frameCastInput(1, 2));
    state.units.require(0).setPendingCast(framePendingCastAction());

    runBattleFrame(state);

    const auto pending = state.units.require(1).pendingCast();
    REQUIRE(pending != nullptr);
    CHECK_FALSE(pending->ultimate);
    CHECK(pending->skill.id == 301);
    CHECK(state.units.requireCore(1).vitals.mp == 100);
}

TEST_CASE("BattleFrameRunner_CommitsRuntimeOwnedPendingCastSound", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    state.units.require(0).combo = {};
    auto& unit = state.units.requireCore(0);
    unit.haveAction = true;
    unit.animation.actFrame = 6;
    unit.operationType = BattleOperationType::RangedProjectile;
    unit.animation.actType = 1;
    unit.animation.cooldown = 10;

    configureRuntimeActionPlan(state, frameCastInput(0, 1));
    auto action = framePendingCastAction();
    action.skill.soundId = 55;
    state.units.require(0).setPendingCast(action);

    auto result = runBattleFrame(state);

    REQUIRE(result.attackSoundIds.size() == 1);
    CHECK(result.attackSoundIds[0] == 55);
}

TEST_CASE("BattleFrameRunner_ConsumesUltimateCasterWhenRuntimeOwnedCastCommits", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    state.units.require(0).combo = {};
    auto& unit = state.units.requireCore(0);
    unit.haveAction = true;
    unit.animation.actFrame = 6;
    unit.operationType = BattleOperationType::RangedProjectile;
    unit.animation.actType = 1;
    unit.animation.cooldown = 10;
    unit.vitals.mp = 100;
    unit.vitals.maxMp = 100;
    state.units.require(0).markUltimateCaster();

    auto cast = frameCastInput(0, 1);
    cast.normalSkill.id = 101;
    configureRuntimeActionPlan(state, cast);
    auto action = framePendingCastAction();
    action.ultimate = true;
    state.units.require(0).setPendingCast(action);

    auto result = runBattleFrame(state);

    CHECK(state.units.pendingCastCount() == 0);
    CHECK(state.units.ultimateCasterCount() == 0);
    CHECK(state.units.requireCore(0).vitals.mp == 0);
}

TEST_CASE("BattleFrameRunner_AppliesCommittedNormalCastMpGain", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    state.units.require(0).combo = {};
    auto& unit = state.units.requireCore(0);
    unit.haveAction = true;
    unit.animation.actFrame = 6;
    unit.operationType = BattleOperationType::RangedProjectile;
    unit.animation.actType = 1;
    unit.animation.cooldown = 10;
    unit.vitals.mp = 0;
    unit.vitals.maxMp = 100;

    auto cast = frameCastInput(0, 1);
    cast.normalSkill.id = 101;
    configureRuntimeActionPlan(state, cast);
    auto action = framePendingCastAction();
    state.units.require(0).setPendingCast(action);

    auto result = runBattleFrame(state);

    CHECK(state.units.pendingCastCount() == 0);
    CHECK(state.units.requireCore(0).vitals.mp == state.action.castConfig.normalCastMpDelta + 1);
}

TEST_CASE("BattleFrameRunner_PrunesFinishedRuntimeAttacksAfterFrame", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    }));
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
    seedRuntimeUnitsFromWorld(state);
    state.attacks.attacks.push_back(attack);

    auto result = runBattleFrame(state);

    REQUIRE(hasProjectilePresentationEvent(result));
    CHECK(std::any_of(
        result.visualEvents.begin(),
        result.visualEvents.end(),
        [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::ProjectileExpired && event.effectId == 77;
        }));
    CHECK(state.attacks.attacks.empty());
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_CommitsRuntimePendingCastInput", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 160, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    state.units.require(0).combo = {};
    auto& unit = state.units.requireCore(0);
    unit.haveAction = true;
    unit.animation.actFrame = 6;
    unit.operationType = BattleOperationType::RangedProjectile;
    unit.animation.actType = 1;
    unit.animation.cooldown = 10;

    configureRuntimeActionPlan(state, frameCastInput(0, 1));
    state.units.require(0).setPendingCast(framePendingCastAction());

    auto result = runBattleFrame(state);

    CHECK(state.units.pendingCastCount() == 0);
    const auto* spawn = findVisualEvent(result, BattleVisualEventType::ProjectileSpawned);
    REQUIRE(spawn);
    CHECK(spawn->sourceUnitId == 0);
    REQUIRE(state.attacks.attacks.size() >= 1);
    CHECK(state.attacks.attacks.front().state.skillId == 101);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ClearsRecoveredActionFrameUnitState", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    auto& unit = state.units.requireCore(0);
    unit.haveAction = true;
    unit.animation.actFrame = 11;
    unit.animation.actType = 2;
    unit.operationType = BattleOperationType::RangedProjectile;
    unit.animation.cooldown = 30;

    runBattleFrame(state);

    const auto& recovered = state.units.requireCore(0);
    CHECK(recovered.animation.actFrame == 12);
    CHECK_FALSE(recovered.haveAction);
    CHECK(recovered.animation.actType == -1);
    CHECK(recovered.operationType == BattleOperationType::None);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DeadUnitActionCleanupClearsAllActionOwners", "[battle][core][ownership]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 120, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 10, { 120, 100, 0 }),
});
    auto& deadBefore = state.units.requireCore(1);
    deadBefore.animation.cooldown = 4;
    deadBefore.animation.actFrame = 2;
    deadBefore.animation.actType = 3;
    deadBefore.operationType = BattleOperationType::Melee;
    deadBefore.haveAction = true;
    state.units.require(1).setPendingCast(BattlePendingCastAction{});
    state.units.require(1).markUltimateCaster();
    queuePendingDamage(state, lethalDamageInput(0, 1));

    runBattleFrame(state);

    const auto& dead = state.units.requireCore(1);
    CHECK(dead.animation.cooldown == 0);
    CHECK(dead.animation.actFrame == 0);
    CHECK(dead.animation.actType == -1);
    CHECK(dead.operationType == BattleOperationType::None);
    CHECK_FALSE(dead.haveAction);
    CHECK(state.units.require(1).pendingCast() == nullptr);
    CHECK_FALSE(state.units.require(1).isUltimateCaster());
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DamageDeathPrecedesBattleEndEvent", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 210, 100, 0 }),
    }));
    state.attacks = attackWorld();
    state.units.require(0).status = statusRuntimeSnapshot(0, 100);
    state.units.require(1).status = statusRuntimeSnapshot(1, 10);
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 10, { 210, 100, 0 }),
});
    queuePendingDamage(state, lethalDamageInput(0, 1));

    auto result = runBattleFrame(state);

    const auto damageLogs = damageLogsFor(result, 1);
    REQUIRE(damageLogs.size() == 1);
    CHECK(damageLogs[0].amount > 0);
    CHECK(state.units.requireCore(1).vitals.hp == 0);
    CHECK_FALSE(state.units.requireCore(1).alive);
    CHECK(state.result.ended);
    CHECK(state.result.winningTeam == 0);

    std::vector<BattleGameplayEventType> gameplayTypes;
    for (const auto& event : result.gameplayEvents)
    {
        gameplayTypes.push_back(event.type);
    }
    REQUIRE(gameplayTypes.size() >= 3);
    CHECK(gameplayTypes[gameplayTypes.size() - 3] == BattleGameplayEventType::DamageApplied);
    CHECK(gameplayTypes[gameplayTypes.size() - 2] == BattleGameplayEventType::UnitDied);
    CHECK(gameplayTypes[gameplayTypes.size() - 1] == BattleGameplayEventType::BattleEnded);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DeathClearsFrozenStatusAuthority", "[battle][core][ownership]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 120, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 10, { 120, 100, 0 }),
});
    state.units.require(0).status =
        makeBattleStatusRuntimeUnit(makeBattleStatusUnitState(state.units.requireCore(0), state.units.require(0).combo));
    state.units.require(1).status =
        makeBattleStatusRuntimeUnit(makeBattleStatusUnitState(state.units.requireCore(1), state.units.require(1).combo));
    queuePendingDamage(state, lethalDamageInput(0, 1));
    state.units.require(1).status.effects.frozenTimer = 5;
    state.units.require(1).status.effects.frozenMaxTimer = 8;

    runBattleFrame(state);

    CHECK(state.units.require(1).status.effects.frozenTimer == 0);
    CHECK(state.units.require(1).status.effects.frozenMaxTimer == 0);
}

TEST_CASE("BattleFrameRunner_PublishesRenderComboFromRuntimeStores", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 210, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 100, { 210, 100, 0 }),
});
    state.units.requireCore(1).shield = 33;

    BattleDamageRuntimeUnit damage;
    damage.id = 1;
    damage.blockFirstHitsRemaining = 2;
    state.units.require(0).damage = { 0 };
    state.units.require(1).damage = damage;

    runBattleFrame(state);

    CHECK(state.units.requireCore(1).shield == 33);
    CHECK(state.units.require(1).damage.blockFirstHitsRemaining == 2);
}

TEST_CASE("BattleFrameRunner_FirstHitBlockGameplayEventHasStatusText", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 120, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 100, { 120, 100, 0 }),
});
    state.units.require(0).combo = KysChess::RoleComboState{};
    state.units.require(1).combo = KysChess::RoleComboState{};
    BattleDamageTransactionInput damage = preResolvedDamageInput(0, 1, 100, 20);
    damage.defender.blockFirstHitsRemaining = 1;
    queuePendingDamage(state, damage);

    auto result = runBattleFrame(state);

    auto event = std::find_if(
        result.gameplayEvents.begin(),
        result.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::StatusApplied
                && event.targetUnitId == 1;
        });
    REQUIRE(event != result.gameplayEvents.end());
    CHECK(event->text == "格擋了首輪傷害");
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_RunsMovementPhysicsInsideCore", "[battle][core][movement]")
{
    BattleRuntimeState state;
    state.gridTransform = { SceneTileWidth, 64 };
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 200, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 100, { 200, 100, 0 }),
});
    state.units.requireCore(0).motion.velocity = { 5, 0, 0 };
    state.units.requireCore(0).motion.acceleration = { 0, 0, -4 };
    state.units.requireCore(0).motion.velocity = { 5, 0, 0 };
    state.units.requireCore(0).motion.acceleration = { 0, 0, -4 };
    state.units.require(0).status = statusRuntimeSnapshot(0, 100);
    state.units.require(1).status = statusRuntimeSnapshot(1, 100);
    state.units.require(1).status.effects.frozenTimer = 2;
    state.movementPhysics.config.gravity = -4.0f;
    state.movementPhysics.config.friction = 0.1f;
    state.movementPhysics.config.postDashSpreadFrames = 6;
    state.movementPhysics.terrain.tileWidth = 100.0;
    state.movementPhysics.terrain.coordCount = 2;
    state.movementPhysics.terrain.defaultSeparationDistance = SceneTileWidth * 1.5;
    state.movementPhysics.terrain.walkableByCell.assign(2 * 2, 1);
    state.units.require(0).movement.physics.position = { 100, 100, 0 };
    state.units.require(0).movement.physics.velocity = { 5, 0, 0 };
    state.units.require(0).movement.physics.acceleration = { 0, 0, -4 };
    state.units.require(0).movement.physics.movementDashFrames = 1;
    state.units.require(1).movement.physics.position = { 200, 100, 0 };
    state.units.require(1).movement.physics.velocity = { 5, 0, 0 };
    state.units.require(1).movement.physics.acceleration = { 0, 0, -4 };

    runBattleFrame(state);

    const auto& movedUnit = state.units.requireCore(0);
    CHECK(movedUnit.motion.position.x == 105.0f);
    CHECK(state.units.require(0).movement.physics.movementDashFrames == 0);
    CHECK(state.units.require(0).movement.physics.movementDashSpreadFrames == 6);

    const auto& stoppedUnit = state.units.requireCore(1);
    CHECK(state.units.require(1).status.effects.frozenTimer == 1);
    CHECK(stoppedUnit.motion.position.x == 200.0f);
    CHECK(stoppedUnit.motion.velocity.x == 0.0f);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_KeepsMovingCorpsesInMovementPhysics", "[battle][core][movement]")
{
    BattleRuntimeState state;
    state.gridTransform = { SceneTileWidth, 64 };
    auto dead = unit(1, 1, { 200, 100, 0 });
    dead.alive = false;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        dead,
    }));
    state.attacks = attackWorld();
    state.units.requireCore(0).motion.velocity = { 5, 0, 0 };
    state.units.requireCore(1).motion.velocity = { 6, 0, 8 };
    state.units.requireCore(1).motion.acceleration = { 0, 0, -4 };
    state.units.require(1).movement.physics.velocity = { 6, 0, 8 };
    state.units.require(1).movement.physics.acceleration = { 0, 0, -4 };
    state.movementPhysics.config.gravity = 0.0f;
    state.movementPhysics.config.friction = 0.0f;
    state.movementPhysics.config.postDashSpreadFrames = 6;
    state.movementPhysics.terrain.tileWidth = 100.0;
    state.movementPhysics.terrain.coordCount = 2;
    state.movementPhysics.terrain.defaultSeparationDistance = SceneTileWidth;
    state.movementPhysics.terrain.walkableByCell.assign(2 * 2, 1);

    runBattleFrame(state);

    const auto& corpse = state.units.requireCore(1);
    CHECK(corpse.motion.position.x == 206.0f);
    CHECK(corpse.motion.position.z == 8.0f);
    CHECK((state.units.find(1) != nullptr));
    CHECK(state.units.require(1).movement.active);
    CHECK(state.units.require(1).movement.physics.position.x == 206.0f);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_InactivatesInertDeadMovementAgents", "[battle][core][movement][ownership]")
{
    BattleRuntimeState state;
    state.gridTransform = { SceneTileWidth, 64 };
    auto dead = unit(1, 1, { 200, 100, 0 });
    dead.alive = false;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        dead,
    }));
    state.attacks = attackWorld();
    state.units.requireCore(0).motion.velocity = { 5, 0, 0 };
    state.movementPhysics.config.gravity = 0.0f;
    state.movementPhysics.config.friction = 0.0f;
    state.movementPhysics.config.postDashSpreadFrames = 6;
    state.movementPhysics.terrain.tileWidth = 100.0;
    state.movementPhysics.terrain.coordCount = 2;
    state.movementPhysics.terrain.defaultSeparationDistance = SceneTileWidth;
    state.movementPhysics.terrain.walkableByCell.assign(2 * 2, 1);

    runBattleFrame(state);

    REQUIRE((state.units.find(1) != nullptr));
    CHECK_FALSE(state.units.require(1).movement.active);
    CHECK(state.units.requireCore(1).motion.position.x == Catch::Approx(200.0f));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AppliesDeathKickVelocity", "[battle][core][movement]")
{
    BattleRuntimeState state;
    state.gridTransform = { SceneTileWidth, 64 };
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 200, 100, 0 }),
    }));
    state.attacks = attackWorld();
    state.movementPhysics.config.gravity = -4.0f;
    state.movementPhysics.config.friction = 0.0f;
    state.movementPhysics.config.postDashSpreadFrames = 6;
    state.movementPhysics.terrain.tileWidth = 100.0;
    state.movementPhysics.terrain.coordCount = 2;
    state.movementPhysics.terrain.defaultSeparationDistance = SceneTileWidth;
    state.movementPhysics.terrain.walkableByCell.assign(2 * 2, 1);
    state.units.requireCore(1).motion.velocity = { -50.0f, 20.0f, 40.0f };
    state.units.require(1).movement.physics.velocity = state.units.requireCore(1).motion.velocity;
    state.units.requireCore(0).stats.speed = 0;
    state.units.requireCore(1).stats.speed = 0;

    state.nextFrame.queueDamage({
        .request = {
            .attackerUnitId = 0,
            .defenderUnitId = 1,
            .baseDamage = 30,
            .preResolvedDamage = true,
        },
    });
    state.units.requireCore(1).vitals.hp = 20;
    writeBattleDamageRuntimeUnit(
        state.units.require(1).damage,
        makeBattleDamageUnitState(state.units.requireCore(1), static_cast<const BattleDamageRuntimeUnit*>(nullptr)));

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).size() == 1);
    CHECK_FALSE(state.units.requireCore(1).alive);
    CHECK_FALSE(gameplayEventsFor(result, BattleGameplayEventType::UnitDied, 1).empty());
    const auto& dead = state.units.requireCore(1);
    const double deathSpeed = std::sqrt(
        dead.motion.velocity.x * dead.motion.velocity.x
        + dead.motion.velocity.y * dead.motion.velocity.y
        + dead.motion.velocity.z * dead.motion.velocity.z);
    CHECK_FALSE(dead.alive);
    CHECK(dead.motion.velocity.x > dead.motion.velocity.z);
    CHECK(dead.motion.velocity.y != Catch::Approx(20.0));
    CHECK(dead.motion.velocity.x > 0.0f);
    CHECK(dead.motion.velocity.z > 0.0f);
    CHECK(dead.motion.velocity.z <= 6.0f);
    CHECK(deathSpeed == Catch::Approx(20.0 / 3.0 + 5.0));
    CHECK(dead.motion.position.x == Catch::Approx(200.0f));
    CHECK(dead.motion.position.z == Catch::Approx(36.0f));
    CHECK((state.units.find(1) != nullptr));
    CHECK(state.units.require(1).movement.physics.velocity.z == dead.motion.velocity.z);

}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ClampsDeathKickVelocity", "[battle][core][movement]")
{
    BattleRuntimeState state;
    state.gridTransform = { SceneTileWidth, 64 };
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 200, 100, 0 }),
    }));
    state.attacks = attackWorld();
    state.movementPhysics.config.gravity = -4.0f;
    state.movementPhysics.config.friction = 0.0f;
    state.movementPhysics.config.postDashSpreadFrames = 6;
    state.movementPhysics.terrain.tileWidth = 100.0;
    state.movementPhysics.terrain.coordCount = 16;
    state.movementPhysics.terrain.defaultSeparationDistance = SceneTileWidth;
    state.movementPhysics.terrain.walkableByCell.assign(16 * 16, 1);

    state.nextFrame.queueDamage({
        .request = {
            .attackerUnitId = 0,
            .defenderUnitId = 1,
            .baseDamage = 1000,
            .preResolvedDamage = true,
        },
    });
    state.units.requireCore(1).vitals.hp = 500;
    writeBattleDamageRuntimeUnit(
        state.units.require(1).damage,
        makeBattleDamageUnitState(state.units.requireCore(1), static_cast<const BattleDamageRuntimeUnit*>(nullptr)));

    auto result = runBattleFrame(state);

    const auto damageAmounts = damageLogAmountsFor(result, 1);
    REQUIRE(damageAmounts.size() == 1);
    CHECK_FALSE(state.units.requireCore(1).alive);
    CHECK_FALSE(gameplayEventsFor(result, BattleGameplayEventType::UnitDied, 1).empty());
    const auto& dead = state.units.requireCore(1);
    CHECK(damageAmounts[0] / 3.0 + 5.0 > 75.0);
    CHECK_FALSE(dead.alive);
    CHECK(dead.motion.velocity.norm() == Catch::Approx(75.0));
    CHECK(dead.motion.velocity.z == Catch::Approx(6.0));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_SeedsDeathKickForNextFramePhysics", "[battle][core][movement]")
{
    BattleRuntimeState state;
    state.gridTransform = { SceneTileWidth, 64 };
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 200, 100, 0 }),
        unit(2, 0, { 120, 100, 0 }),
    }));
    state.attacks = attackWorld();
    state.movementPhysics.config.gravity = -4.0f;
    state.movementPhysics.config.friction = 0.0f;
    state.movementPhysics.config.postDashSpreadFrames = 6;
    state.movementPhysics.terrain.tileWidth = 100.0;
    state.movementPhysics.terrain.coordCount = 16;
    state.movementPhysics.terrain.defaultSeparationDistance = SceneTileWidth;
    state.movementPhysics.terrain.walkableByCell.assign(16 * 16, 1);

    state.nextFrame.queueDamage({
        .request = {
            .attackerUnitId = 0,
            .defenderUnitId = 1,
            .baseDamage = 30,
            .preResolvedDamage = true,
        },
    });
    state.nextFrame.queueDamage({
        .request = {
            .attackerUnitId = 2,
            .defenderUnitId = 1,
            .baseDamage = 30,
            .preResolvedDamage = true,
        },
    });
    state.units.requireCore(1).vitals.hp = 20;
    writeBattleDamageRuntimeUnit(
        state.units.require(1).damage,
        makeBattleDamageUnitState(state.units.requireCore(1), static_cast<const BattleDamageRuntimeUnit*>(nullptr)));

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).size() == 1);
    CHECK_FALSE(state.units.requireCore(1).alive);
    CHECK_FALSE(gameplayEventsFor(result, BattleGameplayEventType::UnitDied, 1).empty());

    const auto& dead = state.units.requireCore(1);
    CHECK_FALSE(dead.alive);
    CHECK(dead.motion.velocity.norm() > 0.01f);
    CHECK(dead.motion.position.z >= 0.0f);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_MovingCorpsePhysicsPersistsIntoRuntime", "[battle][core][movement]")
{
    BattleRuntimeState state;
    state.gridTransform = { SceneTileWidth, 64 };
    auto dead = unit(1, 1, { 200, 100, 0 });
    dead.alive = false;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        dead,
    }));
    state.attacks = attackWorld();
    state.units.requireCore(1).motion.velocity = { 6, 0, 8 };
    state.units.requireCore(1).motion.acceleration = { 0, 0, -4 };
    state.units.require(1).movement.physics.velocity = { 6, 0, 8 };
    state.units.require(1).movement.physics.acceleration = { 0, 0, -4 };
    state.movementPhysics.config.gravity = -4.0f;
    state.movementPhysics.config.friction = 0.0f;
    state.movementPhysics.config.postDashSpreadFrames = 6;
    state.movementPhysics.terrain.tileWidth = 100.0;
    state.movementPhysics.terrain.coordCount = 2;
    state.movementPhysics.terrain.defaultSeparationDistance = SceneTileWidth;
    state.movementPhysics.terrain.walkableByCell.assign(2 * 2, 1);

    runBattleFrame(state);
    const auto& firstFrameCorpse = state.units.requireCore(1);
    CHECK(firstFrameCorpse.motion.position.z == 8.0f);

    runBattleFrame(state);

    const auto& deadUnit = state.units.requireCore(1);
    CHECK(deadUnit.motion.position.z == 12.0f);
    CHECK(deadUnit.motion.velocity.z == 0.0f);
    CHECK(state.units.require(1).movement.physics.position.z == deadUnit.motion.position.z);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_StoresDamageApplicationResultInFrameState", "[battle][core][breakthrough]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 210, 100, 0 }),
        unit(2, 0, { 120, 100, 0 }),
    }));
    state.attacks = attackWorld();
    state.damage.sortPendingDamageByDefenderMagnitude = true;
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 10, { 210, 100, 0 }),
        runtimeUnitSnapshot(2, 0, 80, { 120, 100, 0 }),
});

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

    BattleDamagePresentationInput firstPresentation;
    firstPresentation.enabled = true;
    firstPresentation.skillName = "先手";
    firstPresentation.segments = battleLogText("第一段");
    firstPresentation.normalDamageColor = { 10, 20, 30, 255 };
    firstPresentation.normalDamageTextSize = 22;

    BattleDamagePresentationInput secondPresentation;
    secondPresentation.enabled = true;
    secondPresentation.skillName = "終段";
    secondPresentation.segments = battleLogText("第二段");
    secondPresentation.critical = true;
    secondPresentation.emphasizedDamageColor = { 40, 50, 60, 255 };
    secondPresentation.emphasizedDamageTextSize = 33;

    queuePendingDamage(state, first, firstPresentation);
    queuePendingDamage(state, second, secondPresentation);

    auto result = runBattleFrame(state);

    CHECK(damageLogSourceIdsFor(result, 1) == std::vector<int>{ 2, 0 });
    CHECK(damageLogAmountsFor(result, 1) == std::vector<int>{ 4, 3 });
    CHECK(state.units.requireCore(1).vitals.hp == 3);
    CHECK(std::any_of(
        result.visualEvents.begin(),
        result.visualEvents.end(),
        [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::DamageNumber
                && event.targetUnitId == 1
                && event.amount == 4
                && event.textSize == 33
                && event.color.r == 40;
        }));
    CHECK(std::any_of(
        result.logEvents.begin(),
        result.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Damage
                && event.skillName == "終段"
                && BattleLogTest::textOf(event) == "第二段";
        }));
    CHECK_FALSE(std::any_of(
        result.gameplayEvents.begin(),
        result.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::UnitDied;
        }));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DeathPreventionKeepsRuntimeUnitAlive", "[battle][core][breakthrough]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 200, 100, 0 }),
    }));
    state.attacks = attackWorld();
    state.nextFrame.queueDamage({
        .request = {
            .attackerUnitId = 0,
            .defenderUnitId = 1,
            .baseDamage = 99,
            .preResolvedDamage = true,
        },
    });
    state.units.requireCore(1).vitals.hp = 20;
    auto& runtime = state.units.require(1).damage;
    runtime.deathPrevention = true;
    runtime.deathPreventionFrames = 30;

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).size() == 1);
    CHECK(state.units.requireCore(1).alive);
    CHECK(gameplayEventsFor(result, BattleGameplayEventType::UnitDied, 1).empty());
    const auto& defender = state.units.requireCore(1);
    CHECK(defender.alive);
    CHECK(defender.vitals.hp == 1);
    CHECK(defender.invincible == 30);
    CHECK(runtime.deathPreventionUsed);
    CHECK(runtime.deathPreventionFrames == 30);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DeathPreventionUsedRuntimeDoesNotTriggerAgain", "[battle][core][breakthrough]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 200, 100, 0 }),
    }));
    state.attacks = attackWorld();
    state.nextFrame.queueDamage({
        .request = {
            .attackerUnitId = 0,
            .defenderUnitId = 1,
            .baseDamage = 99,
            .preResolvedDamage = true,
        },
    });
    state.units.requireCore(1).vitals.hp = 20;
    auto& runtime = state.units.require(1).damage;
    runtime.deathPrevention = true;
    runtime.deathPreventionFrames = 30;
    runtime.deathPreventionUsed = true;

    auto result = runBattleFrame(state);

    CHECK_FALSE(state.units.requireCore(1).alive);
    CHECK_FALSE(gameplayEventsFor(result, BattleGameplayEventType::UnitDied, 1).empty());
    CHECK(runtime.deathPreventionUsed);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DoesNotApplyPostSkillInvincibilityOnCooldownFinish", "[battle][core][breakthrough]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 220, 100, 0 }),
    }));
    state.movement.frame = 1;
    state.attacks = attackWorld();
    state.units.require(0).status = statusRuntimeSnapshot(0, 100);
    state.units.require(1).status = statusRuntimeSnapshot(1, 100);
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 100, { 220, 100, 0 }),
});
    state.units.requireCore(0).invincible = 1;
    state.units.requireCore(0).animation.cooldown = 1;
    state.units.requireCore(0).haveAction = true;
    KysChess::ChessBattleEffects::applyEffect(
        state.units.require(0).combo,
        { KysChess::EffectType::PostSkillInvincFrames, 5 });

    auto result = runBattleFrame(state);

    CHECK(state.units.requireCore(0).invincible == 0);
    CHECK(result.logEvents.empty());
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DeathAoeProjectileDamagesOnNextFrame", "[battle][core][breakthrough]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 120, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100.0f, 100.0f, 0.0f }),
        runtimeUnitSnapshot(1, 1, 10, { 120.0f, 100.0f, 0.0f }),
});
    queuePendingDamage(state, lethalDamageInput(0, 1));
    KysChess::ChessBattleEffects::applyEffect(
        state.units.require(1).combo,
        { EffectType::DeathAOE, 50, 1, "", Trigger::Always, 0, 6 });
    state.projectileFollowUps.projectileSpeed = SceneProjectileSpeed;
    state.projectileFollowUps.minimumProjectileFrames = 20;
    state.projectileFollowUps.areaProjectileFramePadding = 15;
    state.projectileFollowUps.areaSpawnDistance = SceneTileWidth;

    auto result = runBattleFrame(state);

    REQUIRE(state.nextFrame.queuedAttacksForTest().size() == 1);
    CHECK(state.nextFrame.queuedAttacksForTest()[0].initial.attackerUnitId == 1);
    CHECK(state.nextFrame.queuedAttacksForTest()[0].initial.preferredTargetUnitId == 0);
    CHECK(state.nextFrame.queuedAttacksForTest()[0].initial.scriptedDamage == 50);
    CHECK(state.nextFrame.queuedAttacksForTest()[0].initial.scriptedStunFrames == 6);
    CHECK(std::none_of(
        result.gameplayEvents.begin(),
        result.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::AttackSpawned
                && event.sourceUnitId == 1
                && event.targetUnitId == 0;
        }));
    CHECK(damageLogsFor(result, 0).empty());

    result = runBattleFrame(state);

    CHECK(std::any_of(
        result.logEvents.begin(),
        result.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Damage
                && event.targetUnitId == 0
                && event.amount == 50;
        }));
    CHECK(state.units.requireCore(0).vitals.hp == 50);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ReducesAllyDeathEffectsInsideDamageLifecycle", "[battle][core][breakthrough]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 120, 100, 0 }),
        unit(2, 1, { 140, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 10, { 120, 100, 0 }),
        runtimeUnitSnapshot(2, 1, 50, { 140, 100, 0 }),
});
    state.units.requireCore(2).stats.attack = 10;
    state.units.require(0).status = statusRuntimeSnapshot(0, 100);
    state.units.require(1).status = statusRuntimeSnapshot(1, 10);
    state.units.require(2).status = statusRuntimeSnapshot(2, 50);
    queuePendingDamage(state, lethalDamageInput(0, 1));

    BattleDeathEffectExtras dead;
    dead.id = 1;
    dead.comboIds = { 9 };
    dead.appliedEffects.push_back({ EffectType::DeathMedical, 20, 0, "", Trigger::Always, 0, 0, 0, 9 });

    BattleDeathEffectExtras ally;
    ally.id = 2;
    ally.shieldPctMaxHp = 30;
    ally.comboIds = { 9 };
    ally.appliedEffects.push_back({ EffectType::AllyDeathStatBoost, 4, 0, "", Trigger::Always, 0, 0, 0, 9 });
    ally.appliedEffects.push_back({ EffectType::ShieldOnAllyDeath, 1, 0, "", Trigger::Always, 0, 0, 0, 9 });
    state.units.require(dead.id).deathEffects = dead;
    state.units.require(ally.id).deathEffects = ally;
    state.deathEffects.store.regularSynergyComboIds.insert(9);

    auto result = runBattleFrame(state);

    CHECK(state.units.requireCore(2).stats.attack == 14);
    CHECK(state.units.requireCore(2).vitals.hp == 70);
    CHECK(state.units.requireCore(2).shield == 30);
    CHECK(std::any_of(
        result.logEvents.begin(),
        result.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.sourceUnitId == 2
                && event.targetUnitId == 2
                && BattleLogTest::textOf(event) == "同袍之死（攻防+4）";
        }));
    CHECK(std::any_of(
        result.logEvents.begin(),
        result.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.sourceUnitId == 1
                && event.targetUnitId == 2
                && BattleLogTest::textOf(event) == "死亡醫療";
        }));
    CHECK(std::any_of(
        result.logEvents.begin(),
        result.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.sourceUnitId == 1
                && event.targetUnitId == 2
                && BattleLogTest::textOf(event) == "護盾重獲（30護盾）";
        }));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ReducesTempAttackBuffInsideCore", "[battle][core][breakthrough]")
{
    auto frame = hitDamageFrameState(70, 100);
    auto& state = frame.state;
    KysChess::RoleComboState defenderCombo;
    defenderCombo.triggeredEffects.push_back(
        triggeredEffect(KysChess::EffectType::TempFlatATK, KysChess::Trigger::OnShieldBreak, 14, 100, 45));
    state.units.require(1).combo = defenderCombo;
    state.units.requireCore(1).shield = 10;

    runBattleFrame(state);

    CHECK(state.units.requireCore(1).stats.attack == 44);
    const auto& status = state.units.require(1).status;
    REQUIRE(status.effects.tempAttackBuffs.size() == 1);
    CHECK(status.effects.tempAttackBuffs[0].attackBonus == 14);
    CHECK(status.effects.tempAttackBuffs[0].remainingFrames == 45);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ReducesAutoUltimateCommandInsideCore", "[battle][core][breakthrough]")
{
    auto frame = hitDamageFrameState(70, 100);
    auto& state = frame.state;
    configureAutoUltimateActionRuntime(state, 1, 0);
    KysChess::RoleComboState defenderCombo;
    KysChess::ChessBattleEffects::applyEffect(
        defenderCombo,
        { KysChess::EffectType::CounterUltimateBlock, 100 });
    state.units.require(1).combo = defenderCombo;

    auto result = runBattleFrame(state);

    REQUIRE(state.nextFrame.queuedAttacksForTest().size() == 4);
    CHECK(state.nextFrame.queuedAttacksForTest()[0].initial.attackerUnitId == 1);
    CHECK(state.nextFrame.queuedAttacksForTest()[0].initial.preferredTargetUnitId == -1);
    CHECK_FALSE(state.nextFrame.queuedAttacksForTest()[0].initial.requirePreferredTarget);
    CHECK(state.nextFrame.queuedAttacksForTest()[0].initial.skillId == 401);
    CHECK(state.nextFrame.queuedAttacksForTest()[1].initial.castSubrequestKind == BattleAttackCastSubrequestKind::ExtraProjectile);
    CHECK(state.nextFrame.queuedAttacksForTest()[1].initial.strengthMultiplier == 0.35f);
    REQUIRE(result.attackSoundIds.size() == 1);
    CHECK(result.attackSoundIds[0] == 55);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_CommitsRuntimeAutoUltimateReadyInsideCore", "[battle][core][breakthrough]")
{
    auto frame = hitDamageFrameState(70, 100);
    auto& state = frame.state;
    configureAutoUltimateActionRuntime(state, 1, 0);
    KysChess::RoleComboState combo;
    KysChess::ChessBattleEffects::applyEffect(combo, { KysChess::EffectType::AutoUltimateAfterFrames, 1 });
    combo.effectFrameTimers[0] = 1;
    state.units.require(1).combo = combo;

    auto result = runBattleFrame(state);

    REQUIRE(state.nextFrame.queuedAttacksForTest().size() == 4);
    CHECK(state.nextFrame.queuedAttacksForTest()[0].initial.attackerUnitId == 1);
    CHECK(state.nextFrame.queuedAttacksForTest()[0].initial.skillId == 401);
    CHECK(state.nextFrame.queuedAttacksForTest()[1].initial.castSubrequestKind == BattleAttackCastSubrequestKind::ExtraProjectile);
    CHECK(state.nextFrame.queuedAttacksForTest()[1].initial.strengthMultiplier == 0.35f);
    CHECK(std::any_of(
        result.logEvents.begin(),
        result.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Status
                && event.sourceUnitId == 1
                && BattleLogTest::textOf(event) == "自動絕招·絕招";
        }));
    REQUIRE(result.attackSoundIds.size() == 1);
    CHECK(result.attackSoundIds[0] == 55);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DropsDeferredAutoUltimateWhenBattleEndsFirst", "[battle][core][breakthrough]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 180, 100, 0 }),
        unit(2, 0, { 120, 120, 0 }, CombatStyle::Ranged),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 10, { 180, 100, 0 }),
        runtimeUnitSnapshot(2, 0, 100, { 120, 120, 0 }),
});
    state.units.require(0).status = statusRuntimeSnapshot(0, 100);
    state.units.require(1).status = statusRuntimeSnapshot(1, 10);
    state.units.require(2).status = statusRuntimeSnapshot(2, 100);

    queuePendingDamage(state, lethalDamageInput(0, 1));
    configureAutoUltimateActionRuntime(state, 2, 1);
    KysChess::RoleComboState combo;
    KysChess::ChessBattleEffects::applyEffect(combo, { KysChess::EffectType::AutoUltimateAfterFrames, 1 });
    combo.effectFrameTimers[0] = 1;
    state.units.require(2).combo = combo;

    auto result = runBattleFrame(state);

    CHECK(state.result.ended);
    CHECK(state.result.winningTeam == 0);
    CHECK(state.nextFrame.queuedAttacksForTest().empty());
    CHECK(std::none_of(
        result.logEvents.begin(),
        result.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Status
                && event.sourceUnitId == 2
                && BattleLogTest::textOf(event) == "自動絕招·絕招";
        }));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_RunsProtectRescueInsideDamageLifecycle", "[battle][core][breakthrough]")
{
    auto state = rescueDamageFrameState(50, 30);

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).size() == 1);
    CHECK(state.units.requireCore(1).motion.position.x == Catch::Approx(2.0f * SceneTileWidth));
    CHECK(state.units.requireCore(1).motion.position.y == Catch::Approx(3.0f * SceneTileWidth));
    CHECK(std::any_of(
        result.visualEvents.begin(),
        result.visualEvents.end(),
        [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::RoleEffect
                && event.targetUnitId == 1
                && event.effectId == KysChess::EFT_HEAL;
        }));
    CHECK(state.units.requireCore(1).vitals.hp == 30);
    CHECK(state.units.requireCore(1).invincible == 10);
    CHECK(state.units.require(2).forcePullProtectRemaining() == 0);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_RunsExecuteRescueAndQueuesCounterAttackInsideDamageLifecycle", "[battle][core][breakthrough]")
{
    auto state = rescueDamageFrameState(20, 10);
    state.units.require(2).rescue.forcePullProtectRemaining = 0;
    KysChess::ChessBattleEffects::applyEffect(
        state.units.require(0).combo,
        { KysChess::EffectType::ForcePullExecute, 2 });
    state.units.require(0).rescue.forcePullExecuteRemaining = 2;
    state.units.requireCore(0).grid = { 10, 10 };
    state.units.requireCore(1).grid = { 5, 5 };
    state.rescue.cells = {
        rescueCell(9, 10),
        rescueCell(10, 9),
        rescueCell(10, 10),
    };

    auto result = runBattleFrame(state);

    CHECK(state.units.requireCore(1).motion.position.x == Catch::Approx(9.0f * SceneTileWidth));
    CHECK(state.units.requireCore(1).motion.position.y == Catch::Approx(10.0f * SceneTileWidth));
    CHECK(state.units.require(0).forcePullExecuteRemaining() == 1);
    REQUIRE(state.nextFrame.queuedAttacksForTest().size() == 1);
    CHECK(state.nextFrame.queuedAttacksForTest().front().initial.attackerUnitId == 0);
    CHECK(state.nextFrame.queuedAttacksForTest().front().initial.preferredTargetUnitId == 1);
    CHECK(state.nextFrame.queuedAttacksForTest().front().initial.skillId == 1);
    CHECK(state.nextFrame.queuedAttacksForTest().front().initial.visualEffectId == 11);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DoesNotEmitRescueDeltaWithoutLegalCell", "[battle][core][breakthrough]")
{
    auto state = rescueDamageFrameState(50, 30);
    state.rescue.cells = {
        rescueCell(2, 3, false),
        rescueCell(5, 5),
    };

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).size() == 1);
    CHECK(state.units.require(2).forcePullProtectRemaining() == 1);
    CHECK(state.units.requireCore(1).motion.position.x == Catch::Approx(180.0f));
    CHECK(state.units.requireCore(1).motion.position.y == Catch::Approx(180.0f));
    CHECK(state.units.requireCore(1).vitals.hp == 20);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_CanonicalUnitsSeeCommittedDamageRewards", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({}));
    state.attacks = attackWorld();
    std::vector<BattleRuntimeUnit> units = {
        runtimeUnitSnapshot(0, 0, 40),
        runtimeUnitSnapshot(1, 1, 10),
    };
    units[0].stats.attack = 12;
    units[0].stats.defence = 8;
    units[1].stats.attack = 9;
    units[1].stats.defence = 6;
    seedRuntimeUnits(state, std::move(units));
    auto input = lethalDamageInput(0, 1);
    input.attacker.vitals.hp = 40;
    input.attacker.vitals.maxHp = 100;
    input.attacker.attack = 12;
    input.attacker.killHealPct = 25;
    input.attacker.bloodlustAttackPerKill = 7;
    queuePendingDamage(state, input);
    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).size() == 1);
    REQUIRE(state.units.size() == 2);
    CHECK(state.units.requireCore(0).vitals.hp == 65);
    CHECK(state.units.requireCore(0).stats.attack == 19);
    CHECK(state.units.requireCore(1).alive == false);
    CHECK(state.units.requireCore(1).vitals.hp == 0);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_BattleEndEventEmitsOnce", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 210, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 10, { 210, 100, 0 }),
});
    queuePendingDamage(state, lethalDamageInput(0, 1));

    auto first = runBattleFrame(state);
    auto second = runBattleFrame(state);

    int firstEndEvents = 0;
    for (const auto& event : first.gameplayEvents)
    {
        if (event.type == BattleGameplayEventType::BattleEnded)
        {
            ++firstEndEvents;
        }
    }
    int secondEndEvents = 0;
    for (const auto& event : second.gameplayEvents)
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
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 105, 100, 0 }),
    }));
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

    seedRuntimeUnitsFromWorld(state);
    state.attacks.attacks.push_back(projectile);
    state.attacks.attacks.push_back(expiringProjectile);

    auto result = runBattleFrame(state);

    REQUIRE(result.gameplayEvents.size() == 4);
    REQUIRE(result.logEvents.empty());
    REQUIRE(result.visualEvents.size() == 4);

    CHECK(result.gameplayEvents[0].type == BattleGameplayEventType::ProjectileMoved);
    CHECK(result.gameplayEvents[0].effectId == 10);
    CHECK(result.gameplayEvents[0].sourceUnitId == 0);
    CHECK(result.gameplayEvents[0].position.x == 105.0f);
    CHECK(result.visualEvents[0].type == BattleVisualEventType::ProjectileMoved);

    CHECK(result.gameplayEvents[1].type == BattleGameplayEventType::ProjectileHit);
    CHECK(result.gameplayEvents[1].effectId == 10);
    CHECK(result.gameplayEvents[1].sourceUnitId == 0);
    CHECK(result.gameplayEvents[1].targetUnitId == 1);
    CHECK(result.visualEvents[1].type == BattleVisualEventType::ProjectileHit);

    CHECK(result.gameplayEvents[2].type == BattleGameplayEventType::ProjectileMoved);
    CHECK(result.gameplayEvents[2].effectId == 20);
    CHECK(result.visualEvents[2].type == BattleVisualEventType::ProjectileMoved);

    CHECK(result.gameplayEvents[3].type == BattleGameplayEventType::ProjectileExpired);
    CHECK(result.gameplayEvents[3].effectId == 20);
    CHECK(result.visualEvents[3].type == BattleVisualEventType::ProjectileExpired);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_QueuesHitGeneratedProjectilesForNextFrame", "[battle][core][ownership]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 105, 100, 0 }),
        unit(2, 1, { 140, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    state.units.require(0).combo.triggeredEffects.push_back(
        triggeredEffect(KysChess::EffectType::NearbyTrackingProjectiles, KysChess::Trigger::OnHit, 80, 100));
    state.units.require(0).combo.triggeredEffects.back().value2 = 45;

    BattleAttackInstance projectile;
    projectile.id = 10;
    projectile.state.attackerUnitId = 0;
    projectile.state.skillId = 101;
    projectile.state.skillMagicPower = 480;
    projectile.state.totalFrame = 30;
    projectile.state.operationType = BattleOperationType::RangedProjectile;
    projectile.state.visualEffectId = 44;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };
    state.attacks.attacks.push_back(projectile);

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).size() == 1);
    REQUIRE(state.nextFrame.queuedAttacksForTest().size() == 2);
    CHECK(state.nextFrame.queuedAttacksForTest()[0].initial.attackerUnitId == 0);
    CHECK(state.nextFrame.queuedAttacksForTest()[0].initial.preferredTargetUnitId == 1);
    CHECK(state.nextFrame.queuedAttacksForTest()[0].initial.suppressNearbyTrackingProjectileProc);
    CHECK(state.nextFrame.queuedAttacksForTest()[1].initial.attackerUnitId == 0);
    CHECK(state.nextFrame.queuedAttacksForTest()[1].initial.preferredTargetUnitId == 2);
    CHECK(state.nextFrame.queuedAttacksForTest()[1].initial.suppressNearbyTrackingProjectileProc);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ResolvesHitEventsWithFrameHitInputs", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 105, 100, 0 }),
    }));
    state.attacks = attackWorld();
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

    const auto damageLogs = damageLogsFor(result, 1);
    REQUIRE(damageLogs.size() == 1);
    CHECK(damageLogs[0].sourceUnitId == 0);
    CHECK(damageLogs[0].amount > 0);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ReducesHitDamageInsideSameFrame", "[battle][core][breakthrough]")
{
    auto frame = hitDamageFrameState(70, 100);
    auto& state = frame.state;

    auto result = runBattleFrame(state);

    const auto damageLogs = damageLogsFor(result, 1);
    REQUIRE(damageLogs.size() == 1);
    CHECK(damageLogs[0].amount > 0);
    CHECK(state.units.requireCore(1).vitals.hp < 100);
    CHECK(state.nextFrame.queuedDamageForTest().empty());
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AppliesDamageTakenMpGainInsideRuntime", "[battle][core][breakthrough]")
{
    auto frame = hitDamageFrameState(70, 100);
    auto& state = frame.state;
    state.movement.frame = 1;
    auto& defender = state.units.requireCore(1);
    defender.vitals.mp = 5;
    defender.vitals.maxMp = 100;
    KysChess::ChessBattleEffects::applyEffect(
        state.units.require(1).combo,
        { KysChess::EffectType::MPRecoveryBonus, 50 });

    auto result = runBattleFrame(state);

    const auto damageAmounts = damageLogAmountsFor(result, 1);
    REQUIRE(damageAmounts.size() == 1);
    const int baseGain = static_cast<int>(
        static_cast<double>(damageAmounts[0]) / state.units.requireCore(1).vitals.maxHp * 75.0);
    const int expectedGain = static_cast<int>(baseGain * 1.5);
    CHECK(state.units.requireCore(1).vitals.mp == 5 + expectedGain);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AccumulatesDamageTakenMpGainAcrossSameFrameHits", "[battle][core][breakthrough]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 210, 100, 0 }),
        unit(2, 0, { 120, 100, 0 }),
    }));
    state.movement.frame = 1;
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 100, { 210, 100, 0 }),
        runtimeUnitSnapshot(2, 0, 80, { 120, 100, 0 }),
});
    state.units.require(0).status = statusRuntimeSnapshot(0, 100);
    state.units.require(1).status = statusRuntimeSnapshot(1, 100);
    state.units.require(2).status = statusRuntimeSnapshot(2, 80);

    auto first = preResolvedDamageInput(0, 1, 100, 20);
    first.defender.vitals.mp = 5;
    first.defender.vitals.maxMp = 100;

    auto second = preResolvedDamageInput(2, 1, 100, 20);
    second.attacker.id = 2;
    second.attacker.vitals.hp = 80;
    second.attacker.vitals.maxHp = 100;
    second.defender.vitals.mp = 5;
    second.defender.vitals.maxMp = 100;
    KysChess::ChessBattleEffects::applyEffect(
        state.units.require(1).combo,
        { KysChess::EffectType::MPRecoveryBonus, 50 });

    queuePendingDamage(state, first);
    queuePendingDamage(state, second);

    auto result = runBattleFrame(state);

    const auto damageAmounts = damageLogAmountsFor(result, 1);
    CHECK(damageAmounts == std::vector<int>{ 20, 20 });
    const int baseGain = static_cast<int>(20.0 / 100.0 * 75.0);
    const int expectedGainPerHit = static_cast<int>(baseGain * 1.5);
    CHECK(expectedGainPerHit > 0);
    CHECK(state.units.requireCore(1).vitals.mp == 5 + expectedGainPerHit * 2);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ExecutePreviewUsesResolvedPendingDamage", "[battle][core][breakthrough]")
{
    auto frame = hitDamageFrameState(3, 30);
    auto& state = frame.state;
    state.movement.frame = 1;
    state.units.requireCore(1).stats.defence = 200;
    state.units.require(0).combo.triggeredEffects.push_back(
        triggeredEffect(KysChess::EffectType::Execute, KysChess::Trigger::OnHit, 5, 100));

    BattleDamageTransactionInput pending;
    pending.request.attackerUnitId = 0;
    pending.request.defenderUnitId = 1;
    pending.request.baseDamage = 40;
    pending.request.preResolvedDamage = false;
    pending.attacker = makeBattleDamageUnitState(
        state.units.requireCore(0),
        &state.units.require(0).damage);
    pending.defender = makeBattleDamageUnitState(
        state.units.requireCore(1),
        &state.units.require(1).damage);
    pending.defenderStatus = makeBattleStatusUnitState(
        state.units.require(1).status,
        state.units.requireCore(1));
    queuePendingDamage(state, pending);

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).size() == 1);
    CHECK_FALSE(state.units.requireCore(1).alive);
    CHECK_FALSE(gameplayEventsFor(result, BattleGameplayEventType::UnitDied, 1).empty());
    CHECK(std::none_of(
        result.visualEvents.begin(),
        result.visualEvents.end(),
        [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::FloatingText
                && event.targetUnitId == 1
                && event.text == "處決！";
        }));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ExecuteUsesCommittedPendingDamage", "[battle][core][breakthrough]")
{
    auto frame = hitDamageFrameState(3, 60);
    auto& state = frame.state;
    state.movement.frame = 1;
    state.units.require(0).combo.triggeredEffects.push_back(
        triggeredEffect(KysChess::EffectType::Execute, KysChess::Trigger::OnHit, 50, 100));
    state.damage.presentationStylesByDefender[1].executeTextSize = 44;

    queuePendingDamage(state, preResolvedDamageInput(0, 1, 60, 25));

    auto result = runBattleFrame(state);

    const auto damageAmounts = damageLogAmountsFor(result, 1);
    REQUIRE(damageAmounts.size() == 2);
    CHECK(damageAmounts[0] == 25);
    CHECK(damageAmounts[1] > 0);
    CHECK_FALSE(state.units.requireCore(1).alive);
    CHECK_FALSE(gameplayEventsFor(result, BattleGameplayEventType::UnitDied, 1).empty());
    CHECK(std::any_of(
        result.visualEvents.begin(),
        result.visualEvents.end(),
        [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::FloatingText
                && event.targetUnitId == 1
                && event.text == "處決！";
        }));
    CHECK(std::any_of(
        result.logEvents.begin(),
        result.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Status
                && event.sourceUnitId == 0
                && event.targetUnitId == 1
                && BattleLogTest::textOf(event) == "觸發處決（斬殺線50%）";
        }));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DamageTakenMpGainHonorsMpBlock", "[battle][core][breakthrough]")
{
    auto frame = hitDamageFrameState(70, 100);
    auto& state = frame.state;
    state.movement.frame = 1;
    auto& defender = state.units.requireCore(1);
    defender.vitals.mp = 5;
    defender.vitals.maxMp = 100;
    state.units.require(1).status.effects.mpBlockTimer = 2;

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).size() == 1);
    CHECK(state.units.requireCore(1).vitals.mp == 5);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AppliesMainProjectileImpactFreezeInCore", "[battle][core][breakthrough]")
{
    auto frame = hitDamageFrameState(70, 100);
    auto& state = frame.state;

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).size() == 1);
    CHECK(state.units.require(1).status.effects.frozenTimer == 5);
    CHECK(state.units.require(1).status.effects.frozenMaxTimer == 5);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DoesNotApplyImpactFreezeForNonMainProjectile", "[battle][core][breakthrough]")
{
    auto frame = hitDamageFrameState(70, 100);
    auto& state = frame.state;
    state.attacks.attacks.front().state.mainProjectile = false;

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).size() == 1);
    CHECK(state.units.require(1).status.effects.frozenTimer == 0);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ReducesLethalHitToDeathAndBattleEndInsideSameFrame", "[battle][core][breakthrough]")
{
    auto frame = hitDamageFrameState(120, 20);
    auto& state = frame.state;

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).size() == 1);
    CHECK_FALSE(state.units.requireCore(1).alive);
    CHECK_FALSE(gameplayEventsFor(result, BattleGameplayEventType::UnitDied, 1).empty());
    CHECK(state.result.ended);
    CHECK(state.result.winningTeam == 0);
    CHECK(std::any_of(
        result.gameplayEvents.begin(),
        result.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::UnitDied
                && event.sourceUnitId == 0
                && event.targetUnitId == 1;
        }));
    CHECK(std::any_of(
        result.gameplayEvents.begin(),
        result.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::BattleEnded
                && event.amount == 0;
        }));
}

TEST_CASE("BattleRuntimeSession_RunFrame_AppliesDeathComboConsequencesBeforeSceneConsumption", "[battle][runtime_session][death]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 1, { 100, 100, 0 }),
        unit(1, 0, { 120, 100, 0 }),
        unit(2, 0, { 140, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 1, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 0, 10, { 120, 100, 0 }),
        runtimeUnitSnapshot(2, 0, 100, { 140, 100, 0 }),
});
    state.units.requireCore(1).realRoleId = 10;
    state.units.requireCore(1).cost = 5;
    state.units.requireCore(2).realRoleId = 20;
    state.units.requireCore(2).cost = 4;

    KysChess::AppliedEffectInstance antiComboEffect;
    antiComboEffect.type = KysChess::EffectType::DodgeChance;
    antiComboEffect.value = 35;
    antiComboEffect.sourceComboId = 33;
    state.units.require(1).combo.appliedEffects.push_back(antiComboEffect);
    state.units.require(1).combo.onSkillTeamHealPending = true;
    state.units.require(0).deathEffects = { .id = 0 };
    state.units.require(1).deathEffects = {
        .id = 1,
        .comboIds = { 33 },
        .appliedEffects = { antiComboEffect },
    };
    state.units.require(2).deathEffects = {
        .id = 2,
        .comboIds = { 33 },
    };

    queuePendingDamage(state, lethalDamageInput(0, 1));
    BattleRuntimeSession session(std::move(state));

    const auto result = session.runFrame();

    const auto& runtime = session.runtime();
    CHECK_FALSE(runtime.units.require(1).combo.onSkillTeamHealPending);
    CHECK(KysChess::sumAlwaysEffectValue(runtime.units.require(2).combo, KysChess::EffectType::DodgeChance) == 35);
    CHECK(std::any_of(
        runtime.units.require(2).combo.appliedEffects.begin(),
        runtime.units.require(2).combo.appliedEffects.end(),
        [](const KysChess::AppliedEffectInstance& effect)
        {
            return effect.sourceComboId == 33
                && effect.type == KysChess::EffectType::DodgeChance
                && effect.value == 35;
        }));
    CHECK(std::any_of(
        result.logEvents.begin(),
        result.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Status
                && event.sourceUnitId == 1
                && event.targetUnitId == 2
                && BattleLogTest::textOf(event) == "獨行轉移";
        }));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_TransferredAntiComboDeathAoeUsesComboState", "[battle][core][ownership]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 120, 100, 0 }),
        unit(2, 1, { 140, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 10, { 120, 100, 0 }),
        runtimeUnitSnapshot(2, 1, 60, { 140, 100, 0 }),
});
    state.projectileFollowUps.projectileSpeed = SceneProjectileSpeed;
    state.projectileFollowUps.minimumProjectileFrames = 20;
    state.projectileFollowUps.areaProjectileFramePadding = 15;
    state.projectileFollowUps.areaSpawnDistance = SceneTileWidth;

    const AppliedEffectInstance antiComboDeathAoe{
        EffectType::DeathAOE,
        50,
        1,
        "",
        Trigger::Always,
        0,
        6,
        0,
        33,
    };
    state.units.require(0).deathEffects = { .id = 0 };
    state.units.require(1).deathEffects = {
        .id = 1,
        .comboIds = { 33 },
        .appliedEffects = { antiComboDeathAoe },
    };
    state.units.require(2).deathEffects = {
        .id = 2,
        .comboIds = { 33 },
    };
    KysChess::ChessBattleEffects::applyEffect(state.units.require(1).combo, antiComboDeathAoe, 33);
    queuePendingDamage(state, lethalDamageInput(0, 1));

    auto result = runBattleFrame(state);

    CHECK(std::any_of(
        state.units.require(2).combo.appliedEffects.begin(),
        state.units.require(2).combo.appliedEffects.end(),
        [](const AppliedEffectInstance& effect)
        {
            return effect.type == EffectType::DeathAOE && effect.sourceComboId == 33;
        }));
    REQUIRE(state.nextFrame.queuedAttacksForTest().size() == 1);
    CHECK(state.nextFrame.queuedAttacksForTest()[0].initial.attackerUnitId == 1);
    CHECK(state.nextFrame.queuedAttacksForTest()[0].initial.preferredTargetUnitId == 0);
    CHECK(state.nextFrame.queuedAttacksForTest()[0].initial.scriptedDamage == 50);
    CHECK(state.nextFrame.queuedAttacksForTest()[0].initial.scriptedStunFrames == 6);

    queuePendingDamage(state, lethalDamageInput(0, 2));
    result = runBattleFrame(state);

    REQUIRE(state.nextFrame.queuedAttacksForTest().size() == 1);
    CHECK(state.nextFrame.queuedAttacksForTest()[0].initial.attackerUnitId == 2);
    CHECK(state.nextFrame.queuedAttacksForTest()[0].initial.preferredTargetUnitId == 0);
    CHECK(state.nextFrame.queuedAttacksForTest()[0].initial.scriptedDamage == 50);
    CHECK(state.nextFrame.queuedAttacksForTest()[0].initial.scriptedStunFrames == 6);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DodgeConsumesHitBeforeDamage", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 105, 100, 0 }),
    }));
    state.attacks = attackWorld();
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
    KysChess::ChessBattleEffects::applyEffect(
        defenderCombo,
        { KysChess::EffectType::DodgeChance, 100 });
    state.units.require(1).combo = defenderCombo;

    auto result = runBattleFrame(state);

    CHECK(state.units.require(1).combo.dodgedLast);
    CHECK(state.nextFrame.queuedDamageForTest().empty());
    CHECK(damageLogAmountsFor(result).empty());
    CHECK(gameplayEventsFor(result, BattleGameplayEventType::DamageApplied).empty());

    const auto dodgeLog = std::find_if(
        result.logEvents.begin(),
        result.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Status
                && event.sourceUnitId == 1
                && event.targetUnitId == 0
                && BattleLogTest::textOf(event) == "閃避了來襲攻擊";
        });
    CHECK(dodgeLog != result.logEvents.end());

    const auto dodgeEffect = std::find_if(
        result.visualEvents.begin(),
        result.visualEvents.end(),
        [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::RoleEffect
                && event.targetUnitId == 1
                && event.effectId == KysChess::EFT_EVADE;
        });
    CHECK(dodgeEffect != result.visualEvents.end());
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ResolvesScriptedHitEvents", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 105, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);

    BattleAttackInstance projectile;
    projectile.id = 10;
    projectile.state.attackerUnitId = 0;
    projectile.state.scriptedDamage = 33;
    projectile.state.totalFrame = 30;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };
    state.attacks.attacks.push_back(projectile);

    KysChess::RoleComboState defenderCombo;
    KysChess::ChessBattleEffects::applyEffect(
        defenderCombo,
        { KysChess::EffectType::DodgeChance, 100 });
    state.units.require(1).combo = defenderCombo;

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1) == std::vector<int>{ 33 });
    CHECK_FALSE(state.units.require(1).combo.dodgedLast);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_RecordsTargetLostCancellationWithoutPairedAttack", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 700, 100, 0 }, CombatStyle::Ranged),
    }));
    state.attacks = attackWorld();

    BattleAttackInstance projectile;
    projectile.id = 10;
    projectile.state.attackerUnitId = 0;
    projectile.state.preferredTargetUnitId = 2;
    projectile.state.requirePreferredTarget = true;
    projectile.state.totalFrame = 30;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };

    seedRuntimeUnitsFromWorld(state);
    state.attacks.attacks.push_back(projectile);

    auto result = runBattleFrame(state);

    REQUIRE(result.gameplayEvents.size() >= 2);
    REQUIRE(result.logEvents.size() == 1);
    REQUIRE(result.visualEvents.size() == 2);

    CHECK(std::any_of(
        result.gameplayEvents.begin(),
        result.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::ProjectileCancelled
                && event.effectId == 10;
        }));
    CHECK(result.gameplayEvents[1].effectId == 10);
    CHECK(result.gameplayEvents[1].targetUnitId == -1);
    CHECK(result.gameplayEvents[1].otherAttackId == -1);
    CHECK(result.visualEvents[1].type == BattleVisualEventType::ProjectileTargetLost);
    CHECK(result.visualEvents[1].amount == -1);
    CHECK(result.logEvents[0].type == BattleLogEventType::Status);
    CHECK(result.logEvents[0].sourceUnitId == 0);
    CHECK(result.logEvents[0].targetUnitId == -1);
    CHECK(BattleLogTest::textOf(result.logEvents[0]) == "彈道停止：1枚目標遺失");
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_LabelsChainedProjectileTargetLost", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 700, 100, 0 }, CombatStyle::Ranged),
    }));
    state.attacks = attackWorld();

    BattleAttackInstance projectile;
    projectile.id = 10;
    projectile.spawnedFromAttackId = 9;
    projectile.state.attackerUnitId = 0;
    projectile.state.preferredTargetUnitId = 1;
    projectile.state.requirePreferredTarget = true;
    projectile.state.totalFrame = 30;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };

    seedRuntimeUnitsFromWorld(state);
    state.units.requireCore(1).alive = false;
    state.attacks.attacks.push_back(projectile);

    auto result = runBattleFrame(state);

    REQUIRE(result.gameplayEvents.size() >= 2);
    CHECK(std::any_of(
        result.gameplayEvents.begin(),
        result.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::ProjectileCancelled
                && event.effectId == 10;
        }));
    CHECK(std::any_of(
        result.logEvents.begin(),
        result.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return BattleLogTest::textOf(event) == "連鎖彈道停止：1枚原目標失效";
        }));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_CoalescesSameFrameChainedProjectileStopLogs", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 700, 100, 0 }, CombatStyle::Ranged),
    }));
    state.attacks = attackWorld();

    BattleAttackInstance first;
    first.id = 10;
    first.spawnedFromAttackId = 8;
    first.state.attackerUnitId = 0;
    first.state.preferredTargetUnitId = 1;
    first.state.requirePreferredTarget = true;
    first.state.totalFrame = 30;
    first.state.position = { 100, 100, 0 };
    first.state.velocity = { 5, 0, 0 };

    BattleAttackInstance second = first;
    second.id = 11;
    second.spawnedFromAttackId = 9;

    seedRuntimeUnitsFromWorld(state);
    state.units.requireCore(1).alive = false;
    state.attacks.attacks.push_back(first);
    state.attacks.attacks.push_back(second);

    auto result = runBattleFrame(state);

    const int targetLostCount = static_cast<int>(std::count_if(
        result.visualEvents.begin(),
        result.visualEvents.end(),
        [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::ProjectileTargetLost;
        }));
    CHECK(targetLostCount == 2);
    const int stopLogCount = static_cast<int>(std::count_if(
        result.logEvents.begin(),
        result.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return BattleLogTest::textOf(event) == "連鎖彈道停止：2枚原目標失效";
        }));
    CHECK(stopLogCount == 1);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AggregatesProjectileContactIgnoredByInvincible", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 105, 100, 0 }, CombatStyle::Ranged),
    }));
    state.attacks = attackWorld();

    BattleAttackInstance projectile;
    projectile.id = 10;
    projectile.state.attackerUnitId = 0;
    projectile.state.totalFrame = 30;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };
    projectile.state.operationType = BattleOperationType::RangedProjectile;

    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 100, { 105, 100, 0 }),
});
    state.units.requireCore(1).invincible = 3;
    state.attacks.attacks.push_back(projectile);
    projectile.id = 11;
    state.attacks.attacks.push_back(projectile);

    auto result = runBattleFrame(state);

    REQUIRE(result.gameplayEvents.size() == 4);
    CHECK(result.gameplayEvents[1].type == BattleGameplayEventType::StatusApplied);
    CHECK(result.gameplayEvents[1].sourceUnitId == 0);
    CHECK(result.gameplayEvents[1].targetUnitId == 1);
    CHECK(result.gameplayEvents[3].type == BattleGameplayEventType::StatusApplied);
    CHECK(result.gameplayEvents[3].sourceUnitId == 0);
    CHECK(result.gameplayEvents[3].targetUnitId == 1);
    CHECK(damageLogAmountsFor(result).empty());
    CHECK(gameplayEventsFor(result, BattleGameplayEventType::DamageApplied).empty());
    CHECK(state.units.requireCore(1).invincible > 0);
    REQUIRE(result.logEvents.size() == 1);
    CHECK(result.logEvents[0].type == BattleLogEventType::Status);
    CHECK(result.logEvents[0].sourceUnitId == 1);
    CHECK(result.logEvents[0].targetUnitId == -1);
    CHECK(BattleLogTest::textOf(result.logEvents[0]) == "彈道命中無敵：2枚傷害忽略");
    CHECK(result.gameplayEvents[1].type == BattleGameplayEventType::StatusApplied);
    CHECK(result.gameplayEvents[1].text == "彈道命中無敵：傷害忽略");
    CHECK(result.gameplayEvents[3].type == BattleGameplayEventType::StatusApplied);
    CHECK(result.gameplayEvents[3].text == "彈道命中無敵：傷害忽略");
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_RecordsProjectileCancelPairWithOtherAttackId", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 900, 900, 0 }, CombatStyle::Ranged),
    }));
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

    seedRuntimeUnitsFromWorld(state);
    state.attacks.attacks.push_back(first);
    state.attacks.attacks.push_back(second);

    auto result = runBattleFrame(state);

    REQUIRE(result.gameplayEvents.size() >= 3);
    CHECK(result.gameplayEvents[2].type == BattleGameplayEventType::ProjectileCancelled);
    CHECK(result.gameplayEvents[2].effectId == 10);
    CHECK(result.gameplayEvents[2].otherAttackId == 20);
    CHECK(result.gameplayEvents[2].amount == 0);
    CHECK(result.visualEvents[2].type == BattleVisualEventType::ProjectileCancelled);
    CHECK(result.visualEvents[2].amount == 20);
    REQUIRE(result.logEvents.size() == 1);
    CHECK(result.logEvents[0].type == BattleLogEventType::Status);
    CHECK(result.logEvents[0].sourceUnitId == 0);
    CHECK(result.logEvents[0].targetUnitId == 1);
    CHECK(result.logEvents[0].amount == 17);
    CHECK(BattleLogTest::textOf(result.logEvents[0]) == "抵消彈道 #10 vs #20（17 - 10 = 7）");
    CHECK(BattleLogTest::hasSegment(result.logEvents[0], "#10", BattleLogTextTone::ProjectileId));
    CHECK(BattleLogTest::hasSegment(result.logEvents[0], "#20", BattleLogTextTone::ProjectileId));
    CHECK(BattleLogTest::hasSegment(result.logEvents[0], " - ", BattleLogTextTone::FormulaValue));
    CHECK(BattleLogTest::hasSegment(result.logEvents[0], " = ", BattleLogTextTone::FormulaValue));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ProjectileCancelLogPutsWinnerOnLeft", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 900, 900, 0 }, CombatStyle::Ranged),
    }));
    state.attacks = attackWorld();

    BattleAttackInstance first;
    first.id = 10;
    first.state.attackerUnitId = 0;
    first.frame = 5;
    first.state.totalFrame = 30;
    first.state.position = { 500, 500, 0 };
    first.state.operationType = BattleOperationType::RangedProjectile;
    first.state.projectileCancelDamage = 10;

    BattleAttackInstance second;
    second.id = 20;
    second.state.attackerUnitId = 1;
    second.frame = 5;
    second.state.totalFrame = 30;
    second.state.position = { 500, 500, 0 };
    second.state.operationType = BattleOperationType::TrackingProjectile;
    second.state.projectileCancelDamage = 11;

    seedRuntimeUnitsFromWorld(state);
    state.attacks.attacks.push_back(first);
    state.attacks.attacks.push_back(second);

    auto result = runBattleFrame(state);

    REQUIRE(result.logEvents.size() == 1);
    CHECK(result.logEvents[0].sourceUnitId == 1);
    CHECK(result.logEvents[0].targetUnitId == 0);
    CHECK(result.logEvents[0].amount == 17);
    CHECK(BattleLogTest::textOf(result.logEvents[0]) == "抵消彈道 #20 vs #10（17 - 10 = 7）");
    CHECK(BattleLogTest::hasSegment(result.logEvents[0], "#20", BattleLogTextTone::ProjectileId));
    CHECK(BattleLogTest::hasSegment(result.logEvents[0], "#10", BattleLogTextTone::ProjectileId));
    CHECK(BattleLogTest::hasSegment(result.logEvents[0], " - ", BattleLogTextTone::FormulaValue));
    CHECK(BattleLogTest::hasSegment(result.logEvents[0], " = ", BattleLogTextTone::FormulaValue));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_RecordsBounceAsAttackSpawnedGameplay", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 105, 100, 0 }),
        unit(2, 1, { 180, 100, 0 }),
    }));
    state.attacks = attackWorld();

    BattleAttackInstance projectile;
    projectile.id = 10;
    projectile.state.attackerUnitId = 0;
    projectile.state.skillId = 101;
    projectile.state.skillMagicPower = 840;
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
    seedRuntimeUnitsFromWorld(state);
    state.attacks.attacks.push_back(projectile);

    auto result = runBattleFrame(state);

    REQUIRE(result.gameplayEvents.size() >= 3);
    const auto* bounce = findVisualEvent(result, BattleVisualEventType::ProjectileBounced, 10);
    REQUIRE(bounce);
    const auto gameplaySpawn = std::find_if(
        result.gameplayEvents.begin(),
        result.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::AttackSpawned
                && event.effectId == 30;
        });
    REQUIRE(gameplaySpawn != result.gameplayEvents.end());
    CHECK(gameplaySpawn->sourceUnitId == 0);
    CHECK(gameplaySpawn->targetUnitId == 2);
    CHECK(bounce->effectId == 10);
    CHECK(bounce->amount == 30);
    const auto visualSpawn = std::find_if(
        result.visualEvents.begin(),
        result.visualEvents.end(),
        [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::ProjectileSpawned
                && event.effectId == 30;
        });
    REQUIRE(visualSpawn != result.visualEvents.end());
    const auto& spawnEvent = *visualSpawn;
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

TEST_CASE("BattleFrameRunner_AdvanceFrame_LogsBounceChainTerminalReasons", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 105, 100, 0 }),
        unit(2, 1, { 400, 100, 0 }),
    }));
    state.attacks = attackWorld();

    BattleAttackInstance projectile;
    projectile.id = 10;
    projectile.state.attackerUnitId = 0;
    projectile.state.skillId = 101;
    projectile.state.skillMagicPower = 840;
    projectile.state.preferredTargetUnitId = 1;
    projectile.state.totalFrame = 30;
    projectile.state.operationType = BattleOperationType::RangedProjectile;
    projectile.state.bounceRemaining = 2;
    projectile.state.bounceRange = 90;
    projectile.state.bounceChancePct = 100;
    projectile.state.bounceRollPct = 0;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };

    seedRuntimeUnitsFromWorld(state);
    state.attacks.attacks.push_back(projectile);

    auto result = runBattleFrame(state);

    REQUIRE(result.gameplayEvents.size() >= 3);
    CHECK(std::any_of(
        result.gameplayEvents.begin(),
        result.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::ProjectileExpired
                && event.effectId == 10;
        }));
    const auto damageLog = std::find_if(
        result.logEvents.begin(),
        result.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Damage
                && event.sourceUnitId == 0
                && event.targetUnitId == 1;
        });
    const auto terminalLog = std::find_if(
        result.logEvents.begin(),
        result.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.sourceUnitId == 0
                && event.targetUnitId == -1
                && BattleLogTest::textOf(event) == "連鎖彈道停止：1枚搜尋範圍內無可連鎖目標";
        });
    REQUIRE(damageLog != result.logEvents.end());
    REQUIRE(terminalLog != result.logEvents.end());
    CHECK(damageLog < terminalLog);
}
