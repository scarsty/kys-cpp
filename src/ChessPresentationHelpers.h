#pragma once

#include "Color.h"

struct Item;

namespace KysChess
{

enum class Difficulty;

const Item* chessEquipmentDisplayItem(int itemId);
Color chessPieceTierColor(int tier);
Color chessRewardTierColor(int tier);
const char* chessDifficultyDisplayName(Difficulty difficulty);

}
