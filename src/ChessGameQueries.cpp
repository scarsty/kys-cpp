#include "ChessGameQueries.h"

#include "ChessManagementRules.h"
#include "ChessProgressionRules.h"
#include "ChessReplayJournal.h"

#include <algorithm>
#include <cassert>
#include <map>
#include <ranges>

namespace KysChess
{

ChessGameplayObservation queryChessGameplayObservation(
    const ChessSessionState& state,
    const ChessGameContent& content,
    const ChessRunRandom& random)
{
    ChessGameplayObservation observation;
    observation.phase = state.phase;
    observation.difficulty = content.difficulty();
    observation.options = state.options;
    observation.money = state.money;
    observation.interestGold = ChessProgressionRules::interestGold(state, content);
    observation.nextInterestThreshold = ChessProgressionRules::nextInterestThreshold(state, content);
    observation.maximumInterestGold = content.balance().interestMax;
    observation.projectedBaseVictoryGold = ChessProgressionRules::baseVictoryGold(state, content);
    observation.projectedVictoryIncome = ChessProgressionRules::projectedVictoryIncome(state, content);
    observation.experience = state.experience;
    observation.experienceForNextLevel = ChessManagementRules::experienceForNextLevel(state, content);
    observation.level = state.level;
    observation.maximumDeployment = ChessManagementRules::maximumDeployment(state, content);
    observation.maximumBanCount = ChessManagementRules::maximumBanCount(state, content);
    observation.fight = state.fight;
    observation.campaignComplete = state.campaignComplete;
    observation.shopLocked = state.shopLocked;
    observation.freeShopRefreshAvailable = state.freeShopRefreshAvailable;
    observation.freeShopRefreshGrantedFight = state.freeShopRefreshGrantedFight;
    observation.shop = state.shop;
    for (const auto& [id, piece] : state.roster)
    {
        observation.roster.push_back(piece);
    }
    for (const auto& [id, equipment] : state.equipmentInventory)
    {
        observation.equipmentInventory.push_back(equipment);
    }
    observation.bans.assign(state.bannedRoleIds.begin(), state.bannedRoleIds.end());
    observation.seenRoles.assign(state.seenRoleIds.begin(), state.seenRoleIds.end());
    observation.obtainedNeigongIds.assign(
        state.obtainedNeigongIds.begin(),
        state.obtainedNeigongIds.end());
    observation.completedChallengeNames.assign(
        state.completedChallengeNames.begin(),
        state.completedChallengeNames.end());
    for (const auto& combo : content.combos())
    {
        const auto progress = evaluateChessComboProgress(state, content, combo);
        observation.combos.push_back({
            combo.id,
            progress.physicalCount,
            progress.effectiveCount,
            progress.activeThresholdIndex,
            progress.nextThresholdIndex,
            progress.contributions,
        });
    }
    observation.preparedBattle = state.preparedBattle;
    if (!state.pendingRewards.empty())
    {
        observation.pendingReward = state.pendingRewards.front();
    }
    observation.lastBattleOutcome = state.lastBattleOutcome;
    observation.lastBattleEndFrame = state.lastBattleEndFrame;
    observation.lastBattleDigest = state.lastBattleDigest;
    observation.stateHash = canonicalChessStateHash(state, random);
    return observation;
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
    int slotIndex,
    const ChessActionValidator& validate)
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
    for (int star = 1; star <= 3; ++star)
    {
        const int count = static_cast<int>(std::ranges::count_if(
            state.roster,
            [&](const auto& entry) {
                return entry.second.roleId == role->ID && entry.second.star == star;
            }));
        result.copiesByStar.push_back({star, count});
        result.ownedCopies += count * (star == 1 ? 1 : star == 2 ? 3 : 9);
    }
    result.goldCost = ChessManagementRules::pieceValue(content, role->ID, 1);
    result.projectedGoldAfter = state.money - result.goldCost;
    ChessAction purchase;
    purchase.type = ChessActionType::BuyShopSlot;
    purchase.shopSlot = slotIndex;
    result.purchaseError = validate(purchase);

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
            const auto& merge = chessSemanticEventDetail<ChessMergeEventDetail>(event);
            if (merge.roleId == role->ID)
            {
                result.projectedResult = ChessProjectedPurchaseResult::Merge;
                result.projectedResultStar = std::max(
                    result.projectedResultStar,
                    merge.resultStar);
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
    const ChessGameContent& content,
    const ChessActionValidator& validate)
{
    ChessShopAnalysis result;
    result.money = state.money;
    result.level = state.level;
    for (int slot = 0; slot < static_cast<int>(state.shop.size()); ++slot)
    {
        result.slots.push_back(queryChessShopSlot(state, content, slot, validate));
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
    std::vector<ChessEquipmentInstance> equipmentInventory;
    for (const auto& [id, equipment] : state.equipmentInventory)
    {
        equipmentInventory.push_back(equipment);
    }
    result.currentStats = chessPieceStats(content, result.piece, equipmentInventory);
    for (const auto& [instanceId, owned] : state.roster)
    {
        if (owned.roleId != result.piece.roleId)
        {
            continue;
        }
        result.oneStarEquivalentCopies += owned.star == 1 ? 1 : owned.star == 2 ? 3 : 9;
        if (owned.star == result.piece.star)
        {
            ++result.sameStarCopies;
        }
    }
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
    const std::vector<ChessActionOffer>& offers)
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
    const auto legalBan = std::ranges::find(offers, ChessActionType::AddBan, &ChessActionOffer::type);
    if (legalBan != offers.end())
    {
        for (const auto& candidate : chessActionOfferDetail<ChessRoleSelectionOffer>(*legalBan).candidates)
        {
            eligible.push_back(candidate.roleId);
        }
    }
    result.eligibleBansByCost = groupedRoles(eligible);
    result.effectTiming = "禁棋只影響之後生成或刷新的商店；目前商店既有棋子仍可購買";
    result.forcedPhaseNote = "強制禁棋只強制先解決獨佔決策階段；可選擇禁棋或略過，略過不消耗禁棋容量";
    return result;
}

}  // namespace KysChess
