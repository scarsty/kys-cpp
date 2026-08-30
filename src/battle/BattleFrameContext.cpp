#include "BattleFrameContext.h"

#include <cassert>

namespace KysChess::Battle
{

const BattleUnitMotion& motionSnapshotForUnit(
    const UnitMotionSnapshotList& snapshots,
    const BattleRuntimeUnit& fallback)
{
    const auto snapshotIt = std::find_if(
        snapshots.begin(),
        snapshots.end(),
        [&](const UnitMotionSnapshot& snapshot)
        {
            return snapshot.unitId == fallback.id;
        });
    if (snapshotIt != snapshots.end())
    {
        return snapshotIt->motion;
    }
    return fallback.motion;
}

BattlePresentationFrame consumeBattleFrameContext(BattleFrameContext&& frame)
{
    assert(frame.drainCommands().empty());
    return std::move(frame.result);
}

UnitMotionSnapshotList makeUnitMotionSnapshot(
    const BattleRuntimeUnits& units,
    std::pmr::memory_resource* frameMemoryResource)
{
    UnitMotionSnapshotList snapshots(frameMemoryResource);
    snapshots.reserve(units.size());
    for (const auto& record : units.all())
    {
        const auto& unit = record.core;
        snapshots.push_back({ unit.id, unit.motion });
    }
    return snapshots;
}


}  // namespace KysChess::Battle
