#include "BattleSceneFrameStateApplier.h"

#include "ChessCombo.h"

#include <algorithm>
#include <cassert>
#include <format>
#include <set>

namespace
{
struct BattleLifecycleApplyResult
{
    bool battleEnded = false;
    int battleResult = -1;
    bool unitDied = false;
    std::vector<int> diedUnitIds;
};

BattleLifecycleApplyResult collectBattleLifecycleApplyResult(
    int currentBattleResult,
    const std::vector<KysChess::Battle::BattleGameplayEvent>& events)
{
    BattleLifecycleApplyResult result;
    for (const auto& event : events)
    {
        switch (event.type)
        {
        case KysChess::Battle::BattleGameplayEventType::UnitDied:
            result.unitDied = true;
            result.diedUnitIds.push_back(event.targetUnitId);
            break;
        case KysChess::Battle::BattleGameplayEventType::BattleEnded:
            if (currentBattleResult == -1)
            {
                result.battleEnded = true;
                result.battleResult = event.amount;
            }
            break;
        default:
            break;
        }
    }
    return result;
}
}  // namespace

BattleSceneFrameStateApplyResult BattleSceneFrameStateApplier::apply(
    const KysChess::Battle::BattleFrameResult& frameResult,
    int currentBattleResult,
    BattleSceneFrameStateApplyContext context) const
{
    assert(context.units);

    BattleSceneFrameStateApplyResult result;
    applyUnitApplications(frameResult.unitApplications, context);
    applyStatusState(frameResult.stateApplications, context);
    applyDamageTransactions(frameResult, currentBattleResult, context, result);
    applyTeamEffectState(frameResult.teamEffectEvents, context);
    applyFrameApplications(frameResult.applications, context, result);
    applyMovementPresentation(frameResult.movementPresentationResults, context);
    applyActionResults(frameResult.actionResults, context, result);
    return result;
}

void BattleSceneFrameStateApplier::applyUnitApplications(
    const std::vector<KysChess::Battle::BattleFrameUnitApplication>& applications,
    BattleSceneFrameStateApplyContext& context) const
{
    for (const auto& application : applications)
    {
        auto& unit = context.units->requireUnit(application.unitId);
        unit.animation.cooldown = application.cooldown;
        unit.animation.actFrame = application.actFrame;
        unit.animation.actType = application.actType;
        unit.vitals.mp = application.finalMp;
        if (application.resetDashVelocity)
        {
            unit.motion.velocity = { 0, 0, 0 };
        }
    }
}

void BattleSceneFrameStateApplier::applyStatusState(
    const KysChess::Battle::BattleFrameStateApplications& applications,
    BattleSceneFrameStateApplyContext& context) const
{
    for (const auto& combo : applications.comboRenderUnits)
    {
        auto& unit = context.units->requireUnit(combo.unitId);
        unit.combo.shield = combo.shield;
        unit.combo.blockFirstHitsRemaining = combo.blockFirstHitsRemaining;
    }
    for (const auto& status : applications.statusRenderUnits)
    {
        auto& unit = context.units->requireUnit(status.unitId);
        unit.invincible = status.invincible;
        unit.frozen = status.frozenFrames;
        unit.frozenMax = status.frozenMaxFrames;
    }
}

