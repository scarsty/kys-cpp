#pragma once

#include <optional>
#include <span>
#include <set>
#include <string>
#include <vector>

namespace KysChess
{

struct ChessComboResolverUnit
{
    int roleId = -1;
    int star = 1;
    std::optional<int> cost;
    int weaponItemId = -1;
    int armorItemId = -1;
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
};

std::vector<ResolvedChessCombo> resolveChessCombos(
    std::span<const ChessComboResolverUnit> units,
    std::span<const ChessComboResolverEquipmentRule> equipmentRules,
    std::span<const ChessComboResolverDefinition> definitions);

}
