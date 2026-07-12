#include "ChessPresentationHelpers.h"

#include "ChessBalance.h"
#include "Save.h"

#include <algorithm>
#include <array>

namespace KysChess
{

const Item* chessEquipmentDisplayItem(int itemId)
{
    return Save::getInstance()->getItem(itemId);
}

Color chessPieceTierColor(int tier)
{
    static const std::array<Color, 5> colors{{
        {175, 238, 238, 255},
        {0, 255, 0, 255},
        {30, 144, 255, 255},
        {186, 96, 255, 255},
        {255, 0, 0, 255},
    }};
    return colors[std::clamp(tier - 1, 0, static_cast<int>(colors.size()) - 1)];
}

Color chessRewardTierColor(int tier)
{
    static const std::array<Color, 4> colors{{
        {100, 200, 100, 255},
        {100, 150, 255, 255},
        {255, 150, 50, 255},
        {255, 100, 255, 255},
    }};
    return colors[std::clamp(tier - 1, 0, static_cast<int>(colors.size()) - 1)];
}

const char* chessDifficultyDisplayName(Difficulty difficulty)
{
    return ChessBalance::difficultyDisplayNameTraditional(difficulty);
}

}
