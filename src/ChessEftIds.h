#pragma once

#include <array>

namespace KysChess
{
enum Id : int
{
    EFT_HEAL = 0,
    EFT_SHIELD_BLAST = 96,
    EFT_DEATH_BLAST = 54,
    EFT_BLOCK = 211,
    EFT_EVADE = 212,
};

inline constexpr std::array<Id, 5> EFT_ALL = {
    EFT_HEAL,
    EFT_SHIELD_BLAST,
    EFT_DEATH_BLAST,
    EFT_BLOCK,
    EFT_EVADE,
};

} // namespace KysChess
