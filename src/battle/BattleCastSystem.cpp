#include "BattleCastSystem.h"

#include "BattleCombatIntent.h"

namespace KysChess::Battle
{

namespace
{

BattleSkillState toCombatSkill(const BattleCastSkillState& skill)
{
    BattleSkillState combatSkill;
    combatSkill.id = skill.id;
    combatSkill.attackAreaType = skill.attackAreaType;
    combatSkill.magicType = skill.magicType;
    combatSkill.reach = skill.reach;
    combatSkill.forceRanged = skill.forceRanged;
    combatSkill.rangedStyle = skill.rangedStyle;
    return combatSkill;
}

}  // namespace

BattleCastResult BattleCastPlanner::plan(const BattleCastInput& input) const
{
    BattleCastResult result;
    auto& decision = result.decision;
    decision.unitId = input.unit.id;
    decision.targetUnitId = input.targetUnitId;

    bool ultimate = false;
    const auto& selectedSkill = selectSkill(input, ultimate);
    decision.ultimate = ultimate;
    decision.skillId = selectedSkill.id;

    decision.reason = blockedReason(input);
    if (decision.reason != BattleCastBlockReason::None)
    {
        return result;
    }

    if (selectedSkill.id < 0)
    {
        decision.reason = BattleCastBlockReason::NoSkill;
        return result;
    }

    CombatIntentInput intentInput;
    intentInput.canStartAttack = true;
    intentInput.hasEquippedSkill = input.unit.hasEquippedSkill;
    intentInput.ultimateReady = ultimate;
    intentInput.movementDashActive = input.unit.movementDashActive;
    intentInput.dashAttackEnabled = input.unit.dashAttackEnabled;
    intentInput.targetDistance = input.targetDistance;
    intentInput.meleeAttackReach = input.unit.meleeAttackReach;
    intentInput.dashAttackReach = input.unit.dashAttackReach;
    intentInput.plannedSkill = toCombatSkill(selectedSkill);

    auto intent = BattleCombatIntentPlanner().select(intentInput);
    decision.equipSkill = intent.equipPlannedSkill;
    decision.announceUltimate = intent.announceUltimate;
    decision.canCast = intent.startAttack;
    decision.operationType = intent.operationType;
    if (!decision.canCast)
    {
        decision.reason = input.unit.movementDashActive
            ? BattleCastBlockReason::MovementDashActive
            : BattleCastBlockReason::OutOfRange;
    }
    return result;
}

const BattleCastSkillState& BattleCastPlanner::selectSkill(const BattleCastInput& input, bool& ultimate) const
{
    ultimate = input.ultimateSkill.id >= 0 && input.unit.mp == input.unit.maxMp;
    if (ultimate)
    {
        return input.ultimateSkill;
    }
    return input.normalSkill;
}

BattleCastBlockReason BattleCastPlanner::blockedReason(const BattleCastInput& input) const
{
    if (!input.unit.alive)
    {
        return BattleCastBlockReason::Dead;
    }
    if (input.unit.frozen)
    {
        return BattleCastBlockReason::Frozen;
    }
    if (input.unit.stunned)
    {
        return BattleCastBlockReason::Stunned;
    }
    if (input.targetUnitId < 0)
    {
        return BattleCastBlockReason::NoTarget;
    }
    return BattleCastBlockReason::None;
}

}  // namespace KysChess::Battle
