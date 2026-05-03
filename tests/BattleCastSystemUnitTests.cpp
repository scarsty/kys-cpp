#include "battle/BattleCastSystem.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>

using namespace KysChess::Battle;

namespace
{

constexpr int SceneTileWidth = 36;

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
    input.geometry.tileWidth = SceneTileWidth;
    input.geometry.meleeAttackEffectOffset = SceneTileWidth * 2.0;
    input.geometry.projectileSpeed = SceneTileWidth / 3.0;
    input.unit.id = 1;
    input.unit.position = { 10.0f, 20.0f, 0.0f };
    input.unit.facing = { 1.0f, 0.0f, 0.0f };
    input.unit.mp = 30;
    input.unit.maxMp = 100;
    input.unit.meleeAttackReach = 137.5;
    input.unit.actProperty = 0;
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
    CHECK(lhs.attackerUnitId == rhs.attackerUnitId);
    CHECK(lhs.skillId == rhs.skillId);
    CHECK(lhs.operationType == rhs.operationType);
    CHECK(lhs.visualEffectId == rhs.visualEffectId);
    CHECK(lhs.preferredTargetUnitId == rhs.preferredTargetUnitId);
    checkPoint(lhs.position, rhs.position);
    checkPoint(lhs.velocity, rhs.velocity);
    CHECK(lhs.totalFrame == rhs.totalFrame);
    CHECK(lhs.through == rhs.through);
    CHECK(lhs.track == rhs.track);
    CHECK(lhs.requirePreferredTarget == rhs.requirePreferredTarget);
    CHECK(lhs.executeCanHitInvincible == rhs.executeCanHitInvincible);
    CHECK(lhs.ignoreProjectileCancel == rhs.ignoreProjectileCancel);
    CHECK(lhs.sharedHitGroupId == rhs.sharedHitGroupId);
    CHECK(lhs.bounceRemaining == rhs.bounceRemaining);
    CHECK(lhs.bounceRange == rhs.bounceRange);
    CHECK(lhs.bounceChancePct == rhs.bounceChancePct);
    CHECK(lhs.bounceRollPct == rhs.bounceRollPct);
    CHECK(lhs.ultimate == rhs.ultimate);
    CHECK(lhs.initialFrame == rhs.initialFrame);
    CHECK(lhs.castSubrequestKind == rhs.castSubrequestKind);
    CHECK(lhs.strengthMultiplier == Catch::Approx(rhs.strengthMultiplier));
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
    CHECK(result.decision.operationType == 0);

    input.unit.mp = input.unit.maxMp;
    result = BattleCastPlanner().plan(input);
    CHECK(result.decision.canCast);
    CHECK(result.decision.ultimate);
    CHECK(result.decision.skillId == 201);
    CHECK(result.decision.operationType == 2);
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
    CHECK(result.decision.operationType == 2);
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
    CHECK(result.decision.operationType == 2);
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
    CHECK(result.decision.operationType == -1);
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
    CHECK(result.decision.operationType == -1);
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
    CHECK(request.attackerUnitId == 1);
    CHECK(request.skillId == 107);
    CHECK(request.operationType == 2);
    CHECK(request.visualEffectId == 88);
    CHECK(request.preferredTargetUnitId == 2);
    CHECK(request.position.x == Catch::Approx(82.0f));
    CHECK(request.position.y == Catch::Approx(20.0f));
    CHECK(request.velocity.x > 0.0f);
    CHECK(request.velocity.y == Catch::Approx(0.0f));
    CHECK(request.totalFrame == 24);
    CHECK(request.through);
    CHECK_FALSE(request.ultimate);
}

