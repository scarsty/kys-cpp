#include "BattleProjectileTargetingSystem.h"

#include <algorithm>
#include <cassert>

namespace KysChess::Battle
{

const BattleProjectileTargetUnit& BattleProjectileTargetingSystem::unitById(
    const BattleProjectileTargetWorld& world,
    int unitId) const
{
    auto it = std::find_if(world.units.begin(), world.units.end(), [&](const BattleProjectileTargetUnit& unit)
        {
            return unit.id == unitId;
        });
    assert(it != world.units.end());
    return *it;
}

double BattleProjectileTargetingSystem::distanceSquared(const BattleProjectileTargetUnit& lhs,
                                                        const BattleProjectileTargetUnit& rhs) const
{
    double dx = lhs.x - rhs.x;
    double dy = lhs.y - rhs.y;
    return dx * dx + dy * dy;
}

bool BattleProjectileTargetingSystem::withinGridArea(const BattleProjectileTargetUnit& origin,
                                                     const BattleProjectileTargetUnit& target,
                                                     int width,
                                                     int height) const
{
    assert(width > 0);
    assert(height > 0);
    return std::abs(origin.gridX - target.gridX) <= width
        && std::abs(origin.gridY - target.gridY) <= height;
}

std::vector<int> BattleProjectileTargetingSystem::selectNearbyTargets(
    const BattleProjectileTargetWorld& world,
    int attackerUnitId,
    int centerUnitId,
    double radius) const
{
    assert(radius > 0.0);

    const auto& attacker = unitById(world, attackerUnitId);
    const auto& center = unitById(world, centerUnitId);
    assert(attacker.alive);
    assert(center.alive);

    double radiusSquared = radius * radius;
    std::vector<const BattleProjectileTargetUnit*> targets;
    for (const auto& unit : world.units)
    {
        if (!unit.alive || unit.team == attacker.team)
        {
            continue;
        }
        if (distanceSquared(center, unit) <= radiusSquared)
        {
            targets.push_back(&unit);
        }
    }

    std::stable_sort(targets.begin(), targets.end(), [&](const auto* lhs, const auto* rhs)
        {
            return distanceSquared(center, *lhs) < distanceSquared(center, *rhs);
        });

    std::vector<int> ids;
    for (const auto* target : targets)
    {
        ids.push_back(target->id);
    }
    return ids;
}

std::vector<int> BattleProjectileTargetingSystem::selectAreaImpactTargets(
    const BattleProjectileTargetWorld& world,
    int originUnitId,
    int areaSize,
    int maxTargets,
    int trackedTargetUnitId) const
{
    assert(areaSize > 0);
    assert(maxTargets >= 0);

    const auto& origin = unitById(world, originUnitId);
    assert(origin.alive);

    std::vector<const BattleProjectileTargetUnit*> targets;
    for (const auto& unit : world.units)
    {
        if (!unit.alive || unit.id == origin.id || unit.team == origin.team)
        {
            continue;
        }
        if (withinGridArea(origin, unit, areaSize, areaSize))
        {
            targets.push_back(&unit);
        }
    }

    std::stable_sort(targets.begin(), targets.end(), [&](const auto* lhs, const auto* rhs)
        {
            return distanceSquared(origin, *lhs) < distanceSquared(origin, *rhs);
        });

    if (maxTargets > 0 && static_cast<int>(targets.size()) > maxTargets)
    {
        targets.resize(maxTargets);
    }

    std::vector<int> ids;
    bool hasTracked = false;
    for (const auto* target : targets)
    {
        ids.push_back(target->id);
        hasTracked = hasTracked || target->id == trackedTargetUnitId;
    }

    if (trackedTargetUnitId >= 0 && !hasTracked)
    {
        const auto& tracked = unitById(world, trackedTargetUnitId);
        if (tracked.alive && tracked.team != origin.team)
        {
            ids.push_back(tracked.id);
        }
    }
    return ids;
}

}  // namespace KysChess::Battle
