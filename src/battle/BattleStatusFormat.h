#pragma once

#include <format>
#include <string>

namespace KysChess::Battle
{

inline std::string formatStatusRange(const char* label, int current, int maxValue, const char* unit)
{
    if (current <= 0 || maxValue <= 0)
    {
        return label;
    }
    return std::format("{}（{}/{}{}）", label, current, maxValue, unit);
}

}  // namespace KysChess::Battle