void BattleSceneFrameStateApplier::applyDamageTransactions(
    const KysChess::Battle::BattleFrameResult& frameResult,
    int currentBattleResult,
    BattleSceneFrameStateApplyContext& context,
    BattleSceneFrameStateApplyResult& result) const
{
    auto lifecycleApply = collectBattleLifecycleApplyResult(
        currentBattleResult,
        frameResult.frame.gameplayEvents);
    result.unitDied = lifecycleApply.unitDied;
    result.diedUnitIds = lifecycleApply.diedUnitIds;
    std::set<int> diedUnitIds(lifecycleApply.diedUnitIds.begin(), lifecycleApply.diedUnitIds.end());

    for (const auto& damage : frameResult.damageRenderApplications)
    {
        auto& defenderUnit = context.units->requireUnit(damage.defender.unitId);
        defenderUnit.vitals.hp = damage.defender.hp;
        defenderUnit.vitals.mp = damage.defender.mp;
        defenderUnit.invincible = damage.defender.invincible;
        defenderUnit.alive = damage.defender.alive;
        defenderUnit.frozen = damage.frozenFrames;
        defenderUnit.frozenMax = damage.frozenMaxFrames;
        defenderUnit.animation.cooldown = damage.cooldown;

        if (damage.attacker.unitId >= 0)
        {
            auto& attackerUnit = context.units->requireUnit(damage.attacker.unitId);
            attackerUnit.vitals.hp = damage.attacker.hp;
            attackerUnit.vitals.mp = damage.attacker.mp;
            attackerUnit.invincible = damage.attacker.invincible;
            attackerUnit.alive = damage.attacker.alive;
        }

        if (damage.committedHpDamage > 0)
        {
            assert(context.random);
            result.bloodEffects.push_back({
                damage.defender.unitId,
                std::format("eft/bld{:03}", int(context.random->rand() * 5)),
            });
            assert(context.hurtFlashTimers);
            (*context.hurtFlashTimers)[damage.defender.unitId] = context.hurtFlashDuration;
        }

        if (diedUnitIds.find(damage.defender.unitId) != diedUnitIds.end())
        {
            assert(context.random);
            assert(context.comboStates);
            auto sit = context.comboStates->find(damage.defender.unitId);
            if (sit != context.comboStates->end())
            {
                sit->second.onSkillTeamHealPending = false;
            }

            assert(context.transferAntiCombo);
            context.transferAntiCombo(damage.defender.unitId);

            defenderUnit.motion.velocity.normTo(15);
            defenderUnit.motion.velocity.z = 12;
            defenderUnit.motion.velocity.normTo(static_cast<float>(damage.committedHpDamage / 2.0));
            result.jitterX = context.random->rand_int(2) - context.random->rand_int(2);
            result.jitterY = context.random->rand_int(2) - context.random->rand_int(2);
            if (!context.manualCameraEnabled)
            {
                result.cameraFocus = defenderUnit.motion.position;
                result.closeUpFrames = std::max(result.closeUpFrames, context.deathZoomFrames);
            }
            result.frozenFrames = 5;
            result.sceneShake = 10;
            result.slowFrames = context.deathSlowFrames;
        }
    }

    for (const auto& rescue : frameResult.rescueResults)
    {
        assert(rescue.teleport);
        assert(context.pos45To90);
        auto& pulledUnit = context.units->requireUnit(rescue.teleport->unitId);
        auto& pullerUnit = context.units->requireUnit(rescue.teleport->pullerUnitId);
        const auto& destination = rescue.teleport->destinationCell;
        auto pos = context.pos45To90(destination.x, destination.y);
        pulledUnit.gridX = destination.x;
        pulledUnit.gridY = destination.y;
        pulledUnit.motion.position = { pos.x, pos.y, 0 };
        pulledUnit.motion.velocity = { 0, 0, 0 };
        pulledUnit.motion.acceleration = { 0, 0, static_cast<float>(context.gravity) };
        pulledUnit.motion.facing = context.units->facingTowardNearestEnemy(pulledUnit.unitId);
        pullerUnit.motion.facing = context.units->facingTowardNearestEnemy(pullerUnit.unitId);

        if (rescue.heal.amount > 0)
        {
            assert(rescue.heal.targetUnitId == pulledUnit.unitId);
            pulledUnit.vitals.hp = std::min(pulledUnit.vitals.maxHp, pulledUnit.vitals.hp + rescue.heal.amount);
        }
        if (rescue.invincibility.frames > 0)
        {
            assert(rescue.invincibility.targetUnitId == pulledUnit.unitId);
            pulledUnit.invincible += rescue.invincibility.frames;
        }
    }

    if (lifecycleApply.battleEnded)
    {
        result.battleEnded = true;
        result.battleResult = lifecycleApply.battleResult;
        if (!context.manualCameraEnabled)
        {
            result.closeUpFrames = std::max(result.closeUpFrames, context.battleEndZoomFrames);
        }
        result.frozenFrames = 60;
        result.slowFrames = context.battleEndSlowFrames;
        result.sceneShake = 60;
    }
}

