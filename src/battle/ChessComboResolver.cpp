#include "ChessComboResolver.h"

#include <algorithm>
#include <cassert>
#include <map>

namespace KysChess
{
namespace
{

bool equipmentRuleApplies(
    const ChessComboResolverEquipmentRule& rule,
    const ChessComboResolverUnit& unit,
    const std::string& comboName)
{
    if (rule.equipmentItemId != unit.weaponItemId
        && rule.equipmentItemId != unit.armorItemId)
    {
        return false;
    }
    if (!rule.roleIds.empty() && !std::ranges::contains(rule.roleIds, unit.roleId))
    {
        return false;
    }
    return std::ranges::contains(rule.comboNames, comboName);
}

std::vector<int> qualifyingEquipmentItems(
    const ChessComboResolverUnit& unit,
    const ChessComboResolverDefinition& definition,
    std::span<const ChessComboResolverEquipmentRule> equipmentRules)
{
    std::vector<int> result;
    for (const auto& rule : equipmentRules)
    {
        if (equipmentRuleApplies(rule, unit, definition.name))
        {
            result.push_back(rule.equipmentItemId);
        }
    }
    std::ranges::sort(result);
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

}

std::vector<ResolvedChessCombo> resolveChessCombos(
    std::span<const ChessComboResolverUnit> units,
    std::span<const ChessComboResolverEquipmentRule> equipmentRules,
    std::span<const ChessComboResolverDefinition> definitions)
{
    std::vector<ResolvedChessCombo> result;
    result.reserve(definitions.size());
    for (const auto& definition : definitions)
    {
        std::map<int, int> starByRole;
        std::map<int, int> costByRole;
        std::map<int, std::vector<int>> unitIdsByRole;
        std::map<int, std::vector<int>> equipmentItemsByRole;
        std::set<int> qualifyingRoleIds;
        for (const auto& unit : units)
        {
            assert(unit.roleId >= 0);
            assert(unit.star >= 1);
            starByRole[unit.roleId] = unit.star;
            if (unit.cost)
            {
                costByRole[unit.roleId] = *unit.cost;
            }
            if (unit.unitId >= 0)
            {
                unitIdsByRole[unit.roleId].push_back(unit.unitId);
            }
            auto equipmentItems = qualifyingEquipmentItems(unit, definition, equipmentRules);
            if (!equipmentItems.empty())
            {
                auto& aggregated = equipmentItemsByRole[unit.roleId];
                aggregated.insert(aggregated.end(), equipmentItems.begin(), equipmentItems.end());
                std::ranges::sort(aggregated);
                aggregated.erase(std::unique(aggregated.begin(), aggregated.end()), aggregated.end());
            }
            if (std::ranges::contains(definition.memberRoleIds, unit.roleId)
                || !equipmentItems.empty())
            {
                qualifyingRoleIds.insert(unit.roleId);
            }
        }

        ResolvedChessCombo resolved;
        resolved.id = definition.id;
        resolved.isAntiCombo = definition.isAntiCombo;
        for (const int roleId : qualifyingRoleIds)
        {
            resolved.memberRoleIds.insert(roleId);
            ResolvedChessComboContribution contribution;
            contribution.roleId = roleId;
            contribution.unitIds = unitIdsByRole[roleId];
            contribution.countedStar = starByRole.at(roleId);
            contribution.starBonusPoints = definition.starSynergyBonus
                ? contribution.countedStar - 1
                : 0;
            contribution.naturalMember = std::ranges::contains(definition.memberRoleIds, roleId);
            contribution.equipmentItemIds = equipmentItemsByRole[roleId];
            resolved.physicalMemberCount += contribution.physicalPoints;
            resolved.effectiveMemberCount += contribution.physicalPoints + contribution.starBonusPoints;
            resolved.contributions.push_back(std::move(contribution));
        }

        if (definition.thresholdCounts.empty())
        {
            result.push_back(std::move(resolved));
            continue;
        }
        if (definition.isAntiCombo)
        {
            int selectedRoleId = -1;
            int selectedCost = -1;
            for (const int roleId : resolved.memberRoleIds)
            {
                assert(costByRole.contains(roleId));
                const int cost = costByRole.at(roleId);
                if (cost > selectedCost)
                {
                    selectedRoleId = roleId;
                    selectedCost = cost;
                }
            }
            resolved.memberRoleIds.clear();
            resolved.contributions.erase(
                std::remove_if(
                    resolved.contributions.begin(),
                    resolved.contributions.end(),
                    [&](const auto& contribution) { return contribution.roleId != selectedRoleId; }),
                resolved.contributions.end());
            if (selectedRoleId >= 0)
            {
                resolved.memberRoleIds.insert(selectedRoleId);
                resolved.physicalMemberCount = 1;
                resolved.effectiveMemberCount = 1;
                resolved.activeThresholdIndex = 0;
                assert(resolved.contributions.size() == 1);
                resolved.contributions.front().physicalPoints = 1;
                resolved.contributions.front().starBonusPoints = 0;
            }
            else
            {
                resolved.physicalMemberCount = 0;
                resolved.effectiveMemberCount = 0;
            }
            result.push_back(std::move(resolved));
            continue;
        }

        for (int index = 0; index < static_cast<int>(definition.thresholdCounts.size()); ++index)
        {
            if (resolved.effectiveMemberCount >= definition.thresholdCounts[index])
            {
                resolved.activeThresholdIndex = index;
                continue;
            }
            resolved.nextThresholdIndex = index;
            break;
        }
        result.push_back(std::move(resolved));
    }
    return result;
}

}
