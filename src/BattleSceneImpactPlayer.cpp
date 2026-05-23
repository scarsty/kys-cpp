#include "BattleSceneImpactPlayer.h"

#include <cassert>

std::vector<BattleSceneImpactCommand> BattleSceneImpactPlanner::plan(
    const std::vector<KysChess::Battle::BattleVisualEvent>& events) const
{
    std::vector<BattleSceneImpactCommand> commands;
    for (const auto& event : events)
    {
        if (event.type != KysChess::Battle::BattleVisualEventType::ProjectileHit)
        {
            continue;
        }

        BattleSceneImpactCommand command;
        command.unitId = event.targetUnitId;
        command.effectSoundId = event.impactEffectSoundId;
        command.unitShake = event.impactUnitShake;
        command.sceneShake = event.impactSceneShake;
        command.rumble = event.impactRumble;
        commands.push_back(command);
    }
    return commands;
}

void BattleSceneImpactPlayer::play(
    const KysChess::Battle::BattlePresentationFrame& frame,
    const Bindings& bindings) const
{
    assert(bindings.units);
    assert(bindings.sceneShake);

    for (const auto& command : planner_.plan(frame.visualEvents))
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
