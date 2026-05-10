#pragma once

#include "../Point.h"

namespace KysChess::Battle
{

struct BattleUnitVitals
{
    int hp{};
    int maxHp{};
    int mp{};
    int maxMp{};
};

struct BattleUnitStats
{
    int attack{};
    int defence{};
    int speed{};
};

struct BattleUnitMotion
{
    Pointf position;
    Pointf velocity;
    Pointf acceleration;
    Pointf facing;
};

struct BattleUnitAnimationState
{
    int cooldown{};
    int cooldownMax{};
    int actFrame{};
    int actType = -1;
};

}  // namespace KysChess::Battle