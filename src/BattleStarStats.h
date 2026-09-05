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
    int extraFightWinGrowthDef = 0,
    int amplifiedGrowthPercent = 0)
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

    const auto scaled = [&](int base, int growth, double multiplier, int flat) {
        const int amplified = static_cast<int>(static_cast<long long>(growth) * amplifiedGrowthPercent / 100);
        return static_cast<int>(std::floor((base + amplified) * multiplier)) + flat + growth - amplified;
    };
    return {
        scaled(stats.maxHp, winHP, hpMultiplier, config.flatHpPerStar * starLevel),
        scaled(stats.attack, winATK, attackMultiplier, config.flatAttackPerStar * starLevel),
        scaled(stats.defence, winDEF, defenceMultiplier, config.flatDefencePerStar * starLevel),
        scaled(stats.speed, winSPD, speedMultiplier, 0),
        scaled(stats.fist, winWeapon, martialMultiplier, actionFlat),
        scaled(stats.sword, winWeapon, martialMultiplier, actionFlat),
        scaled(stats.knife, winWeapon, martialMultiplier, actionFlat),
        scaled(stats.unusual, winWeapon, martialMultiplier, actionFlat),
        scaled(stats.hiddenWeapon, winWeapon, martialMultiplier, actionFlat),
    };
}

}  // namespace KysChess
