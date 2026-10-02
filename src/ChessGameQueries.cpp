#include "ChessGameQueries.h"

#include "ChessManagementRules.h"

#include <algorithm>
#include <cassert>
#include <map>
#include <ranges>

namespace KysChess
{

ChessRoleCopiesAnalysis queryChessRoleCopies(const ChessSessionState& state, int roleId)
{
    ChessRoleCopiesAnalysis result{};
    constexpr std::array equivalents{1, 3, 9};
    for (const auto& [id, piece] : state.roster)
    {
        if (piece.roleId == roleId)
        {
            ++result.copiesByStar.at(piece.star - 1);
            result.ownedCopies += equivalents.at(piece.star - 1);
            result.highestStar = std::max(result.highestStar, piece.star);
        }
    }
    return result;
}

ChessShopOddsAnalysis queryChessShopOdds(
    const ChessSessionState& state,
    const ChessGameContent& content,
    int level)
{
    assert(level >= 0 && level < static_cast<int>(content.balance().shopWeights.size()));
    ChessShopOddsAnalysis result;
    result.level = level;
    for (int tier = 1; tier <= 5; ++tier)
    {
        const auto candidates = ChessManagementRules::shopCandidatesForTier(state, content, tier);
        if (!candidates.empty())
        {
            result.totalEffectiveWeight += content.balance().shopWeights[level][tier - 1];
        }
    }
    for (int tier = 1; tier <= 5; ++tier)
    {
        ChessShopTierOddsAnalysis tierAnalysis;
        tierAnalysis.tier = tier;
        tierAnalysis.configuredWeight = content.balance().shopWeights[level][tier - 1];
        tierAnalysis.availableRoleIds = ChessManagementRules::shopCandidatesForTier(state, content, tier);
        if (!tierAnalysis.availableRoleIds.empty() && result.totalEffectiveWeight > 0)
        {
            tierAnalysis.probability = static_cast<double>(tierAnalysis.configuredWeight)
                / result.totalEffectiveWeight;
        }
        result.tiers.push_back(std::move(tierAnalysis));
    }
    result.poolNote = "機率只在目前仍有候選角色的費用層級間重新正規化；候選已排除禁棋及本次刷新避免立即重複的角色";
    return result;
}

ChessShopSlotAnalysis queryChessShopSlot(
    const ChessSessionState& state,
    const ChessGameContent& content,
    int slotIndex)
{
    assert(slotIndex >= 0 && slotIndex < static_cast<int>(state.shop.size()));
    ChessShopSlotAnalysis result;
    result.slot = slotIndex;
    const auto& slot = state.shop[slotIndex];
    if (slot.roleId < 0)
    {
        return result;
    }
    const auto* role = content.role(slot.roleId);
    assert(role);
    result.occupied = true;
    result.roleId = role->ID;
    result.cost = role->Cost;
    result.shopTier = slot.tier;
    const auto copies = queryChessRoleCopies(state, role->ID);
    result.ownedCopies = copies.ownedCopies;
    for (int star = 1; star <= 3; ++star)
    {
        result.copiesByStar.push_back({star, copies.copiesByStar.at(star - 1)});
    }
    result.goldCost = ChessManagementRules::pieceValue(content, role->ID, 1);
    result.projectedGoldAfter = state.money - result.goldCost;
    ChessAction purchase;
    purchase.type = ChessActionType::BuyShopSlot;
    purchase.shopSlot = slotIndex;
    result.purchaseError = ChessManagementRules::validate(state, content, purchase);

    auto projected = state;
    std::vector<ChessSemanticEvent> projectedEvents;
    if (ChessManagementRules::canGrantPiece(projected, content, role->ID))
    {
        ChessManagementRules::grantPiece(projected, content, role->ID, projectedEvents);
        result.projectedResult = ChessProjectedPurchaseResult::AddOneStar;
        for (const auto& event : projectedEvents)
        {
            if (event.type != ChessSemanticEventType::ChessMerged)
            {
                continue;
            }
            if (event.secondaryId == role->ID)
            {
                result.projectedResult = ChessProjectedPurchaseResult::Merge;
                result.projectedResultStar = std::max(
                    result.projectedResultStar,
                    event.value);
            }
        }
    }
    for (const auto& combo : content.combos())
    {
        if (!std::ranges::contains(combo.memberRoleIds, role->ID))
        {
            continue;
        }
        const auto current = evaluateChessComboProgress(state, content, combo);
        const auto afterPurchase = evaluateChessComboProgress(projected, content, combo);
        result.synergies.push_back({
            combo.id,
            combo.name,
            current.effectiveCount,
            afterPurchase.effectiveCount,
        });
    }
    const auto odds = queryChessShopOdds(state, content, state.level);
    result.levelTierProbability = odds.tiers.at(slot.tier - 1).probability;
    return result;
}

ChessShopAnalysis queryChessShop(
    const ChessSessionState& state,
    const ChessGameContent& content)
{
    ChessShopAnalysis result;
    result.money = state.money;
    result.level = state.level;
    for (int slot = 0; slot < static_cast<int>(state.shop.size()); ++slot)
    {
        result.slots.push_back(queryChessShopSlot(state, content, slot));
    }
    result.odds = queryChessShopOdds(state, content, state.level);
    return result;
}

ChessInstanceAnalysis queryChessInstance(
    const ChessSessionState& state,
    const ChessGameContent& content,
    int chessInstanceId)
{
    const auto foundPiece = state.roster.find(chessInstanceId);
    assert(foundPiece != state.roster.end());
    ChessInstanceAnalysis result;
    result.piece = foundPiece->second;
    result.luckChancePercent = content.balance().talent(state.talent).luckChance(result.piece.luckStacks);
    result.currentStats = chessPieceStats(content, result.piece, content.balance().talent(state.talent).amplifiedGrowthPercent);
    const auto copies = queryChessRoleCopies(state, result.piece.roleId);
    result.oneStarEquivalentCopies = copies.ownedCopies;
    result.sameStarCopies = copies.copiesByStar.at(result.piece.star - 1);
    result.copiesRequiredForNextStar = result.piece.star >= 3
        ? 0
        : std::max(0, 3 - result.sameStarCopies);
    for (const int equipmentInstanceId : {
             result.piece.weaponInstanceId,
             result.piece.armorInstanceId,
         })
    {
        if (equipmentInstanceId >= 0)
        {
            result.equipment.push_back(state.equipmentInventory.at(equipmentInstanceId));
        }
    }
    for (const auto& definition : content.combos())
    {
        const auto progress = evaluateChessComboProgress(state, content, definition);
        const auto contribution = std::ranges::find_if(
            progress.contributions,
            [&](const auto& item) {
                return std::ranges::contains(item.unitIds, chessInstanceId);
            });
        if (contribution != progress.contributions.end())
        {
            result.synergyContributions.push_back(chessComboMetadata(
                content,
                definition,
                progress.physicalCount,
                progress.effectiveCount,
                progress.activeThresholdIndex,
                progress.nextThresholdIndex,
                progress.contributions));
        }
    }
    return result;
}

ChessBanAnalysis queryChessBans(
    const ChessSessionState& state,
    const ChessGameContent& content,
    const std::vector<ChessLegalActionDescriptor>& legalActions)
{
    ChessBanAnalysis result;
    result.currentBanCount = static_cast<int>(state.bannedRoleIds.size());
    result.maximumBanCount = ChessManagementRules::maximumBanCount(state, content);
    result.remainingBanCapacity = std::max(0, result.maximumBanCount - result.currentBanCount);
    const auto groupedRoles = [&](const std::vector<int>& roleIds) {
        std::map<int, std::vector<int>> groups;
        for (const int roleId : roleIds)
        {
            const auto* role = content.role(roleId);
            assert(role);
            groups[role->Cost].push_back(roleId);
        }
        std::vector<ChessBanTierGroup> grouped;
        for (auto& [cost, roles] : groups)
        {
            grouped.push_back({cost, std::move(roles)});
        }
        return grouped;
    };
    result.currentBansByCost = groupedRoles(std::vector<int>(
        state.bannedRoleIds.begin(),
        state.bannedRoleIds.end()));
    std::vector<int> eligible;
    const auto legalBan = std::ranges::find(
        legalActions,
        ChessActionType::AddBan,
        &ChessLegalActionDescriptor::type);
    if (legalBan != legalActions.end())
    {
        eligible = legalBan->candidateIds;
    }
    result.eligibleBansByCost = groupedRoles(eligible);
    result.effectTiming = "禁棋只影響之後生成或刷新的商店；目前商店既有棋子仍可購買";
    result.forcedPhaseNote = "強制禁棋必須在目前獎勵階段使用；可選擇禁棋或放棄，放棄的次數不會保留";
    return result;
}

}  // namespace KysChess
