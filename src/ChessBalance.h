#pragma once

#include "Save.h"
#include <array>
#include <map>
#include <string>
#include <vector>

namespace YAML
{
class Node;
}

namespace KysChess
{

enum class Difficulty { Easy, Normal, Hard };

struct BattlePieceDef
{
    int roleId = -1;
    int star = 1;
    int weaponId = -1;
    int armorId = -1;
};

struct BalanceConfig
{
    // Star scaling
    double starHPMult = 0.80;
    double starAtkMult = 0.80;
    double starDefMult = 0.50;
    double starSpdMult = 0.25;
    int starFlatHP = 200;
    int starFlatAtk = 15;
    int starFlatDef = 10;

    // Economy
    int initialMoney = 20;
    int refreshCost = 2;
    int buyExpCost = 5;
    int buyExpAmount = 5;
    int battleExp = 4;
    int bossBattleExp = 8;
    int rewardBase = 3;
    int rewardGrowth = 12;
    int bossRewardBonus = 3;
    int interestPercent = 10;
    int interestMax = 3;

    // Chess cost
    std::array<int, 5> tierPrices = {1, 2, 3, 4, 5};
    int starCostMult = 3;

    // Progression
    std::vector<int> expTable = {4, 6, 10, 14, 18, 22, 26, 32, 38};
    int maxLevel = 9;
    int benchSize = 10;
    int minBattleSize = 2;
    int shopSlotCount = 5;
    int banBaseCount = 0;
    int banCountPerLevel = 0;

    // Ban unlock: after winning fight N (1-indexed), grant X ban slots for tiers up to Y
    struct BanUnlock { int afterFight; int slots; int maxTier; };
    std::vector<BanUnlock> banUnlocks;

    // Fights where enemies should have no synergy (1-indexed fight numbers)
    std::vector<int> noSynergyFights;

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

    // Enemy table: per-round list of {tier, star} pairs
    struct EnemySlot { int tier = 1; int star = 1; };
    std::vector<std::vector<EnemySlot>> enemyTable;

    // Stage progress
    int totalFights = 28;
    int bossInterval = 4;

    // Enemy equipment progression
    struct EnemyEquipmentLevel { int fight; int maxTier; int count; bool equipBoth = false; };
    std::vector<EnemyEquipmentLevel> enemyEquipmentLevels;

    // Player equipment rewards
    struct PlayerEquipmentReward { int fight; int maxTier; int choices; int refreshCost; };
    std::vector<PlayerEquipmentReward> playerEquipmentRewards;

    // Expedition challenges
    enum class ChallengeRewardType { Gold, GetPiece, GetNeigong, StarUp1to2, StarUp2to3, GetEquipment, GetSpecificEquipment };
    struct ChallengeReward { ChallengeRewardType type; int value = 0; int value2 = 0; };
    using ChallengeEnemy = BattlePieceDef;
    struct ChallengeDef
    {
        std::string name;
        std::string description;
        std::vector<ChallengeEnemy> enemies;
        std::vector<ChallengeReward> rewards;
    };
    std::vector<ChallengeDef> challenges;
};

class ChessBalance
{
public:
    static bool loadConfig(const std::string& path);
    static void setDifficulty(Difficulty d);
    static Difficulty getDifficulty();
    static const char* difficultyConfigSuffix(Difficulty d);
    static const char* difficultyDisplayNameTraditional(Difficulty d);
    static const BalanceConfig& config();

private:
    static inline BalanceConfig config_;
    static inline Difficulty difficulty_ = Difficulty::Easy;
    static inline bool loaded_ = false;
};

bool parseBattlePieceNode(const YAML::Node& node, BattlePieceDef& out, const std::string& context);

}    // namespace KysChess