void BattleSceneFrameStateApplier::applyTeamEffectState(
    const std::vector<KysChess::Battle::BattleTeamEffectEvent>& events,
    BattleSceneFrameStateApplyContext& context) const
{
    for (const auto& event : events)
    {
        auto& unit = context.units->requireUnit(event.targetUnitId);
        switch (event.type)
        {
        case KysChess::Battle::BattleTeamEffectEventType::Heal:
            unit.vitals.hp = event.after;
            break;
        case KysChess::Battle::BattleTeamEffectEventType::MpRestore:
            unit.vitals.mp = event.after;
            break;
        case KysChess::Battle::BattleTeamEffectEventType::ShieldGain:
            unit.combo.shield = event.after;
            break;
        case KysChess::Battle::BattleTeamEffectEventType::CooldownReduced:
            unit.animation.cooldown = event.after;
            break;
        }
    }
}

void BattleSceneFrameStateApplier::applyFrameApplications(
    const KysChess::Battle::BattleFrameApplications& applications,
    BattleSceneFrameStateApplyContext& context,
    BattleSceneFrameStateApplyResult& result) const
{
    for (const auto& knockback : applications.knockbacks)
    {
        auto& targetUnit = context.units->requireUnit(knockback.targetUnitId);
        targetUnit.motion.velocity += knockback.velocityDelta;
        if (knockback.velocityCap > 0.0 && targetUnit.motion.velocity.norm() > knockback.velocityCap)
        {
            targetUnit.motion.velocity.normTo(static_cast<float>(knockback.velocityCap));
        }
    }
    for (const auto& restore : applications.mpRestores)
    {
        auto& unit = context.units->requireUnit(restore.unitId);
        unit.vitals.mp = std::min(unit.vitals.maxMp, unit.vitals.mp + restore.amount);
    }
    for (const auto& heal : applications.unitHeals)
    {
        auto& unit = context.units->requireUnit(heal.targetUnitId);
        unit.vitals.hp = std::min(unit.vitals.maxHp, unit.vitals.hp + heal.amount);
    }
    result.attackSoundIds.insert(
        result.attackSoundIds.end(),
        applications.attackSoundIds.begin(),
        applications.attackSoundIds.end());
    result.rumbles.insert(
        result.rumbles.end(),
        applications.rumbles.begin(),
        applications.rumbles.end());
}

void BattleSceneFrameStateApplier::applyMovementPresentation(
    const std::vector<KysChess::Battle::BattleFrameMovementPresentationUnitResult>& movements,
    BattleSceneFrameStateApplyContext& context) const
{
    for (const auto& movement : movements)
    {
        auto& unit = context.units->requireUnit(movement.unitId);
        unit.frozen = movement.frozenFrames;
        unit.motion.position = movement.position;
        unit.motion.velocity = movement.velocity;
        unit.motion.acceleration = movement.acceleration;
        auto facing = movement.facing;
        if (facing.norm() > 0.01)
        {
            unit.motion.facing = facing;
        }
    }
}

void BattleSceneFrameStateApplier::applyActionResults(
    const std::vector<KysChess::Battle::BattleFrameActionUnitResult>& actions,
    BattleSceneFrameStateApplyContext& context,
    BattleSceneFrameStateApplyResult& result) const
{
    for (const auto& action : actions)
    {
        auto& unit = context.units->requireUnit(action.unitId);
        unit.animation.actFrame = action.state.actFrame;
        unit.animation.actType = action.state.actType;
        unit.animation.cooldown = action.state.cooldown;
        if (action.castStarted)
        {
            unit.animation.actFrame = 0;
            unit.animation.actType = action.state.actType;
            unit.animation.cooldown = action.state.cooldown;
            unit.animation.cooldownMax = action.state.cooldown;
            unit.motion.velocity = { 0, 0, 0 };
        }
        for (const auto& teleport : action.actionResult.blinkTeleports)
        {
            assert(context.blinkSoundEffectId >= 0);
            unit.gridX = teleport.gridX;
            unit.gridY = teleport.gridY;
            unit.motion.position = { teleport.position.x, teleport.position.y, 0 };
            unit.motion.velocity = { 0, 0, 0 };
            unit.motion.acceleration = { 0, 0, static_cast<float>(context.gravity) };
            unit.motion.facing = teleport.facing;
            result.effectSoundIds.push_back(context.blinkSoundEffectId);
        }
    }
}
