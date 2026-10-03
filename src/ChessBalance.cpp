#include "ChessBalance.h"
#include <filesystem>
#include <stdexcept>

#include "yaml-cpp/yaml.h"

#include <array>
#include <print>
#include <set>
#include <vector>

namespace KysChess
{

bool parseBattlePieceNode(
    const YAML::Node& node,
    BattlePieceDef& out,
    const std::string& context,
    const ChessDiagnosticSink& diagnostics)
{
    auto mark = node.Mark();
    auto fail = [&](const std::string& msg) {
        const auto detail = !mark.is_null()
            ? std::format("「{}」(第{}行，第{}列) {}", context, mark.line + 1, mark.column + 1, msg)
            : std::format("「{}」{}", context, msg);
        emitChessDiagnostic(diagnostics, ChessDiagnosticSeverity::Error, "棋子配置", detail);
        return false;
    };

    if (!node)
        return fail("棋子節點為空");

    if (node.IsSequence())
    {
        if (node.size() < 2)
            return fail("陣列寫法至少需要 [角色ID, 星級]");

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
            return fail(std::format("陣列寫法解析失敗: {}", ex.what()));
        }
    }

    if (!node.IsMap())
        return fail("棋子節點必須是陣列或映射表");

    auto roleNode = node["角色ID"] ? node["角色ID"] : node["角色"];
    if (!roleNode)
        return fail("缺少「角色ID」欄位");

    try
    {
        out.roleId = roleNode.as<int>();
        if (node["星級"]) out.star = node["星級"].as<int>();
        if (node["武器"]) out.weaponId = node["武器"].as<int>();
        if (node["防具"]) out.armorId = node["防具"].as<int>();
        return true;
    }
    catch (const YAML::Exception& ex)
    {
        return fail(std::format("映射寫法解析失敗: {}", ex.what()));
    }
}

