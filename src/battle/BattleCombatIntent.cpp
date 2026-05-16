#include "BattleCombatIntent.h"

namespace KysChess::Battle
{

BattleOperationType BattleCombatIntentPlanner::operationTypeForAttackArea(int attackAreaType) const
{
    if (attackAreaType == 0)
    {
        return BattleOperationType::Melee;
    }
    if (attackAreaType == 1 || attackAreaType == 2)
    {
        return BattleOperationType::RangedProjectile;
    }
    if (attackAreaType == 3)
    {
        return BattleOperationType::TrackingProjectile;
    }
    return BattleOperationType::None;
}

CombatIntent BattleCombatIntentPlanner::select(const CombatIntentInput& input) const
{
    CombatIntent intent;
    bool hasPlannedSkill = input.plannedSkill.id >= 0;
    intent.equipPlannedSkill = input.canStartAttack && !input.hasEquippedSkill && hasPlannedSkill;
    intent.announceUltimate = intent.equipPlannedSkill && input.ultimateReady;

    if (input.movementDashActive || !input.canStartAttack || !hasPlannedSkill)
    {
        return intent;
    }

    bool hasSkillForAttack = input.hasEquippedSkill || intent.equipPlannedSkill;
    if (!hasSkillForAttack)
    {
        return intent;
    }

    bool useDashAttack = input.dashAttackEnabled
        && !input.plannedSkill.rangedStyle
        && input.targetDistance <= input.dashAttackReach
        && input.targetDistance > input.meleeAttackReach;
    if (useDashAttack)
    {
        intent.startAttack = true;
        intent.operationType = BattleOperationType::Dash;
        return intent;
    }

    if (input.plannedSkill.attackAreaType == 0)
    {
        if (input.plannedSkill.forceRanged && input.targetDistance <= input.plannedSkill.reach)
        {
            intent.startAttack = true;
            intent.operationType = BattleOperationType::RangedProjectile;
        }
        else if (!input.plannedSkill.forceRanged && input.targetDistance <= input.meleeAttackReach)
        {
            intent.startAttack = true;
            intent.operationType = BattleOperationType::Melee;
        }
        return intent;
    }

    BattleOperationType operationType = operationTypeForAttackArea(input.plannedSkill.attackAreaType);
    if (operationType != BattleOperationType::None && input.targetDistance <= input.plannedSkill.reach)
    {
        intent.startAttack = true;
        intent.operationType = operationType;
    }
    return intent;
}

}  // namespace KysChess::Battle
