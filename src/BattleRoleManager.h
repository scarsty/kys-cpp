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
    static void applyStarBonus(Role* role, int stars, int fightsWon = 0, int extraFightWinGrowthHP = 0, int extraFightWinGrowthAtk = 0, int extraFightWinGrowthDef = 0);
    static StarBoostedStats computeStarStats(const Role* role, int stars, int fightsWon = 0, int extraFightWinGrowthHP = 0, int extraFightWinGrowthAtk = 0, int extraFightWinGrowthDef = 0);
};

}  // namespace KysChess
