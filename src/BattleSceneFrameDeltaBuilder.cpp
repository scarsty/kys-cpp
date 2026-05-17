#include "BattleSceneFrameDeltaBuilder.h"

#include "ChessCombo.h"
#include "battle/BattleLogSegments.h"

#include <algorithm>
#include <cassert>
#include <format>
#include <set>

namespace
{
KysChess::Battle::BattleLogEvent makeAntiComboTransferLog(const KysChess::AntiComboTransferEvent& event)
{
    KysChess::Battle::BattleLogEvent log;
    log.type = KysChess::Battle::BattleLogEventType::Status;
    log.sourceUnitId = event.sourceUnitId;
    log.targetUnitId = event.targetUnitId;
    log.segments = KysChess::Battle::battleLogText("獨行轉移", KysChess::Battle::BattleLogTextTone::SkillName);
    return log;
}

struct BattleLifecycleSceneEffects
{
    bool battleEnded = false;
    int battleResult = -1;
    bool unitDied = false;
    std::vector<int> diedUnitIds;
};

BattleLifecycleSceneEffects collectBattleLifecycleSceneEffects(
    int currentBattleResult,
    const std::vector<KysChess::Battle::BattleGameplayEvent>& events)
{
    BattleLifecycleSceneEffects result;
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

BattleSceneFrameDelta BattleSceneFrameDeltaBuilder::build(
    const KysChess::Battle::BattleFrameResult& frameResult,
    int currentBattleResult,
    BattleSceneFrameDeltaBuildContext context) const
{
    assert(context.units);

    BattleSceneFrameDelta result;
    collectDamageSceneEffects(frameResult, currentBattleResult, context, result);
    collectFrameApplicationSceneEffects(frameResult.applications, result);
    collectActionSceneEffects(frameResult.actionResults, context, result);
    return result;
}

void BattleSceneFrameDeltaBuilder::collectDamageSceneEffects(
    const KysChess::Battle::BattleFrameResult& frameResult,
    int currentBattleResult,
    BattleSceneFrameDeltaBuildContext& context,
    BattleSceneFrameDelta& result) const
{
    auto lifecycleEffects = collectBattleLifecycleSceneEffects(
        currentBattleResult,
        frameResult.frame.gameplayEvents);
    result.unitDied = lifecycleEffects.unitDied;
    result.diedUnitIds = lifecycleEffects.diedUnitIds;
    std::set<int> diedUnitIds(lifecycleEffects.diedUnitIds.begin(), lifecycleEffects.diedUnitIds.end());

    for (const auto& damage : frameResult.damageRenderApplications)
    {
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
            for (const auto& event : context.transferAntiCombo(damage.defender.unitId))
            {
                result.logEvents.push_back(makeAntiComboTransferLog(event));
            }

            const auto& defenderUnit = context.units->requireRuntimeUnit(damage.defender.unitId);
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
        assert(rescue.teleport->unitId >= 0);
        assert(rescue.teleport->pullerUnitId >= 0);
    }

    if (lifecycleEffects.battleEnded)
    {
        result.battleEnded = true;
        result.battleResult = lifecycleEffects.battleResult;
        if (!context.manualCameraEnabled)
        {
            result.closeUpFrames = std::max(result.closeUpFrames, context.battleEndZoomFrames);
        }
        result.frozenFrames = 60;
        result.slowFrames = context.battleEndSlowFrames;
        result.sceneShake = 60;
    }
}

void BattleSceneFrameDeltaBuilder::collectFrameApplicationSceneEffects(
    const KysChess::Battle::BattleFrameApplications& applications,
    BattleSceneFrameDelta& result) const
{
    result.attackSoundIds.insert(
        result.attackSoundIds.end(),
        applications.attackSoundIds.begin(),
        applications.attackSoundIds.end());
    result.rumbles.insert(
        result.rumbles.end(),
        applications.rumbles.begin(),
        applications.rumbles.end());
}

void BattleSceneFrameDeltaBuilder::collectActionSceneEffects(
    const std::vector<KysChess::Battle::BattleFrameActionUnitResult>& actions,
    const BattleSceneFrameDeltaBuildContext& context,
    BattleSceneFrameDelta& result) const
{
    for (const auto& action : actions)
    {
        for (const auto& teleport : action.actionResult.blinkTeleports)
        {
            assert(context.blinkSoundEffectId >= 0);
            result.effectSoundIds.push_back(context.blinkSoundEffectId);
        }
    }
}