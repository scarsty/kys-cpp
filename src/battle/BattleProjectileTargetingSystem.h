#pragma once

#include <vector>

namespace KysChess::Battle
{

struct BattleRuntimeUnit;
class BattleRuntimeUnitRecords;

class BattleProjectileTargetingSystem
{
public:
    std::vector<int> selectNearbyTargets(const BattleRuntimeUnitRecords& units,
                                         int attackerUnitId,
                                         int centerUnitId,
                                         double radius) const;

    std::vector<int> selectAreaImpactTargets(const BattleRuntimeUnitRecords& units,
                                             int originUnitId,
                                             int areaSize,
                                             int maxTargets,
                                             int trackedTargetUnitId) const;

    int selectRandomEnemy(const BattleRuntimeUnitRecords& units,
                          int sourceTeam,
                          int randomIndex) const;

    int selectWeakestVulnerableEnemy(const BattleRuntimeUnitRecords& units,
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
