#pragma once

#include "BattleAttackSystem.h"
#include "BattleCastSystem.h"
#include "BattleDamageQueue.h"
#include "BattleDamageSystem.h"
#include "BattleHitResolver.h"
#include "BattleMovement.h"
#include "BattlePresentation.h"
#include "BattleRescueRepositionSystem.h"
#include "BattleRuntimeActions.h"
#include "BattleRuntimeQueues.h"
#include "BattleRuntimeRandom.h"
#include "BattleRuntimeUnits.h"
#include "BattleStatusSystem.h"
#include "BattleUnitStore.h"
#include "BattleUnitValues.h"

#include <cassert>
#include <cstddef>
#include <vector>

namespace KysChess::Battle
{

void cancelBattleRuntimeForBattleEnd(BattleRuntimeState& runtime, int frame);

class BattleFrameRunner
{
public:
    BattleFrameRunner();

    BattlePresentationFrame runFrame(BattleRuntimeState& runtime) const;
    BattlePresentationFrame runFrame(
        BattleRuntimeState& runtime,
        BattlePresentationFrame recycledPresentation) const;

private:
    static constexpr std::size_t FrameMemoryBytes = 256 * 1024;
    mutable std::vector<std::byte> frameMemoryStorage_;
};

}  // namespace KysChess::Battle
