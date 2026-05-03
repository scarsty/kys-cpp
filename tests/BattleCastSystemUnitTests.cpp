#include "battle/BattleCastSystem.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace KysChess::Battle;

namespace
{

BattleCastSkillState skill(int id, int attackAreaType, double reach, bool forceRanged = false)
{
    BattleCastSkillState state;
    state.id = id;
    state.attackAreaType = attackAreaType;
    state.magicType = 1;
    state.reach = reach;
    state.forceRanged = forceRanged;
    state.rangedStyle = forceRanged || attackAreaType == 1 || attackAreaType == 2 || attackAreaType == 3;
    return state;
}

BattleCastInput basicInput()
{
    BattleCastInput input;
    input.unit.id = 1;
    input.unit.position = { 10.0f, 20.0f, 0.0f };
    input.unit.facing = { 1.0f, 0.0f, 0.0f };
    input.unit.mp = 30;
    input.unit.maxMp = 100;
    input.unit.meleeAttackReach = 137.5;
    input.unit.actProperty = 0;
    input.unit.speed = 0;
    input.targetUnitId = 2;
    input.targetPosition = { 110.0f, 20.0f, 0.0f };
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
    effect.executorKey = "添加无敌帧";
    effect.value = 30;
    return effect;
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
    CHECK(request.position.x == Catch::Approx(10.0f));
    CHECK(request.position.y == Catch::Approx(20.0f));
    CHECK(request.velocity.x > 0.0f);
    CHECK(request.velocity.y == Catch::Approx(0.0f));
    CHECK(request.totalFrame > 1);
    CHECK(request.through);
    CHECK_FALSE(request.ultimate);
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
    input.randomSeed = 12345;
    input.normalSkill = skill(108, 1, 400.0);
    input.normalSkill.name = "定序招式";
    input.normalSkill.visualEffectId = 99;
    input.targetDistance = 250.0;

    auto first = BattleCastPlanner().plan(input);
    auto second = BattleCastPlanner().plan(input);

    REQUIRE(first.decision.canCast);
    REQUIRE(second.decision.canCast);
    REQUIRE(first.attackSpawnRequests.size() == second.attackSpawnRequests.size());
    CHECK(first.cooldownDelta == second.cooldownDelta);
    CHECK(first.mpDelta == second.mpDelta);
    CHECK(first.animation.castFrame == second.animation.castFrame);
    CHECK(first.gameplayEvents.size() == second.gameplayEvents.size());
    CHECK(first.presentationEvents.size() == second.presentationEvents.size());
    CHECK(first.effectEvents.size() == second.effectEvents.size());
    CHECK(first.attackSpawnRequests[0].velocity.x == Catch::Approx(second.attackSpawnRequests[0].velocity.x));
    CHECK(first.attackSpawnRequests[0].velocity.y == Catch::Approx(second.attackSpawnRequests[0].velocity.y));
    CHECK(first.presentationEvents[0].text == second.presentationEvents[0].text);
}
