#pragma once

#include "Color.h"
#include "ChessCatalogQueries.h"
#include "ChessGameContent.h"
#include "ChessManagementRules.h"
#include "ChessRewardRules.h"

#include <algorithm>
#include <cassert>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace KysChess
{

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
            hasOtherStarPiece = hasOtherStarPiece || piece.star < 3;
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
    auto nextState = state;
    ++nextState.level;
    const int next = ChessManagementRules::maximumDeployment(nextState, content);
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

}    // namespace KysChess
