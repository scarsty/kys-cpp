#pragma once

#include "BattleAttackSystem.h"
#include "BattleMovement.h"
#include "BattlePresentation.h"

namespace KysChess::Battle
{

class BattleCore
{
public:
    explicit BattleCore(BattleWorldState& world);

    BattleTickResult tickMovement();

private:
    BattleWorldState& world_;
};

struct BattleFrameState
{
    BattleWorldState world;
    BattleAttackWorld attacks;
};

struct BattleFrameResult
{
    BattlePresentationFrame frame;
    BattleTickResult movement;
    std::vector<BattleAttackEvent> attackEvents;
};

class BattleFrameRunner
{
public:
    BattleFrameResult advanceFrame(BattleFrameState& state) const;
};

}  // namespace KysChess::Battle
