#pragma once

#include "../Point.h"

#include <algorithm>
#include <cassert>
#include <cstdint>

namespace KysChess::Battle
{

struct BattleUnitVitals
{
    int hp{};
    int maxHp{};
    int mp{};
    int maxMp{};
};

inline int scaleByMissingHp(int maximumValue, int hp, int maxHp)
{
    assert(maximumValue >= 0);
    assert(maxHp > 0);
    const int missingHp = std::clamp(maxHp - hp, 0, maxHp);
    return static_cast<int>(
        static_cast<std::int64_t>(maximumValue) * missingHp / maxHp);
}

inline int scaleByMissingHp(int maximumValue, const BattleUnitVitals& vitals)
{
    return scaleByMissingHp(maximumValue, vitals.hp, vitals.maxHp);
}

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
