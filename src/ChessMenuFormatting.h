#pragma once

#include "Color.h"
#include "ChessCatalogQueries.h"
#include "ChessGameContent.h"
#include "ChessManagementRules.h"
#include "ChessRewardRules.h"
#include "ChessScreenLayout.h"

#include <algorithm>
#include <cassert>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace KysChess
{

struct ChessMenuColumnRow
{
    std::string prefix;
    std::string name;
    std::string middle;
    std::string right;
};

struct ChessMenuColumnMinimumWidths
{
    int name = ChessScreenLayout::kMenuNameUnits;
    int middle = ChessScreenLayout::kMenuStarUnits;
    int right = ChessScreenLayout::kMenuCostUnits;
};

inline std::string chessMenuPrefixWithSeparator(std::string prefix)
{
    if (!prefix.empty() && prefix.back() != ' ')
    {
        prefix += ' ';
    }
    return prefix;
}

inline std::string chessEquipmentAssignmentColumn(bool assigned)
{
    return assigned ? " [已裝]" : std::string{};
}

inline std::string chessRewardOptionCostColumn(const ChessRewardOption& option)
{
    return option.goldCost > 0 ? std::format("+${}", option.goldCost) : std::string{};
}

inline std::string chessStarUpgradeRewardTitle(
    const ChessSessionState& state,
    const ChessPendingReward& pending)
{
    assert(pending.kind == ChessRewardKind::StarUpgrade);
    assert(!pending.options.empty());
    const auto firstPiece = state.roster.find(pending.options.front().value);
    assert(firstPiece != state.roster.end());
    const int sourceStar = firstPiece->second.star;
    const int targetStar = pending.options.front().value2;
    assert(std::ranges::all_of(pending.options, [&](const ChessRewardOption& option) {
        const auto piece = state.roster.find(option.value);
        return piece != state.roster.end()
            && piece->second.star == sourceStar
            && option.value2 == targetStar;
    }));
    return std::format("選擇升星 {}★→{}★", sourceStar, targetStar);
}

struct ChessMenuOutlineStyle
{
    Color color{};
    bool animated = false;
    int thickness = 1;
};

enum class ChessShopRowEmphasis
{
    None,
    Owned,
    Merge,
};

inline ChessShopRowEmphasis chessShopRowEmphasis(const ChessSessionState& state, int roleId)
{
    if (ChessManagementRules::wouldGrantPieceMerge(state, roleId))
    {
        return ChessShopRowEmphasis::Merge;
    }

    int matchingPieces = 0;
    bool hasOtherStarPiece = false;
    for (const auto& entry : state.roster)
    {
        const auto& piece = entry.second;
        if (piece.roleId == roleId)
        {
            ++matchingPieces;
            hasOtherStarPiece = hasOtherStarPiece || piece.star != 3;
        }
    }
    if (matchingPieces == 0 || (matchingPieces == 1 && !hasOtherStarPiece))
    {
        return ChessShopRowEmphasis::None;
    }
    return ChessShopRowEmphasis::Owned;
}

inline ChessMenuOutlineStyle chessShopRowOutlineStyle(ChessShopRowEmphasis emphasis)
{
    switch (emphasis)
    {
    case ChessShopRowEmphasis::None:
        return {};
    case ChessShopRowEmphasis::Owned:
        return {{100, 200, 255, 220}, true, 1};
    case ChessShopRowEmphasis::Merge:
        return {{255, 215, 0, 255}, true, 3};
    }
    std::unreachable();
}

struct ChessBuyExpDeploymentPreview
{
    int current{};
    std::optional<int> next;
};

inline ChessBuyExpDeploymentPreview buildChessBuyExpDeploymentPreview(
    const ChessSessionState& state,
    const ChessGameContent& content)
{
    assert(state.level < content.balance().maxLevel);
    ChessBuyExpDeploymentPreview preview;
    preview.current = ChessManagementRules::maximumDeployment(state, content);
    const int next = ChessManagementRules::maximumDeploymentAtLevel(content, state.level + 1);
    if (next > preview.current)
    {
        preview.next = next;
    }
    return preview;
}

inline std::vector<int> buildChessPoolBrowseOrder(const ChessGameContent& content)
{
    std::vector<int> roleIds = content.poolRoleIds();
    std::stable_sort(roleIds.begin(), roleIds.end(), [&](int leftId, int rightId) {
        const auto* left = content.role(leftId);
        const auto* right = content.role(rightId);
        assert(left && right);
        return left->Cost < right->Cost;
    });
    return roleIds;
}

struct ChessBanManagementRow
{
    int roleId{};
    bool banned = false;
};

inline std::vector<ChessBanManagementRow> buildChessBanManagementRows(
    const ChessGameplayObservation& observation,
    const ChessGameContent& content)
{
    std::vector<int> roleIds = observation.seenRoles;
    roleIds.insert(roleIds.end(), observation.bans.begin(), observation.bans.end());
    std::ranges::sort(roleIds, [&](int leftId, int rightId) {
        const auto* left = content.role(leftId);
        const auto* right = content.role(rightId);
        assert(left && right);
        return left->Cost != right->Cost ? left->Cost < right->Cost : leftId < rightId;
    });

    roleIds.erase(std::unique(roleIds.begin(), roleIds.end()), roleIds.end());

    std::vector<ChessBanManagementRow> rows;
    rows.reserve(roleIds.size());
    for (const int roleId : roleIds)
    {
        rows.push_back({roleId, std::ranges::contains(observation.bans, roleId)});
    }
    return rows;
}

struct ChessEquipmentMenuOrder
{
    bool owned{};
    int tier{};
    int itemId{};

    bool operator==(const ChessEquipmentMenuOrder&) const = default;
};

inline bool chessEquipmentMenuOrderLess(
    const ChessEquipmentMenuOrder& left,
    const ChessEquipmentMenuOrder& right)
{
    if (left.owned != right.owned)
    {
        return left.owned;
    }
    if (left.tier != right.tier)
    {
        return left.tier < right.tier;
    }
    return left.itemId < right.itemId;
}

template <typename DisplayWidth>
std::string padRightToDisplayWidth(std::string text, int targetWidth, DisplayWidth displayWidth)
{
    while (displayWidth(text) < targetWidth)
    {
        text += ' ';
    }
    return text;
}

template <typename DisplayWidth>
std::string padLeftToDisplayWidth(std::string text, int targetWidth, DisplayWidth displayWidth)
{
    while (displayWidth(text) < targetWidth)
    {
        text = ' ' + text;
    }
    return text;
}

template <typename DisplayWidth>
std::vector<std::string> buildAlignedChessMenuLabels(
    const std::vector<ChessMenuColumnRow>& rows,
    DisplayWidth displayWidth,
    ChessMenuColumnMinimumWidths minimumWidths = {})
{
    int nameWidth = minimumWidths.name;
    int middleWidth = minimumWidths.middle;
    int rightWidth = minimumWidths.right;
    for (const auto& row : rows)
    {
        nameWidth = std::max(nameWidth, displayWidth(row.prefix + row.name));
        middleWidth = std::max(middleWidth, displayWidth(row.middle));
        rightWidth = std::max(rightWidth, displayWidth(row.right));
    }

    std::vector<std::string> labels;
    labels.reserve(rows.size());
    for (const auto& row : rows)
    {
        labels.push_back(
            padRightToDisplayWidth(row.prefix + row.name, nameWidth, displayWidth)
            + padRightToDisplayWidth(row.middle, middleWidth, displayWidth)
            + padLeftToDisplayWidth(row.right, rightWidth, displayWidth));
    }
    return labels;
}

template <typename DisplayWidth>
std::vector<std::string> buildContentSizedAlignedChessMenuLabels(
    const std::vector<ChessMenuColumnRow>& rows,
    DisplayWidth displayWidth)
{
    return buildAlignedChessMenuLabels(
        rows,
        displayWidth,
        ChessMenuColumnMinimumWidths{0, 0, 0});
}

template <typename DisplayWidth>
std::vector<std::string> buildAlignedChessRewardMenuLabels(
    ChessRewardKind rewardKind,
    const std::vector<ChessMenuColumnRow>& rows,
    DisplayWidth displayWidth)
{
    if (rewardKind == ChessRewardKind::Equipment)
    {
        return buildContentSizedAlignedChessMenuLabels(rows, displayWidth);
    }
    return buildAlignedChessMenuLabels(rows, displayWidth);
}

template <typename DisplayWidth>
std::vector<std::string> buildAlignedComboCatalogLabels(
    const std::vector<std::pair<std::string, std::string>>& rows,
    DisplayWidth displayWidth)
{
    int nameWidth = 0;
    int countWidth = 0;
    for (const auto& [name, count] : rows)
    {
        nameWidth = std::max(nameWidth, displayWidth(name));
        countWidth = std::max(countWidth, displayWidth(count));
    }

    std::vector<std::string> labels;
    labels.reserve(rows.size());
    for (const auto& [name, count] : rows)
    {
        labels.push_back(std::format(
            "{} ({})",
            padRightToDisplayWidth(name, nameWidth, displayWidth),
            padRightToDisplayWidth(count, countWidth, displayWidth)));
    }
    return labels;
}

inline std::string chessStars(int star)
{
    std::string result;
    for (int index = 0; index < star; ++index)
    {
        result += "★";
    }
    return result;
}

inline std::string chessVictoryComboBonusText(
    const ChessGameContent& content,
    const ChessSemanticEvent& goldEvent,
    int bonus)
{
    assert(goldEvent.type == ChessSemanticEventType::GoldAwarded);
    assert(bonus > 0);
    const ComboDef* source = nullptr;
    for (const auto& combo : content.combos())
    {
        if (goldEvent.stableId == std::format("combo:{}", combo.id))
        {
            source = &combo;
            break;
        }
    }
    return std::format("({}+${})", source ? source->name : "羈絆", bonus);
}

inline const char* chessChallengeCompletionLabel(bool completed)
{
    return completed ? "[已通關]" : "";
}

inline Color chessChallengeMenuRowColor(bool completed)
{
    return completed ? Color{120, 120, 120, 255} : Color{255, 200, 100, 255};
}

inline std::vector<std::string> chessChallengeBrowseNames(const ChessGameContent& content)
{
    std::vector<std::string> names;
    names.reserve(content.balance().challenges.size());
    for (const auto& challenge : content.balance().challenges)
    {
        names.push_back(challenge.name);
    }
    return names;
}

inline std::string chessChallengeEnemyDescription(
    const ChessGameContent& content,
    const BalanceConfig::ChallengeEnemy& enemy)
{
    const auto* role = content.role(enemy.roleId);
    assert(role);
    auto description = std::format("  {} {} ({}費)", role->Name, chessStars(enemy.star), role->Cost);
    if (enemy.weaponId >= 0)
    {
        description += std::format("　{}", chessItemDisplayName(content, enemy.weaponId));
    }
    if (enemy.armorId >= 0)
    {
        description += std::format("{}{}", enemy.weaponId >= 0 ? "／" : "　", chessItemDisplayName(content, enemy.armorId));
    }
    return description;
}

}    // namespace KysChess