TEST_CASE("BattleCastSystem_MeleeSpawnUsesLegacyOriginAndFrameCount", "[battle][cast]")
{
    auto input = basicInput();
    input.normalSkill = skill(109, 0, 137.5);

    auto result = BattleCastPlanner().plan(input);

    REQUIRE(result.decision.operationType == 0);
    REQUIRE(result.attackSpawnRequests.size() == 1);
    const auto& request = result.attackSpawnRequests[0];
    CHECK(request.position.x == Catch::Approx(82.0f));
    CHECK(request.position.y == Catch::Approx(20.0f));
    CHECK(request.velocity.x == Catch::Approx(0.0f));
    CHECK(request.velocity.y == Catch::Approx(0.0f));
    CHECK(request.totalFrame == 10);
    CHECK_FALSE(request.track);
    CHECK_FALSE(request.through);
    CHECK(request.castSubrequestKind == BattleAttackCastSubrequestKind::SkillHit);
    CHECK(request.initialFrame == 0);
    CHECK(request.strengthMultiplier == Catch::Approx(1.0f));
}

TEST_CASE("BattleCastSystem_StrengthenedMeleeSpawnUsesLegacyTrackingProjectileShape", "[battle][cast]")
{
    auto input = basicInput();
    input.unit.operationCount = 2;
    input.normalSkill = skill(112, 0, 137.5);
    input.normalSkill.selectDistance = 6;

    auto result = BattleCastPlanner().plan(input);

    REQUIRE(result.decision.operationType == 0);
    REQUIRE(result.attackSpawnRequests.size() == 1);
    const auto& request = result.attackSpawnRequests[0];
    CHECK(request.totalFrame == 30);
    CHECK(request.track);
    CHECK(request.velocity.x == Catch::Approx(3.0f));
    CHECK(request.velocity.y == Catch::Approx(0.0f));
    CHECK(request.strengthMultiplier == Catch::Approx(2.0f));
    CHECK(request.castSubrequestKind == BattleAttackCastSubrequestKind::SkillHit);
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

    REQUIRE(result.decision.operationType == 0);
    REQUIRE(result.attackSpawnRequests.size() == 4);
    CHECK(result.attackSpawnRequests[0].strengthMultiplier == Catch::Approx(2.0f));
    CHECK(result.attackSpawnRequests[0].track);
    CHECK(result.attackSpawnRequests[0].totalFrame == 30);

    CHECK(result.attackSpawnRequests[1].castSubrequestKind == BattleAttackCastSubrequestKind::MeleeSplash);
    CHECK(result.attackSpawnRequests[1].strengthMultiplier == Catch::Approx(0.5f));
    CHECK(result.attackSpawnRequests[1].track);
    CHECK(result.attackSpawnRequests[1].totalFrame == 60);
    CHECK(result.attackSpawnRequests[1].initialFrame == 5);
    CHECK(result.attackSpawnRequests[1].velocity.x == Catch::Approx(3.0f));

    CHECK(result.attackSpawnRequests[2].castSubrequestKind == BattleAttackCastSubrequestKind::ExtraProjectile);
    CHECK(result.attackSpawnRequests[3].castSubrequestKind == BattleAttackCastSubrequestKind::ExtraProjectile);
}

TEST_CASE("BattleCastSystem_OperationOneSpawnTracksForLegacyFrameCount", "[battle][cast]")
{
    auto input = basicInput();
    input.normalSkill = skill(110, 3, 180.0);
    input.targetDistance = 150.0;

    auto result = BattleCastPlanner().plan(input);

    REQUIRE(result.decision.operationType == 1);
    REQUIRE(result.attackSpawnRequests.size() == 1);
    const auto& request = result.attackSpawnRequests[0];
    CHECK(request.position.x == Catch::Approx(82.0f));
    CHECK(request.velocity.x == Catch::Approx(12.0f));
    CHECK(request.velocity.y == Catch::Approx(0.0f));
    CHECK(request.totalFrame == 120);
    CHECK(request.track);
    CHECK_FALSE(request.through);
}

