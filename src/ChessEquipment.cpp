#include "ChessEquipment.h"
#include "ChessBattleEffects.h"
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
        int effectOrdinal = 0;
        for (const auto& eNode : entry["效果"])
        {
            ++effectOrdinal;
            auto effectContext = std::format("裝備羈絆裝備{}效果#{}", def.equipmentId, effectOrdinal);
            EffectRule rule;
            if (!ChessBattleEffects::parseEffectRule(
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

    if (config["装备列表"])
    {
        std::uint64_t nextRuleId = 1;
        for (const auto& entry : config["装备列表"])
        {
            EquipmentDef def;
            def.itemId = entry["装备ID"].as<int>();
            def.tier = entry["层级"].as<int>();
            def.equipType = entry["装备类型"].as<int>();

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
                std::size_t effectOrdinal{};
                for (const auto& eNode : entry["效果"])
                {
                    ++effectOrdinal;
                    auto effectContext = std::format("裝備{}效果#{}", def.itemId, effectOrdinal);
                    EffectRule rule;
                    if (!ChessBattleEffects::parseEffectRule(
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

            if (!appendEquipmentManagementRules(
                    entry,
                    toTraditional,
                    std::format("裝備{}", def.itemId),
                    diagnostics,
                    def.managementRules))
            {
                return false;
            }

            if (entry["装备羁绊"] && !entry["装备羁绊"].IsSequence())
            {
                emitChessDiagnostic(
                    diagnostics,
                    ChessDiagnosticSeverity::Error,
                    "裝備配置",
                    std::format("裝備{}的「裝備羈絆」必須是列表", def.itemId));
                return false;
            }
            if (entry["装备羁绊"])
            {
                for (const auto& synergyNode : entry["装备羁绊"])
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
    }
    emitChessDiagnostic(diagnostics, ChessDiagnosticSeverity::Info, "裝備配置", std::format("成功載入{}件裝備", equipment.size()));
    return true;
}

}    // namespace KysChess
