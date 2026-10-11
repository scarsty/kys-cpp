#pragma once

#include <optional>
#include <span>
#include <set>
#include <string>
#include <vector>

namespace KysChess
{

struct ChessComboModifiers
{
    int selectedComboId = -1;
    int selectedComboBonus{};
    int reducedThresholdMinimum{};
    int thresholdReduction{};

    auto operator<=>(const ChessComboModifiers&) const = default;
};

struct ChessComboResolverUnit
{
    int roleId = -1;
    int star = 1;
    std::optional<int> cost;
    int weaponItemId = -1;
    int armorItemId = -1;
    int unitId = -1;
};

struct ResolvedChessComboContribution
{
    int roleId = -1;
    std::vector<int> unitIds;
    int countedStar = 1;
    int physicalPoints = 1;
    int starBonusPoints{};
    bool naturalMember{};
    std::vector<int> equipmentItemIds;

    auto operator<=>(const ResolvedChessComboContribution&) const = default;
};

struct ChessComboResolverEquipmentRule
{
    int equipmentItemId = -1;
    std::vector<int> roleIds;
    std::vector<std::string> comboNames;
};

struct ChessComboResolverDefinition
{
    int id = -1;
    std::string name;
    std::vector<int> memberRoleIds;
    std::vector<int> thresholdCounts;
    bool isAntiCombo = false;
    bool starSynergyBonus = false;
};

struct ResolvedChessCombo
{
    int id = -1;
    std::set<int> memberRoleIds;
    int physicalMemberCount{};
    int effectiveMemberCount{};
    int activeThresholdIndex = -1;
    int nextThresholdIndex = -1;
    bool isAntiCombo = false;
    std::vector<ResolvedChessComboContribution> contributions;
    std::vector<int> requiredCounts;
};

std::vector<ResolvedChessCombo> resolveChessCombos(
    std::span<const ChessComboResolverUnit> units,
    std::span<const ChessComboResolverEquipmentRule> equipmentRules,
    std::span<const ChessComboResolverDefinition> definitions,
    const ChessComboModifiers& modifiers = {});

}
