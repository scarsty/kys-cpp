#include "BattleRuntimeUnits.h"

#include "BattleResourceRules.h"

#include <algorithm>
#include <cassert>

namespace KysChess::Battle
{

BattleRuntimeUnitFrameTickResult BattleRuntimeUnitRecord::advanceFrameTick(
    const BattleRuntimeUnitFrameTickConfig& config)
{
    assert(config.frame >= 0);
    assert(config.mpRegenIntervalFrames > 0);
    assert(config.physicalPowerRegenIntervalFrames > 0);
    assert(core.animation.cooldown >= 0);
    assert(core.physicalPower >= 0);

    BattleRuntimeUnitFrameTickResult result;

    const int previousCooldown = core.animation.cooldown;
    if (!frozen() && core.animation.cooldown > 0)
    {
        --core.animation.cooldown;
    }
    result.skillFinished = previousCooldown > 0 && core.animation.cooldown == 0;

    if (core.animation.cooldown == 0)
    {
        core.animation.cooldownMax = 0;
        if (config.frame % config.physicalPowerRegenIntervalFrames == 0)
        {
            ++core.physicalPower;
        }
        core.animation.actFrame = 0;
        if (core.operationType == BattleOperationType::Dash)
        {
            core.motion.velocity = { 0, 0, 0 };
        }
        core.operationType = BattleOperationType::None;
        core.animation.actType = -1;
        core.haveAction = false;
    }

    if (config.frame % config.mpRegenIntervalFrames == 0)
    {
        core.vitals.mp += adjustedMpRestore(mpBlocked(), sumAlways(EffectType::MPRecoveryBonus), 1);
        core.vitals.mp = std::clamp(core.vitals.mp, 0, core.vitals.maxMp);
    }

    return result;
}

}  // namespace KysChess::Battle
