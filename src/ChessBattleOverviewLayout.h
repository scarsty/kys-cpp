#pragma once

#include <SDL3/SDL_rect.h>
#include <cassert>

namespace KysChess::ChessBattleOverviewLayout
{
inline constexpr int maximumUnits = 13;
inline constexpr int rowHeight = 26;
inline constexpr int rowsTop = 136;
inline constexpr int synergyTop = rowsTop + maximumUnits * rowHeight + 8;
inline constexpr int synergiesPerPage = 16;
inline constexpr SDL_Rect details{16, 550, 1248, 96};
inline constexpr SDL_Rect proceed{1000, 658, 248, 46};

inline SDL_Rect team(int side) { return {16 + side * 632, 80, 616, 462}; }
inline SDL_Rect row(int side, int index)
{
    assert(side >= 0 && side < 2 && index >= 0 && index < maximumUnits);
    return {28 + side * 632, rowsTop + index * rowHeight, 592, rowHeight};
}
inline SDL_Rect equipment(int side, int index, int slot)
{
    const auto frame = row(side, index);
    return {frame.x + 524 + slot * 32, frame.y + 1, 24, 24};
}
inline SDL_Rect synergy(int side, int index)
{
    assert(index >= 0 && index < synergiesPerPage);
    return {28 + side * 632 + index % 8 * 70, synergyTop + index / 8 * 26, 68, 24};
}
}
