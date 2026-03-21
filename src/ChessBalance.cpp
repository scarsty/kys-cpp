#include "ChessBalance.h"

#include "Font.h"
#include "GameUtil.h"
#include "yaml-cpp/yaml.h"

#include <array>
#include <print>
#include <vector>

namespace KysChess
{

bool parseBattlePieceNode(const YAML::Node& node, BattlePieceDef& out, const std::string& context)
{
    auto mark = node.Mark();
    auto fail = [&](const std::string& msg) {
        if (!mark.is_null())
            std::print("【棋子配置】「{}」(第{}行，第{}列) {}\n", context, mark.line + 1, mark.column + 1, msg);
        else
            std::print("【棋子配置】「{}」{}\n", context, msg);
        return false;
    };

    if (!node)
        return fail("棋子节点为空");

    if (node.IsSequence())
    {
        if (node.size() < 2)
            return fail("数组写法至少需要 [角色ID, 星级]");

        try
        {
            out.roleId = node[0].as<int>();
            out.star = node[1].as<int>();
            if (node.size() > 2) out.weaponId = node[2].as<int>();
            if (node.size() > 3) out.armorId = node[3].as<int>();
            return true;
        }
        catch (const YAML::Exception& ex)
        {
            return fail(std::format("数组写法解析失败: {}", ex.what()));
        }
    }

    if (!node.IsMap())
        return fail("棋子节点必须是数组或映射表");

    auto roleNode = node["角色ID"] ? node["角色ID"] : node["角色"];
    if (!roleNode)
        return fail("缺少「角色ID」字段");

    try
    {
        out.roleId = roleNode.as<int>();
        if (node["星级"]) out.star = node["星级"].as<int>();
        if (node["武器"]) out.weaponId = node["武器"].as<int>();
        if (node["防具"]) out.armorId = node["防具"].as<int>();
        return true;
    }
    catch (const YAML::Exception& ex)
    {
        return fail(std::format("映射写法解析失败: {}", ex.what()));
    }
}

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
        if (n["利息百分比"]) c.interestPercent = n["利息百分比"].as<int>();
        if (n["利息上限"]) c.interestMax = n["利息上限"].as<int>();
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
    if (root["商店数量"]) c.shopSlotCount = root["商店数量"].as<int>();
    if (root["基础禁棋数"]) c.banBaseCount = root["基础禁棋数"].as<int>();
    if (root["每级增加禁棋数"]) c.banCountPerLevel = root["每级增加禁棋数"].as<int>();

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

    try {
        auto ch = YAML::LoadFile(GameUtil::PATH() + "config/chess_challenge.yaml");
        if (ch["远征挑战"])
        {
            for (const auto& entry : ch["远征挑战"])
            {
                BalanceConfig::ChallengeDef def;
                def.name = Font::getInstance()->S2T(entry["名称"].as<std::string>());
                if (entry["描述"]) def.description = Font::getInstance()->S2T(entry["描述"].as<std::string>());
                for (const auto& e : entry["敌人"])
                {
                    BattlePieceDef piece;
                    auto pieceContext = std::format("远征挑战「{}」敌人#{}", def.name, def.enemies.size() + 1);
                    if (!parseBattlePieceNode(e, piece, pieceContext))
                        return false;
                    def.enemies.push_back(piece);
                }
                if (entry["奖励"])
                {
                    for (const auto& r : entry["奖励"])
                    {
                        BalanceConfig::ChallengeReward reward;
                        auto t = r["类型"].as<std::string>();
                        if (t == "获取金币") reward.type = BalanceConfig::ChallengeRewardType::Gold;
                        else if (t == "获取棋子") reward.type = BalanceConfig::ChallengeRewardType::GetPiece;
                        else if (t == "获取内功") reward.type = BalanceConfig::ChallengeRewardType::GetNeigong;
                        else if (t == "升星1到2") reward.type = BalanceConfig::ChallengeRewardType::StarUp1to2;
                        else if (t == "升星2到3") reward.type = BalanceConfig::ChallengeRewardType::StarUp2to3;
                        else if (t == "获取装备") reward.type = BalanceConfig::ChallengeRewardType::GetEquipment;
                        else if (t == "获取指定装备")
                        {
                            reward.type = BalanceConfig::ChallengeRewardType::GetSpecificEquipment;
                            reward.value = r["装备ID"].as<int>();
                        }
                        if (r["数值"]) reward.value = r["数值"].as<int>();
                        else if (r["最高费用"]) reward.value = r["最高费用"].as<int>();
                        else if (r["最高层级"]) reward.value = r["最高层级"].as<int>();
                        def.rewards.push_back(reward);
                    }
                }
                c.challenges.push_back(std::move(def));
            }
        }
    } catch (...) {}

    if (root["敌人装备"])
    {
        for (const auto& entry : root["敌人装备"])
        {
            BalanceConfig::EnemyEquipmentLevel level;
            level.fight = entry["关卡"].as<int>();
            level.maxTier = entry["最高层级"].as<int>();
            level.count = entry["装备数量"].as<int>();
            if (entry["双装备"]) level.equipBoth = entry["双装备"].as<bool>();
            c.enemyEquipmentLevels.push_back(level);
        }
    }

    if (root["玩家装备奖励"])
    {
        for (const auto& entry : root["玩家装备奖励"])
        {
            BalanceConfig::PlayerEquipmentReward reward;
            reward.fight = entry["关卡"].as<int>();
            reward.maxTier = entry["最高层级"].as<int>();
            reward.choices = entry["选项数量"].as<int>();
            reward.refreshCost = entry["刷新费用"].as<int>();
            c.playerEquipmentRewards.push_back(reward);
        }
    }

    config_ = std::move(c);
    std::print("【平衡配置】加载成功\n");
    return true;
}

void ChessBalance::setDifficulty(Difficulty d)
{
    difficulty_ = d;
    loaded_ = false;
}

Difficulty ChessBalance::getDifficulty()
{
    return difficulty_;
}

const BalanceConfig& ChessBalance::config()
{
    if (!loaded_)
    {
        loaded_ = true;
        const char* suffix = (difficulty_ == Difficulty::Normal) ? "normal" : "easy";
        loadConfig(GameUtil::PATH() + std::format("config/chess_balance_{}.yaml", suffix));
    }
    return config_;
}

}    // namespace KysChess
