#pragma once

#include <compare>

namespace KysChess::Battle
{

struct BattleStrengthening
{
    int attackCharges{};
    int defenseCharges{};
    int damagePercent{};

    auto operator<=>(const BattleStrengthening&) const = default;
};

}  // namespace KysChess::Battle
