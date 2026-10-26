#pragma once

#include <array>

namespace KysChess
{
enum Id : int
{
    EFT_HEAL = 0,
    EFT_HIT_1 = 37,
    EFT_HIT_2 = 35,
    EFT_HIT_3 = 11,
    EFT_HIT_4 = 12,
    EFT_HIT_5 = 14,
    EFT_SHIELD_BLAST = 96,
    EFT_DEATH_BLAST = 54,
    EFT_BLOCK = 211,
    EFT_EVADE = 212,
    EFT_AREA_FIRE = 213,
    EFT_AREA_SAND = 214,
    EFT_AREA_WARD = 215,
    EFT_CUE_POSITIVE = 216,
    EFT_CUE_CLEANSE = 217,
    EFT_CUE_SWORD = 218,
    EFT_CUE_GUARDIAN = 219,
    EFT_CUE_FIRE = 220,
};

inline constexpr std::array EFT_HIT = {
    EFT_HIT_1,
    EFT_HIT_2,
    EFT_HIT_3,
    EFT_HIT_4,
    EFT_HIT_5,
};

inline constexpr std::array EFT_ALL = {
    EFT_HEAL,
    EFT_HIT_1,
    EFT_HIT_2,
    EFT_HIT_3,
    EFT_HIT_4,
    EFT_HIT_5,
    EFT_SHIELD_BLAST,
    EFT_DEATH_BLAST,
    EFT_BLOCK,
    EFT_EVADE,
    EFT_AREA_FIRE,
    EFT_AREA_SAND,
    EFT_AREA_WARD,
    EFT_CUE_POSITIVE,
    EFT_CUE_CLEANSE,
    EFT_CUE_SWORD,
    EFT_CUE_GUARDIAN,
    EFT_CUE_FIRE,
};

} // namespace KysChess
