#include "BattleProjectileTargetingSystem.h"

#include "BattleRuntimeUnits.h"

#include <algorithm>
#include <cassert>

namespace KysChess::Battle
{

double BattleProjectileTargetingSystem::distanceSquared(const BattleRuntimeUnit& lhs,
                                                        const BattleRuntimeUnit& rhs) const
{
    double dx = lhs.motion.position.x - rhs.motion.position.x;
    double dy = lhs.motion.position.y - rhs.motion.position.y;
    return dx * dx + dy * dy;
}

bool BattleProjectileTargetingSystem::withinGridArea(const BattleRuntimeUnit& origin,
                                                     const BattleRuntimeUnit& target,
                                                     int width,
                                                     int height) const
{
    assert(width > 0);
    assert(height > 0);
    return std::abs(origin.grid.x - target.grid.x) <= width
        && std::abs(origin.grid.y - target.grid.y) <= height;
}

std::vector<int> BattleProjectileTargetingSystem::selectNearbyTargets(
    const BattleRuntimeUnits& units,
    int attackerUnitId,
    int centerUnitId,
    double radius) const
{
    assert(radius > 0.0);

    const auto& attacker = units.requireCore(attackerUnitId);
    const auto& center = units.requireCore(centerUnitId);

    double radiusSquared = radius * radius;
    std::vector<const BattleRuntimeUnit*> targets;
    for (const auto& unitRecord : units.live())
    {
        const auto& unit = unitRecord.core;
        if (unit.team == attacker.team)
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
    const BattleRuntimeUnits& units,
    int originUnitId,
    int areaSize,
    int maxTargets,
    int trackedTargetUnitId) const
{
    assert(areaSize > 0);
    assert(maxTargets >= 0);

    const auto& origin = units.requireCore(originUnitId);

    std::vector<const BattleRuntimeUnit*> targets;
    for (const auto& unitRecord : units.live())
    {
        const auto& unit = unitRecord.core;
        if (unit.id == origin.id || unit.team == origin.team)
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
        const auto& tracked = units.requireCore(trackedTargetUnitId);
        if (tracked.alive && tracked.team != origin.team)
        {
            ids.push_back(tracked.id);
        }
    }
    return ids;
}

int BattleProjectileTargetingSystem::selectRandomEnemy(
    const BattleRuntimeUnits& units,
    int sourceTeam,
    int randomIndex) const
{
    assert(randomIndex >= 0);

    std::vector<int> ids;
    for (const auto& unitRecord : units.live())
    {
        const auto& unit = unitRecord.core;
        if (unit.team != sourceTeam)
        {
            ids.push_back(unit.id);
        }
    }
    if (ids.empty())
    {
        return -1;
    }
    return ids[randomIndex % ids.size()];
}

int BattleProjectileTargetingSystem::selectWeakestVulnerableEnemy(
    const BattleRuntimeUnits& units,
    int sourceTeam,
    double defenseWeight) const
{
    assert(defenseWeight >= 0.0);

    const BattleRuntimeUnit* weakest = nullptr;
    double weakestEffectiveHp = 0.0;
    for (const auto& unitRecord : units.live())
    {
        const auto& unit = unitRecord.core;
        if (unit.team == sourceTeam || unit.invincible > 0)
        {
            continue;
        }
        const double effectiveHp = unit.vitals.maxHp + unit.stats.defence * defenseWeight;
        if (!weakest || effectiveHp < weakestEffectiveHp)
        {
            weakest = &unit;
            weakestEffectiveHp = effectiveHp;
        }
    }
    return weakest ? weakest->id : -1;
}

}  // namespace KysChess::Battle
