#pragma once

#include "ChessDiagnostics.h"
#include "ChessTalent.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <map>
#include <limits>
#include <string>
#include <vector>

namespace YAML
{
class Node;
}

namespace KysChess
{

enum class Difficulty : std::uint8_t { Easy, Normal, Hard };

struct BattlePieceDef
{
    int roleId = -1;
    int star = 1;
    int weaponId = -1;
    int armorId = -1;
};

struct BalanceConfig
{
    ChessTalentId defaultTalent = ChessTalentId::DivineArms;
    std::vector<ChessTalentId> availableTalents = {ChessTalentId::DivineArms};
    std::map<ChessTalentId, ChessTalentDefinition> talents = {
        {ChessTalentId::DivineArms, {.legendaryShop = true}}};

    // Star scaling
    double starHPMult = 0.80;
    double starAtkMult = 0.80;
    double starDefMult = 0.50;
    double starMartialMult = 0.50;
    double starSpdMult = 0.25;
    int starFlatHP = 200;
    int starFlatAtk = 15;
    int starFlatDef = 10;

    // Per-instance growth
    double fightWinGrowthHP = 15.0;
    double fightWinGrowthAtk = 2.0;
    double fightWinGrowthDef = 2.0;
    double fightWinGrowthWeapon = 0.0;
    double fightWinGrowthSpeed = 0.0;

    // Economy
    int initialMoney = 20;
    int refreshCost = 2;
    int enemyRerollCost = 3;
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

    // Forced ban reward: after winning fight N (1-indexed), offer X immediate bans for tiers up to Y
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

    // Legendary equipment shop
    struct LegendaryShopConfig {
        int unlockFight = 0;
        int price = 40;
    };
    LegendaryShopConfig legendaryShop;

    // Enemy equipment progression
    struct EnemyEquipmentLevel { int fight; int maxTier; int count; bool equipBoth = false; };
    std::vector<EnemyEquipmentLevel> enemyEquipmentLevels;

    // Player equipment rewards
    struct PlayerEquipmentReward { int fight{}; int maxTier{}; int choices{}; int additionalOptionCost{}; int minTier = 1; };
    std::vector<PlayerEquipmentReward> playerEquipmentRewards;
    std::map<ChessTalentId, std::vector<PlayerEquipmentReward>> talentEquipmentRewards;

    const ChessTalentDefinition& talent(ChessTalentId id) const { return talents.at(id); }
    bool allowsTalent(ChessTalentId id) const { return std::ranges::contains(availableTalents, id); }

