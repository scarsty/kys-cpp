#include "BattleSceneImpactPlayer.h"

#include "battle/BattleCore.h"

#include <cassert>

namespace
{
bool hasScriptedImpact(const KysChess::Battle::BattleAttackEvent& event)
{
    return event.scriptedDamage > 0 || event.scriptedStunFrames > 0 || event.scriptedBleedStacks > 0;
}
}  // namespace

std::vector<BattleSceneImpactCommand> BattleSceneImpactPlanner::plan(
    const std::vector<KysChess::Battle::BattleAttackEvent>& events) const
{
    std::vector<BattleSceneImpactCommand> commands;
    for (const auto& event : events)
    {
        if (event.type != KysChess::Battle::BattleAttackEventType::Hit)
        {
            continue;
        }

        BattleSceneImpactCommand command;
        command.unitId = event.unitId;
        command.effectSoundId = event.skillEffectId;
        if (hasScriptedImpact(event))
        {
            command.unitShake = 5;
        }
        else
        {
            command.sceneShake = event.ultimate ? 10 : 0;
            command.unitShake = event.ultimate ? 10 : 5;
            command.rumble = event.operationType != KysChess::Battle::BattleOperationType::None;
        }
        commands.push_back(command);
    }
    return commands;
}

void BattleSceneImpactPlayer::play(
    const std::vector<KysChess::Battle::BattleAttackEvent>& events,
    const Bindings& bindings) const
{
    assert(bindings.units);
    assert(bindings.sceneShake);

    for (const auto& command : planner_.plan(events))
    {
        bindings.units->setUnitShake(command.unitId, command.unitShake);
        if (command.effectSoundId >= 0)
        {
            assert(bindings.playEffectSound);
            bindings.playEffectSound(command.effectSoundId);
        }
        if (command.sceneShake > 0)
        {
            *bindings.sceneShake = command.sceneShake;
        }
        if (command.rumble)
        {
            assert(bindings.rumble);
            bindings.rumble(100, 100, 50);
        }
    }
}

void BattleSceneImpactPlayer::play(
    const KysChess::Battle::BattleFrameResult& frameResult,
    const Bindings& bindings) const
{
    play(frameResult.attackEvents, bindings);
}
