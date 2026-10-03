#include "ChessEquipment.h"
#include "ChessGameplayEffect.h"
#include "yaml-cpp/yaml.h"
#include <algorithm>
#include <format>
#include <print>

namespace KysChess
{

namespace
{

bool appendEquipmentManagementRules(
    const YAML::Node& parent,
    const ChessTextConverter& toTraditional,
    std::string_view context,
    const ChessDiagnosticSink& diagnostics,
    std::vector<ChessNonBattleRule>& rules)
{
    const auto configured = parent["管理規則"];
    if (!configured)
    {
        return true;
    }
    if (!configured.IsSequence())
    {
        emitChessDiagnostic(
            diagnostics,
            ChessDiagnosticSeverity::Error,
            "裝備配置",
            std::format("{}的「管理規則」必須是列表", context));
        return false;
    }
    std::size_t ordinal{};
    for (const auto& ruleNode : configured)
    {
        ++ordinal;
        const auto ruleContext = std::format("{}管理規則#{}", context, ordinal);
        auto rule = parseChessNonBattleRule(ruleNode, toTraditional, ruleContext, diagnostics);
        if (!rule)
        {
            return false;
        }
        if (!std::holds_alternative<CountsAsComboRule>(*rule))
        {
            emitChessDiagnostic(
                diagnostics,
                ChessDiagnosticSeverity::Error,
                "裝備配置",
                std::format("{}只允許「計作羈絆」", ruleContext));
            return false;
        }
        rules.push_back(std::move(*rule));
    }
    return true;
}

bool appendSynergyDef(
    const YAML::Node& entry,
    int equipmentId,
    const ChessTextConverter& toTraditional,
    const ChessDiagnosticSink& diagnostics,
    std::uint64_t& nextRuleId,
    std::vector<EquipmentSynergyDef>& synergies)
{
    EquipmentSynergyDef def;
    def.equipmentId = equipmentId;

    auto roleIdsNode = entry["角色ID"];
    if (!roleIdsNode)
    {
        return true;
    }
    if (roleIdsNode.IsSequence())
    {
        for (const auto& roleNode : roleIdsNode)
        {
            def.roleIds.push_back(roleNode.as<int>());
        }
    }
    else
    {
        def.roleIds.push_back(roleIdsNode.as<int>());
    }
    if (def.roleIds.empty())
    {
        return true;
    }

    if (entry["效果"] && !entry["效果"].IsSequence())
    {
        emitChessDiagnostic(
            diagnostics,
            ChessDiagnosticSeverity::Error,
            "裝備配置",
            std::format("裝備羈絆裝備{}的「效果」必須是列表", def.equipmentId));
        return false;
    }
    if (entry["效果"])
    {
        if (!parseGameplayEffects(entry["效果"], def.effects, def.rules, nextRuleId,
                std::format("裝備{}", def.equipmentId), diagnostics))
            return false;
    }

    if (!appendEquipmentManagementRules(
            entry,
            toTraditional,
            std::format("裝備羈絆裝備{}", def.equipmentId),
            diagnostics,
            def.managementRules))
    {
        return false;
    }

    synergies.push_back(def);
    return true;
}

}    // namespace

bool loadChessEquipment(
    const std::string& path,
    const ChessTextConverter& toTraditional,
    const ChessDiagnosticSink& diagnostics,
    std::vector<EquipmentDef>& equipment,
    std::vector<EquipmentSynergyDef>& synergies)
{
    equipment.clear();
    synergies.clear();
    YAML::Node config;
    try
    {
        config = YAML::LoadFile(path);
    }
    catch (const YAML::Exception& ex)
    {
        emitChessDiagnostic(diagnostics, ChessDiagnosticSeverity::Error, "裝備配置", std::format("無法讀取檔案 {}: {}", path, ex.what()));
        return false;
    }

    if (!config["裝備列表"] || !config["裝備列表"].IsSequence())
    {
        emitChessDiagnostic(
            diagnostics,
            ChessDiagnosticSeverity::Error,
            "裝備配置",
            "檔案需要「裝備列表」根節點且其值必須是列表");
        return false;
    }
    std::uint64_t nextRuleId = 1;
    for (const auto& entry : config["裝備列表"])
    {
        if (!entry["裝備ID"] || !entry["層級"] || !entry["裝備類型"])
        {
            emitChessDiagnostic(
                diagnostics,
                ChessDiagnosticSeverity::Error,
                "裝備配置",
                "裝備項目缺少「裝備ID」、「層級」或「裝備類型」");
            return false;
        }
        EquipmentDef def;
        def.itemId = entry["裝備ID"].as<int>();
        def.tier = entry["層級"].as<int>();
        def.equipType = entry["裝備類型"].as<int>();
        if (entry["名稱"])
        {
            def.name = toTraditional(entry["名稱"].as<std::string>());
            if (def.name.empty())
            {
                emitChessDiagnostic(diagnostics, ChessDiagnosticSeverity::Error, "裝備配置", "裝備名稱不可為空");
                return false;
            }
        }

        if (entry["效果"] && !entry["效果"].IsSequence())
        {
            emitChessDiagnostic(
                diagnostics,
                ChessDiagnosticSeverity::Error,
                "裝備配置",
                std::format("裝備{}的「效果」必須是列表", def.itemId));
            return false;
        }
        if (entry["效果"])
        {
            if (!parseGameplayEffects(entry["效果"], def.effects, def.rules, nextRuleId,
                    std::format("裝備{}", def.itemId), diagnostics))
                return false;
        }

        if (!appendEquipmentManagementRules(
                entry,
                toTraditional,
                std::format("裝備{}", def.itemId),
                diagnostics,
                def.managementRules))
        {
            return false;
        }

        if (entry["裝備羈絆"] && !entry["裝備羈絆"].IsSequence())
        {
            emitChessDiagnostic(
                diagnostics,
                ChessDiagnosticSeverity::Error,
                "裝備配置",
                std::format("裝備{}的「裝備羈絆」必須是列表", def.itemId));
            return false;
        }
        if (entry["裝備羈絆"])
        {
            for (const auto& synergyNode : entry["裝備羈絆"])
            {
                if (!appendSynergyDef(
                    synergyNode,
                    def.itemId,
                    toTraditional,
                    diagnostics,
                    nextRuleId,
                    synergies))
                {
                    return false;
                }
            }
        }

        equipment.push_back(def);
    }
    emitChessDiagnostic(diagnostics, ChessDiagnosticSeverity::Info, "裝備配置", std::format("成功載入{}件裝備", equipment.size()));
    return true;
}

}    // namespace KysChess
