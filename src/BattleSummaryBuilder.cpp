#include "BattleSummaryBuilder.h"

#include <algorithm>

namespace KysChess
{

BattleSummary BattleSummaryBuilder::build(
    const Battle::BattleRuntimeSession& session,
    const BattleReport& report)
{
    BattleSummary result;
    result.outcome = session.runtime().result.outcome;
    result.endFrame = session.runtime().result.endedFrame;
    for (const auto& unit : session.runtimeUnits())
    {
        if (!unit.alive)
        {
            continue;
        }
        result.survivors.push_back({
            unit.id,
            unit.chessInstanceId,
            unit.realRoleId,
            unit.team,
            unit.vitals.hp,
            unit.vitals.mp,
        });
    }
    std::ranges::sort(result.survivors, {}, &BattleSurvivorSummary::unitId);
    return result;
}

}
