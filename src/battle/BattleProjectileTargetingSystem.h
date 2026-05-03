#pragma once

#include <vector>

namespace KysChess::Battle
{

struct BattleProjectileTargetUnit
{
    int id = -1;
    int team = 0;
    bool alive = true;
    double x = 0.0;
    double y = 0.0;
    int gridX = 0;
    int gridY = 0;
};

struct BattleProjectileTargetWorld
{
    std::vector<BattleProjectileTargetUnit> units;
};

class BattleProjectileTargetingSystem
{
public:
    std::vector<int> selectNearbyTargets(const BattleProjectileTargetWorld& world,
                                         int attackerUnitId,
                                         int centerUnitId,
                                         double radius) const;

    std::vector<int> selectAreaImpactTargets(const BattleProjectileTargetWorld& world,
                                             int originUnitId,
                                             int areaSize,
                                             int maxTargets,
                                             int trackedTargetUnitId) const;

private:
    const BattleProjectileTargetUnit& unitById(const BattleProjectileTargetWorld& world, int unitId) const;
    double distanceSquared(const BattleProjectileTargetUnit& lhs, const BattleProjectileTargetUnit& rhs) const;
    bool withinGridArea(const BattleProjectileTargetUnit& origin,
                        const BattleProjectileTargetUnit& target,
                        int width,
                        int height) const;
};

}  // namespace KysChess::Battle
