#include "BattleUnitStore.h"

#include "BattleMath.h"
#include "BattleRuntimeUnits.h"

#include <cassert>

namespace KysChess::Battle
{

Point BattleGridTransform::toGrid(Pointf position) const
{
    assert(tileWidth > 0.0);
    assert(coordCount > 0);
    return battleIsometricGridPosition(position, coordCount, tileWidth);
}

int findNearestEnemyUnitId(const BattleRuntimeUnits& units, int sourceUnitId)
{
    const auto& source = units.requireCore(sourceUnitId);
    int targetUnitId = -1;
    std::uint64_t bestDistanceSquared{};
    for (const auto& candidateRecord : units.live())
    {
        const auto& candidate = candidateRecord.core;
        if (candidate.team == source.team)
        {
            continue;
        }

        const std::uint64_t distanceSquared = battleDistanceSquared3d(
            candidate.motion.position,
            source.motion.position);
        if (targetUnitId < 0
            || distanceSquared < bestDistanceSquared
            || (distanceSquared == bestDistanceSquared && candidate.id < targetUnitId))
        {
            targetUnitId = candidate.id;
            bestDistanceSquared = distanceSquared;
        }
    }
    return targetUnitId;
}

int findFarthestEnemyUnitId(const BattleRuntimeUnits& units, int sourceUnitId)
{
    const auto& source = units.requireCore(sourceUnitId);
    int targetUnitId = -1;
    std::uint64_t bestDistanceSquared{};
    for (const auto& candidateRecord : units.live())
    {
        const auto& candidate = candidateRecord.core;
        if (candidate.team == source.team)
        {
            continue;
        }

        const std::uint64_t distanceSquared = battleDistanceSquared3d(
            candidate.motion.position,
            source.motion.position);
        if (targetUnitId < 0
            || distanceSquared > bestDistanceSquared
            || (distanceSquared == bestDistanceSquared && candidate.id < targetUnitId))
        {
            targetUnitId = candidate.id;
            bestDistanceSquared = distanceSquared;
        }
    }
    return targetUnitId;
}

BattleUnitState makeBattleMovementPlanUnit(const BattleRuntimeUnit& runtimeUnit, double moveSpeedDivisor)
{
    assert(moveSpeedDivisor != 0.0);

    BattleUnitState unit;
    unit.id = runtimeUnit.id;
    unit.team = runtimeUnit.team;
    unit.alive = runtimeUnit.alive;
    unit.position = runtimeUnit.motion.position;
    unit.velocity = runtimeUnit.motion.velocity;
    unit.speed = runtimeUnit.stats.speed / moveSpeedDivisor;
    unit.canAttack = runtimeUnit.animation.cooldown == 0;
    unit.reach = runtimeUnit.reach;
    unit.style = runtimeUnit.style;
    return unit;
}

}  // namespace KysChess::Battle