TEST_CASE("BattleCastSystem_RangedCastExpandsExplicitExtraProjectiles", "[battle][cast]")
{
    auto input = basicInput();
    input.normalSkill = skill(115, 1, 400.0);
    input.normalSkill.selectDistance = 4;
    input.normalSkill.extraProjectileCount = 2;
    input.targetDistance = 300.0;

    auto result = BattleCastPlanner().plan(input);

    REQUIRE(result.decision.operationType == 2);
    REQUIRE(result.attackSpawnRequests.size() == 3);
    CHECK(result.attackSpawnRequests[0].castSubrequestKind == BattleAttackCastSubrequestKind::SkillHit);
    CHECK(result.attackSpawnRequests[0].operationType == 2);
    CHECK(result.attackSpawnRequests[0].totalFrame == 24);
    CHECK(result.attackSpawnRequests[0].through);

    CHECK(result.attackSpawnRequests[1].castSubrequestKind == BattleAttackCastSubrequestKind::ExtraProjectile);
    CHECK(result.attackSpawnRequests[1].operationType == 2);
    CHECK(result.attackSpawnRequests[1].totalFrame == 24);
    CHECK(result.attackSpawnRequests[1].through);
    CHECK(result.attackSpawnRequests[1].velocity.x == Catch::Approx(12.0f));

    CHECK(result.attackSpawnRequests[2].castSubrequestKind == BattleAttackCastSubrequestKind::ExtraProjectile);
    CHECK(result.attackSpawnRequests[2].operationType == 2);
}

TEST_CASE("BattleCastSystem_DashCastUsesDashRecoveryAndHitEffectRequest", "[battle][cast]")
{
    auto input = basicInput();
    input.unit.dashAttackEnabled = true;
    input.unit.dashAttackReach = 350.0;
    input.unit.dashVelocity = { 4.0f, 0.0f, 0.0f };
    input.unit.dashHitCount = 3;
    input.normalSkill = skill(111, 3, 400.0);
    input.targetDistance = 220.0;

    auto result = BattleCastPlanner().plan(input);

    REQUIRE(result.decision.operationType == 3);
    CHECK(result.animation.recoveryFrames == 5);
    REQUIRE(result.attackSpawnRequests.size() == 3);

    CHECK(result.attackSpawnRequests[0].position.x == Catch::Approx(74.0f));
    CHECK(result.attackSpawnRequests[0].initialFrame == 3);
    CHECK(result.attackSpawnRequests[0].velocity.x == Catch::Approx(4.0f));
    CHECK(result.attackSpawnRequests[0].castSubrequestKind == BattleAttackCastSubrequestKind::DashHit);

    CHECK(result.attackSpawnRequests[1].position.x == Catch::Approx(82.0f));
    CHECK(result.attackSpawnRequests[1].initialFrame == 6);

    CHECK(result.attackSpawnRequests[2].position.x == Catch::Approx(90.0f));
    CHECK(result.attackSpawnRequests[2].initialFrame == 9);
    CHECK(result.attackSpawnRequests[2].totalFrame == 30);
    CHECK_FALSE(result.attackSpawnRequests[2].track);
    CHECK_FALSE(result.attackSpawnRequests[2].through);
}

TEST_CASE("BattleCastSystem_DashCastCanEmitExplicitFollowUpSkillRequest", "[battle][cast]")
{
    auto input = basicInput();
    input.unit.dashAttackEnabled = true;
    input.unit.dashAttackReach = 350.0;
    input.unit.dashVelocity = { 4.0f, 0.0f, 0.0f };
    input.unit.dashHitCount = 1;
    input.unit.emitDashFollowUpSkillAttack = true;
    input.unit.dashFollowUpOperationType = 2;
    input.normalSkill = skill(114, 3, 400.0);
    input.normalSkill.selectDistance = 4;
    input.targetDistance = 220.0;

    auto result = BattleCastPlanner().plan(input);

    REQUIRE(result.decision.operationType == 3);
    REQUIRE(result.attackSpawnRequests.size() == 2);
    CHECK(result.attackSpawnRequests[0].castSubrequestKind == BattleAttackCastSubrequestKind::DashHit);
    CHECK(result.attackSpawnRequests[1].castSubrequestKind == BattleAttackCastSubrequestKind::DashFollowUpSkill);
    CHECK(result.attackSpawnRequests[1].operationType == 2);
    CHECK(result.attackSpawnRequests[1].totalFrame == 24);
    CHECK(result.attackSpawnRequests[1].velocity.x == Catch::Approx(12.0f));
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
