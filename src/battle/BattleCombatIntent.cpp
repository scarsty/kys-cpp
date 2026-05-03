#include "BattleCombatIntent.h"

namespace KysChess::Battle
{

int BattleCombatIntentPlanner::operationTypeForAttackArea(int attackAreaType) const
{
    if (attackAreaType == 0)
    {
        return 0;
    }
    if (attackAreaType == 1 || attackAreaType == 2)
    {
        return 2;
    }
    if (attackAreaType == 3)
    {
        return 1;
    }
    return -1;
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
        && input.targetDistance <= input.dashAttackReach
        && (input.targetDistance > input.meleeAttackReach || input.plannedSkill.rangedStyle);
    if (useDashAttack)
    {
        intent.startAttack = true;
        intent.operationType = 3;
        return intent;
    }

    if (input.plannedSkill.attackAreaType == 0)
    {
        if (input.plannedSkill.forceRanged && input.targetDistance <= input.plannedSkill.reach)
        {
            intent.startAttack = true;
            intent.operationType = 2;
        }
        else if (!input.plannedSkill.forceRanged && input.targetDistance <= input.meleeAttackReach)
        {
            intent.startAttack = true;
            intent.operationType = 0;
        }
        return intent;
    }

    int operationType = operationTypeForAttackArea(input.plannedSkill.attackAreaType);
    if (operationType >= 0 && input.targetDistance <= input.plannedSkill.reach)
    {
        intent.startAttack = true;
        intent.operationType = operationType;
    }
    return intent;
}

}  // namespace KysChess::Battle
