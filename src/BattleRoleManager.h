#pragma once

#include "Save.h"

namespace KysChess
{

struct StarBoostedStats
{
    int hp, atk, def, spd;
    int fist, sword, knife, unusual, hidden;
};

class BattleRoleManager
{
public:
    static void applyStarBonus(Role* role, int stars);
    static StarBoostedStats computeStarStats(const Role* role, int stars);
};

}  // namespace KysChess
