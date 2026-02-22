#include "ChessBalance.h"

#include "ChessBattleEffects.h"
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
        auto ng = YAML::LoadFile(GameUtil::PATH() + "config/chess_neigong.yaml");
        if (ng["刷新费用"]) c.neigongRerollCost = ng["刷新费用"].as<int>();
        if (ng["选择数量"]) c.neigongChoiceCount = ng["选择数量"].as<int>();
        if (ng["Boss可选层级"])
            for (const auto& kv : ng["Boss可选层级"])
            {
                int idx = kv.first.as<int>();
                for (const auto& t : kv.second)
                    c.neigongTiersByBoss[idx].push_back(t.as<int>());
            }
    } catch (...) {}

    try {
        auto ch = YAML::LoadFile(GameUtil::PATH() + "config/chess_challenge.yaml");
        if (ch["远征挑战"])
        {
            for (const auto& entry : ch["远征挑战"])
            {
                BalanceConfig::ChallengeDef def;
                def.name = entry["名称"].as<std::string>();
                if (entry["描述"]) def.description = entry["描述"].as<std::string>();
                for (const auto& e : entry["敌人"])
                    def.enemies.push_back({e[0].as<int>(), e[1].as<int>()});
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

const std::vector<NeigongDef>& ChessBalance::getNeigongPool()
{
    if (!neigongPool_.empty()) return neigongPool_;

    // Build item lookup: magicId -> itemId for 秘籍 (ItemType==2)
    std::map<int, int> magicToItem;
    for (auto* item : Save::getInstance()->getItems())
        if (item && item->ItemType == 2 && item->MagicID > 0)
            magicToItem.try_emplace(item->MagicID, item->ID);

    // Load YAML for tier assignments and effects
    YAML::Node ng;
    try { ng = YAML::LoadFile(GameUtil::PATH() + "config/chess_neigong.yaml"); }
    catch (...) { return neigongPool_; }

    // Parse tier assignments
    std::map<int, int> magicTier;
    if (ng["层级分配"])
        for (const auto& entry : ng["层级分配"])
        {
            int tier = entry["层级"].as<int>();
            for (const auto& mid : entry["武功"])
                magicTier[mid.as<int>()] = tier;
        }

    // Build pool
    auto save = Save::getInstance();
    for (auto& [magicId, tier] : magicTier)
    {
        auto itItem = magicToItem.find(magicId);
        if (itItem == magicToItem.end()) continue;  // no 秘籍, skip

        auto* magic = save->getMagic(magicId);
        NeigongDef def;
        def.magicId = magicId;
        def.itemId = itItem->second;
        def.tier = tier;
        def.name = magic ? magic->Name : std::format("内功{}", magicId);

        // Parse effects
        auto effNode = ng["效果"][std::to_string(magicId)];
        if (effNode && effNode.IsSequence())
        {
            for (const auto& eNode : effNode)
            {
                ComboEffect eff;
                if (ChessBattleEffects::parseEffect(eNode, eff, def.name))
                    def.effects.push_back(eff);
            }
        }
        neigongPool_.push_back(std::move(def));
    }

    // Sort by tier then magicId
    std::sort(neigongPool_.begin(), neigongPool_.end(),
        [](auto& a, auto& b) { return a.tier != b.tier ? a.tier < b.tier : a.magicId < b.magicId; });

    std::print("【内功配置】加载{}个内功\n", neigongPool_.size());
    return neigongPool_;
}


}    // namespace KysChess
