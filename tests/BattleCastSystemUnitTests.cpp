#include "battle/BattleCastSystem.h"

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
    input.unit.mp = 30;
    input.unit.maxMp = 100;
    input.unit.meleeAttackReach = 137.5;
    input.targetUnitId = 2;
    input.targetDistance = 100.0;
    input.normalSkill = skill(101, 0, 137.5);
    input.ultimateSkill = skill(201, 1, 400.0);
    return input;
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
