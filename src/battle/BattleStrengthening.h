#pragma once

#include <compare>

namespace KysChess::Battle
{

struct BattleStrengthening
{
    int charges{};
    int damagePercent{};

    auto operator<=>(const BattleStrengthening&) const = default;
};

}  // namespace KysChess::Battle
