#include "ChessBattleEffectParser.h"
#include <yaml-cpp/yaml.h>

#include <format>
#include <set>

namespace KysChess
{

bool parseMagicEffects(const YAML::Node& root, std::vector<ChessMagicEffectDefinition>& out, const std::string& context,
                       const ChessDiagnosticSink& diagnostics)
{
    const auto fail = [&](std::string message)
    {
        emitChessDiagnostic(
            diagnostics, ChessDiagnosticSeverity::Error, "武功效果", std::format("{}：{}", context, message));
        return false;
    };
    if (!root.IsMap() || root.size() != 1 || !root["絕招"] || !root["絕招"].IsSequence())
    {
        return fail("根節點必須包含「絕招」列表");
    }
    std::vector<ChessMagicEffectDefinition> definitions;
    std::set<int> ids;
    try
    {
        for (const auto& entry : root["絕招"])
        {
            if (!entry.IsMap()) { return fail("絕招必須是映射表"); }
            std::set<std::string> keys;
            for (const auto& field : entry)
            {
                const auto key = field.first.as<std::string>();
                if (!keys.insert(key).second) { return fail(std::format("重複欄位「{}」", key)); }
                if (key != "武功" && key != "名稱" && key != "效果")
                {
                    return fail(std::format("未知欄位「{}」", key));
                }
            }
            if (!entry["武功"] || !entry["名稱"] || !entry["效果"] || entry["效果"].size() == 0)
            {
                return fail("絕招需要武功、名稱及非空效果列表");
            }
            ChessMagicEffectDefinition definition;
            definition.magicId = entry["武功"].as<int>();
            definition.name = entry["名稱"].as<std::string>();
            definition.purpose = "絕招";
            if (definition.magicId < 0 || !ids.insert(definition.magicId).second)
            {
                return fail("武功編號必須為非負整數且不得重複");
            }
            std::uint64_t nextRuleId = static_cast<std::uint64_t>(definition.magicId) << 32;
            if (!parseGameplayEffects(entry["效果"],
                                      definition.effects,
                                      definition.rules,
                                      nextRuleId,
                                      std::format("{}：絕招「{}」", context, definition.name),
                                      diagnostics))
            {
                return false;
            }
            for (const auto& rule : definition.rules)
            {
                if (rule.selector.kind == EffectSelectorKind::ComboMembers)
                {
                    return fail("絕招不能使用需要羈絆成員的效果");
                }
                if (rule.event == EffectEvent::BattleInitialized) { return fail("絕招不能使用開場效果"); }
            }
            definitions.push_back(std::move(definition));
        }
    } catch (const YAML::Exception& ex) { return fail(std::format("絕招欄位格式錯誤：{}", ex.what())); }
    out = std::move(definitions);
    return true;
}

bool loadMagicEffectsFile(const std::string& path, std::vector<ChessMagicEffectDefinition>& out,
                          const ChessDiagnosticSink& diagnostics)
{
    try
    {
        return parseMagicEffects(YAML::LoadFile(path), out, path, diagnostics);
    } catch (const YAML::Exception& ex)
    {
        emitChessDiagnostic(diagnostics,
                            ChessDiagnosticSeverity::Error,
                            "武功效果",
                            std::format("讀取「{}」失敗：{}", path, ex.what()));
        return false;
    }
}

}    // namespace KysChess
