#include "ChessNeigong.h"

#include "ChessBattleEffects.h"
#include "GameUtil.h"
#include "Save.h"
#include "yaml-cpp/yaml.h"

#include <format>
#include <print>

namespace KysChess
{

const NeigongConfig& ChessNeigong::config()
{
    if (!configLoaded_)
    {
        configLoaded_ = true;
        try {
            auto ng = YAML::LoadFile(GameUtil::PATH() + "config/chess_neigong.yaml");
            if (ng["刷新费用"]) config_.rerollCost = ng["刷新费用"].as<int>();
            if (ng["选择数量"]) config_.choiceCount = ng["选择数量"].as<int>();
            if (ng["Boss可选层级"])
                for (const auto& kv : ng["Boss可选层级"])
                {
                    int idx = kv.first.as<int>();
                    for (const auto& t : kv.second)
                        config_.tiersByBoss[idx].push_back(t.as<int>());
                }
        } catch (...) {}
    }
    return config_;
}

const std::vector<NeigongDef>& ChessNeigong::getPool()
{
    if (!pool_.empty()) return pool_;

    // Build item lookup: magicId -> itemId for 秘籍 (ItemType==2)
    std::map<int, int> magicToItem;
    for (auto* item : Save::getInstance()->getItems())
        if (item && item->ItemType == 2 && item->MagicID > 0)
            magicToItem.try_emplace(item->MagicID, item->ID);

    // Load YAML for tier assignments and effects
    YAML::Node ng;
    try { ng = YAML::LoadFile(GameUtil::PATH() + "config/chess_neigong.yaml"); }
    catch (...) { return pool_; }

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
        if (itItem == magicToItem.end()) continue;

        auto* magic = save->getMagic(magicId);
        NeigongDef def;
        def.magicId = magicId;
        def.itemId = itItem->second;
        def.tier = tier;
        def.name = magic ? magic->Name : std::format("内功{}", magicId);

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
        pool_.push_back(std::move(def));
    }

    std::sort(pool_.begin(), pool_.end(),
        [](auto& a, auto& b) { return a.tier != b.tier ? a.tier < b.tier : a.magicId < b.magicId; });

    std::print("【内功配置】加载{}个内功\n", pool_.size());
    return pool_;
}

}    // namespace KysChess
