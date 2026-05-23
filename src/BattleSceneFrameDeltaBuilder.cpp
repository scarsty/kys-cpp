#include "BattleSceneFrameDeltaBuilder.h"

#include <algorithm>
#include <cassert>
#include <format>
#include <set>

namespace
{
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
    const KysChess::Battle::BattlePresentationFrame& frame,
    const KysChess::Battle::BattleFrameApplications& applications,
    int currentBattleResult,
    BattleSceneFrameDeltaBuildContext context) const
{
    assert(context.units);

    BattleSceneFrameDelta result;
    collectDamageSceneEffects(frame, currentBattleResult, context, result);
    collectFrameApplicationSceneEffects(applications, context, result);
    return result;
}

void BattleSceneFrameDeltaBuilder::collectDamageSceneEffects(
    const KysChess::Battle::BattlePresentationFrame& frame,
    int currentBattleResult,
    BattleSceneFrameDeltaBuildContext& context,
    BattleSceneFrameDelta& result) const
{
    auto lifecycleEffects = collectBattleLifecycleSceneEffects(
        currentBattleResult,
        frame.gameplayEvents);
    result.unitDied = lifecycleEffects.unitDied;
    result.diedUnitIds = lifecycleEffects.diedUnitIds;
    std::set<int> diedUnitIds(lifecycleEffects.diedUnitIds.begin(), lifecycleEffects.diedUnitIds.end());

    std::set<int> damagedUnitIds;
    for (const auto& log : frame.logEvents)
    {
        if (log.type == KysChess::Battle::BattleLogEventType::Damage
            && log.amount > 0
            && log.targetUnitId >= 0)
        {
            damagedUnitIds.insert(log.targetUnitId);
        }
    }

    for (const int unitId : damagedUnitIds)
    {
        assert(context.random);
        result.bloodEffects.push_back({
            unitId,
            std::format("eft/bld{:03}", int(context.random->rand() * 5)),
        });
        result.hurtFlashes.push_back({
            unitId,
            context.hurtFlashDuration,
        });
    }

    for (const int unitId : diedUnitIds)
    {
        assert(context.random);
        const auto& defenderUnit = context.units->requireRuntimeUnit(unitId);
        result.jitterX = context.random->rand_int(2) - context.random->rand_int(2);
        result.jitterY = context.random->rand_int(2) - context.random->rand_int(2);
        if (!context.manualCameraEnabled)
        {
            auto cameraFocus = defenderUnit.motion.position;
            cameraFocus.y -= defenderUnit.motion.position.z * 2.0f;
            result.cameraFocus = cameraFocus;
            result.closeUpFrames = std::max(result.closeUpFrames, context.deathZoomFrames);
        }
        result.frozenFrames = 5;
        result.sceneShake = 10;
        result.slowFrames = context.deathSlowFrames;
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
    const BattleSceneFrameDeltaBuildContext& context,
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
    for (int i = 0; i < applications.blinkSoundCount; ++i)
    {
        assert(context.blinkSoundEffectId >= 0);
        result.effectSoundIds.push_back(context.blinkSoundEffectId);
    }
}
