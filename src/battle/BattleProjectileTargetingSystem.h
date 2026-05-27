#pragma once

#include <vector>

namespace KysChess::Battle
{

struct BattleRuntimeUnit;
class BattleRuntimeUnits;

class BattleProjectileTargetingSystem
{
public:
    std::vector<int> selectNearbyTargets(const BattleRuntimeUnits& units,
                                         int attackerUnitId,
                                         int centerUnitId,
                                         double radius) const;

    std::vector<int> selectAreaImpactTargets(const BattleRuntimeUnits& units,
                                             int originUnitId,
                                             int areaSize,
                                             int maxTargets,
                                             int trackedTargetUnitId) const;

    int selectRandomEnemy(const BattleRuntimeUnits& units,
                          int sourceTeam,
                          int randomIndex) const;

    int selectWeakestVulnerableEnemy(const BattleRuntimeUnits& units,
                                     int sourceTeam,
                                     double defenseWeight) const;

private:
    double distanceSquared(const BattleRuntimeUnit& lhs, const BattleRuntimeUnit& rhs) const;
    bool withinGridArea(const BattleRuntimeUnit& origin,
                        const BattleRuntimeUnit& target,
                        int width,
                        int height) const;
};

}  // namespace KysChess::Battle
