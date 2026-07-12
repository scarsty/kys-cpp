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

void appendSynergyDef(
    const YAML::Node& entry,
    int equipmentId,
    const ChessTextConverter& toTraditional,
    const ChessDiagnosticSink& diagnostics,
    std::vector<EquipmentSynergyDef>& synergies)
{
    EquipmentSynergyDef def;
    def.equipmentId = equipmentId;

    auto roleIdsNode = entry["角色ID"];
    if (!roleIdsNode)
    {
        return;
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
        return;
    }

    if (entry["效果"] && entry["效果"].IsSequence())
    {
        int effectOrdinal = 0;
        for (const auto& eNode : entry["效果"])
        {
            ++effectOrdinal;
            ComboEffect eff;
            auto effectContext = std::format("裝備羈絆裝備{}效果#{}", def.equipmentId, effectOrdinal);
            if (!ChessBattleEffects::parseEffect(eNode, eff, effectContext, diagnostics))
            {
                continue;
            }
            if (eff.type == EffectType::ActAsCombo)
            {
                def.actAsComboNames.push_back(toTraditional(eff.text));
                continue;
            }
            def.effects.push_back(eff);
        }
    }

    synergies.push_back(def);
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
        for (const auto& entry : config["装备列表"])
        {
            EquipmentDef def;
            def.itemId = entry["装备ID"].as<int>();
            def.tier = entry["层级"].as<int>();
            def.equipType = entry["装备类型"].as<int>();

            if (entry["效果"] && entry["效果"].IsSequence())
            {
                for (const auto& eNode : entry["效果"])
                {
                    ComboEffect eff;
                    auto effectContext = std::format("裝備{}效果#{}", def.itemId, def.effects.size() + 1);
                    if (ChessBattleEffects::parseEffect(eNode, eff, effectContext, diagnostics))
                    {
                        if (eff.type == EffectType::ActAsCombo)
                        {
                            def.actAsComboNames.push_back(toTraditional(eff.text));
                        }
                        def.effects.push_back(eff);
                    }
                }
            }

            if (entry["装备羁绊"] && entry["装备羁绊"].IsSequence())
            {
                for (const auto& synergyNode : entry["装备羁绊"])
                {
                    appendSynergyDef(synergyNode, def.itemId, toTraditional, diagnostics, synergies);
                }
            }

            equipment.push_back(def);
        }
    }
    emitChessDiagnostic(diagnostics, ChessDiagnosticSeverity::Info, "裝備配置", std::format("成功載入{}件裝備", equipment.size()));
    return true;
}

}    // namespace KysChess
