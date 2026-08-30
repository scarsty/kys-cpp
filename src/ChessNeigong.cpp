#include "ChessNeigong.h"

#include "ChessBattleEffectParser.h"
#include "ChessBattleEffectValidation.h"
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
    if (!ng["選擇數量"] || !ng["層級分配"] || !ng["效果"])
    {
        emitChessDiagnostic(
            diagnostics,
            ChessDiagnosticSeverity::Error,
            "內功配置",
            "檔案缺少「選擇數量」、「層級分配」或「效果」根節點");
        return false;
    }
    if (ng["追加選項費用"]) config.additionalOptionCost = ng["追加選項費用"].as<int>();
    if (ng["選擇數量"]) config.choiceCount = ng["選擇數量"].as<int>();
    if (ng["Boss可選層級"])
    {
        for (const auto& kv : ng["Boss可選層級"])
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
    if (ng["層級分配"])
        for (const auto& entry : ng["層級分配"])
        {
            int tier = entry["層級"].as<int>();
            for (const auto& mid : entry["武功"])
                magicTier[mid.as<int>()] = tier;
        }

    // Build pool
    std::uint64_t nextRuleId = 1;
    for (auto& [magicId, tier] : magicTier)
    {
        auto itItem = magicToItem.find(magicId);
        if (itItem == magicToItem.end()) continue;

        auto* magic = findMagic(magicId);
        NeigongDef def;
        def.magicId = magicId;
        def.itemId = itItem->second;
        def.tier = tier;
        const auto configuredNames = ng["名稱"];
        const auto configuredName = configuredNames
            ? configuredNames[std::to_string(magicId)]
            : YAML::Node{};
        def.name = configuredName
            ? configuredName.as<std::string>()
            : magic ? magic->Name : std::format("內功{}", magicId);

        auto effNode = ng["效果"][std::to_string(magicId)];
        if (effNode && effNode.IsSequence())
        {
            std::size_t effectOrdinal{};
            for (const auto& eNode : effNode)
            {
                ++effectOrdinal;
                auto effectContext = std::format("內功「{}」效果#{}", def.name, effectOrdinal);
                EffectRule rule;
                if (!parseEffectRule(
                        eNode,
                        rule,
                        EffectRuleId{ nextRuleId++ },
                        effectContext,
                        diagnostics))
                {
                    return false;
                }
                def.rules.push_back(std::move(rule));
            }
        }
        std::string lifecycleError;
        if (!validateEffectRules(def.rules, lifecycleError))
        {
            emitChessDiagnostic(
                diagnostics,
                ChessDiagnosticSeverity::Error,
                "內功配置",
                std::format("內功「{}」：{}", def.name, lifecycleError));
            return false;
        }
        pool.push_back(std::move(def));
    }

    std::sort(pool.begin(), pool.end(),
        [](auto& a, auto& b) { return a.tier != b.tier ? a.tier < b.tier : a.magicId < b.magicId; });

    emitChessDiagnostic(diagnostics, ChessDiagnosticSeverity::Info, "內功配置", std::format("載入{}個內功", pool.size()));
    return true;
}

}    // namespace KysChess
