#pragma once

namespace KysChess::Battle
{
struct BattleLethalRecovery
{
    int chancePercent{};
    int survivalHp{};
    int invincibleFrames{};
    bool used = false;
    auto operator<=>(const BattleLethalRecovery&) const = default;
};
}
