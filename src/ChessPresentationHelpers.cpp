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
        {185, 198, 194, 255},
        {151, 181, 132, 255},
        {129, 165, 195, 255},
        {176, 144, 188, 255},
        {207, 139, 119, 255},
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

Color chessEquipmentTypeColor(int equipType)
{
    return equipType == 0
        ? Color{255, 145, 90, 255}
        : Color{100, 185, 255, 255};
}

const char* chessDifficultyDisplayName(Difficulty difficulty)
{
    return ChessBalance::difficultyDisplayNameTraditional(difficulty);
}

}
