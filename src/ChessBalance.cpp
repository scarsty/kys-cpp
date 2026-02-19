#include "ChessBalance.h"

#include "GameUtil.h"
#include "yaml-cpp/yaml.h"

#include <array>
#include <print>
#include <vector>

namespace KysChess
{

bool ChessBalance::loadConfig(const std::string& path)
{
    YAML::Node root;
    try { root = YAML::LoadFile(path); }
    catch (const YAML::Exception& e)
    {
        std::print("【平衡配置】无法读取文件 {}: {}\n", path, e.what());
        return false;
    }

    BalanceConfig c;

    if (auto n = root["星级加成"])
    {
        if (n["生命攻击倍率"]) c.starHPAtkMult = n["生命攻击倍率"].as<double>();
        if (n["防御倍率"]) c.starDefMult = n["防御倍率"].as<double>();
        if (n["速度倍率"]) c.starSpdMult = n["速度倍率"].as<double>();
    }

    if (auto n = root["经济"])
    {
        if (n["初始金币"]) c.initialMoney = n["初始金币"].as<int>();
        if (n["刷新费用"]) c.refreshCost = n["刷新费用"].as<int>();
        if (n["购买经验费用"]) c.buyExpCost = n["购买经验费用"].as<int>();
        if (n["购买经验数量"]) c.buyExpAmount = n["购买经验数量"].as<int>();
        if (n["战斗经验"]) c.battleExp = n["战斗经验"].as<int>();
        if (n["Boss战斗经验"]) c.bossBattleExp = n["Boss战斗经验"].as<int>();
        if (n["战斗奖励基础"]) c.rewardBase = n["战斗奖励基础"].as<int>();
        if (n["战斗奖励阶段系数"]) c.rewardStageCoeff = n["战斗奖励阶段系数"].as<int>();
    }

    if (auto n = root["棋子费用"])
    {
        if (n["各费价格"])
            for (int i = 0; i < 5 && i < (int)n["各费价格"].size(); ++i)
                c.tierPrices[i] = n["各费价格"][i].as<int>();
        if (n["星级倍率"]) c.starCostMult = n["星级倍率"].as<int>();
    }

    if (root["经验表"])
    {
        c.expTable.clear();
        for (const auto& v : root["经验表"])
            c.expTable.push_back(v.as<int>());
    }

    if (root["最高等级"]) c.maxLevel = root["最高等级"].as<int>();
    if (root["背包上限"]) c.benchSize = root["背包上限"].as<int>();
    if (root["最低出战人数"]) c.minBattleSize = root["最低出战人数"].as<int>();

    if (root["商店权重"])
    {
        int lvl = 0;
        for (const auto& row : root["商店权重"])
        {
            if (lvl >= 10) break;
            for (int t = 0; t < 5 && t < (int)row.size(); ++t)
                c.shopWeights[lvl][t] = row[t].as<int>();
            lvl++;
        }
    }

    if (auto n = root["敌人"])
    {
        if (n["数量基础"]) c.enemyCountBase = n["数量基础"].as<int>();
        if (n["数量上限"]) c.enemyCountMax = n["数量上限"].as<int>();
        if (n["低级混入概率"]) c.lowerTierMixPct = n["低级混入概率"].as<int>();
        if (n["星级提升起始关卡"]) c.starUpgradeStartSub = n["星级提升起始关卡"].as<int>();
        if (n["星级提升概率"]) c.starUpgradePct = n["星级提升概率"].as<int>();
        if (n["二星提升起始阶段"]) c.star2UpgradeStartStage = n["二星提升起始阶段"].as<int>();
        if (n["二星提升概率"]) c.star2UpgradePct = n["二星提升概率"].as<int>();
        if (n["Boss升阶"]) c.bossTierUp = n["Boss升阶"].as<int>();
        if (n["Boss最高费"]) c.bossMaxTier = n["Boss最高费"].as<int>();
        if (n["Boss星级起始阶段"]) c.bossStarStartStage = n["Boss星级起始阶段"].as<int>();
    }

    if (auto n = root["进度"])
    {
        if (n["每阶段关卡数"]) c.substagesPerStage = n["每阶段关卡数"].as<int>();
        if (n["Boss关卡序号"]) c.bossSubstage = n["Boss关卡序号"].as<int>();
        if (n["通关阶段数"]) c.gameCompleteStage = n["通关阶段数"].as<int>();
    }

    config_ = std::move(c);
    std::print("【平衡配置】加载成功\n");
    return true;
}

const BalanceConfig& ChessBalance::config()
{
    static bool loaded = false;
    if (!loaded)
    {
        loaded = true;
        loadConfig(GameUtil::PATH() + "config/chess_balance.yaml");
    }
    return config_;
}


}    // namespace KysChess
