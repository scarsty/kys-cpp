#include "battle/BattleCastSystem.h"
#include "battle/BattleCore.h"
#include "battle/BattleRuntimeUnitSpawn.h"
#include "BattleLogTestHelpers.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <map>
#include <memory>

using namespace KysChess::Battle;

namespace
{

constexpr int SceneTileWidth = 36;
constexpr double TestMinimumFacingNorm = 0.0001;
constexpr int TestActionRecoveryFrames = 4;
constexpr int TestDashMomentumFrames = 5;
constexpr int TestNormalCastMpDelta = 5;
constexpr int TestCooldownAfterCastPadding = 2;
constexpr int TestCooldownMaxSpeed = 150;
constexpr double TestSpeedCooldownReductionRatio = 0.5;
constexpr int TestMeleeHitTotalFrame = 10;
constexpr int TestStrengthenedMeleeTotalFrame = 30;
constexpr double TestStrengthenedMeleeSelectDistanceDivisor = 2.0;
constexpr float TestStrengthenedMeleeMultiplier = 2.0f;
constexpr int TestStrengthenedMeleeOperationCountThreshold = 2;
constexpr int TestMeleeSplashTotalFrame = 60;
constexpr int TestMeleeSplashInitialFrame = 5;
constexpr float TestMeleeSplashStrengthMultiplier = 0.5f;
constexpr int TestTrackingProjectileTotalFrame = 120;
constexpr int TestDashHitTotalFrame = 30;
constexpr double TestMeleeSplashProjectileSpeed = 3.0;
constexpr double TestDashHitPositionSpacing = 2.0;
constexpr int TestDashHitFrameStep = 3;

BattleCastConfig testCastConfig()
{
    BattleCastConfig config;
    config.castFrames = { 25, 30, 20, 25 };
    config.baseCooldownFrames = { 105, 185, 115, 45 };
    config.minimumCooldownFrames = { 60, 70, 70, 45 };
    config.cooldownActPropertyDivisors = { 2, 1, 2, 0 };
    config.recoveryFrames = {
        TestActionRecoveryFrames,
        TestActionRecoveryFrames,
        TestActionRecoveryFrames,
        TestDashMomentumFrames,
    };
    config.maxCooldownSpeed = TestCooldownMaxSpeed;
    config.speedCooldownReductionRatio = TestSpeedCooldownReductionRatio;
    config.minimumCooldownAfterCastPadding = TestCooldownAfterCastPadding;
    config.normalCastMpDelta = TestNormalCastMpDelta;
    config.minimumFacingNorm = TestMinimumFacingNorm;
    config.meleeHitTotalFrame = TestMeleeHitTotalFrame;
    config.strengthenedMeleeTotalFrame = TestStrengthenedMeleeTotalFrame;
    config.strengthenedMeleeSelectDistanceDivisor = TestStrengthenedMeleeSelectDistanceDivisor;
    config.strengthenedMeleeMultiplier = TestStrengthenedMeleeMultiplier;
    config.meleeSplashTotalFrame = TestMeleeSplashTotalFrame;
    config.meleeSplashInitialFrame = TestMeleeSplashInitialFrame;
    config.meleeSplashStrengthMultiplier = TestMeleeSplashStrengthMultiplier;
    config.trackingProjectileTotalFrame = TestTrackingProjectileTotalFrame;
    config.dashHitTotalFrame = TestDashHitTotalFrame;
    config.strengthenedMeleeOperationCountThreshold = TestStrengthenedMeleeOperationCountThreshold;
    return config;
}

BattleCastSkillState skill(int id, int attackAreaType, double reach, bool forceRanged = false)
{
    BattleCastSkillState state;
    state.id = id;
    state.attackAreaType = attackAreaType;
    state.magicType = 1;
    state.reach = reach;
    state.selectDistance = 1;
    state.forceRanged = forceRanged;
    state.rangedStyle = forceRanged || attackAreaType == 1 || attackAreaType == 2 || attackAreaType == 3;
    return state;
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

BattleCastInput basicInput()
{
    BattleCastInput input;
    input.config = testCastConfig();
    input.geometry.meleeAttackEffectOffset = SceneTileWidth * 2.0;
    input.geometry.projectileSpeed = SceneTileWidth / 3.0;
    input.geometry.projectileSpawnOffset = SceneTileWidth * 2.0;
    input.geometry.projectileBaseTravel = SceneTileWidth * 5.0;
    input.geometry.projectileTravelPerSelectDistance = SceneTileWidth;
    input.geometry.meleeSplashProjectileSpeed = TestMeleeSplashProjectileSpeed;
    input.geometry.dashHitPositionSpacing = TestDashHitPositionSpacing;
    input.geometry.dashHitFrameStep = TestDashHitFrameStep;
    input.unit.id = 1;
    input.unit.position = { 10.0f, 20.0f, 0.0f };
    input.unit.facing = { 1.0f, 0.0f, 0.0f };
    input.unit.mp = 30;
    input.unit.maxMp = 100;
    input.unit.meleeAttackReach = 137.5;
    input.unit.speed = 0;
    input.targetUnitId = 2;
    input.targetPosition = { 82.0f, 20.0f, 0.0f };
    input.targetDistance = 100.0;
    input.normalSkill = skill(101, 0, 137.5);
    input.ultimateSkill = skill(201, 1, 400.0);
    input.normalSkill.name = "普通招式";
    input.normalSkill.visualEffectId = 31;
    input.ultimateSkill.name = "絕招";
    input.ultimateSkill.visualEffectId = 41;
    return input;
}

BattleRuntimeUnit runtimeUnit(int id, int team, const BattleCastUnitState& castUnit)
{
    BattleRuntimeUnit unit;
    unit.id = id;
    unit.team = team;
    unit.alive = true;
    unit.vitals = { 100, 100, castUnit.mp, castUnit.maxMp };
    unit.stats.speed = castUnit.speed;
    unit.motion.position = castUnit.position;
    unit.motion.facing = castUnit.facing;
    unit.reach = castUnit.meleeAttackReach;
    return unit;
}

void configureRuntimeActionPlan(BattleRuntimeState& state, const BattleCastInput& input)
{
    BattleMovementGeometry movementGeometry;
    movementGeometry.tileWidth = SceneTileWidth;
    movementGeometry.meleeAttackEffectOffset = SceneTileWidth * 2.0;
    movementGeometry.meleeAttackHitRadius = SceneTileWidth * 2.0;
    movementGeometry.dashFrames = TestDashMomentumFrames;
    movementGeometry.dashCooldownFrames = 18;
    movementGeometry.maxRangedReach = 400.0;
    state.movement.config = BattleGeometry(movementGeometry).movementConfig();

    state.action.castConfig = input.config;
    state.action.castGeometry = input.geometry;
    state.action.castFrames.assign(input.config.castFrames.begin(), input.config.castFrames.end());
    state.action.actionRules.tileWidth = SceneTileWidth;
    state.action.actionRules.maxEffectiveBattleReach = 400.0;
    state.action.actionRules.meleeAttackReach = input.unit.meleeAttackReach;
    state.action.actionRules.dashAttackMeleeReach = input.unit.dashAttackReach;
    state.action.actionRules.meleeAttackHitRadius = SceneTileWidth * 2.0;
    state.action.actionRules.dashMomentumFrames = TestDashMomentumFrames;
    state.action.actionRules.actionRecoveryFrames = TestActionRecoveryFrames;
    state.action.actionRules.dashRecoveryFrames = TestDashMomentumFrames;
    state.action.actionRecoveryFrames = TestActionRecoveryFrames;
    state.action.dashRecoveryFrames = TestDashMomentumFrames;
    state.attacks.hitRadius = SceneTileWidth * 2.0;
    state.attacks.minimumVectorNorm = TestMinimumFacingNorm;
    state.attacks.nextAttackId = 1;
    state.attacks.bounceSpawnDistance = SceneTileWidth;
    state.attacks.defaultProjectileSpeed = SceneTileWidth / 3.0;

    BattleActionPlanSeed seed;
    seed.unitId = input.unit.id;
    seed.hasEquippedSkill = input.unit.hasEquippedSkill;
    seed.normalSkill = actionSkillSeedFromCastSkill(input.normalSkill);
    seed.ultimateSkill = actionSkillSeedFromCastSkill(input.ultimateSkill);
    state.action.planSeeds()[input.unit.id] = std::move(seed);
}

void checkPoint(const Pointf& actual, const Pointf& expected)
{
    CHECK(actual.x == Catch::Approx(expected.x));
    CHECK(actual.y == Catch::Approx(expected.y));
    CHECK(actual.z == Catch::Approx(expected.z));
}

void checkDecisionEquals(const BattleCastDecision& lhs, const BattleCastDecision& rhs)
{
    CHECK(lhs.canCast == rhs.canCast);
    CHECK(lhs.ultimate == rhs.ultimate);
    CHECK(lhs.equipSkill == rhs.equipSkill);
    CHECK(lhs.announceUltimate == rhs.announceUltimate);
    CHECK(lhs.unitId == rhs.unitId);
    CHECK(lhs.targetUnitId == rhs.targetUnitId);
    CHECK(lhs.skillId == rhs.skillId);
    CHECK(lhs.operationType == rhs.operationType);
    CHECK(lhs.reason == rhs.reason);
}

void checkSpawnRequestEquals(const BattleAttackSpawnRequest& lhs, const BattleAttackSpawnRequest& rhs)
{
    CHECK(lhs.initial.attackerUnitId == rhs.initial.attackerUnitId);
    CHECK(lhs.initial.skillId == rhs.initial.skillId);
    CHECK(lhs.initial.operationType == rhs.initial.operationType);
    CHECK(lhs.initial.visualEffectId == rhs.initial.visualEffectId);
    CHECK(lhs.initial.preferredTargetUnitId == rhs.initial.preferredTargetUnitId);
    checkPoint(lhs.initial.position, rhs.initial.position);
    checkPoint(lhs.initial.velocity, rhs.initial.velocity);
    CHECK(lhs.initial.totalFrame == rhs.initial.totalFrame);
    CHECK(lhs.initial.through == rhs.initial.through);
    CHECK(lhs.initial.track == rhs.initial.track);
    CHECK(lhs.initial.requirePreferredTarget == rhs.initial.requirePreferredTarget);
    CHECK(lhs.initial.executeCanHitInvincible == rhs.initial.executeCanHitInvincible);
    CHECK(lhs.initial.ignoreProjectileCancel == rhs.initial.ignoreProjectileCancel);
    CHECK(lhs.initial.sharedHitGroupId == rhs.initial.sharedHitGroupId);
    CHECK(lhs.initial.bounceRemaining == rhs.initial.bounceRemaining);
    CHECK(lhs.initial.bounceRange == rhs.initial.bounceRange);
    CHECK(lhs.initial.bounceChancePct == rhs.initial.bounceChancePct);
    CHECK(lhs.initial.bounceRollPct == rhs.initial.bounceRollPct);
    CHECK(lhs.initial.ultimate == rhs.initial.ultimate);
    CHECK(lhs.initialFrame == rhs.initialFrame);
    CHECK(lhs.initial.castSubrequestKind == rhs.initial.castSubrequestKind);
    CHECK(lhs.initial.strengthMultiplier == Catch::Approx(rhs.initial.strengthMultiplier));
}

void checkGameplayEventEquals(const BattleGameplayEvent& lhs, const BattleGameplayEvent& rhs)
{
    CHECK(lhs.type == rhs.type);
    CHECK(lhs.frame == rhs.frame);
    CHECK(lhs.sourceUnitId == rhs.sourceUnitId);
    CHECK(lhs.targetUnitId == rhs.targetUnitId);
    CHECK(lhs.amount == rhs.amount);
    CHECK(lhs.effectId == rhs.effectId);
    checkPoint(lhs.position, rhs.position);
    CHECK(lhs.text == rhs.text);
    CHECK(lhs.otherAttackId == rhs.otherAttackId);
}

void checkVisualEventEquals(const BattleVisualEvent& lhs, const BattleVisualEvent& rhs)
{
    CHECK(lhs.type == rhs.type);
    CHECK(lhs.frame == rhs.frame);
    CHECK(lhs.sourceUnitId == rhs.sourceUnitId);
    CHECK(lhs.targetUnitId == rhs.targetUnitId);
    CHECK(lhs.amount == rhs.amount);
    CHECK(lhs.durationFrames == rhs.durationFrames);
    CHECK(lhs.effectId == rhs.effectId);
    CHECK(lhs.textSize == rhs.textSize);
    CHECK(lhs.textMotionType == rhs.textMotionType);
    CHECK(lhs.text == rhs.text);
    CHECK(lhs.skillName == rhs.skillName);
    CHECK(BattleLogTest::joinSegments(lhs.segments) == BattleLogTest::joinSegments(rhs.segments));
    CHECK(lhs.color.r == rhs.color.r);
    CHECK(lhs.color.g == rhs.color.g);
    CHECK(lhs.color.b == rhs.color.b);
    CHECK(lhs.color.a == rhs.color.a);
    checkPoint(lhs.position, rhs.position);
    CHECK(lhs.visualEffectId == rhs.visualEffectId);
    checkPoint(lhs.velocity, rhs.velocity);
    CHECK(lhs.operationKind == rhs.operationKind);
}

void checkLogEventEquals(const BattleLogEvent& lhs, const BattleLogEvent& rhs)
{
    CHECK(lhs.type == rhs.type);
    CHECK(lhs.frame == rhs.frame);
    CHECK(lhs.sourceUnitId == rhs.sourceUnitId);
    CHECK(lhs.targetUnitId == rhs.targetUnitId);
    CHECK(lhs.amount == rhs.amount);
    CHECK(lhs.category == rhs.category);
    CHECK(BattleLogTest::joinSegments(lhs.segments) == BattleLogTest::joinSegments(rhs.segments));
    CHECK(lhs.skillName == rhs.skillName);
}

void checkResultEquals(const BattleCastResult& lhs, const BattleCastResult& rhs)
{
    checkDecisionEquals(lhs.decision, rhs.decision);
    CHECK(lhs.cooldownDelta == rhs.cooldownDelta);
    CHECK(lhs.mpDelta == rhs.mpDelta);
    CHECK(lhs.animation.castFrame == rhs.animation.castFrame);
    CHECK(lhs.animation.cooldownFrames == rhs.animation.cooldownFrames);
    CHECK(lhs.animation.recoveryFrames == rhs.animation.recoveryFrames);

    REQUIRE(lhs.attackSpawnRequests.size() == rhs.attackSpawnRequests.size());
    for (size_t i = 0; i < lhs.attackSpawnRequests.size(); ++i)
    {
        checkSpawnRequestEquals(lhs.attackSpawnRequests[i], rhs.attackSpawnRequests[i]);
    }

    REQUIRE(lhs.gameplayEvents.size() == rhs.gameplayEvents.size());
    for (size_t i = 0; i < lhs.gameplayEvents.size(); ++i)
    {
        checkGameplayEventEquals(lhs.gameplayEvents[i], rhs.gameplayEvents[i]);
    }

    REQUIRE(lhs.logEvents.size() == rhs.logEvents.size());
    for (size_t i = 0; i < lhs.logEvents.size(); ++i)
    {
        checkLogEventEquals(lhs.logEvents[i], rhs.logEvents[i]);
    }

    checkPoint(lhs.postDashRetreatVelocity, rhs.postDashRetreatVelocity);
    CHECK(lhs.postDashRetreatFrames == rhs.postDashRetreatFrames);

    REQUIRE(lhs.visualEvents.size() == rhs.visualEvents.size());
    for (size_t i = 0; i < lhs.visualEvents.size(); ++i)
    {
        checkVisualEventEquals(lhs.visualEvents[i], rhs.visualEvents[i]);
    }
}

}  // namespace

TEST_CASE("BattleCastSystem_UltimateCastsOnlyAtMaxMp", "[battle][cast]")
{
    auto input = basicInput();
    input.unit.mp = input.unit.maxMp - 1;

    auto result = BattleCastPlanner().plan(input);
    CHECK(result.decision.canCast);
    CHECK_FALSE(result.decision.ultimate);
    CHECK(result.decision.skillId == 101);
    CHECK(result.decision.operationType == BattleOperationType::Melee);

    input.unit.mp = input.unit.maxMp;
    result = BattleCastPlanner().plan(input);
    CHECK(result.decision.canCast);
    CHECK(result.decision.ultimate);
    CHECK(result.decision.skillId == 201);
    CHECK(result.decision.operationType == BattleOperationType::RangedProjectile);
}

TEST_CASE("BattleCastSystem_CooldownUsesSelectedSkillActProperty", "[battle][cast]")
{
    auto input = basicInput();
    input.unit.mp = input.unit.maxMp;
    input.normalSkill = skill(118, 0, 137.5);
    input.normalSkill.actProperty = 0;
    input.ultimateSkill = skill(218, 1, 400.0);
    input.ultimateSkill.actProperty = 80;
    input.targetDistance = 300.0;

    auto result = BattleCastPlanner().plan(input);

    REQUIRE(result.decision.canCast);
    REQUIRE(result.decision.ultimate);
    CHECK(result.cooldownDelta == 75);
}

TEST_CASE("BattleCastSystem_RuntimeCastPlanningUsesConfiguredCdrEffect", "[battle][cast]")
{
    auto input = basicInput();
    input.unit.id = 0;
    input.unit.mp = 20;
    input.unit.maxMp = 100;
    input.unit.hasEquippedSkill = true;
    input.normalSkill = skill(120, 0, 137.5);
    input.ultimateSkill = skill(220, 1, 400.0);
    input.targetUnitId = 1;
    input.targetDistance = 100.0;

    BattleRuntimeState state;
    KysChess::RoleComboState combo;
    KysChess::ChessBattleEffects::applyEffect(combo, { KysChess::EffectType::CDR, 20 });
    appendRuntimeUnit(state, makeRuntimeUnitSpawn(runtimeUnit(0, 0, input.unit), combo));

    auto target = runtimeUnit(1, 1, input.unit);
    target.motion.position = input.targetPosition;
    appendRuntimeUnit(state, makeRuntimeUnitSpawn(target, KysChess::RoleComboState{}));
    configureRuntimeActionPlan(state, input);

    auto result = BattleFrameRunner().runFrame(state);

    REQUIRE(state.action.hasPendingCastForTest(0));
    CHECK(state.unitStore.requireUnit(0).animation.cooldown == 84);
    CHECK(result.gameplayEvents.front().type == BattleGameplayEventType::CastStarted);
}

TEST_CASE("BattleCastSystem_NormalSkillRemainsBehaviorCompatibleWhenUltimateUnavailable", "[battle][cast]")
{
    auto input = basicInput();
    input.unit.mp = 0;
    input.targetDistance = 300.0;
    input.normalSkill = skill(102, 1, 400.0);
    input.ultimateSkill = skill(202, 1, 400.0);

    auto result = BattleCastPlanner().plan(input);
    CHECK(result.decision.canCast);
    CHECK_FALSE(result.decision.ultimate);
    CHECK(result.decision.skillId == 102);
    CHECK(result.decision.operationType == BattleOperationType::RangedProjectile);
}

TEST_CASE("BattleCastSystem_MeleeNormalAttackMayPairWithRangedUltimate", "[battle][cast]")
{
    auto input = basicInput();
    input.unit.mp = input.unit.maxMp;
    input.targetDistance = 300.0;
    input.normalSkill = skill(103, 0, 137.5);
    input.ultimateSkill = skill(203, 1, 400.0);

    auto result = BattleCastPlanner().plan(input);
    CHECK(result.decision.canCast);
    CHECK(result.decision.ultimate);
    CHECK(result.decision.skillId == 203);
    CHECK(result.decision.operationType == BattleOperationType::RangedProjectile);
}

TEST_CASE("BattleCastSystem_DoesNotRewriteNormalAttackRangeFromUltimate", "[battle][cast]")
{
    auto input = basicInput();
    input.unit.mp = input.unit.maxMp - 1;
    input.targetDistance = 300.0;
    input.normalSkill = skill(104, 0, 137.5);
    input.ultimateSkill = skill(204, 1, 400.0);

    auto result = BattleCastPlanner().plan(input);
    CHECK_FALSE(result.decision.canCast);
    CHECK_FALSE(result.decision.ultimate);
    CHECK(result.decision.skillId == 104);
    CHECK(result.decision.operationType == BattleOperationType::None);
}

TEST_CASE("BattleCastSystem_BlocksCastWhenUnitIsNotAttackReady", "[battle][cast]")
{
    auto input = basicInput();
    input.unit.canStartAttack = false;
    input.targetDistance = 100.0;
    input.normalSkill = skill(105, 0, 137.5);

    auto result = BattleCastPlanner().plan(input);
    CHECK_FALSE(result.decision.canCast);
    CHECK(result.decision.reason == BattleCastBlockReason::AttackNotReady);
    CHECK(result.decision.skillId == 105);
    CHECK(result.decision.operationType == BattleOperationType::None);
}

TEST_CASE("BattleCastSystem_BlocksCastWhileDeadFrozenStunnedOrWithoutTarget", "[battle][cast]")
{
    auto assertNoCast = [](BattleCastInput input, BattleCastBlockReason reason) {
        auto result = BattleCastPlanner().plan(input);
        CHECK_FALSE(result.decision.canCast);
        CHECK(result.decision.reason == reason);
    };

    auto input = basicInput();
    input.unit.alive = false;
    assertNoCast(input, BattleCastBlockReason::Dead);

    input = basicInput();
    input.unit.frozen = true;
    assertNoCast(input, BattleCastBlockReason::Frozen);

    input = basicInput();
    input.unit.stunned = true;
    assertNoCast(input, BattleCastBlockReason::Stunned);

    input = basicInput();
    input.targetUnitId = -1;
    assertNoCast(input, BattleCastBlockReason::NoTarget);
}

TEST_CASE("BattleCastSystem_CommittedCastReturnsResourceDeltasTimingAndEvents", "[battle][cast]")
{
    auto input = basicInput();
    input.unit.mp = 20;
    input.unit.maxMp = 100;
    input.normalSkill = skill(106, 0, 137.5);
    input.normalSkill.name = "野球拳";
    input.normalSkill.visualEffectId = 77;

    auto result = BattleCastPlanner().plan(input);

    REQUIRE(result.decision.canCast);
    CHECK(result.cooldownDelta == 105);
    CHECK(result.mpDelta == 5);
    CHECK(result.animation.castFrame == 25);
    CHECK(result.animation.cooldownFrames == 105);
    REQUIRE(result.gameplayEvents.size() == 1);
    CHECK(result.gameplayEvents[0].type == BattleGameplayEventType::CastStarted);
    CHECK(result.gameplayEvents[0].sourceUnitId == 1);
    CHECK(result.gameplayEvents[0].targetUnitId == 2);
    REQUIRE(result.logEvents.size() == 1);
    CHECK(result.logEvents[0].type == BattleLogEventType::Status);
    CHECK(result.logEvents[0].sourceUnitId == 1);
    CHECK(result.logEvents[0].targetUnitId == 2);
    CHECK(BattleLogTest::textOf(result.logEvents[0]) == "施放野球拳");
    CHECK(result.logEvents[0].skillName == "野球拳");
    REQUIRE(result.visualEvents.size() == 1);
    CHECK(result.visualEvents[0].type == BattleVisualEventType::RoleEffect);
    CHECK(result.visualEvents[0].targetUnitId == 1);
    CHECK(result.visualEvents[0].effectId == 77);
}

TEST_CASE("BattleCastSystem_UltimateCastEmitsHighlightedFloatingText", "[battle][cast]")
{
    auto input = basicInput();
    input.unit.mp = input.unit.maxMp;
    input.targetDistance = 300.0;

    auto result = BattleCastPlanner().plan(input);

    REQUIRE(result.decision.canCast);
    REQUIRE(result.decision.ultimate);
    REQUIRE(result.decision.announceUltimate);
    REQUIRE(result.visualEvents.size() == 2);
    CHECK(result.visualEvents[0].type == BattleVisualEventType::FloatingText);
    CHECK(result.visualEvents[0].targetUnitId == 1);
    CHECK(result.visualEvents[0].text == "絕招");
    CHECK(result.visualEvents[0].textSize == 36);
    CHECK(result.visualEvents[0].color.r == 255);
    CHECK(result.visualEvents[0].color.g == 215);
    CHECK(result.visualEvents[0].color.b == 0);
    CHECK(result.visualEvents[0].color.a == 255);
    CHECK(result.visualEvents[1].type == BattleVisualEventType::RoleEffect);
    CHECK(result.visualEvents[1].targetUnitId == 1);
    CHECK(result.visualEvents[1].effectId == 41);
}

TEST_CASE("BattleCastSystem_CommitSelectedCastUsesExplicitOperationType", "[battle][cast]")
{
    auto input = basicInput();
    input.normalSkill = skill(118, 0, 400.0);
    input.normalSkill.visualEffectId = 33;
    input.targetDistance = 300.0;

    auto result = BattleCastPlanner().commitSelectedCast(
        input,
        false,
        BattleOperationType::RangedProjectile);

    REQUIRE(result.decision.canCast);
    CHECK(result.decision.skillId == 118);
    CHECK(result.decision.operationType == BattleOperationType::RangedProjectile);
    REQUIRE(result.attackSpawnRequests.size() == 1);
    CHECK(result.attackSpawnRequests[0].initial.operationType == BattleOperationType::RangedProjectile);
    CHECK(result.attackSpawnRequests[0].initial.visualEffectId == 33);
}

TEST_CASE("BattleCastSystem_CommittedCastOnlyRequiresSelectedOperationConfig", "[battle][cast]")
{
    auto input = basicInput();
    const auto fullConfig = input.config;
    input.config = {};
    input.config.castFrames[2] = fullConfig.castFrames[2];
    input.config.baseCooldownFrames[2] = fullConfig.baseCooldownFrames[2];
    input.config.minimumCooldownFrames[2] = fullConfig.minimumCooldownFrames[2];
    input.config.cooldownActPropertyDivisors[2] = fullConfig.cooldownActPropertyDivisors[2];
    input.config.recoveryFrames[2] = fullConfig.recoveryFrames[2];
    input.config.maxCooldownSpeed = fullConfig.maxCooldownSpeed;
    input.config.speedCooldownReductionRatio = fullConfig.speedCooldownReductionRatio;
    input.config.minimumCooldownAfterCastPadding = fullConfig.minimumCooldownAfterCastPadding;
    input.config.normalCastMpDelta = fullConfig.normalCastMpDelta;
    input.config.minimumFacingNorm = fullConfig.minimumFacingNorm;
    input.normalSkill = skill(119, 0, 400.0, true);
    input.normalSkill.visualEffectId = 33;
    input.normalSkill.selectDistance = 4;
    input.targetDistance = 300.0;

    auto result = BattleCastPlanner().commitSelectedCast(
        input,
        false,
        BattleOperationType::RangedProjectile);

    REQUIRE(result.decision.canCast);
    CHECK(result.decision.operationType == BattleOperationType::RangedProjectile);
    CHECK(result.animation.castFrame == fullConfig.castFrames[2]);
    CHECK(result.animation.cooldownFrames == fullConfig.baseCooldownFrames[2]);
    CHECK(result.animation.recoveryFrames == fullConfig.recoveryFrames[2]);
    REQUIRE(result.attackSpawnRequests.size() == 1);
    CHECK(result.attackSpawnRequests[0].initial.operationType == BattleOperationType::RangedProjectile);
}

TEST_CASE("BattleCastSystem_CommittedCastReturnsAttackSpawnRequest", "[battle][cast]")
{
    auto input = basicInput();
    input.normalSkill = skill(107, 0, 400.0, true);
    input.normalSkill.visualEffectId = 88;
    input.normalSkill.selectDistance = 4;
    input.targetDistance = 300.0;

    auto result = BattleCastPlanner().commitSelectedCast(
        input,
        false,
        BattleOperationType::RangedProjectile);

    REQUIRE(result.decision.canCast);
    REQUIRE(result.attackSpawnRequests.size() == 1);
    const auto& request = result.attackSpawnRequests[0];
    CHECK(request.initial.attackerUnitId == 1);
    CHECK(request.initial.skillId == 107);
    CHECK(request.initial.operationType == BattleOperationType::RangedProjectile);
    CHECK(request.initial.visualEffectId == 88);
    CHECK(request.initial.preferredTargetUnitId == -1);
    CHECK_FALSE(request.initial.requirePreferredTarget);
    CHECK(request.initial.position.x == Catch::Approx(82.0f));
    CHECK(request.initial.position.y == Catch::Approx(20.0f));
    CHECK(request.initial.velocity.x > 0.0f);
    CHECK(request.initial.velocity.y == Catch::Approx(0.0f));
    CHECK(request.initial.totalFrame == 24);
    CHECK_FALSE(request.initial.through);
    CHECK_FALSE(request.initial.ultimate);
}

TEST_CASE("BattleCastSystem_MeleeSpawnUsesConfiguredOriginAndFrameCount", "[battle][cast]")
{
    auto input = basicInput();
    input.normalSkill = skill(109, 0, 137.5);

    auto result = BattleCastPlanner().commitSelectedCast(
        input,
        false,
        BattleOperationType::Melee);

    REQUIRE(result.decision.operationType == BattleOperationType::Melee);
    REQUIRE(result.attackSpawnRequests.size() == 1);
    const auto& request = result.attackSpawnRequests[0];
    CHECK(request.initial.position.x == Catch::Approx(82.0f));
    CHECK(request.initial.position.y == Catch::Approx(20.0f));
    CHECK(request.initial.velocity.x == Catch::Approx(0.0f));
    CHECK(request.initial.velocity.y == Catch::Approx(0.0f));
    CHECK(request.initial.totalFrame == 10);
    CHECK_FALSE(request.initial.track);
    CHECK_FALSE(request.initial.through);
    CHECK(request.initial.castSubrequestKind == BattleAttackCastSubrequestKind::SkillHit);
    CHECK(request.initialFrame == 0);
    CHECK(request.initial.strengthMultiplier == Catch::Approx(1.0f));
}

TEST_CASE("BattleCastSystem_StrengthenedMeleeSpawnUsesTrackingProjectileShape", "[battle][cast]")
{
    auto input = basicInput();
    input.unit.operationCount = 2;
    input.normalSkill = skill(112, 0, 137.5);
    input.normalSkill.selectDistance = 6;

    auto result = BattleCastPlanner().commitSelectedCast(
        input,
        false,
        BattleOperationType::Melee);

    REQUIRE(result.decision.operationType == BattleOperationType::Melee);
    REQUIRE(result.attackSpawnRequests.size() == 1);
    const auto& request = result.attackSpawnRequests[0];
    CHECK(request.initial.totalFrame == 30);
    CHECK(request.initial.track);
    CHECK(request.initial.velocity.x == Catch::Approx(3.0f));
    CHECK(request.initial.velocity.y == Catch::Approx(0.0f));
    CHECK(request.initial.strengthMultiplier == Catch::Approx(2.0f));
    CHECK(request.initial.castSubrequestKind == BattleAttackCastSubrequestKind::SkillHit);
}

TEST_CASE("BattleCastSystem_UltimateMeleeCanEmitExplicitSplashAndExtraProjectiles", "[battle][cast]")
{
    auto input = basicInput();
    input.unit.mp = input.unit.maxMp;
    input.ultimateSkill = skill(113, 0, 137.5);
    input.ultimateSkill.selectDistance = 6;
    input.ultimateSkill.meleeSplashCount = 1;
    input.ultimateSkill.extraProjectileCount = 2;

    auto result = BattleCastPlanner().commitSelectedCast(
        input,
        true,
        BattleOperationType::Melee);

    REQUIRE(result.decision.operationType == BattleOperationType::Melee);
    REQUIRE(result.attackSpawnRequests.size() == 4);
    CHECK(result.attackSpawnRequests[0].initial.strengthMultiplier == Catch::Approx(2.0f));
    CHECK(result.attackSpawnRequests[0].initial.track);
    CHECK(result.attackSpawnRequests[0].initial.totalFrame == 30);

    CHECK(result.attackSpawnRequests[1].initial.castSubrequestKind == BattleAttackCastSubrequestKind::MeleeSplash);
    CHECK(result.attackSpawnRequests[1].initial.strengthMultiplier == Catch::Approx(0.5f));
    CHECK(result.attackSpawnRequests[1].initial.track);
    CHECK(result.attackSpawnRequests[1].initial.totalFrame == 60);
    CHECK(result.attackSpawnRequests[1].initialFrame == 5);
    CHECK(result.attackSpawnRequests[1].initial.velocity.x == Catch::Approx(TestMeleeSplashProjectileSpeed));

    CHECK(result.attackSpawnRequests[2].initial.castSubrequestKind == BattleAttackCastSubrequestKind::ExtraProjectile);
    CHECK(result.attackSpawnRequests[3].initial.castSubrequestKind == BattleAttackCastSubrequestKind::ExtraProjectile);
}

TEST_CASE("BattleCastSystem_TrackingProjectileSpawnUsesConfiguredFrameCount", "[battle][cast]")
{
    auto input = basicInput();
    input.normalSkill = skill(110, 3, 180.0);
    input.targetDistance = 150.0;

    auto result = BattleCastPlanner().commitSelectedCast(
        input,
        false,
        BattleOperationType::TrackingProjectile);

    REQUIRE(result.decision.operationType == BattleOperationType::TrackingProjectile);
    REQUIRE(result.attackSpawnRequests.size() == 1);
    const auto& request = result.attackSpawnRequests[0];
    CHECK(request.initial.position.x == Catch::Approx(82.0f));
    CHECK(request.initial.velocity.x == Catch::Approx(12.0f));
    CHECK(request.initial.velocity.y == Catch::Approx(0.0f));
    CHECK(request.initial.totalFrame == 120);
    CHECK(request.initial.track);
    CHECK_FALSE(request.initial.through);
    CHECK(request.initial.castSubrequestKind == BattleAttackCastSubrequestKind::SkillHit);
}

TEST_CASE("BattleCastSystem_RangedCastExpandsExplicitExtraProjectiles", "[battle][cast]")
{
    auto input = basicInput();
    input.normalSkill = skill(115, 0, 400.0, true);
    input.normalSkill.selectDistance = 4;
    input.normalSkill.extraProjectileCount = 2;
    input.targetDistance = 300.0;

    auto result = BattleCastPlanner().commitSelectedCast(
        input,
        false,
        BattleOperationType::RangedProjectile);

    REQUIRE(result.decision.operationType == BattleOperationType::RangedProjectile);
    REQUIRE(result.attackSpawnRequests.size() == 3);
    CHECK(result.attackSpawnRequests[0].initial.castSubrequestKind == BattleAttackCastSubrequestKind::SkillHit);
    CHECK(result.attackSpawnRequests[0].initial.operationType == BattleOperationType::RangedProjectile);
    CHECK(result.attackSpawnRequests[0].initial.totalFrame == 24);
    CHECK_FALSE(result.attackSpawnRequests[0].initial.through);

    CHECK(result.attackSpawnRequests[1].initial.castSubrequestKind == BattleAttackCastSubrequestKind::ExtraProjectile);
    CHECK(result.attackSpawnRequests[1].initial.operationType == BattleOperationType::RangedProjectile);
    CHECK(result.attackSpawnRequests[1].initial.totalFrame == 24);
    CHECK_FALSE(result.attackSpawnRequests[1].initial.through);
    CHECK_FALSE(result.attackSpawnRequests[1].initial.mainProjectile);
    CHECK(result.attackSpawnRequests[1].initial.velocity.x == Catch::Approx(12.0f));

    CHECK(result.attackSpawnRequests[2].initial.castSubrequestKind == BattleAttackCastSubrequestKind::ExtraProjectile);
    CHECK(result.attackSpawnRequests[2].initial.operationType == BattleOperationType::RangedProjectile);
    CHECK_FALSE(result.attackSpawnRequests[2].initial.mainProjectile);
}

TEST_CASE("BattleCastSystem_ExtraProjectilesPreferAlternateSpreadTargets", "[battle][cast]")
{
    auto input = basicInput();
    input.unit.mp = input.unit.maxMp;
    input.ultimateSkill = skill(230, 1, 400.0);
    input.ultimateSkill.selectDistance = 4;
    input.ultimateSkill.extraProjectileCount = 2;
    input.targetUnitId = 2;
    input.targetPosition = { 82.0f, 20.0f, 0.0f };
    input.targetDistance = 72.0;
    input.projectileSpreadTargets = {
        { 2, { 82.0f, 20.0f, 0.0f } },
        { 3, { 82.0f, 56.0f, 0.0f } },
        { 4, { 82.0f, -16.0f, 0.0f } },
    };

    auto result = BattleCastPlanner().commitSelectedCast(
        input,
        true,
        BattleOperationType::RangedProjectile);

    REQUIRE(result.decision.ultimate);
    REQUIRE(result.attackSpawnRequests.size() == 6);
    CHECK(result.attackSpawnRequests[1].initial.castSubrequestKind == BattleAttackCastSubrequestKind::ExtraProjectile);
    CHECK(result.attackSpawnRequests[1].initial.preferredTargetUnitId == 3);
    CHECK(result.attackSpawnRequests[1].initial.requirePreferredTarget);
    CHECK(result.attackSpawnRequests[2].initial.castSubrequestKind == BattleAttackCastSubrequestKind::ExtraProjectile);
    CHECK(result.attackSpawnRequests[2].initial.preferredTargetUnitId == 4);
    CHECK(result.attackSpawnRequests[2].initial.requirePreferredTarget);
}

TEST_CASE("BattleCastSystem_RangedAreaCastEmitsSideProjectiles", "[battle][cast]")
{
    auto input = basicInput();
    input.normalSkill = skill(120, 1, 400.0);
    input.normalSkill.selectDistance = 4;
    input.targetDistance = 300.0;

    auto result = BattleCastPlanner().commitSelectedCast(
        input,
        false,
        BattleOperationType::RangedProjectile);

    REQUIRE(result.decision.operationType == BattleOperationType::RangedProjectile);
    REQUIRE(result.attackSpawnRequests.size() == 3);

    const auto& main = result.attackSpawnRequests[0];
    CHECK(main.initial.castSubrequestKind == BattleAttackCastSubrequestKind::SkillHit);
    CHECK(main.initial.mainProjectile);
    CHECK(main.initial.through);
    CHECK_FALSE(main.initial.track);
    CHECK(main.initial.preferredTargetUnitId == -1);
    CHECK_FALSE(main.initial.requirePreferredTarget);
    CHECK(main.initial.strengthMultiplier == Catch::Approx(1.0f));

    for (std::size_t i = 1; i < result.attackSpawnRequests.size(); ++i)
    {
        const auto& side = result.attackSpawnRequests[i];
        CHECK(side.initial.castSubrequestKind == BattleAttackCastSubrequestKind::ExtraProjectile);
        CHECK(side.initial.operationType == BattleOperationType::RangedProjectile);
        CHECK(side.initial.through);
        CHECK_FALSE(side.initial.mainProjectile);
        CHECK_FALSE(side.initial.track);
        CHECK(side.initial.preferredTargetUnitId == -1);
        CHECK_FALSE(side.initial.requirePreferredTarget);
        CHECK(side.initial.strengthMultiplier == Catch::Approx(0.2f));
        CHECK(side.initial.velocity.x > 0.0f);
        CHECK(side.initial.velocity.y != Catch::Approx(0.0f));
    }
}

TEST_CASE("BattleCastSystem_RangedSideProjectilesUseProjectileSpeedMultiplier", "[battle][cast]")
{
    auto input = basicInput();
    input.normalSkill = skill(121, 1, 400.0);
    input.normalSkill.selectDistance = 4;
    input.normalSkill.projectileSpeedMultiplierPct = 200;
    input.targetDistance = 300.0;

    auto result = BattleCastPlanner().commitSelectedCast(
        input,
        false,
        BattleOperationType::RangedProjectile);

    REQUIRE(result.decision.operationType == BattleOperationType::RangedProjectile);
    REQUIRE(result.attackSpawnRequests.size() == 3);
    CHECK(result.attackSpawnRequests[1].initial.velocity.norm() == Catch::Approx(10.0f));
    CHECK(result.attackSpawnRequests[2].initial.velocity.norm() == Catch::Approx(9.0f));
}

TEST_CASE("BattleCastSystem_TrackingUltimateEmitsTwoProjectileSpread", "[battle][cast]")
{
    auto input = basicInput();
    input.unit.mp = input.unit.maxMp;
    input.ultimateSkill = skill(220, 3, 400.0);
    input.ultimateSkill.selectDistance = 4;
    input.targetDistance = 180.0;

    auto result = BattleCastPlanner().commitSelectedCast(
        input,
        true,
        BattleOperationType::TrackingProjectile);

    REQUIRE(result.decision.ultimate);
    REQUIRE(result.decision.operationType == BattleOperationType::TrackingProjectile);
    REQUIRE(result.attackSpawnRequests.size() == 2);
    CHECK(result.attackSpawnRequests[0].initial.mainProjectile);
    CHECK_FALSE(result.attackSpawnRequests[1].initial.mainProjectile);
    CHECK(result.attackSpawnRequests[0].initialFrame == 0);
    CHECK(result.attackSpawnRequests[1].initialFrame == 5);
    CHECK(result.attackSpawnRequests[0].initial.track);
    CHECK(result.attackSpawnRequests[1].initial.track);
    CHECK(result.attackSpawnRequests[0].initial.totalFrame == TestTrackingProjectileTotalFrame);
    CHECK(result.attackSpawnRequests[1].initial.totalFrame == TestTrackingProjectileTotalFrame);
}

TEST_CASE("BattleCastSystem_TrackingUltimateSpreadAssignsAlternateTarget", "[battle][cast]")
{
    auto input = basicInput();
    input.unit.mp = input.unit.maxMp;
    input.ultimateSkill = skill(231, 3, 400.0);
    input.ultimateSkill.selectDistance = 4;
    input.targetUnitId = 2;
    input.targetPosition = { 82.0f, 20.0f, 0.0f };
    input.targetDistance = 72.0;
    input.projectileSpreadTargets = {
        { 2, { 82.0f, 20.0f, 0.0f } },
        { 3, { 86.0f, 54.0f, 0.0f } },
    };

    auto result = BattleCastPlanner().commitSelectedCast(
        input,
        true,
        BattleOperationType::TrackingProjectile);

    REQUIRE(result.decision.ultimate);
    REQUIRE(result.decision.operationType == BattleOperationType::TrackingProjectile);
    REQUIRE(result.attackSpawnRequests.size() == 2);
    CHECK(result.attackSpawnRequests[0].initial.preferredTargetUnitId == 2);
    CHECK(result.attackSpawnRequests[1].initial.preferredTargetUnitId == 3);
    CHECK(result.attackSpawnRequests[1].initial.requirePreferredTarget);
}

TEST_CASE("BattleCastSystem_ProjectileCastsUseExplicitProjectileSpawnOffset", "[battle][cast]")
{
    auto input = basicInput();
    input.geometry.meleeAttackEffectOffset = 72.0;
    input.geometry.projectileSpawnOffset = 108.0;

    input.normalSkill = skill(116, 3, 180.0);
    input.targetDistance = 150.0;

    auto trackingResult = BattleCastPlanner().commitSelectedCast(
        input,
        false,
        BattleOperationType::TrackingProjectile);

    REQUIRE(trackingResult.decision.operationType == BattleOperationType::TrackingProjectile);
    REQUIRE(trackingResult.attackSpawnRequests.size() == 1);
    CHECK(trackingResult.attackSpawnRequests[0].initial.position.x == Catch::Approx(118.0f));
    CHECK(trackingResult.attackSpawnRequests[0].initial.position.y == Catch::Approx(20.0f));

    input = basicInput();
    input.geometry.meleeAttackEffectOffset = 72.0;
    input.geometry.projectileSpawnOffset = 108.0;
    input.normalSkill = skill(116, 0, 400.0, true);
    input.normalSkill.selectDistance = 4;
    input.normalSkill.extraProjectileCount = 1;
    input.targetDistance = 300.0;

    auto rangedResult = BattleCastPlanner().commitSelectedCast(
        input,
        false,
        BattleOperationType::RangedProjectile);

    REQUIRE(rangedResult.decision.operationType == BattleOperationType::RangedProjectile);
    REQUIRE(rangedResult.attackSpawnRequests.size() == 2);
    CHECK(rangedResult.attackSpawnRequests[0].initial.position.x == Catch::Approx(118.0f));
    CHECK(rangedResult.attackSpawnRequests[0].initial.position.y == Catch::Approx(20.0f));
    CHECK(rangedResult.attackSpawnRequests[1].initial.castSubrequestKind == BattleAttackCastSubrequestKind::ExtraProjectile);
    CHECK(rangedResult.attackSpawnRequests[1].initial.position.x == Catch::Approx(118.0f));
    CHECK(rangedResult.attackSpawnRequests[1].initial.position.y == Catch::Approx(20.0f));
}

TEST_CASE("BattleCastSystem_DashCastUsesDashRecoveryAndHitEffectRequest", "[battle][cast]")
{
    auto input = basicInput();
    input.unit.dashAttackEnabled = true;
    input.unit.dashAttackReach = 350.0;
    input.unit.dashVelocity = { 4.0f, 0.0f, 0.0f };
    input.unit.dashHitCount = 3;
    input.geometry.dashHitPositionSpacing = 1.5;
    input.geometry.dashHitFrameStep = 4;
    input.normalSkill = skill(111, 0, 137.5);
    input.targetDistance = 220.0;

    auto result = BattleCastPlanner().commitSelectedCast(
        input,
        false,
        BattleOperationType::Dash);

    REQUIRE(result.decision.operationType == BattleOperationType::Dash);
    CHECK(result.animation.recoveryFrames == 5);
    REQUIRE(result.attackSpawnRequests.size() == 3);

    CHECK(result.attackSpawnRequests[0].initial.position.x == Catch::Approx(80.5f));
    CHECK(result.attackSpawnRequests[0].initialFrame == 4);
    CHECK(result.attackSpawnRequests[0].initial.velocity.x == Catch::Approx(4.0f));
    CHECK(result.attackSpawnRequests[0].initial.castSubrequestKind == BattleAttackCastSubrequestKind::DashHit);

    CHECK(result.attackSpawnRequests[1].initial.position.x == Catch::Approx(82.0f));
    CHECK(result.attackSpawnRequests[1].initialFrame == 8);

    CHECK(result.attackSpawnRequests[2].initial.position.x == Catch::Approx(83.5f));
    CHECK(result.attackSpawnRequests[2].initialFrame == 12);
    CHECK(result.attackSpawnRequests[2].initial.totalFrame == 30);
    CHECK_FALSE(result.attackSpawnRequests[2].initial.track);
    CHECK_FALSE(result.attackSpawnRequests[2].initial.through);
    CHECK(result.postDashRetreatVelocity.x == Catch::Approx(-4.0f));
    CHECK(result.postDashRetreatVelocity.y == Catch::Approx(0.0f));
    CHECK(result.postDashRetreatFrames == 5);

    input.unit.dashVelocity = { 8.0f, 0.0f, 0.0f };
    auto fasterResult = BattleCastPlanner().commitSelectedCast(
        input,
        false,
        BattleOperationType::Dash);

    REQUIRE(fasterResult.attackSpawnRequests.size() == 3);
    CHECK(fasterResult.attackSpawnRequests[0].initial.position.x == Catch::Approx(result.attackSpawnRequests[0].initial.position.x));
    CHECK(fasterResult.attackSpawnRequests[1].initial.position.x == Catch::Approx(result.attackSpawnRequests[1].initial.position.x));
    CHECK(fasterResult.attackSpawnRequests[2].initial.position.x == Catch::Approx(result.attackSpawnRequests[2].initial.position.x));
    CHECK(fasterResult.attackSpawnRequests[0].initial.velocity.x == Catch::Approx(8.0f));
    CHECK(fasterResult.postDashRetreatVelocity.x == Catch::Approx(-8.0f));
}

TEST_CASE("BattleCastSystem_DashCastCanEmitExplicitFollowUpSkillRequest", "[battle][cast]")
{
    auto input = basicInput();
    input.unit.dashAttackEnabled = true;
    input.unit.dashAttackReach = 350.0;
    input.unit.dashVelocity = { 4.0f, 0.0f, 0.0f };
    input.unit.dashHitCount = 1;
    input.unit.emitDashFollowUpSkillAttack = true;
    input.unit.dashFollowUpOperationType = BattleOperationType::RangedProjectile;
    input.normalSkill = skill(114, 0, 137.5);
    input.normalSkill.selectDistance = 4;
    input.targetDistance = 220.0;

    auto result = BattleCastPlanner().commitSelectedCast(
        input,
        false,
        BattleOperationType::Dash);

    REQUIRE(result.decision.operationType == BattleOperationType::Dash);
    REQUIRE(result.attackSpawnRequests.size() == 2);
    CHECK(result.attackSpawnRequests[0].initial.castSubrequestKind == BattleAttackCastSubrequestKind::DashHit);
    CHECK(result.attackSpawnRequests[1].initial.castSubrequestKind == BattleAttackCastSubrequestKind::DashFollowUpSkill);
    CHECK(result.attackSpawnRequests[1].initial.operationType == BattleOperationType::RangedProjectile);
    CHECK(result.attackSpawnRequests[1].initial.totalFrame == 24);
    CHECK(result.attackSpawnRequests[1].initial.velocity.x == Catch::Approx(12.0f));
}

TEST_CASE("BattleCastSystem_OutputIsDeterministicForSameSeedAndInput", "[battle][cast]")
{
    auto input = basicInput();
    input.normalSkill = skill(108, 1, 400.0);
    input.normalSkill.name = "定序招式";
    input.normalSkill.visualEffectId = 99;
    input.normalSkill.selectDistance = 4;
    input.targetDistance = 250.0;

    auto first = BattleCastPlanner().plan(input);
    auto second = BattleCastPlanner().plan(input);

    REQUIRE(first.decision.canCast);
    REQUIRE(second.decision.canCast);
    checkResultEquals(first, second);
}

TEST_CASE("BattleCastSystem_AdvanceOperationCountAfterCommittedMeleeCast", "[battle][cast][unit]")
{
    CHECK(advanceOperationCountAfterCommittedCast(
              0,
              false,
              BattleOperationType::Melee,
              TestStrengthenedMeleeOperationCountThreshold) == 1);
    CHECK(advanceOperationCountAfterCommittedCast(
              2,
              false,
              BattleOperationType::Melee,
              TestStrengthenedMeleeOperationCountThreshold) == 0);
    CHECK(advanceOperationCountAfterCommittedCast(
              1,
              true,
              BattleOperationType::Melee,
              TestStrengthenedMeleeOperationCountThreshold) == 0);
    CHECK(advanceOperationCountAfterCommittedCast(
              2,
              false,
              BattleOperationType::TrackingProjectile,
              TestStrengthenedMeleeOperationCountThreshold) == 2);
}