    // Expedition challenges
    enum class ChallengeRewardType { Gold, GetPiece, GetNeigong, StarUp1to2, StarUp2to3, GetEquipment, GetSpecificEquipment };
    struct ChallengeReward { ChallengeRewardType type{}; int value = 0; int value2 = 0; };
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

enum class ChessTalentFactKind
{
    Equipment,
    TalentEquipment,
    Shop,
    Mechanic,
};

struct ChessTalentFactRow
{
    std::string category;
    std::string label;
    std::string value;
    ChessTalentFactKind kind{};
    int fight{};
};

struct ChessTalentPresentation
{
    std::string description;
    std::vector<ChessTalentFactRow> facts;
};

inline std::string formatChessTalentPercent(double ratio)
{
    auto result = std::format("{:.2f}", ratio * 100.0);
    while (!result.empty() && result.back() == '0')
    {
        result.pop_back();
    }
    if (!result.empty() && result.back() == '.')
    {
        result.pop_back();
    }
    result += '%';
    return result;
}

inline void appendChessTalentEquipmentFacts(
    std::vector<ChessTalentFactRow>& facts,
    const std::string& category,
    const std::vector<BalanceConfig::PlayerEquipmentReward>& rewards,
    ChessTalentFactKind kind)
{
    if (rewards.empty())
    {
        facts.push_back({
            category,
            "—",
            "無配置",
            kind,
            std::numeric_limits<int>::max(),
        });
        return;
    }

    for (const auto& reward : rewards)
    {
        facts.push_back({
            category,
            std::format("第{}關", reward.fight),
            std::format(
                "{}～{}階｜{}選項",
                reward.minTier,
                reward.maxTier,
                reward.choices),
            kind,
            reward.fight,
        });
    }
}

inline ChessTalentPresentation buildChessTalentPresentation(
    const BalanceConfig& balance,
    ChessTalentId id)
{
    const auto& talent = balance.talent(id);
    ChessTalentPresentation result{
        .description = talent.description,
    };
    const auto appendDescriptionLine = [&result](std::string text) {
        if (!result.description.empty())
        {
            result.description += '\n';
        }
        result.description += std::move(text);
    };
    const auto appendMechanic = [&result](
        std::string category,
        std::string label,
        std::string value) {
        result.facts.push_back({
            std::move(category),
            std::move(label),
            std::move(value),
            ChessTalentFactKind::Mechanic,
        });
    };
    if (id == ChessTalentId::DivineArms)
    {
        appendChessTalentEquipmentFacts(
            result.facts,
            "基本",
            balance.playerEquipmentRewards,
            ChessTalentFactKind::Equipment);

        const auto extra = balance.talentEquipmentRewards.find(id);
        if (extra == balance.talentEquipmentRewards.end())
        {
            appendChessTalentEquipmentFacts(
                result.facts,
                "天賦額外",
                std::vector<BalanceConfig::PlayerEquipmentReward>{},
                ChessTalentFactKind::TalentEquipment);
        }
        else
        {
            appendChessTalentEquipmentFacts(
                result.facts,
                "天賦額外",
                extra->second,
                ChessTalentFactKind::TalentEquipment);
        }
        if (talent.legendaryShop)
        {
            const auto shopUnlock = balance.legendaryShop.unlockFight > 0
                ? std::format("第{}關後開放", balance.legendaryShop.unlockFight)
                : "目前未開放";
            appendDescriptionLine(std::format(
                "神兵商店：{}，每件{}金。",
                shopUnlock,
                balance.legendaryShop.price));
            result.facts.push_back({
                "神兵商店",
                balance.legendaryShop.unlockFight > 0
                    ? std::format("第{}關後", balance.legendaryShop.unlockFight)
                    : "—",
                std::format("每件{}金", balance.legendaryShop.price),
                ChessTalentFactKind::Shop,
                balance.legendaryShop.unlockFight > 0
                    ? balance.legendaryShop.unlockFight
                    : std::numeric_limits<int>::max(),
            });
        }
        std::stable_sort(
            result.facts.begin(),
            result.facts.end(),
            [](const auto& left, const auto& right) { return left.fight < right.fight; });
    }

    switch (id)
    {
    case ChessTalentId::DivineArms:
        break;
    case ChessTalentId::LateBloomer:
        appendMechanic(
            "勝場成長",
            "每場基礎",
            std::format(
                "生命 +{}、攻擊 +{}、防禦 +{}、兵器 +{}、輕功 +{}",
                balance.fightWinGrowthHP,
                balance.fightWinGrowthAtk,
                balance.fightWinGrowthDef,
                balance.fightWinGrowthWeapon,
                balance.fightWinGrowthSpeed));
        appendMechanic(
            "一般基準",
            "加成",
            "0%（勝場成長不受星級倍率放大）");
        appendMechanic(
            "晚成",
            "加成",
            std::format(
                "{}%（勝場成長會併入星級倍率）",
                talent.amplifiedGrowthPercent));
        appendMechanic(
            "每多1星",
            "星級倍率",
            std::format(
                "生命 +{}、攻擊 +{}、防禦 +{}、武功 +{}、輕功 +{}",
                formatChessTalentPercent(balance.starHPMult),
                formatChessTalentPercent(balance.starAtkMult),
                formatChessTalentPercent(balance.starDefMult),
                formatChessTalentPercent(balance.starMartialMult),
                formatChessTalentPercent(balance.starSpdMult)));
        appendMechanic(
            "計算方式",
            "放大部分",
            "勝場成長 × 晚成比例 × 每星倍率 ×（星數−1）；1星不放大，2星放大1次，3星放大2次。小數按實際計算取整。");
        break;
    case ChessTalentId::Gambler:
        appendMechanic(
            "開局",
            "額外禁棋",
            std::format(
                "{}枚（{}～{}費）",
                talent.openingBans,
                talent.banMinTier,
                talent.banMaxTier));
        appendMechanic(
            "賭運",
            "取得方式",
            std::format(
                "第1～{}關付費刷新；隨機選{}～{}費未滿層棋子 +{}層，優先上場棋子",
                talent.luckLastFight,
                talent.luckMinTier,
                talent.luckMaxTier,
                talent.luckPerRefresh));
        appendMechanic(
            "賭運",
            "觸發機率",
            std::format(
                "每層 +{}%，最多{}層（{}%）",
                talent.luckChancePerStack,
                talent.luckStackCap,
                talent.luckChance(talent.luckStackCap)));
        appendMechanic(
            "致命傷害",
            "成功效果",
            std::format(
                "保留{}生命，無敵{}幀",
                talent.luckSurvivalHp,
                talent.luckInvincibleFrames));
        break;
    case ChessTalentId::Backbone:
        appendMechanic(
            "開場內力",
            "適用對象",
            std::format(
                "{}費棋子；只計算非{}費友軍的額外星級",
                talent.targetTier,
                talent.targetTier));
        appendMechanic(
            "開場內力",
            "計算方式",
            std::format(
                "非{}費友軍每多1星 +{}，最多計{}星，最高 +{}",
                talent.targetTier,
                talent.mpPerExtraStar,
                talent.extraStarCap,
                talent.mpPerExtraStar * talent.extraStarCap));
        appendMechanic(
            "攻防強化",
            "開場次數",
            std::format(
                "同開場內力的星級計算；每多1星 +{}次，最多{}次",
                talent.strengtheningChargesPerExtraStar,
                talent.strengtheningChargesPerExtraStar * talent.extraStarCap));
        appendMechanic(
            "攻防強化",
            "共用消耗",
            std::format(
                "造成直接傷害 +{}%或受到傷害 -{}%，每次消耗1次；攻防共用",
                talent.strengtheningDamagePercent,
                talent.strengtheningDamagePercent));
        appendMechanic(
            "定向增援",
            "觸發時機",
            std::format(
                "{}費棋子由1星升至2星時，商店保證{}枚同名棋子",
                talent.targetTier,
                talent.guaranteeCount));
        appendMechanic(
            "定向增援",
            "升至3星",
            "升至3星後取消尚未使用的保證棋子");
        break;
    }
    return result;
}

inline std::string chessTalentDescription(const BalanceConfig& balance, ChessTalentId id)
{
    const auto presentation = buildChessTalentPresentation(balance, id);
    std::string description = presentation.description;
    for (const auto& fact : presentation.facts)
    {
        if (fact.kind == ChessTalentFactKind::Shop)
        {
            continue;
        }
        description += std::format("\n{}｜{}：{}", fact.category, fact.label, fact.value);
    }
    return description;
}

class ChessBalance
{
public:
    static const char* difficultyConfigSuffix(Difficulty d);
    static const char* difficultyDisplayNameTraditional(Difficulty d);
};

bool parseBattlePieceNode(
    const YAML::Node& node,
    BattlePieceDef& out,
    const std::string& context,
    const ChessDiagnosticSink& diagnostics = {});
bool loadBalanceConfig(
    const std::string& balancePath,
    const std::string& challengePath,
    const ChessTextConverter& toTraditional,
    const ChessDiagnosticSink& diagnostics,
    BalanceConfig& out);

}    // namespace KysChess
