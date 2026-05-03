#include "battle/BattleCastSystem.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>

using namespace KysChess::Battle;

namespace
{

constexpr int SceneTileWidth = 36;
constexpr double LegacyMinimumFacingNorm = 0.0001;
constexpr int LegacyActionRecoveryFrames = 4;
constexpr int LegacyDashMomentumFrames = 5;
constexpr int LegacyNormalCastMpDelta = 5;
constexpr int LegacyCooldownAfterCastPadding = 2;
constexpr int LegacyCooldownMaxSpeed = 150;
constexpr double LegacySpeedCooldownReductionRatio = 0.5;
constexpr int LegacyMeleeHitTotalFrame = 10;
constexpr int LegacyStrengthenedMeleeTotalFrame = 30;
constexpr double LegacyStrengthenedMeleeSelectDistanceDivisor = 2.0;
constexpr float LegacyStrengthenedMeleeMultiplier = 2.0f;
constexpr int LegacyStrengthenedMeleeOperationCountThreshold = 2;
constexpr int LegacyMeleeSplashTotalFrame = 60;
constexpr int LegacyMeleeSplashInitialFrame = 5;
constexpr float LegacyMeleeSplashStrengthMultiplier = 0.5f;
constexpr int LegacyTrackingProjectileTotalFrame = 120;
constexpr int LegacyDashHitTotalFrame = 30;
constexpr double LegacyMeleeSplashProjectileSpeed = 3.0;
constexpr double LegacyDashHitPositionSpacing = 2.0;
constexpr int LegacyDashHitFrameStep = 3;

BattleCastConfig legacyCastConfig()
{
    BattleCastConfig config;
    config.castFrames = { 25, 30, 20, 25 };
    config.baseCooldownFrames = { 105, 185, 115, 45 };
    config.minimumCooldownFrames = { 60, 70, 70, 45 };
    config.cooldownActPropertyDivisors = { 2, 1, 2, 0 };
    config.recoveryFrames = {
        LegacyActionRecoveryFrames,
        LegacyActionRecoveryFrames,
        LegacyActionRecoveryFrames,
        LegacyDashMomentumFrames,
    };
    config.maxCooldownSpeed = LegacyCooldownMaxSpeed;
    config.speedCooldownReductionRatio = LegacySpeedCooldownReductionRatio;
    config.minimumCooldownAfterCastPadding = LegacyCooldownAfterCastPadding;
    config.normalCastMpDelta = LegacyNormalCastMpDelta;
    config.minimumFacingNorm = LegacyMinimumFacingNorm;
    config.meleeHitTotalFrame = LegacyMeleeHitTotalFrame;
    config.strengthenedMeleeTotalFrame = LegacyStrengthenedMeleeTotalFrame;
    config.strengthenedMeleeSelectDistanceDivisor = LegacyStrengthenedMeleeSelectDistanceDivisor;
    config.strengthenedMeleeMultiplier = LegacyStrengthenedMeleeMultiplier;
    config.meleeSplashTotalFrame = LegacyMeleeSplashTotalFrame;
    config.meleeSplashInitialFrame = LegacyMeleeSplashInitialFrame;
    config.meleeSplashStrengthMultiplier = LegacyMeleeSplashStrengthMultiplier;
    config.trackingProjectileTotalFrame = LegacyTrackingProjectileTotalFrame;
    config.dashHitTotalFrame = LegacyDashHitTotalFrame;
    config.strengthenedMeleeOperationCountThreshold = LegacyStrengthenedMeleeOperationCountThreshold;
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

BattleCastInput basicInput()
{
    BattleCastInput input;
    input.config = legacyCastConfig();
    input.geometry.meleeAttackEffectOffset = SceneTileWidth * 2.0;
    input.geometry.projectileSpeed = SceneTileWidth / 3.0;
    input.geometry.projectileSpawnOffset = SceneTileWidth * 2.0;
    input.geometry.projectileBaseTravel = SceneTileWidth * 5.0;
    input.geometry.projectileTravelPerSelectDistance = SceneTileWidth;
    input.geometry.meleeSplashProjectileSpeed = LegacyMeleeSplashProjectileSpeed;
    input.geometry.dashHitPositionSpacing = LegacyDashHitPositionSpacing;
    input.geometry.dashHitFrameStep = LegacyDashHitFrameStep;
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

BattleEffectDefinition invincibilityEffect()
{
    BattleEffectDefinition effect;
    effect.id = 11;
    effect.type = "技能後無敵";
    effect.hook = BattleHook::SkillFinished;
    effect.target = BattleEffectTarget::Source;
    effect.executorKey = "添加無敵幀";
    effect.value = 30;
    return effect;
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

void checkPresentationEventEquals(const BattlePresentationEvent& lhs, const BattlePresentationEvent& rhs)
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
    CHECK(lhs.detailText == rhs.detailText);
    CHECK(lhs.color.r == rhs.color.r);
    CHECK(lhs.color.g == rhs.color.g);
    CHECK(lhs.color.b == rhs.color.b);
    CHECK(lhs.color.a == rhs.color.a);
    checkPoint(lhs.position, rhs.position);
    CHECK(lhs.visualEffectId == rhs.visualEffectId);
    checkPoint(lhs.velocity, rhs.velocity);
    CHECK(lhs.operationKind == rhs.operationKind);
}

void checkEffectEventEquals(const BattleEffectEvent& lhs, const BattleEffectEvent& rhs)
{
    CHECK(lhs.hook == rhs.hook);
    CHECK(lhs.sourceUnitId == rhs.sourceUnitId);
    CHECK(lhs.targetUnitId == rhs.targetUnitId);
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

    REQUIRE(lhs.presentationEvents.size() == rhs.presentationEvents.size());
    for (size_t i = 0; i < lhs.presentationEvents.size(); ++i)
    {
        checkPresentationEventEquals(lhs.presentationEvents[i], rhs.presentationEvents[i]);
    }

    REQUIRE(lhs.effectEvents.size() == rhs.effectEvents.size());
    for (size_t i = 0; i < lhs.effectEvents.size(); ++i)
    {
        checkEffectEventEquals(lhs.effectEvents[i], rhs.effectEvents[i]);
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
    REQUIRE(result.presentationEvents.size() == 2);
    CHECK(result.presentationEvents[0].type == BattlePresentationEventType::FloatingText);
    CHECK(result.presentationEvents[0].targetUnitId == 1);
    CHECK(result.presentationEvents[0].text == "野球拳");
    CHECK(result.presentationEvents[1].type == BattlePresentationEventType::RoleEffect);
    CHECK(result.presentationEvents[1].targetUnitId == 1);
    CHECK(result.presentationEvents[1].effectId == 77);
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
    input.normalSkill = skill(119, 1, 400.0);
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
    input.normalSkill = skill(107, 1, 400.0);
    input.normalSkill.visualEffectId = 88;
    input.normalSkill.selectDistance = 4;
    input.targetDistance = 300.0;

    auto result = BattleCastPlanner().plan(input);

    REQUIRE(result.decision.canCast);
    REQUIRE(result.attackSpawnRequests.size() == 1);
    const auto& request = result.attackSpawnRequests[0];
    CHECK(request.initial.attackerUnitId == 1);
    CHECK(request.initial.skillId == 107);
    CHECK(request.initial.operationType == BattleOperationType::RangedProjectile);
    CHECK(request.initial.visualEffectId == 88);
    CHECK(request.initial.preferredTargetUnitId == 2);
    CHECK(request.initial.position.x == Catch::Approx(82.0f));
    CHECK(request.initial.position.y == Catch::Approx(20.0f));
    CHECK(request.initial.velocity.x > 0.0f);
    CHECK(request.initial.velocity.y == Catch::Approx(0.0f));
    CHECK(request.initial.totalFrame == 24);
    CHECK(request.initial.through);
    CHECK_FALSE(request.initial.ultimate);
}

TEST_CASE("BattleCastSystem_MeleeSpawnUsesLegacyOriginAndFrameCount", "[battle][cast]")
{
    auto input = basicInput();
    input.normalSkill = skill(109, 0, 137.5);

    auto result = BattleCastPlanner().plan(input);

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

TEST_CASE("BattleCastSystem_StrengthenedMeleeSpawnUsesLegacyTrackingProjectileShape", "[battle][cast]")
{
    auto input = basicInput();
    input.unit.operationCount = 2;
    input.normalSkill = skill(112, 0, 137.5);
    input.normalSkill.selectDistance = 6;

    auto result = BattleCastPlanner().plan(input);

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

    auto result = BattleCastPlanner().plan(input);

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
    CHECK(result.attackSpawnRequests[1].initial.velocity.x == Catch::Approx(LegacyMeleeSplashProjectileSpeed));

    CHECK(result.attackSpawnRequests[2].initial.castSubrequestKind == BattleAttackCastSubrequestKind::ExtraProjectile);
    CHECK(result.attackSpawnRequests[3].initial.castSubrequestKind == BattleAttackCastSubrequestKind::ExtraProjectile);
}

TEST_CASE("BattleCastSystem_OperationOneSpawnTracksForLegacyFrameCount", "[battle][cast]")
{
    auto input = basicInput();
    input.normalSkill = skill(110, 3, 180.0);
    input.targetDistance = 150.0;

    auto result = BattleCastPlanner().plan(input);

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
    input.normalSkill = skill(115, 1, 400.0);
    input.normalSkill.selectDistance = 4;
    input.normalSkill.extraProjectileCount = 2;
    input.targetDistance = 300.0;

    auto result = BattleCastPlanner().plan(input);

    REQUIRE(result.decision.operationType == BattleOperationType::RangedProjectile);
    REQUIRE(result.attackSpawnRequests.size() == 3);
    CHECK(result.attackSpawnRequests[0].initial.castSubrequestKind == BattleAttackCastSubrequestKind::SkillHit);
    CHECK(result.attackSpawnRequests[0].initial.operationType == BattleOperationType::RangedProjectile);
    CHECK(result.attackSpawnRequests[0].initial.totalFrame == 24);
    CHECK(result.attackSpawnRequests[0].initial.through);

    CHECK(result.attackSpawnRequests[1].initial.castSubrequestKind == BattleAttackCastSubrequestKind::ExtraProjectile);
    CHECK(result.attackSpawnRequests[1].initial.operationType == BattleOperationType::RangedProjectile);
    CHECK(result.attackSpawnRequests[1].initial.totalFrame == 24);
    CHECK(result.attackSpawnRequests[1].initial.through);
    CHECK(result.attackSpawnRequests[1].initial.velocity.x == Catch::Approx(12.0f));

    CHECK(result.attackSpawnRequests[2].initial.castSubrequestKind == BattleAttackCastSubrequestKind::ExtraProjectile);
    CHECK(result.attackSpawnRequests[2].initial.operationType == BattleOperationType::RangedProjectile);
}

TEST_CASE("BattleCastSystem_ProjectileCastsUseExplicitProjectileSpawnOffset", "[battle][cast]")
{
    auto input = basicInput();
    input.geometry.meleeAttackEffectOffset = 72.0;
    input.geometry.projectileSpawnOffset = 108.0;

    input.normalSkill = skill(116, 3, 180.0);
    input.targetDistance = 150.0;

    auto trackingResult = BattleCastPlanner().plan(input);

    REQUIRE(trackingResult.decision.operationType == BattleOperationType::TrackingProjectile);
    REQUIRE(trackingResult.attackSpawnRequests.size() == 1);
    CHECK(trackingResult.attackSpawnRequests[0].initial.position.x == Catch::Approx(118.0f));
    CHECK(trackingResult.attackSpawnRequests[0].initial.position.y == Catch::Approx(20.0f));

    input = basicInput();
    input.geometry.meleeAttackEffectOffset = 72.0;
    input.geometry.projectileSpawnOffset = 108.0;
    input.normalSkill = skill(116, 1, 400.0);
    input.normalSkill.selectDistance = 4;
    input.normalSkill.extraProjectileCount = 1;
    input.targetDistance = 300.0;

    auto rangedResult = BattleCastPlanner().plan(input);

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
    input.normalSkill = skill(111, 3, 400.0);
    input.targetDistance = 220.0;

    auto result = BattleCastPlanner().plan(input);

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

    input.unit.dashVelocity = { 8.0f, 0.0f, 0.0f };
    auto fasterResult = BattleCastPlanner().plan(input);

    REQUIRE(fasterResult.attackSpawnRequests.size() == 3);
    CHECK(fasterResult.attackSpawnRequests[0].initial.position.x == Catch::Approx(result.attackSpawnRequests[0].initial.position.x));
    CHECK(fasterResult.attackSpawnRequests[1].initial.position.x == Catch::Approx(result.attackSpawnRequests[1].initial.position.x));
    CHECK(fasterResult.attackSpawnRequests[2].initial.position.x == Catch::Approx(result.attackSpawnRequests[2].initial.position.x));
    CHECK(fasterResult.attackSpawnRequests[0].initial.velocity.x == Catch::Approx(8.0f));
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
    input.normalSkill = skill(114, 3, 400.0);
    input.normalSkill.selectDistance = 4;
    input.targetDistance = 220.0;

    auto result = BattleCastPlanner().plan(input);

    REQUIRE(result.decision.operationType == BattleOperationType::Dash);
    REQUIRE(result.attackSpawnRequests.size() == 2);
    CHECK(result.attackSpawnRequests[0].initial.castSubrequestKind == BattleAttackCastSubrequestKind::DashHit);
    CHECK(result.attackSpawnRequests[1].initial.castSubrequestKind == BattleAttackCastSubrequestKind::DashFollowUpSkill);
    CHECK(result.attackSpawnRequests[1].initial.operationType == BattleOperationType::RangedProjectile);
    CHECK(result.attackSpawnRequests[1].initial.totalFrame == 24);
    CHECK(result.attackSpawnRequests[1].initial.velocity.x == Catch::Approx(12.0f));
}

TEST_CASE("BattleCastSystem_PostSkillHookIsEmittedForNormalAndUltimateCasts", "[battle][cast]")
{
    auto input = basicInput();

    auto normal = BattleCastPlanner().plan(input);
    REQUIRE(normal.effectEvents.size() == 1);
    CHECK(normal.effectEvents[0].hook == BattleHook::SkillFinished);
    CHECK(normal.effectEvents[0].sourceUnitId == 1);
    CHECK(normal.effectEvents[0].targetUnitId == 2);

    input.unit.mp = input.unit.maxMp;
    auto ultimate = BattleCastPlanner().plan(input);
    REQUIRE(ultimate.effectEvents.size() == 2);
    CHECK(ultimate.effectEvents[0].hook == BattleHook::SkillFinished);
    CHECK(ultimate.effectEvents[1].hook == BattleHook::UltimateCast);

    BattleEffectWorld world;
    BattleEffectUnit caster;
    caster.id = input.unit.id;
    caster.alive = true;
    world.units.push_back(caster);

    BattleEffectRegistry registry;
    registry.registerExecutor("添加無敵幀", std::make_unique<AddInvincibilityExecutor>());
    BattleEffectDispatcher dispatcher(registry);
    dispatcher.addEffect(invincibilityEffect());
    auto commands = dispatcher.dispatch(world, normal.effectEvents[0]);

    REQUIRE(commands.size() == 1);
    CHECK(commands[0].type == BattleEffectCommandType::AddInvincibility);
    CHECK(commands[0].targetUnitId == input.unit.id);
    CHECK(world.units[0].invincible == 30);
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
