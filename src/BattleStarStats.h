#pragma once

#include "ChessBalance.h"

#include <cmath>

namespace KysChess
{

struct BattleStarStatInputs
{
    int maxHp = 0;
    int attack = 0;
    int defence = 0;
    int speed = 0;
    int fist = 0;
    int sword = 0;
    int knife = 0;
    int unusual = 0;
    int hiddenWeapon = 0;
};

struct StarBoostedStats
{
    int hp = 0;
    int atk = 0;
    int def = 0;
    int spd = 0;
    int fist = 0;
    int sword = 0;
    int knife = 0;
    int unusual = 0;
    int hidden = 0;
};

inline int normalizeBattleStar(int star)
{
    return (std::max)(star, 1);
}

inline StarBoostedStats computeStarBoostedStats(
    const BattleStarStatInputs& stats,
    int stars,
    int fightsWon = 0,
    int extraFightWinGrowthHP = 0,
    int extraFightWinGrowthAtk = 0,
    int extraFightWinGrowthDef = 0)
{
    auto& cfg = ChessBalance::config();
    const int normalizedStars = normalizeBattleStar(stars);
    const int normalizedFightsWon = (std::max)(fightsWon, 0);
    const int starLevel = normalizedStars - 1;
    const int winHP = static_cast<int>(std::floor(normalizedFightsWon * (cfg.fightWinGrowthHP + extraFightWinGrowthHP)));
    const int winATK = static_cast<int>(std::floor(normalizedFightsWon * (cfg.fightWinGrowthAtk + extraFightWinGrowthAtk)));
    const int winDEF = static_cast<int>(std::floor(normalizedFightsWon * (cfg.fightWinGrowthDef + extraFightWinGrowthDef)));
    const int winSPD = static_cast<int>(std::floor(normalizedFightsWon * cfg.fightWinGrowthSpeed));
    const int winWeapon = static_cast<int>(std::floor(normalizedFightsWon * cfg.fightWinGrowthWeapon));
    const double hpMultiplier = 1.0 + cfg.starHPMult * starLevel;
    const double attackMultiplier = 1.0 + cfg.starAtkMult * starLevel;
    const double defenceMultiplier = 1.0 + cfg.starDefMult * starLevel;
    const double martialMultiplier = 1.0 + cfg.starMartialMult * starLevel;
    const double speedMultiplier = 1.0 + cfg.starSpdMult * starLevel;
    const int actionFlat = 15 * starLevel;

    return {
        static_cast<int>(stats.maxHp * hpMultiplier) + cfg.starFlatHP * starLevel + winHP,
        static_cast<int>(stats.attack * attackMultiplier) + cfg.starFlatAtk * starLevel + winATK,
        static_cast<int>(stats.defence * defenceMultiplier) + cfg.starFlatDef * starLevel + winDEF,
        static_cast<int>(stats.speed * speedMultiplier) + winSPD,
        static_cast<int>(stats.fist * martialMultiplier) + actionFlat + winWeapon,
        static_cast<int>(stats.sword * martialMultiplier) + actionFlat + winWeapon,
        static_cast<int>(stats.knife * martialMultiplier) + actionFlat + winWeapon,
        static_cast<int>(stats.unusual * martialMultiplier) + actionFlat + winWeapon,
        static_cast<int>(stats.hiddenWeapon * martialMultiplier) + actionFlat + winWeapon,
    };
}

}  // namespace KysChess
