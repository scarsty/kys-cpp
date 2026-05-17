#pragma once

#include "BattlePresentation.h"

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace KysChess::Battle
{

namespace BattleLogSegmentDetail
{
template <typename T>
std::string segmentText(T&& value)
{
    using Value = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<Value, std::string>)
    {
        return std::forward<T>(value);
    }
    else if constexpr (std::is_convertible_v<T, std::string_view>)
    {
        return std::string(std::string_view{ std::forward<T>(value) });
    }
    else if constexpr (std::is_arithmetic_v<Value>)
    {
        return std::to_string(value);
    }
    else
    {
        return std::string(std::forward<T>(value));
    }
}

template <BattleLogTextTone DefaultTone, typename T>
void appendSegment(std::vector<BattleLogTextSegment>& segments, T&& value)
{
    segments.push_back({ segmentText(std::forward<T>(value)), DefaultTone });
}

template <BattleLogTextTone DefaultTone, typename T>
void appendSegment(std::vector<BattleLogTextSegment>& segments, std::pair<BattleLogTextTone, T>&& value)
{
    segments.push_back({ segmentText(std::move(value.second)), value.first });
}

template <BattleLogTextTone DefaultTone, typename T>
void appendSegment(std::vector<BattleLogTextSegment>& segments, const std::pair<BattleLogTextTone, T>& value)
{
    segments.push_back({ segmentText(value.second), value.first });
}
}  // namespace BattleLogSegmentDetail

template <BattleLogTextTone DefaultTone = BattleLogTextTone::Default, typename... Parts>
std::vector<BattleLogTextSegment> logSegments(Parts&&... parts)
{
    std::vector<BattleLogTextSegment> segments;
    segments.reserve(sizeof...(Parts));
    (BattleLogSegmentDetail::appendSegment<DefaultTone>(segments, std::forward<Parts>(parts)), ...);
    return segments;
}

inline std::vector<BattleLogTextSegment> battleLogText(
    std::string text,
    BattleLogTextTone tone = BattleLogTextTone::Default)
{
    return logSegments<BattleLogTextTone::Default>(std::pair{ tone, std::move(text) });
}

template <BattleLogTextTone DefaultTone = BattleLogTextTone::SkillName>
std::vector<BattleLogTextSegment> logStatusFrames(const char* label, int frames)
{
    if (frames <= 0)
    {
        return logSegments<DefaultTone>(label);
    }
    return logSegments<DefaultTone>(
        label,
        "（",
        std::pair{ BattleLogTextTone::DurationValue, frames },
        std::pair{ BattleLogTextTone::DurationValue, "幀" },
        "）");
}

template <BattleLogTextTone DefaultTone = BattleLogTextTone::SkillName>
std::vector<BattleLogTextSegment> logStatusRange(const char* label, int current, int maxValue, const char* unit)
{
    if (current <= 0 || maxValue <= 0)
    {
        return logSegments<DefaultTone>(label);
    }
    return logSegments<DefaultTone>(
        label,
        "（",
        std::pair{ BattleLogTextTone::ResourceValue, current },
        "/",
        std::pair{ BattleLogTextTone::ResourceValue, maxValue },
        unit,
        "）");
}

}  // namespace KysChess::Battle
