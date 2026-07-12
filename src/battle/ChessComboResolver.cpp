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

bool unitQualifies(
    const ChessComboResolverUnit& unit,
    const ChessComboResolverDefinition& definition,
    std::span<const ChessComboResolverEquipmentRule> equipmentRules)
{
    return std::ranges::contains(definition.memberRoleIds, unit.roleId)
        || std::ranges::any_of(equipmentRules, [&](const auto& rule) {
            return equipmentRuleApplies(rule, unit, definition.name);
        });
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
            if (unitQualifies(unit, definition, equipmentRules))
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
            ++resolved.physicalMemberCount;
            resolved.effectiveMemberCount += definition.starSynergyBonus ? starByRole.at(roleId) : 1;
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
            if (selectedRoleId >= 0)
            {
                resolved.memberRoleIds.insert(selectedRoleId);
                resolved.physicalMemberCount = 1;
                resolved.effectiveMemberCount = 1;
                resolved.activeThresholdIndex = 0;
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
