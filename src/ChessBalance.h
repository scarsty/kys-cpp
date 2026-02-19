#pragma once

#include "Save.h"
#include <array>
#include <string>
#include <vector>

namespace KysChess
{

struct BalanceConfig
{
    // Star scaling
    double starHPAtkMult = 0.80;
    double starDefMult = 0.50;
    double starSpdMult = 0.25;

    // Economy
    int initialMoney = 20;
    int refreshCost = 2;
    int buyExpCost = 5;
    int buyExpAmount = 5;
    int battleExp = 4;
    int bossBattleExp = 8;
    int rewardBase = 2;
    int rewardStageCoeff = 3;

    // Chess cost
    std::array<int, 5> tierPrices = {1, 2, 3, 4, 5};
    int starCostMult = 3;

    // Progression
    std::vector<int> expTable = {4, 6, 10, 14, 18, 22, 26, 32, 38};
    int maxLevel = 9;
    int benchSize = 10;
    int minBattleSize = 2;

    // Shop weights [level][tier]
    std::array<std::array<int, 5>, 10> shopWeights = {{
        {100, 0, 0, 0, 0},
        {70, 30, 0, 0, 0},
        {60, 35, 5, 0, 0},
        {50, 35, 15, 0, 0},
        {40, 35, 23, 2, 0},
        {33, 30, 30, 7, 0},
        {30, 30, 30, 10, 0},
        {24, 30, 30, 15, 1},
        {22, 30, 25, 20, 3},
        {19, 25, 25, 25, 6},
    }};

    // Enemy generation
    int enemyCountBase = 2;
    int enemyCountMax = 10;
    int lowerTierMixPct = 30;
    int starUpgradeStartSub = 2;
    int starUpgradePct = 20;
    int star2UpgradeStartStage = 3;
    int star2UpgradePct = 15;
    int bossTierUp = 1;
    int bossMaxTier = 4;
    int bossStarStartStage = 3;

    // Stage progress
    int substagesPerStage = 5;
    int bossSubstage = 4;
    int gameCompleteStage = 5;
};

class ChessBalance
{
public:
    static bool loadConfig(const std::string& path);
    static const BalanceConfig& config();

private:
    static inline BalanceConfig config_;
};

}    // namespace KysChess
