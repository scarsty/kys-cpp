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

}

std::vector<ResolvedChessCombo> resolveChessCombos(
    std::span<const ChessComboResolverUnit> units,
    std::span<const ChessComboResolverEquipmentRule> equipmentRules,
    std::span<const ChessComboResolverDefinition> definitions,
    const ChessComboModifiers& modifiers)
{
    assert(modifiers.selectedComboBonus >= 0 && modifiers.selectedComboBonus <= 1);
    assert(modifiers.thresholdReduction >= 0);
    assert(modifiers.thresholdReduction == 0 || modifiers.thresholdReduction < modifiers.reducedThresholdMinimum);
    std::map<int, int> costByRole;
    for (const auto& unit : units)
    {
        assert(unit.roleId >= 0);
        assert(unit.star >= 1);
        if (unit.cost)
        {
            costByRole[unit.roleId] = *unit.cost;
        }
    }

    std::vector<ResolvedChessCombo> result;
    result.reserve(definitions.size());
    for (const auto& definition : definitions)
    {
        std::map<int, ResolvedChessComboContribution> contributionsByRole;
        for (const auto& unit : units)
        {
            std::vector<int> equipmentItems;
            for (const auto& rule : equipmentRules)
            {
                if (!equipmentRuleApplies(rule, unit, definition.name))
                {
                    continue;
                }
                equipmentItems.push_back(rule.equipmentItemId);
            }
            const bool naturalMember = std::ranges::contains(definition.memberRoleIds, unit.roleId);
            if (!naturalMember && equipmentItems.empty())
            {
                continue;
            }
            auto& contribution = contributionsByRole[unit.roleId];
            contribution.roleId = unit.roleId;
            contribution.countedStar = unit.star;
            contribution.naturalMember = naturalMember;
            contribution.equipmentItemIds.insert(
                contribution.equipmentItemIds.end(), equipmentItems.begin(), equipmentItems.end());
            if (unit.unitId >= 0)
            {
                contribution.unitIds.push_back(unit.unitId);
            }
        }

        ResolvedChessCombo resolved;
        resolved.id = definition.id;
        resolved.isAntiCombo = definition.isAntiCombo;
        for (const int count : definition.thresholdCounts)
        {
            resolved.requiredCounts.push_back(!definition.isAntiCombo && count >= modifiers.reducedThresholdMinimum
                ? count - modifiers.thresholdReduction : count);
        }
        for (auto& [roleId, contribution] : contributionsByRole)
        {
            resolved.memberRoleIds.insert(roleId);
            contribution.starBonusPoints = definition.starSynergyBonus
                ? contribution.countedStar - 1
                : 0;
            auto& equipmentItems = contribution.equipmentItemIds;
            std::ranges::sort(equipmentItems);
            equipmentItems.erase(std::unique(equipmentItems.begin(), equipmentItems.end()), equipmentItems.end());
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

        if (definition.id == modifiers.selectedComboId)
        {
            resolved.effectiveMemberCount += modifiers.selectedComboBonus;
        }
        for (int index = 0; index < static_cast<int>(resolved.requiredCounts.size()); ++index)
        {
            if (resolved.effectiveMemberCount >= resolved.requiredCounts[index])
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