bool loadBalanceConfig(
    const std::string& path,
    const std::string& challengePath,
    const ChessTextConverter&,
    const ChessDiagnosticSink& diagnostics,
    BalanceConfig& out)
{
    YAML::Node root;
    try { root = YAML::LoadFile(path); }
    catch (const YAML::Exception& e)
    {
        emitChessDiagnostic(
            diagnostics,
            ChessDiagnosticSeverity::Error,
            "平衡配置",
            std::format("無法讀取檔案 {}: {}", path, e.what()));
        return false;
    }

    BalanceConfig c;
    try
    {
    const std::set<std::string> balanceKeys{"Boss戰鬥經驗", "Boss獎勵加成", "Boss間隔", "價格", "初始金幣", "利息上限", "利息百分比", "刷新費用", "各費價格", "名稱", "商店數量", "商店權重", "固定攻擊", "固定生命", "固定防禦", "基本", "基礎禁棋數", "天賦額外", "戰鬥獎勵基礎", "戰鬥獎勵增長", "戰鬥經驗", "描述", "攻擊倍率", "敵人", "敵人表", "敵人裝備", "數值", "星級", "星級倍率", "星級加成", "最低出戰人數", "最低層級", "最高層級", "最高等級", "最高費用", "棋子成長", "棋子費用", "武功倍率", "武器", "每勝兵器", "每勝攻擊", "每勝生命", "每勝輕功", "每勝防禦", "每級增加禁棋數", "無羈絆關卡", "獎勵", "玩家裝備獎勵", "生命倍率", "神兵商店", "禁棋數", "禁棋解鎖", "經濟", "經驗表", "總關卡數", "背包上限", "裝備ID", "裝備數量", "角色", "角色ID", "購買經驗數量", "購買經驗費用", "追加選項費用", "逆天改命費用", "通關後", "速度倍率", "進度", "遠征挑戰", "選項數量", "關卡", "防具", "防禦倍率", "雙裝備", "類型", "棋手天賦", "預設", "可選", "神兵", "晚成", "賭徒", "中堅"};
    const auto validateKeys = [&](const auto& self, const YAML::Node& node) -> void {
        if (node.IsMap()) for (const auto& entry : node)
        {
            const auto key = entry.first.as<std::string>();
            if (!balanceKeys.contains(key)) throw std::runtime_error(std::format("未知平衡配置欄位「{}」", key));
            self(self, entry.second);
        }
        else if (node.IsSequence()) for (const auto& entry : node) self(self, entry);
    };
    validateKeys(validateKeys, root);
    const auto talentPath = std::filesystem::path(path).parent_path() / "chess_talents.yaml";
    const auto catalogRoot = YAML::LoadFile(talentPath.string());
    if (!catalogRoot.IsMap() || catalogRoot.size() != 1)
        throw std::runtime_error("天賦配置根節點只允許棋手天賦");
    const auto catalog = catalogRoot["棋手天賦"];
    if (!catalog.IsMap() || catalog.size() != kChessTalentIds.size())
        throw std::runtime_error("棋手天賦目錄必須完整包含四種天賦");
    c.talents.clear();
    for (const auto& entry : catalog)
    {
        const auto id = parseChessTalent(entry.first.as<std::string>());
        if (!id || entry.first.as<std::string>() != chessTalentName(*id) || c.talents.contains(*id))
            throw std::runtime_error("未知或重複棋手天賦");
        const auto n = entry.second;
        const auto keys = [](const YAML::Node& node, std::initializer_list<const char*> allowed) {
            if (!node.IsMap()) throw std::runtime_error("天賦配置必須是映射表");
            for (const auto& entry : node)
            {
                const auto key = entry.first.as<std::string>();
                if (std::ranges::none_of(allowed, [&](const char* expected) { return key == expected; }))
                    throw std::runtime_error(std::format("未知天賦配置欄位「{}」", key));
            }
        };
        switch (*id)
        {
        case ChessTalentId::DivineArms: keys(n, {"說明", "可使用神兵商店"}); break;
        case ChessTalentId::LateBloomer: keys(n, {"說明", "勝場成長受加成比例"}); break;
        case ChessTalentId::Gambler:
            keys(n, {"說明", "開局額外禁棋", "賭運"});
            keys(n["開局額外禁棋"], {"次數", "最低費用", "最高費用"});
            keys(n["賭運"], {"累積截止關卡", "目標最低費用", "目標最高費用", "每次增加層數", "每層觸發機率百分點", "層數上限", "觸發後生命", "無敵幀數"});
            break;
        case ChessTalentId::Backbone:
            keys(n, {"說明", "目標費用", "額外星級加成", "刷新保證"});
            keys(n["額外星級加成"], {"每顆開場內力", "每顆強化次數", "強化傷害百分比", "計算上限"});
            keys(n["刷新保證"], {"觸發星級", "每次數量"});
            break;
        }
        ChessTalentDefinition def;
        def.description = n["說明"].as<std::string>();
        if (def.description.empty()) throw std::runtime_error("天賦說明不得空白");
        const auto number = [](const YAML::Node& node, const char* key, int minimum, int maximum) {
            const int value = node[key].as<int>();
            if (value < minimum || value > maximum)
                throw std::runtime_error(std::format("天賦欄位「{}」必須介於 {} 至 {}", key, minimum, maximum));
            return value;
        };
        switch (*id)
        {
        case ChessTalentId::DivineArms:
            def.legendaryShop = n["可使用神兵商店"].as<bool>();
            break;
        case ChessTalentId::LateBloomer:
            def.amplifiedGrowthPercent = number(n, "勝場成長受加成比例", 0, 100);
            break;
        case ChessTalentId::Gambler:
        {
            const auto bans = n["開局額外禁棋"];
            def.openingBans = number(bans, "次數", 1, 100);
            def.banMinTier = number(bans, "最低費用", 1, 5);
            def.banMaxTier = number(bans, "最高費用", def.banMinTier, 5);
            const auto luck = n["賭運"];
            def.luckLastFight = number(luck, "累積截止關卡", 1, 1000);
            def.luckMinTier = number(luck, "目標最低費用", 1, 5);
            def.luckMaxTier = number(luck, "目標最高費用", def.luckMinTier, 5);
            def.luckPerRefresh = number(luck, "每次增加層數", 1, 100);
            def.luckChancePerStack = number(luck, "每層觸發機率百分點", 0, 100);
            def.luckStackCap = number(luck, "層數上限", 1, 100);
            def.luckSurvivalHp = number(luck, "觸發後生命", 1, 100000);
            def.luckInvincibleFrames = number(luck, "無敵幀數", 0, 100000);
            break;
        }
        case ChessTalentId::Backbone:
            def.targetTier = number(n, "目標費用", 1, 5);
            def.mpPerExtraStar = number(n["額外星級加成"], "每顆開場內力", 0, 10000);
            def.strengtheningChargesPerExtraStar = number(n["額外星級加成"], "每顆強化次數", 0, 100);
            def.strengtheningDamagePercent = number(n["額外星級加成"], "強化傷害百分比", 1, 100);
            def.extraStarCap = number(n["額外星級加成"], "計算上限", 0, 100);
            def.guaranteeStar = number(n["刷新保證"], "觸發星級", 2, 2);
            def.guaranteeCount = number(n["刷新保證"], "每次數量", 1, 100);
            break;
        }
        c.talents.emplace(*id, std::move(def));
    }
    const auto selection = root["棋手天賦"];
    const auto defaultTalent = parseChessTalent(selection["預設"].as<std::string>());
    if (!defaultTalent) throw std::runtime_error("未知預設天賦");
    c.defaultTalent = *defaultTalent;
    c.availableTalents.clear();
    if (!selection["可選"].IsSequence()) throw std::runtime_error("可選天賦必須是清單");
    for (const auto& entry : selection["可選"])
    {
        const auto id = parseChessTalent(entry.as<std::string>());
        if (!id || c.allowsTalent(*id)) throw std::runtime_error("未知或重複可選天賦");
        c.availableTalents.push_back(*id);
    }
    if (!c.allowsTalent(c.defaultTalent)) throw std::runtime_error("預設天賦必須包含於可選清單");


    if (auto n = root["星級加成"])
    {
        if (n["生命倍率"]) c.starHPMult = n["生命倍率"].as<double>();
        if (n["攻擊倍率"]) c.starAtkMult = n["攻擊倍率"].as<double>();
        if (n["防禦倍率"]) c.starDefMult = n["防禦倍率"].as<double>();
        if (n["武功倍率"]) c.starMartialMult = n["武功倍率"].as<double>();
        if (n["速度倍率"]) c.starSpdMult = n["速度倍率"].as<double>();
        if (n["固定生命"]) c.starFlatHP = n["固定生命"].as<int>();
        if (n["固定攻擊"]) c.starFlatAtk = n["固定攻擊"].as<int>();
        if (n["固定防禦"]) c.starFlatDef = n["固定防禦"].as<int>();
    }

    if (auto n = root["棋子成長"])
    {
        if (n["每勝生命"]) c.fightWinGrowthHP = n["每勝生命"].as<double>();
        if (n["每勝攻擊"]) c.fightWinGrowthAtk = n["每勝攻擊"].as<double>();
        if (n["每勝防禦"]) c.fightWinGrowthDef = n["每勝防禦"].as<double>();
        if (n["每勝兵器"]) c.fightWinGrowthWeapon = n["每勝兵器"].as<double>();
        if (n["每勝輕功"]) c.fightWinGrowthSpeed = n["每勝輕功"].as<double>();
    }

    if (auto n = root["經濟"])
    {
        if (n["初始金幣"]) c.initialMoney = n["初始金幣"].as<int>();
        if (n["刷新費用"]) c.refreshCost = n["刷新費用"].as<int>();
        if (n["逆天改命費用"]) c.enemyRerollCost = n["逆天改命費用"].as<int>();
        if (n["購買經驗費用"]) c.buyExpCost = n["購買經驗費用"].as<int>();
        if (n["購買經驗數量"]) c.buyExpAmount = n["購買經驗數量"].as<int>();
        if (n["戰鬥經驗"]) c.battleExp = n["戰鬥經驗"].as<int>();
        if (n["Boss戰鬥經驗"]) c.bossBattleExp = n["Boss戰鬥經驗"].as<int>();
        if (n["戰鬥獎勵基礎"]) c.rewardBase = n["戰鬥獎勵基礎"].as<int>();
        if (n["戰鬥獎勵增長"]) c.rewardGrowth = n["戰鬥獎勵增長"].as<int>();
        if (n["Boss獎勵加成"]) c.bossRewardBonus = n["Boss獎勵加成"].as<int>();
        if (n["利息百分比"]) c.interestPercent = n["利息百分比"].as<int>();
        if (n["利息上限"]) c.interestMax = n["利息上限"].as<int>();
    }

    if (auto n = root["棋子費用"])
    {
        if (n["各費價格"])
            for (int i = 0; i < 5 && i < (int)n["各費價格"].size(); ++i)
                c.tierPrices[i] = n["各費價格"][i].as<int>();
        if (n["星級倍率"]) c.starCostMult = n["星級倍率"].as<int>();
    }

    if (root["經驗表"])
    {
        c.expTable.clear();
        for (const auto& v : root["經驗表"])
            c.expTable.push_back(v.as<int>());
    }

    if (root["最高等級"]) c.maxLevel = root["最高等級"].as<int>();
    if (root["背包上限"]) c.benchSize = root["背包上限"].as<int>();
    if (root["最低出戰人數"]) c.minBattleSize = root["最低出戰人數"].as<int>();
    if (root["商店數量"]) c.shopSlotCount = root["商店數量"].as<int>();
    if (root["基礎禁棋數"]) c.banBaseCount = root["基礎禁棋數"].as<int>();
    if (root["每級增加禁棋數"]) c.banCountPerLevel = root["每級增加禁棋數"].as<int>();

    if (root["禁棋解鎖"])
    {
        c.banUnlocks.clear();
        for (const auto& entry : root["禁棋解鎖"])
        {
            BalanceConfig::BanUnlock unlock;
            unlock.afterFight = entry["通關後"].as<int>();
            unlock.slots = entry["禁棋數"].as<int>();
            unlock.maxTier = entry["最高費用"] ? entry["最高費用"].as<int>() : 5;
            c.banUnlocks.push_back(unlock);
        }
    }

    if (root["無羈絆關卡"])
    {
        c.noSynergyFights.clear();
        for (const auto& v : root["無羈絆關卡"])
            c.noSynergyFights.push_back(v.as<int>());
    }

    if (root["商店權重"])
    {
        int lvl = 0;
        for (const auto& row : root["商店權重"])
        {
            if (lvl >= 10) break;
            for (int t = 0; t < 5 && t < (int)row.size(); ++t)
                c.shopWeights[lvl][t] = row[t].as<int>();
            lvl++;
        }
    }

    if (root["敵人表"])
    {
        c.enemyTable.clear();
        for (const auto& round : root["敵人表"])
        {
            std::vector<BalanceConfig::EnemySlot> slots;
            for (const auto& slot : round)
                slots.push_back({slot[0].as<int>(), slot[1].as<int>()});
            c.enemyTable.push_back(std::move(slots));
        }
    }

    if (auto n = root["進度"])
    {
        if (n["總關卡數"]) c.totalFights = n["總關卡數"].as<int>();
        if (n["Boss間隔"]) c.bossInterval = n["Boss間隔"].as<int>();
    }

    if (auto n = root["神兵商店"])
    {
        if (n["通關後"]) c.legendaryShop.unlockFight = n["通關後"].as<int>();
        if (n["價格"]) c.legendaryShop.price = n["價格"].as<int>();
        if (c.legendaryShop.price <= 0 || c.legendaryShop.unlockFight < 0
            || c.legendaryShop.unlockFight > c.totalFights)
            throw std::runtime_error("神兵商店價格必須為正數，開放關卡不得超出總關卡數");
    }

    try {
        auto ch = YAML::LoadFile(challengePath);
        if (ch["遠征挑戰"])
        {
            std::set<std::string> challengeNames;
            for (const auto& entry : ch["遠征挑戰"])
            {
                BalanceConfig::ChallengeDef def;
                def.name = entry["名稱"].as<std::string>();
                if (def.name.empty() || !challengeNames.insert(def.name).second)
                {
                    emitChessDiagnostic(
                        diagnostics,
                        ChessDiagnosticSeverity::Error,
                        "遠征挑戰配置",
                        std::format("遠征挑戰名稱「{}」空白或重複", def.name));
                    return false;
                }
                if (entry["描述"]) def.description = entry["描述"].as<std::string>();
                for (const auto& e : entry["敵人"])
                {
                    BattlePieceDef piece;
                    auto pieceContext = std::format("遠征挑戰「{}」敵人#{}", def.name, def.enemies.size() + 1);
                    if (!parseBattlePieceNode(e, piece, pieceContext, diagnostics))
                        return false;
                    def.enemies.push_back(piece);
                }
                if (entry["獎勵"])
                {
                    std::set<std::pair<int, int>> rewards;
                    for (const auto& r : entry["獎勵"])
                    {
                        BalanceConfig::ChallengeReward reward;
                        auto t = r["類型"].as<std::string>();
                        bool recognized = true;
                        if (t == "獲取金幣") reward.type = BalanceConfig::ChallengeRewardType::Gold;
                        else if (t == "獲取棋子") reward.type = BalanceConfig::ChallengeRewardType::GetPiece;
                        else if (t == "獲取內功") reward.type = BalanceConfig::ChallengeRewardType::GetNeigong;
                        else if (t == "升星1到2") reward.type = BalanceConfig::ChallengeRewardType::StarUp1to2;
                        else if (t == "升星2到3") reward.type = BalanceConfig::ChallengeRewardType::StarUp2to3;
                        else if (t == "獲取裝備") reward.type = BalanceConfig::ChallengeRewardType::GetEquipment;
                        else if (t == "獲取指定裝備")
                        {
                            reward.type = BalanceConfig::ChallengeRewardType::GetSpecificEquipment;
                            reward.value = r["裝備ID"].as<int>();
                        }
                        else
                        {
                            recognized = false;
                        }
                        if (!recognized)
                        {
                            emitChessDiagnostic(
                                diagnostics,
                                ChessDiagnosticSeverity::Error,
                                "遠征挑戰配置",
                                std::format("遠征挑戰「{}」含未知獎勵類型「{}」", def.name, t));
                            return false;
                        }
                        if (r["數值"]) reward.value = r["數值"].as<int>();
                        else if (r["最高費用"]) reward.value = r["最高費用"].as<int>();
                        else if (r["最高層級"]) reward.value = r["最高層級"].as<int>();
                        if (!rewards.emplace(static_cast<int>(reward.type), reward.value).second)
                        {
                            emitChessDiagnostic(
                                diagnostics,
                                ChessDiagnosticSeverity::Error,
                                "遠征挑戰配置",
                                std::format("遠征挑戰「{}」含重複獎勵", def.name));
                            return false;
                        }
                        def.rewards.push_back(reward);
                    }
                }
                c.challenges.push_back(std::move(def));
            }
        }
    }
    catch (const YAML::Exception& ex)
    {
        emitChessDiagnostic(
            diagnostics,
            ChessDiagnosticSeverity::Error,
            "遠征挑戰配置",
            std::format("無法讀取檔案 {}: {}", challengePath, ex.what()));
        return false;
    }

    if (root["敵人裝備"])
    {
        for (const auto& entry : root["敵人裝備"])
        {
            BalanceConfig::EnemyEquipmentLevel level;
            level.fight = entry["關卡"].as<int>();
            level.maxTier = entry["最高層級"].as<int>();
            level.count = entry["裝備數量"].as<int>();
            if (entry["雙裝備"]) level.equipBoth = entry["雙裝備"].as<bool>();
            c.enemyEquipmentLevels.push_back(level);
        }
    }

    auto parseRewards = [&](const YAML::Node& entries, auto& output) {
        if (!entries.IsSequence()) throw std::runtime_error("裝備獎勵必須是清單");
        int previousFight = 0;
        for (const auto& entry : entries)
        {
            BalanceConfig::PlayerEquipmentReward reward;
            reward.fight = entry["關卡"].as<int>();
            reward.minTier = entry["最低層級"].as<int>();
            reward.maxTier = entry["最高層級"].as<int>();
            reward.choices = entry["選項數量"].as<int>();
            reward.additionalOptionCost = entry["追加選項費用"].as<int>();
            if (reward.fight <= previousFight || reward.fight > c.totalFights
                || reward.minTier < 1 || reward.maxTier > 4 || reward.minTier > reward.maxTier
                || reward.choices <= 0 || reward.additionalOptionCost < 0)
                throw std::runtime_error("裝備獎勵關卡、層級範圍、選項數量或費用不合法");
            previousFight = reward.fight;
            output.push_back(reward);
        }
    };
    const auto rewards = root["玩家裝備獎勵"];
    parseRewards(rewards["基本"], c.playerEquipmentRewards);
    if (const auto extra = rewards["天賦額外"])
    {
        for (const auto& entry : extra)
        {
            const auto id = parseChessTalent(entry.first.as<std::string>());
            if (!id || !c.talents.contains(*id) || c.talentEquipmentRewards.contains(*id))
                throw std::runtime_error("天賦額外獎勵引用未知或重複天賦");
            parseRewards(entry.second, c.talentEquipmentRewards[*id]);
        }
    }

    }
    catch (const std::exception& error)
    {
        emitChessDiagnostic(diagnostics, ChessDiagnosticSeverity::Error, "平衡配置", error.what());
        return false;
    }
    out = std::move(c);
    emitChessDiagnostic(diagnostics, ChessDiagnosticSeverity::Info, "平衡配置", "載入成功");
    return true;
}

const char* ChessBalance::difficultyConfigSuffix(Difficulty d)
{
    switch (d)
    {
    case Difficulty::Easy:
        return "easy";
    case Difficulty::Normal:
        return "normal";
    case Difficulty::Hard:
        return "hard";
    }
    return "easy";
}

const char* ChessBalance::difficultyDisplayNameTraditional(Difficulty d)
{
    switch (d)
    {
    case Difficulty::Easy: return "簡單";
    case Difficulty::Normal: return "標準";
    case Difficulty::Hard: return "困難";
    }
    return "簡單";
}

}    // namespace KysChess
