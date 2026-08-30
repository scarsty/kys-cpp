#pragma once

#include "BattleGeometry.h"
#include "BattleMovementPhysics.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory_resource>
#include <vector>

namespace KysChess::Battle
{

bool battleMovementTaXueUnstable(const BattleUnitState& unit);

BattleMovementTerrainLayout makeBattleMovementTerrainLayout(
    const std::vector<BattleTerrainCell>& terrainCells,
    double tileWidth);

class BattleMovementPlanner
{
public:
    explicit BattleMovementPlanner(BattleMovementPlanInput world);

    BattleTickResult tick();
    MoveProbe probeMove(const BattleUnitState& unit,
                        Pointf nextPosition,
                        bool ignoreUnits,
                        const std::map<int, Pointf>& reservations = {}) const;

private:
    BattleMovementPathState localPathState_;
    BattleMovementPlanInput world_;
};

}  // namespace KysChess::Battle
