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
        if (n["生命倍率"]) c.starHPMult = n["生命倍率"].as<double>();
        if (n["攻击倍率"]) c.starAtkMult = n["攻击倍率"].as<double>();
        if (n["防御倍率"]) c.starDefMult = n["防御倍率"].as<double>();
        if (n["速度倍率"]) c.starSpdMult = n["速度倍率"].as<double>();
        if (n["固定生命"]) c.starFlatHP = n["固定生命"].as<int>();
        if (n["固定攻击"]) c.starFlatAtk = n["固定攻击"].as<int>();
        if (n["固定防御"]) c.starFlatDef = n["固定防御"].as<int>();
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
        if (n["战斗奖励增长"]) c.rewardGrowth = n["战斗奖励增长"].as<int>();
        if (n["Boss奖励加成"]) c.bossRewardBonus = n["Boss奖励加成"].as<int>();
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

    if (root["敌人表"])
    {
        c.enemyTable.clear();
        for (const auto& round : root["敌人表"])
        {
            std::vector<BalanceConfig::EnemySlot> slots;
            for (const auto& slot : round)
                slots.push_back({slot[0].as<int>(), slot[1].as<int>()});
            c.enemyTable.push_back(std::move(slots));
        }
    }

    if (auto n = root["进度"])
    {
        if (n["总关卡数"]) c.totalFights = n["总关卡数"].as<int>();
        if (n["Boss间隔"]) c.bossInterval = n["Boss间隔"].as<int>();
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
