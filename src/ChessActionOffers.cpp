#include "ChessActionOffers.h"

#include "ChessGameContent.h"
#include "ChessManagementRules.h"

#include <algorithm>
#include <cassert>
#include <ranges>
#include <utility>

namespace KysChess
{
namespace
{

ChessActionOffer noSelectionOffer(ChessActionType type)
{
    ChessActionOffer result;
    result.type = type;
    result.payload = ChessNoSelectionOffer{};
    return result;
}

ChessActionEconomicConsequence costConsequence(
    const ChessSessionState& state,
    int cost)
{
    ChessActionEconomicConsequence result;
    result.currentGold = state.money;
    result.goldCost = cost;
    result.projectedGoldAfter = state.money - cost;
    return result;
}

}  // namespace

std::vector<ChessActionOffer> buildChessActionOffers(
    const ChessSessionState& state,
    const ChessGameContent& content,
    const ChessActionValidator& validate)
{
    if (state.phase == ChessSessionPhase::Complete)
    {
        return {};
    }
    if (state.phase == ChessSessionPhase::BattlePreparation)
    {
        assert(state.preparedBattle);
        std::vector<ChessActionOffer> result;
        if (state.preparedBattle->chosenMapId < 0
            && !state.preparedBattle->mapCandidates.empty())
        {
            ChessActionOffer choose;
            choose.type = ChessActionType::ChooseMap;
            choose.minimumSelection = 1;
            choose.maximumSelection = 1;
            ChessMapSelectionOffer detail;
            for (const int mapId : state.preparedBattle->mapCandidates)
            {
                detail.candidates.push_back({mapId});
            }
            choose.payload = std::move(detail);
            result.push_back(std::move(choose));
        }
        if (state.options.positionSwapEnabled)
        {
            ChessActionOffer swap;
            swap.type = ChessActionType::SwapPositions;
            swap.minimumSelection = 2;
            swap.maximumSelection = 2;
            ChessPositionSwapOffer detail;
            for (const auto& unit : state.preparedBattle->units)
            {
                if (unit.team == 0)
                {
                    detail.candidates.push_back({unit.unitId, unit.roleId, unit.x, unit.y});
                }
            }
            swap.payload = std::move(detail);
            result.push_back(std::move(swap));
        }
        if (state.preparedBattle->chosenMapId >= 0 || state.preparedBattle->mapCandidates.empty())
        {
            result.push_back(noSelectionOffer(ChessActionType::StartBattle));
        }
        return result;
    }
    if (state.phase == ChessSessionPhase::RewardChoice)
    {
        assert(!state.pendingRewards.empty());
        const auto& pending = state.pendingRewards.front();
        if (pending.kind == ChessRewardKind::ForcedBan)
        {
            ChessActionOffer ban;
            ban.type = ChessActionType::AddBan;
            ban.minimumSelection = 1;
            ban.maximumSelection = 1;
            ChessRoleSelectionOffer detail;
            for (const auto& option : pending.options)
            {
                ChessAction action;
                action.type = ChessActionType::AddBan;
                action.roleId = option.value;
                if (validate(action) == ChessRuleErrorCode::None)
                {
                    detail.candidates.push_back({option.value});
                }
            }
            ban.payload = std::move(detail);
            return {std::move(ban), noSelectionOffer(ChessActionType::SkipForcedBans)};
        }
        ChessActionOffer choose;
        choose.type = ChessActionType::ChooseReward;
        choose.minimumSelection = 1;
        choose.maximumSelection = 1;
        ChessRewardSelectionOffer selection;
        for (const auto& option : pending.options)
        {
            selection.candidates.push_back({option.id, option.kind, option.value, option.value2});
        }
        choose.payload = std::move(selection);
        std::vector<ChessActionOffer> result{std::move(choose)};
        ChessAction reroll;
        reroll.type = ChessActionType::RerollReward;
        if (validate(reroll) == ChessRuleErrorCode::None)
        {
            auto offer = noSelectionOffer(ChessActionType::RerollReward);
            auto consequence = costConsequence(state, pending.rerollCost);
            consequence.affectedOptionCount = static_cast<int>(pending.options.size());
            offer.economicConsequence = std::move(consequence);
            result.push_back(std::move(offer));
        }
        return result;
    }
    if (state.phase != ChessSessionPhase::Management)
    {
        return {};
    }

    std::vector<ChessActionOffer> result;
    ChessAction action;
    action.type = ChessActionType::RefreshShop;
    if (validate(action) == ChessRuleErrorCode::None)
    {
        auto offer = noSelectionOffer(ChessActionType::RefreshShop);
        const int cost = state.freeShopRefreshAvailable ? 0 : content.balance().refreshCost;
        auto consequence = costConsequence(state, cost);
        consequence.affectedOptionCount = static_cast<int>(state.shop.size());
        consequence.consumesFreeShopRefresh = state.freeShopRefreshAvailable;
        offer.economicConsequence = std::move(consequence);
        result.push_back(std::move(offer));
    }

    ChessActionOffer shopLock;
    shopLock.type = ChessActionType::SetShopLocked;
    shopLock.payload = ChessToggleOffer{state.shopLocked};
    result.push_back(std::move(shopLock));

    ChessActionOffer buy;
    buy.type = ChessActionType::BuyShopSlot;
    buy.minimumSelection = 1;
    buy.maximumSelection = 1;
    ChessShopSlotSelectionOffer buyDetail;
    for (int slot = 0; slot < static_cast<int>(state.shop.size()); ++slot)
    {
        action = {};
        action.type = ChessActionType::BuyShopSlot;
        action.shopSlot = slot;
        if (validate(action) == ChessRuleErrorCode::None)
        {
            const int roleId = state.shop[slot].roleId;
            const int cost = ChessManagementRules::pieceValue(content, roleId, 1);
            buyDetail.candidates.push_back({slot, roleId, cost, state.money - cost});
        }
    }
    if (!buyDetail.candidates.empty())
    {
        buy.payload = std::move(buyDetail);
        result.push_back(std::move(buy));
    }

    ChessActionOffer sell;
    sell.type = ChessActionType::SellChess;
    sell.minimumSelection = 1;
    sell.maximumSelection = 1;
    ChessPieceSelectionOffer sellDetail;
    ChessActionOffer deployment;
    deployment.type = ChessActionType::SetDeployment;
    deployment.maximumSelection = ChessManagementRules::maximumDeployment(state, content);
    ChessPieceSelectionOffer deploymentDetail;
    for (const auto& [id, piece] : state.roster)
    {
        const int gain = ChessManagementRules::pieceValue(content, piece.roleId, piece.star);
        sellDetail.candidates.push_back({
            id,
            piece.roleId,
            piece.star,
            piece.deployed,
            gain,
            state.money + gain,
        });
        deploymentDetail.candidates.push_back({
            id,
            piece.roleId,
            piece.star,
            piece.deployed,
            {},
            {},
        });
    }
    if (!sellDetail.candidates.empty())
    {
        sell.payload = std::move(sellDetail);
        result.push_back(std::move(sell));
    }
    deployment.payload = std::move(deploymentDetail);
    result.push_back(std::move(deployment));

    if (state.bannedRoleIds.size()
        < static_cast<std::size_t>(ChessManagementRules::maximumBanCount(state, content)))
    {
        ChessActionOffer ban;
        ban.type = ChessActionType::AddBan;
        ban.minimumSelection = 1;
        ban.maximumSelection = 1;
        ChessRoleSelectionOffer banDetail;
        for (const int roleId : state.seenRoleIds)
        {
            action = {};
            action.type = ChessActionType::AddBan;
            action.roleId = roleId;
            if (validate(action) == ChessRuleErrorCode::None)
            {
                banDetail.candidates.push_back({roleId});
            }
        }
        if (!banDetail.candidates.empty())
        {
            ban.payload = std::move(banDetail);
            result.push_back(std::move(ban));
        }
    }

    ChessActionOffer positionSwap;
    positionSwap.type = ChessActionType::SetPositionSwapEnabled;
    positionSwap.payload = ChessToggleOffer{state.options.positionSwapEnabled};
    result.push_back(std::move(positionSwap));

    action = {};
    action.type = ChessActionType::RerollEnemySeed;
    if (validate(action) == ChessRuleErrorCode::None)
    {
        auto offer = noSelectionOffer(ChessActionType::RerollEnemySeed);
        offer.economicConsequence = costConsequence(state, content.balance().enemyRerollCost);
        result.push_back(std::move(offer));
    }

    if (!state.equipmentInventory.empty() && !state.roster.empty())
    {
        ChessActionOffer equip;
        equip.type = ChessActionType::Equip;
        equip.minimumSelection = 1;
        equip.maximumSelection = 1;
        ChessEquipmentAssignmentOffer detail;
        std::vector<ChessEquipmentOfferCandidate> assigned;
        for (const auto& [id, equipment] : state.equipmentInventory)
        {
            ChessEquipmentOfferCandidate candidate{
                id,
                equipment.itemId,
                equipment.assignedChessInstanceId,
            };
            if (equipment.assignedChessInstanceId < 0)
            {
                detail.equipment.push_back(candidate);
            }
            else
            {
                assigned.push_back(candidate);
            }
        }
        detail.equipment.insert(detail.equipment.end(), assigned.begin(), assigned.end());
        for (const auto& [id, piece] : state.roster)
        {
            detail.targets.push_back({id, piece.roleId, piece.star, piece.deployed});
        }
        equip.payload = std::move(detail);
        result.push_back(std::move(equip));
    }

    if (content.balance().legendaryShop.unlockFight > 0
        && state.fight >= content.balance().legendaryShop.unlockFight)
    {
        ChessActionOffer legendary;
        legendary.type = ChessActionType::BuyLegendaryEquipment;
        legendary.minimumSelection = 1;
        legendary.maximumSelection = 1;
        ChessItemSelectionOffer detail;
        for (const auto& equipment : content.equipment())
        {
            if (equipment.tier != 4)
            {
                continue;
            }
            action = {};
            action.type = ChessActionType::BuyLegendaryEquipment;
            action.itemId = equipment.itemId;
            if (validate(action) == ChessRuleErrorCode::None)
            {
                const int cost = content.balance().legendaryShop.price;
                detail.candidates.push_back({equipment.itemId, cost, state.money - cost});
            }
        }
        if (!detail.candidates.empty())
        {
            legendary.payload = std::move(detail);
            auto consequence = costConsequence(state, content.balance().legendaryShop.price);
            consequence.affectedOptionCount = 1;
            legendary.economicConsequence = std::move(consequence);
            result.push_back(std::move(legendary));
        }
    }

    action = {};
    action.type = ChessActionType::BuyExp;
    if (validate(action) == ChessRuleErrorCode::None)
    {
        auto offer = noSelectionOffer(ChessActionType::BuyExp);
        auto projected = state;
        ChessManagementRules::gainExperience(
            projected,
            content,
            content.balance().buyExpAmount);
        auto consequence = costConsequence(state, content.balance().buyExpCost);
        consequence.experienceGained = content.balance().buyExpAmount;
        consequence.projectedExperienceAfter = projected.experience;
        consequence.projectedLevelAfter = projected.level;
        offer.economicConsequence = std::move(consequence);
        result.push_back(std::move(offer));
    }

    const int deployed = static_cast<int>(std::ranges::count_if(state.roster, [](const auto& entry) {
        return entry.second.deployed;
    }));
    if (deployed > 0)
    {
        if (!state.campaignComplete)
        {
            action = {};
            action.type = ChessActionType::PrepareBattle;
            if (validate(action) == ChessRuleErrorCode::None)
            {
                result.push_back(noSelectionOffer(ChessActionType::PrepareBattle));
            }
        }
        ChessActionOffer challenge;
        challenge.type = ChessActionType::StartChallenge;
        challenge.minimumSelection = 1;
        challenge.maximumSelection = 1;
        ChessChallengeSelectionOffer detail;
        for (const auto& definition : content.balance().challenges)
        {
            action = {};
            action.type = ChessActionType::StartChallenge;
            action.challengeName = definition.name;
            if (validate(action) == ChessRuleErrorCode::None)
            {
                detail.candidates.push_back({definition.name});
            }
        }
        if (!detail.candidates.empty())
        {
            challenge.payload = std::move(detail);
            result.push_back(std::move(challenge));
        }
    }
    if (state.campaignComplete)
    {
        result.push_back(noSelectionOffer(ChessActionType::FinishRun));
    }
    return result;
}

}  // namespace KysChess
