#include "BattleCastSystem.h"

#include "BattleCombatIntent.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace KysChess::Battle
{

namespace
{
constexpr int ActionRecoveryFrames = 4;
constexpr int DashMomentumFrames = 5;
constexpr int BattleTileWidth = 50;
constexpr int ProjectileSpeed = BattleTileWidth / 3;
constexpr double MeleeAttackEffectOffset = BattleTileWidth * 2.0;

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

double norm(const Pointf& point)
{
    return std::sqrt(static_cast<double>(point.x) * point.x
        + static_cast<double>(point.y) * point.y
        + static_cast<double>(point.z) * point.z);
}

Pointf normalizedTo(Pointf point, double length)
{
    const double current = norm(point);
    if (current <= 0.0001)
    {
        return {};
    }
    point.x = static_cast<float>(point.x * length / current);
    point.y = static_cast<float>(point.y * length / current);
    point.z = static_cast<float>(point.z * length / current);
    return point;
}

Pointf castFacing(const BattleCastInput& input)
{
    auto facing = input.targetPosition - input.unit.position;
    if (norm(facing) <= 0.0001)
    {
        facing = input.unit.facing;
    }
    if (norm(facing) <= 0.0001)
    {
        facing = { 1.0f, 0.0f, 0.0f };
    }
    return normalizedTo(facing, 1.0);
}

int castFrameForOperation(int operationType)
{
    constexpr int CastFrames[] = { 25, 30, 20, 25 };
    assert(operationType >= 0 && operationType <= 3);
    return CastFrames[operationType];
}

int cooldownForOperation(const BattleCastInput& input, int operationType)
{
    constexpr int BaseCooldowns[] = { 105, 185, 115, 45 };
    constexpr int MinCooldowns[] = { 60, 70, 70, 45 };
    assert(operationType >= 0 && operationType <= 3);

    int cooldown = std::max(MinCooldowns[operationType], BaseCooldowns[operationType] - input.unit.actProperty / (operationType == 1 ? 1 : 2));
    const int speed = std::min(150, input.unit.speed);
    cooldown = static_cast<int>(cooldown * (1.0 - 0.5 * speed / 150.0));
    cooldown = std::max(castFrameForOperation(operationType) + 2, cooldown);
    if (input.unit.cooldownReductionPct > 0)
    {
        cooldown = static_cast<int>(cooldown * (1.0 - input.unit.cooldownReductionPct / 100.0));
        cooldown = std::max(castFrameForOperation(operationType) + 2, cooldown);
    }
    return cooldown;
}

int recoveryFramesForOperation(int operationType)
{
    assert(operationType >= 0 && operationType <= 3);
    return operationType == 3 ? DashMomentumFrames : ActionRecoveryFrames;
}

int mpDeltaForCast(const BattleCastInput& input, bool ultimate)
{
    return ultimate ? -input.unit.maxMp : 5;
}

int projectileFramesForSelectDistance(int selectDistance)
{
    assert(selectDistance > 0);
    constexpr int spawnOffset = BattleTileWidth * 2;
    const int reach = spawnOffset + BattleTileWidth * 5 + (selectDistance - 1) * BattleTileWidth;
    return (reach - spawnOffset) / ProjectileSpeed;
}

BattlePresentationColor ultimateTextColor()
{
    return { 255, 215, 0, 255 };
}

BattleAttackSpawnRequest makeAttackSpawnRequest(
    const BattleCastResult& result,
    const BattleCastInput& input,
    const BattleCastSkillState& selectedSkill)
{
    auto facing = castFacing(input);

    BattleAttackSpawnRequest request;
    request.attackerUnitId = input.unit.id;
    request.skillId = selectedSkill.id;
    request.operationType = result.decision.operationType;
    request.visualEffectId = selectedSkill.visualEffectId;
    request.preferredTargetUnitId = input.targetUnitId;
    request.position = input.unit.position + Pointf{
        static_cast<float>(facing.x * MeleeAttackEffectOffset),
        static_cast<float>(facing.y * MeleeAttackEffectOffset),
        static_cast<float>(facing.z * MeleeAttackEffectOffset),
    };
    request.ultimate = result.decision.ultimate;

    switch (result.decision.operationType)
    {
    case 0:
        request.totalFrame = 10;
        break;
    case 1:
        request.velocity = normalizedTo(facing, ProjectileSpeed);
        request.totalFrame = 120;
        request.track = true;
        break;
    case 2:
        request.velocity = normalizedTo(facing, ProjectileSpeed);
        request.totalFrame = projectileFramesForSelectDistance(selectedSkill.selectDistance);
        request.through = selectedSkill.attackAreaType == 1 || selectedSkill.attackAreaType == 2;
        break;
    case 3:
        request.totalFrame = 30;
        break;
    default:
        assert(false);
        break;
    }
    return request;
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
    intentInput.canStartAttack = input.unit.canStartAttack;
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
            : !input.unit.canStartAttack
                ? BattleCastBlockReason::AttackNotReady
                : BattleCastBlockReason::OutOfRange;
        return result;
    }
    appendCommittedCastOutput(result, input, selectedSkill);
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

void BattleCastPlanner::appendCommittedCastOutput(BattleCastResult& result,
                                                  const BattleCastInput& input,
                                                  const BattleCastSkillState& selectedSkill) const
{
    assert(result.decision.canCast);
    assert(result.decision.operationType >= 0 && result.decision.operationType <= 3);

    result.animation.castFrame = castFrameForOperation(result.decision.operationType);
    result.animation.cooldownFrames = cooldownForOperation(input, result.decision.operationType);
    result.animation.recoveryFrames = recoveryFramesForOperation(result.decision.operationType);
    result.cooldownDelta = result.animation.cooldownFrames;
    result.mpDelta = mpDeltaForCast(input, result.decision.ultimate);

    result.attackSpawnRequests.push_back(makeAttackSpawnRequest(result, input, selectedSkill));

    BattleGameplayEvent gameplayEvent;
    gameplayEvent.type = BattleGameplayEventType::CastStarted;
    gameplayEvent.sourceUnitId = input.unit.id;
    gameplayEvent.targetUnitId = input.targetUnitId;
    gameplayEvent.effectId = selectedSkill.id;
    result.gameplayEvents.push_back(gameplayEvent);

    if (!selectedSkill.name.empty())
    {
        BattlePresentationEvent textEvent;
        textEvent.type = BattlePresentationEventType::FloatingText;
        textEvent.targetUnitId = input.unit.id;
        textEvent.text = selectedSkill.name;
        textEvent.skillName = selectedSkill.name;
        textEvent.textSize = result.decision.ultimate ? 36 : 24;
        textEvent.color = result.decision.ultimate ? ultimateTextColor() : BattlePresentationColor{};
        result.presentationEvents.push_back(textEvent);
    }

    if (selectedSkill.visualEffectId >= 0)
    {
        BattlePresentationEvent effectEvent;
        effectEvent.type = BattlePresentationEventType::RoleEffect;
        effectEvent.targetUnitId = input.unit.id;
        effectEvent.effectId = selectedSkill.visualEffectId;
        effectEvent.visualEffectId = selectedSkill.visualEffectId;
        effectEvent.durationFrames = result.animation.castFrame;
        result.presentationEvents.push_back(effectEvent);
    }

    result.effectEvents.push_back({ BattleHook::SkillFinished, input.unit.id, input.targetUnitId });
    if (result.decision.ultimate)
    {
        result.effectEvents.push_back({ BattleHook::UltimateCast, input.unit.id, input.targetUnitId });
    }
}

}  // namespace KysChess::Battle
