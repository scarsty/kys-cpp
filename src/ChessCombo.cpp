#include "ChessCombo.h"

#include "ChessGameContent.h"
#include "ChessSessionTypes.h"
#include "battle/ChessComboResolver.h"

#include "yaml-cpp/yaml.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <format>
#include <ranges>

namespace KysChess
{
namespace
{

int equipmentItemId(const ChessSessionState& state, int equipmentInstanceId)
{
    if (equipmentInstanceId < 0)
    {
        return -1;
    }
    const auto found = state.equipmentInventory.find(equipmentInstanceId);
    assert(found != state.equipmentInventory.end());
    return found->second.itemId;
}

std::vector<ChessComboResolverUnit> resolverUnits(
    const ChessSessionState& state,
    const ChessGameContent& content)
{
    std::vector<ChessComboResolverUnit> result;
    for (const auto& [instanceId, piece] : state.roster)
    {
        if (!piece.deployed)
        {
            continue;
        }
        const auto* role = content.role(piece.roleId);
        result.push_back({
            piece.roleId,
            piece.star,
            role ? std::optional<int>{role->Cost} : std::nullopt,
            equipmentItemId(state, piece.weaponInstanceId),
            equipmentItemId(state, piece.armorInstanceId),
        });
    }
    return result;
}

std::vector<ChessComboResolverEquipmentRule> resolverEquipmentRules(
    const ChessGameContent& content)
{
    std::vector<ChessComboResolverEquipmentRule> result;
    for (const auto& equipment : content.equipment())
    {
        if (!equipment.actAsComboNames.empty())
        {
            result.push_back({equipment.itemId, {}, equipment.actAsComboNames});
        }
    }
    for (const auto& synergy : content.equipmentSynergies())
    {
        if (!synergy.actAsComboNames.empty())
        {
            result.push_back({synergy.equipmentId, synergy.roleIds, synergy.actAsComboNames});
        }
    }
    return result;
}

ChessComboResolverDefinition resolverDefinition(const ComboDef& combo)
{
    ChessComboResolverDefinition result;
    result.id = combo.id;
    result.name = combo.name;
    result.memberRoleIds = combo.memberRoleIds;
    result.isAntiCombo = combo.isAntiCombo;
    result.starSynergyBonus = combo.starSynergyBonus;
    for (const auto& threshold : combo.thresholds)
    {
        result.thresholdCounts.push_back(threshold.count);
    }
    return result;
}

std::vector<ResolvedChessCombo> resolveRosterCombos(
    const ChessSessionState& state,
    const ChessGameContent& content)
{
    const auto units = resolverUnits(state, content);
    const auto equipmentRules = resolverEquipmentRules(content);
    std::vector<ChessComboResolverDefinition> definitions;
    definitions.reserve(content.combos().size());
    for (const auto& combo : content.combos())
    {
        definitions.push_back(resolverDefinition(combo));
    }
    return resolveChessCombos(units, equipmentRules, definitions);
}

}

ChessComboProgress chessComboProgress(
    const ComboDef& combo,
    const ResolvedChessCombo& resolved)
{
    ChessComboProgress progress;
    progress.memberRoleIds = resolved.memberRoleIds;
    progress.physicalCount = resolved.physicalMemberCount;
    progress.effectiveCount = resolved.effectiveMemberCount;
    progress.activeThresholdIndex = resolved.activeThresholdIndex;
    progress.nextThresholdIndex = resolved.nextThresholdIndex;
    progress.active = progress.activeThresholdIndex >= 0;
    progress.isAntiCombo = combo.isAntiCombo;
    progress.starSynergyBonus = combo.starSynergyBonus;
    if (!combo.thresholds.empty())
    {
        progress.displayTargetCount = combo.isAntiCombo
            ? combo.thresholds.front().count
            : progress.nextThresholdIndex >= 0
                ? combo.thresholds[progress.nextThresholdIndex].count
                : combo.thresholds.back().count;
    }
    return progress;
}

std::string formatChessComboProgressCount(const ChessComboProgress& progress)
{
    if (progress.isAntiCombo)
    {
        return progress.active
            ? std::format("獨/{} ✓", progress.displayTargetCount)
            : std::format("獨/{}", progress.displayTargetCount);
    }

    const int extraStars = progress.effectiveCount - progress.physicalCount;
    std::string result = progress.starSynergyBonus && extraStars > 0
        ? std::format("{}+{}/{}", progress.physicalCount, extraStars, progress.displayTargetCount)
        : std::format("{}/{}", progress.physicalCount, progress.displayTargetCount);
    if (progress.active)
    {
        result += " ✓";
    }
    return result;
}

std::vector<ComboDef> loadChessCombos(
    const std::string& path,
    const ChessTextConverter& toTraditional,
    const ChessDiagnosticSink& diagnostics)
{
    YAML::Node root;
    try { root = YAML::LoadFile(path); }
    catch (const YAML::Exception& e)
    {
        emitChessDiagnostic(diagnostics, ChessDiagnosticSeverity::Error, "羈絆配置", std::format("無法讀取檔案 {}: {}", path, e.what()));
        return {};
    }

    if (!root["羁绊"])
    {
        emitChessDiagnostic(diagnostics, ChessDiagnosticSeverity::Error, "羈絆配置", "檔案缺少「羈絆」根節點");
        return {};
    }

    std::vector<ComboDef> combos;
    int idx = 0;

    for (const auto& node : root["羁绊"])
    {
        ComboDef def;
        if (!node["名称"])
        {
            emitChessDiagnostic(diagnostics, ChessDiagnosticSeverity::Error, "羈絆配置", std::format("第{}個羈絆缺少「名稱」", idx + 1));
            return {};
        }
        def.name = toTraditional(node["名称"].as<std::string>());
        def.id = idx;
        def.isAntiCombo = node["反向羁绊"] && node["反向羁绊"].as<bool>();
        def.starSynergyBonus = node["星级羁绊加成"] && node["星级羁绊加成"].as<bool>();

        if (!node["成员"])
        {
            emitChessDiagnostic(diagnostics, ChessDiagnosticSeverity::Error, "羈絆配置", std::format("「{}」缺少「成員」", def.name));
            return {};
        }
        for (const auto& member : node["成员"])
            def.memberRoleIds.push_back(member.as<int>());

        if (!node["阈值"])
        {
            emitChessDiagnostic(diagnostics, ChessDiagnosticSeverity::Error, "羈絆配置", std::format("「{}」缺少「閾值」", def.name));
            return {};
        }
        for (const auto& tNode : node["阈值"])
        {
            ComboThreshold thresh;
            if (!tNode["人数"] || !tNode["名称"])
            {
                emitChessDiagnostic(diagnostics, ChessDiagnosticSeverity::Error, "羈絆配置", std::format("「{}」閾值缺少「人數」或「名稱」", def.name));
                return {};
            }
            thresh.count = tNode["人数"].as<int>();
            thresh.name = toTraditional(tNode["名称"].as<std::string>());

            if (!tNode["效果"])
            {
                emitChessDiagnostic(diagnostics, ChessDiagnosticSeverity::Error, "羈絆配置", std::format("「{}」閾值「{}」缺少「效果」", def.name, thresh.name));
                return {};
            }
            for (const auto& eNode : tNode["效果"])
            {
                ComboEffect eff;
                auto effectContext = std::format("羈絆「{}」閾值「{}」效果#{}", def.name, thresh.name, thresh.effects.size() + 1);
                if (!ChessBattleEffects::parseEffect(eNode, eff, effectContext, diagnostics))
                    return {};
                thresh.effects.push_back(eff);
            }
            def.thresholds.push_back(thresh);
        }
        combos.push_back(std::move(def));
        idx++;
    }

    emitChessDiagnostic(diagnostics, ChessDiagnosticSeverity::Info, "羈絆配置", std::format("成功載入{}個羈絆", combos.size()));
    return combos;
}

ChessComboProgress evaluateChessComboProgress(
    const ChessSessionState& state,
    const ChessGameContent& content,
    const ComboDef& combo)
{
    const auto units = resolverUnits(state, content);
    const auto equipmentRules = resolverEquipmentRules(content);
    const std::array definitions{resolverDefinition(combo)};
    const auto resolved = resolveChessCombos(units, equipmentRules, definitions);
    assert(resolved.size() == 1);
    return chessComboProgress(combo, resolved.front());
}

bool chessRosterHasActiveComboEffect(
    const ChessSessionState& state,
    const ChessGameContent& content,
    EffectType effectType)
{
    const auto resolved = resolveRosterCombos(state, content);
    assert(resolved.size() == content.combos().size());
    for (std::size_t index = 0; index < content.combos().size(); ++index)
    {
        const auto& combo = content.combos()[index];
        const auto& active = resolved[index];
        assert(active.id == combo.id);
        if (active.activeThresholdIndex < 0)
        {
            continue;
        }
        const auto& threshold = combo.thresholds[active.activeThresholdIndex];
        if (std::ranges::any_of(threshold.effects, [&](const ComboEffect& effect) {
                return effect.type == effectType;
            }))
        {
            return true;
        }
    }
    return false;
}

ChessComboGoldBonus resolveChessComboGoldBonus(
    const ChessSessionState& state,
    const ChessGameContent& content,
    const std::set<int>& survivingChessInstanceIds)
{
    int maximumSurvivorStar = 0;
    for (const int instanceId : survivingChessInstanceIds)
    {
        maximumSurvivorStar = std::max(maximumSurvivorStar, state.roster.at(instanceId).star);
    }
    if (maximumSurvivorStar == 0)
    {
        return {};
    }

    const auto resolved = resolveRosterCombos(state, content);
    assert(resolved.size() == content.combos().size());
    for (std::size_t index = 0; index < content.combos().size(); ++index)
    {
        const auto& combo = content.combos()[index];
        const auto& active = resolved[index];
        assert(active.id == combo.id);
        if (active.activeThresholdIndex < 0)
        {
            continue;
        }
        const auto& threshold = combo.thresholds[active.activeThresholdIndex];
        for (const auto& effect : threshold.effects)
        {
            if (effect.type != EffectType::GoldCoefficient)
            {
                continue;
            }
            const bool comboMemberSurvived = std::ranges::any_of(
                 survivingChessInstanceIds,
                 [&](int instanceId) {
                     return active.memberRoleIds.contains(state.roster.at(instanceId).roleId);
                 });
            if (comboMemberSurvived)
            {
                return {maximumSurvivorStar * effect.value, combo.id};
            }
        }
    }
    return {};
}

int calculateChessComboGoldBonus(
    const ChessSessionState& state,
    const ChessGameContent& content,
    const std::set<int>& survivingChessInstanceIds)
{
    return resolveChessComboGoldBonus(state, content, survivingChessInstanceIds).amount;
}


}  // namespace KysChess
