#include "battle/BattleCombatIntent.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess::Battle;

namespace
{

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

}  // namespace

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

    CombatIntentInput forcedRangedArea = forcedRanged;
    forcedRangedArea.plannedSkill = skill(3, 425.0, true);
    intent = planner.select(forcedRangedArea);
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

TEST_CASE("BattleCombatIntent_BlinkAttackStartsMeleeOutsideNormalReach", "[battle][intent]")
{
    CombatIntentInput input;
    input.canStartAttack = true;
    input.hasEquippedSkill = true;
    input.blinkAttackEnabled = true;
    input.targetDistance = 1200.0;
    input.meleeAttackReach = 137.5;
    input.dashAttackReach = 375.0;
    input.plannedSkill = skill(0, 137.5);

    auto intent = BattleCombatIntentPlanner().select(input);

    CHECK(intent.startAttack);
    CHECK(intent.operationType == BattleOperationType::Melee);
}
