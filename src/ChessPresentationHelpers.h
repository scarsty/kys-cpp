#pragma once

#include "Color.h"

#include <cstdint>

struct Item;

namespace KysChess
{

enum class Difficulty : std::uint8_t;

const Item* chessEquipmentDisplayItem(int itemId);
Color chessPieceTierColor(int tier);
Color chessRewardTierColor(int tier);
const char* chessDifficultyDisplayName(Difficulty difficulty);

}
