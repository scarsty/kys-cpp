#pragma once

#include "battle/BattlePresentation.h"
#include "battle/BattleUnitStore.h"

#include <string>
#include <utility>
#include <vector>

namespace BattleLogTest
{
inline std::string joinSegments(const std::vector<KysChess::Battle::BattleLogTextSegment>& segments)
{
    std::string text;
    for (const auto& segment : segments)
    {
        text += segment.text;
    }
    return text;
}

inline std::string textOf(const KysChess::Battle::BattleLogEvent& event)
{
    return joinSegments(event.segments);
}

inline KysChess::Battle::BattleRuntimeUnit reportUnit(
    int battleId,
    int realRoleId,
    int team,
    int headId,
    std::string name)
{
    KysChess::Battle::BattleRuntimeUnit unit;
    unit.id = battleId;
    unit.realRoleId = realRoleId;
    unit.team = team;
    unit.headId = headId;
    unit.name = std::move(name);
    return unit;
}

inline bool hasSegment(
    const KysChess::Battle::BattleLogEvent& event,
    const std::string& text,
    KysChess::Battle::BattleLogTextTone tone)
{
    for (const auto& segment : event.segments)
    {
        if (segment.text == text && segment.tone == tone)
        {
            return true;
        }
    }
    return false;
}

}
