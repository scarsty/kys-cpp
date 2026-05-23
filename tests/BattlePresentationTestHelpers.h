#pragma once

#include "battle/BattlePresentation.h"

#include <vector>

namespace BattlePresentationTest
{
inline std::vector<KysChess::Battle::BattleLogEvent> damageLogsFor(
    const KysChess::Battle::BattlePresentationFrame& frame,
    int targetUnitId = -1)
{
    std::vector<KysChess::Battle::BattleLogEvent> result;
    for (const auto& event : frame.logEvents)
    {
        if (event.type == KysChess::Battle::BattleLogEventType::Damage
            && event.amount > 0
            && (targetUnitId < 0 || event.targetUnitId == targetUnitId))
        {
            result.push_back(event);
        }
    }
    return result;
}

inline std::vector<int> damageLogAmountsFor(
    const KysChess::Battle::BattlePresentationFrame& frame,
    int targetUnitId = -1)
{
    std::vector<int> result;
    for (const auto& event : damageLogsFor(frame, targetUnitId))
    {
        result.push_back(event.amount);
    }
    return result;
}

inline std::vector<int> damageLogSourceIdsFor(
    const KysChess::Battle::BattlePresentationFrame& frame,
    int targetUnitId = -1)
{
    std::vector<int> result;
    for (const auto& event : damageLogsFor(frame, targetUnitId))
    {
        result.push_back(event.sourceUnitId);
    }
    return result;
}

inline std::vector<KysChess::Battle::BattleGameplayEvent> gameplayEventsFor(
    const KysChess::Battle::BattlePresentationFrame& frame,
    KysChess::Battle::BattleGameplayEventType type,
    int targetUnitId = -1)
{
    std::vector<KysChess::Battle::BattleGameplayEvent> result;
    for (const auto& event : frame.gameplayEvents)
    {
        if (event.type == type
            && (targetUnitId < 0 || event.targetUnitId == targetUnitId))
        {
            result.push_back(event);
        }
    }
    return result;
}

inline std::vector<KysChess::Battle::BattleVisualEvent> visualEventsFor(
    const KysChess::Battle::BattlePresentationFrame& frame,
    KysChess::Battle::BattleVisualEventType type,
    int targetUnitId = -1)
{
    std::vector<KysChess::Battle::BattleVisualEvent> result;
    for (const auto& event : frame.visualEvents)
    {
        if (event.type == type
            && (targetUnitId < 0 || event.targetUnitId == targetUnitId))
        {
            result.push_back(event);
        }
    }
    return result;
}
}
