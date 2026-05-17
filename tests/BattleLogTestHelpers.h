#pragma once

#include "battle/BattlePresentation.h"

#include <string>
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
