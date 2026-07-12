#include "ChessNeigong.h"

#include "ChessBattleEffects.h"
#include "yaml-cpp/yaml.h"

#include <algorithm>
#include <format>
#include <print>

namespace KysChess
{

bool loadChessNeigong(
    const std::string& path,
    std::span<Item* const> items,
    const std::function<const Magic*(int)>& findMagic,
    const ChessDiagnosticSink& diagnostics,
    NeigongConfig& config,
    std::vector<NeigongDef>& pool)
{
    config = {};
    pool.clear();

    YAML::Node ng;
    try
    {
        ng = YAML::LoadFile(path);
    }
    catch (const YAML::Exception& ex)
    {
        emitChessDiagnostic(diagnostics, ChessDiagnosticSeverity::Error, "內功配置", std::format("無法讀取檔案 {}: {}", path, ex.what()));
        return false;
    }
    if (ng["刷新费用"]) config.rerollCost = ng["刷新费用"].as<int>();
    if (ng["选择数量"]) config.choiceCount = ng["选择数量"].as<int>();
    if (ng["Boss可选层级"])
    {
        for (const auto& kv : ng["Boss可选层级"])
        {
            const int index = kv.first.as<int>();
            for (const auto& tier : kv.second)
            {
                config.tiersByBoss[index].push_back(tier.as<int>());
            }
        }
    }

    // Build item lookup: magicId -> itemId for 秘籍 (ItemType==2)
    std::map<int, int> magicToItem;
    for (auto* item : items)
        if (item && item->ItemType == 2 && item->MagicID > 0)
            magicToItem.try_emplace(item->MagicID, item->ID);

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
    for (auto& [magicId, tier] : magicTier)
    {
        auto itItem = magicToItem.find(magicId);
        if (itItem == magicToItem.end()) continue;

        auto* magic = findMagic(magicId);
        NeigongDef def;
        def.magicId = magicId;
        def.itemId = itItem->second;
        def.tier = tier;
        def.name = magic ? magic->Name : std::format("內功{}", magicId);

        auto effNode = ng["效果"][std::to_string(magicId)];
        if (effNode && effNode.IsSequence())
        {
            for (const auto& eNode : effNode)
            {
                ComboEffect eff;
                auto effectContext = std::format("內功「{}」效果#{}", def.name, def.effects.size() + 1);
                if (ChessBattleEffects::parseEffect(eNode, eff, effectContext, diagnostics))
                    def.effects.push_back(eff);
            }
        }
        pool.push_back(std::move(def));
    }

    std::sort(pool.begin(), pool.end(),
        [](auto& a, auto& b) { return a.tier != b.tier ? a.tier < b.tier : a.magicId < b.magicId; });

    emitChessDiagnostic(diagnostics, ChessDiagnosticSeverity::Info, "內功配置", std::format("載入{}個內功", pool.size()));
    return true;
}

}    // namespace KysChess
