#pragma once

#include "BattleAttackSystem.h"
#include "BattleCastSystem.h"
#include "BattleComboTriggerSystem.h"
#include "BattleDamageQueue.h"
#include "BattleDamageSystem.h"
#include "BattleDeathEffectSystem.h"
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
#include "BattleTeamEffectSystem.h"
#include "BattleUnitValues.h"

#include <cassert>
#include <cstddef>
#include <vector>

namespace KysChess::Battle
{

struct BattleUnitFrameTickState
{
    int cooldown = 0;
    int actFrame = 0;
    int actType = -1;
    BattleOperationType operationType = BattleOperationType::None;
    bool haveAction = false;
    int physicalPower = 0;
};

struct BattleUnitFrameTickInput
{
    BattleUnitFrameTickState state;
    int frame = 0;
    bool frozen = false;
    int mpRegenIntervalFrames = 0;
    int physicalPowerRegenIntervalFrames = 0;
};

struct BattleUnitFrameTickResult
{
    BattleUnitFrameTickState state;
    int mpDelta = 0;
    bool skillFinished = false;
    bool resetDashVelocity = false;
};

class BattleUnitFrameTickSystem
{
public:
    BattleUnitFrameTickResult advance(const BattleUnitFrameTickInput& input) const;
};

class BattleFrameRunner
{
public:
    BattlePresentationFrame runFrame(BattleRuntimeState& runtime) const;
};

}  // namespace KysChess::Battle
