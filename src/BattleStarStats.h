#pragma once

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

struct BattleStarGrowthConfig
{
    double hpMultiplierPerStar = 0.80;
    double attackMultiplierPerStar = 0.80;
    double defenceMultiplierPerStar = 0.50;
    double martialMultiplierPerStar = 0.50;
    double speedMultiplierPerStar = 0.25;
    int flatHpPerStar = 200;
    int flatAttackPerStar = 15;
    int flatDefencePerStar = 10;
    double hpPerWin = 15.0;
    double attackPerWin = 2.0;
    double defencePerWin = 2.0;
    double weaponSkillPerWin = 0.0;
    double speedPerWin = 0.0;
};

inline int normalizeBattleStar(int star)
{
    return (std::max)(star, 1);
}

inline StarBoostedStats computeStarBoostedStats(
    const BattleStarStatInputs& stats,
    const BattleStarGrowthConfig& config,
    int stars,
    int fightsWon = 0,
    int extraFightWinGrowthHP = 0,
    int extraFightWinGrowthAtk = 0,
    int extraFightWinGrowthDef = 0)
{
    const int normalizedStars = normalizeBattleStar(stars);
    const int normalizedFightsWon = (std::max)(fightsWon, 0);
    const int starLevel = normalizedStars - 1;
    const int winHP = static_cast<int>(std::floor(normalizedFightsWon * (config.hpPerWin + extraFightWinGrowthHP)));
    const int winATK = static_cast<int>(std::floor(normalizedFightsWon * (config.attackPerWin + extraFightWinGrowthAtk)));
    const int winDEF = static_cast<int>(std::floor(normalizedFightsWon * (config.defencePerWin + extraFightWinGrowthDef)));
    const int winSPD = static_cast<int>(std::floor(normalizedFightsWon * config.speedPerWin));
    const int winWeapon = static_cast<int>(std::floor(normalizedFightsWon * config.weaponSkillPerWin));
    const double hpMultiplier = 1.0 + config.hpMultiplierPerStar * starLevel;
    const double attackMultiplier = 1.0 + config.attackMultiplierPerStar * starLevel;
    const double defenceMultiplier = 1.0 + config.defenceMultiplierPerStar * starLevel;
    const double martialMultiplier = 1.0 + config.martialMultiplierPerStar * starLevel;
    const double speedMultiplier = 1.0 + config.speedMultiplierPerStar * starLevel;
    const int actionFlat = 15 * starLevel;

    return {
        static_cast<int>(stats.maxHp * hpMultiplier) + config.flatHpPerStar * starLevel + winHP,
        static_cast<int>(stats.attack * attackMultiplier) + config.flatAttackPerStar * starLevel + winATK,
        static_cast<int>(stats.defence * defenceMultiplier) + config.flatDefencePerStar * starLevel + winDEF,
        static_cast<int>(stats.speed * speedMultiplier) + winSPD,
        static_cast<int>(stats.fist * martialMultiplier) + actionFlat + winWeapon,
        static_cast<int>(stats.sword * martialMultiplier) + actionFlat + winWeapon,
        static_cast<int>(stats.knife * martialMultiplier) + actionFlat + winWeapon,
        static_cast<int>(stats.unusual * martialMultiplier) + actionFlat + winWeapon,
        static_cast<int>(stats.hiddenWeapon * martialMultiplier) + actionFlat + winWeapon,
    };
}

}  // namespace KysChess
