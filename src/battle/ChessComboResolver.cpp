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
    std::span<const ChessComboResolverDefinition> definitions)
{
    std::map<int, int> starByRole;
    std::map<int, int> costByRole;
    std::map<int, std::vector<int>> unitIdsByRole;
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
    }

    std::vector<ResolvedChessCombo> result;
    result.reserve(definitions.size());
    for (const auto& definition : definitions)
    {
        std::map<int, std::vector<int>> equipmentItemsByRole;
        std::set<int> qualifyingRoleIds;
        for (const auto& unit : units)
        {
            bool qualifiesByEquipment = false;
            for (const auto& rule : equipmentRules)
            {
                if (!equipmentRuleApplies(rule, unit, definition.name))
                {
                    continue;
                }
                equipmentItemsByRole[unit.roleId].push_back(rule.equipmentItemId);
                qualifiesByEquipment = true;
            }
            if (std::ranges::contains(definition.memberRoleIds, unit.roleId)
                || qualifiesByEquipment)
            {
                qualifyingRoleIds.insert(unit.roleId);
            }
        }
        for (auto& [roleId, equipmentItems] : equipmentItemsByRole)
        {
            std::ranges::sort(equipmentItems);
            equipmentItems.erase(std::unique(equipmentItems.begin(), equipmentItems.end()), equipmentItems.end());
        }

        ResolvedChessCombo resolved;
        resolved.id = definition.id;
        resolved.isAntiCombo = definition.isAntiCombo;
        for (const int roleId : qualifyingRoleIds)
        {
            resolved.memberRoleIds.insert(roleId);
            ResolvedChessComboContribution contribution;
            contribution.roleId = roleId;
            const auto unitIds = unitIdsByRole.find(roleId);
            if (unitIds != unitIdsByRole.end())
            {
                contribution.unitIds = unitIds->second;
            }
            contribution.countedStar = starByRole.at(roleId);
            contribution.starBonusPoints = definition.starSynergyBonus
                ? contribution.countedStar - 1
                : 0;
            contribution.naturalMember = std::ranges::contains(definition.memberRoleIds, roleId);
            const auto equipmentItems = equipmentItemsByRole.find(roleId);
            if (equipmentItems != equipmentItemsByRole.end())
            {
                contribution.equipmentItemIds = equipmentItems->second;
            }
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
