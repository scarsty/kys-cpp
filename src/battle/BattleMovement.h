#pragma once

#include "BattleGeometry.h"

#include <map>

namespace KysChess::Battle
{

class BattleMovementPlanner
{
public:
    explicit BattleMovementPlanner(BattleWorldState& world);

    BattleTickResult tick();
    MoveProbe probeMove(const BattleUnitState& unit,
                        Pointf nextPosition,
                        bool ignoreUnits,
                        const std::map<int, Pointf>& reservations = {}) const;

private:
    BattleWorldState& world_;
};

class BattleMovementSimulator
{
public:
    explicit BattleMovementSimulator(BattleWorldState world);

    MovementRunResult run(int frames, unsigned int seed);

private:
    BattleWorldState world_;
};

}  // namespace KysChess::Battle
