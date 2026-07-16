#include "ChessManagementRules.h"

#include "ChessRewardRules.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <utility>

namespace KysChess
{
namespace
{

int deployedCount(const ChessSessionState& state)
{
    return static_cast<int>(std::ranges::count_if(state.roster, [](const auto& entry) {
        return entry.second.deployed;
    }));
}

int benchCount(const ChessSessionState& state)
{
    return static_cast<int>(state.roster.size()) - deployedCount(state);
}

const EquipmentDef* equipmentDefinition(const ChessGameContent& content, int itemId)
{
    const auto found = std::ranges::find(content.equipment(), itemId, &EquipmentDef::itemId);
    return found == content.equipment().end() ? nullptr : &*found;
}

int matchingCount(const ChessSessionState& state, int roleId, int star)
{
    return static_cast<int>(std::ranges::count_if(state.roster, [&](const auto& entry) {
        return entry.second.roleId == roleId && entry.second.star == star;
    }));
}

void mergeAvailablePieces(
    ChessSessionState& state,
    const ChessGameContent& content,
    int roleId,
    std::vector<ChessSemanticEvent>& events)
{
    for (int star = 1; star <= 2; ++star)
    {
        while (matchingCount(state, roleId, star) >= 3)
        {
            std::vector<int> consumed;
            bool deployed = false;
            std::vector<int> weapons;
            std::vector<int> armor;
            int fightsWon = 0;
            for (const auto& [id, piece] : state.roster)
            {
                if (piece.roleId == roleId && piece.star == star)
                {
                    consumed.push_back(id);
                    deployed = deployed || piece.deployed;
                    fightsWon = std::max(fightsWon, piece.fightsWon);
                    if (piece.weaponInstanceId >= 0) weapons.push_back(piece.weaponInstanceId);
                    if (piece.armorInstanceId >= 0) armor.push_back(piece.armorInstanceId);
                    if (consumed.size() == 3)
                    {
                        break;
                    }
                }
            }
            for (const int id : consumed)
            {
                state.roster.erase(id);
            }
            ChessSessionPiece upgraded;
            upgraded.instanceId = state.nextChessInstanceId++;
            upgraded.roleId = roleId;
            upgraded.star = star + 1;
            upgraded.deployed = deployed;
            upgraded.fightsWon = fightsWon;
            const auto equipmentOrder = [&](int lhs, int rhs) {
                const auto* lhsDefinition = equipmentDefinition(
                    content,
                    state.equipmentInventory.at(lhs).itemId);
                const auto* rhsDefinition = equipmentDefinition(
                    content,
                    state.equipmentInventory.at(rhs).itemId);
                assert(lhsDefinition && rhsDefinition);
                if (lhsDefinition->tier != rhsDefinition->tier)
                {
                    return lhsDefinition->tier > rhsDefinition->tier;
                }
                return false;
            };
            std::stable_sort(weapons.begin(), weapons.end(), equipmentOrder);
            std::stable_sort(armor.begin(), armor.end(), equipmentOrder);
            upgraded.weaponInstanceId = weapons.empty() ? -1 : weapons.front();
            upgraded.armorInstanceId = armor.empty() ? -1 : armor.front();
            state.roster.emplace(upgraded.instanceId, upgraded);
            for (const int equipmentId : weapons)
            {
                state.equipmentInventory.at(equipmentId).assignedChessInstanceId =
                    equipmentId == upgraded.weaponInstanceId ? upgraded.instanceId : -1;
            }
            for (const int equipmentId : armor)
            {
                state.equipmentInventory.at(equipmentId).assignedChessInstanceId =
                    equipmentId == upgraded.armorInstanceId ? upgraded.instanceId : -1;
            }
            for (auto& event : events)
            {
                if (event.merge && std::ranges::contains(consumed, event.primaryId))
                {
                    event.merge->recursiveMergeFollowed = true;
                }
            }
            ChessSemanticEvent event{
                ChessSemanticEventType::ChessMerged,
                upgraded.instanceId,
                roleId,
                upgraded.star,
            };
            ChessMergeEventDetail detail;
            detail.consumedInstanceIds = std::move(consumed);
            detail.inheritedFightsWon = fightsWon;
            detail.deployed = deployed;
            if (upgraded.weaponInstanceId >= 0)
            {
                detail.transferredEquipmentInstanceIds.push_back(upgraded.weaponInstanceId);
            }
            if (upgraded.armorInstanceId >= 0)
            {
                detail.transferredEquipmentInstanceIds.push_back(upgraded.armorInstanceId);
            }
            event.merge = std::move(detail);
            events.push_back(std::move(event));
        }
    }
}

std::vector<int> candidatesForTier(
    const ChessSessionState& state,
    const ChessGameContent& content,
    int tier)
{
    std::vector<int> preferred;
    for (const int roleId : content.poolRoleIds())
    {
        const auto* role = content.role(roleId);
        if (!role || role->Cost != tier || state.bannedRoleIds.contains(roleId))
        {
            continue;
        }
        if (tier <= 4 && state.rejectedRoleIds.contains(roleId))
        {
            continue;
        }
        preferred.push_back(roleId);
    }
    std::ranges::sort(preferred);
    return preferred;
}

void generateShop(ChessSessionState& state, const ChessGameContent& content, ChessRunRandom& random)
{
    const auto& balance = content.balance();
    state.shop.clear();
    state.shop.reserve(balance.shopSlotCount);
    for (int slot = 0; slot < balance.shopSlotCount; ++slot)
    {
        std::array<std::vector<int>, 5> candidates;
        int totalWeight = 0;
        for (int tier = 1; tier <= 5; ++tier)
        {
            candidates[tier - 1] = candidatesForTier(state, content, tier);
            if (!candidates[tier - 1].empty())
            {
                totalWeight += balance.shopWeights[state.level][tier - 1];
            }
        }
        if (totalWeight == 0)
        {
            break;
        }
        const int roll = random.nextInt(ChessRngStream::Shop, totalWeight);
        int cumulative = 0;
        int selectedTier = 1;
        for (int tier = 1; tier <= 5; ++tier)
        {
            if (!candidates[tier - 1].empty())
            {
                cumulative += balance.shopWeights[state.level][tier - 1];
                if (roll < cumulative)
                {
                    selectedTier = tier;
                    break;
                }
            }
        }
        const auto& tierCandidates = candidates[selectedTier - 1];
        const int roleId = tierCandidates[random.nextInt(ChessRngStream::Shop, static_cast<int>(tierCandidates.size()))];
        state.shop.push_back({roleId, selectedTier});
        state.seenRoleIds.insert(roleId);
    }
}

}

void ChessManagementRules::initializeShop(
    ChessSessionState& state,
    const ChessGameContent& content,
    ChessRunRandom& random)
{
    generateShop(state, content, random);
}

void ChessManagementRules::refreshShop(
    ChessSessionState& state,
    const ChessGameContent& content,
    ChessRunRandom& random)
{
    state.rejectedRoleIds.clear();
    for (const auto& slot : state.shop)
    {
        if (slot.roleId >= 0)
        {
            state.rejectedRoleIds.insert(slot.roleId);
        }
    }
    generateShop(state, content, random);
    state.shopLocked = false;
}

int ChessManagementRules::maximumDeployment(const ChessSessionState& state, const ChessGameContent& content)
{
    return maximumDeploymentAtLevel(content, state.level);
}

int ChessManagementRules::maximumDeploymentAtLevel(const ChessGameContent& content, int level)
{
    return std::max(level + 1, content.balance().minBattleSize);
}

int ChessManagementRules::experienceForNextLevel(const ChessSessionState& state, const ChessGameContent& content)
{
    const auto& balance = content.balance();
    return state.level < balance.maxLevel && state.level < static_cast<int>(balance.expTable.size())
        ? balance.expTable[state.level]
        : state.experience;
}

bool ChessManagementRules::gainExperience(
    ChessSessionState& state,
    const ChessGameContent& content,
    int amount)
{
    const int oldLevel = state.level;
    state.experience += amount;
    if (state.level < content.balance().maxLevel
        && state.level < static_cast<int>(content.balance().expTable.size())
        && state.experience >= content.balance().expTable[state.level])
    {
        state.experience -= content.balance().expTable[state.level];
        ++state.level;
    }
    return state.level != oldLevel;
}

int ChessManagementRules::maximumBanCount(const ChessSessionState& state, const ChessGameContent& content)
{
    const auto& balance = content.balance();
    int count = std::max(0, balance.banBaseCount + state.level * balance.banCountPerLevel);
    for (const auto& unlock : balance.banUnlocks)
    {
        if (state.fight >= unlock.afterFight)
        {
            count += unlock.slots;
        }
    }
    return count;
}

std::vector<int> ChessManagementRules::shopCandidatesForTier(
    const ChessSessionState& state,
    const ChessGameContent& content,
    int tier)
{
    assert(tier >= 1 && tier <= 5);
    return candidatesForTier(state, content, tier);
}

int ChessManagementRules::pieceValue(const ChessGameContent& content, int roleId, int star)
{
    const auto* role = content.role(roleId);
    assert(role);
    const auto& balance = content.balance();
    return balance.tierPrices[role->Cost - 1]
        * static_cast<int>(std::pow(balance.starCostMult, star - 1));
}

ChessRuleErrorCode ChessManagementRules::validate(
    const ChessSessionState& state,
    const ChessGameContent& content,
    const ChessAction& action)
{
    const bool forcedBan = state.phase == ChessSessionPhase::RewardChoice
        && !state.pendingRewards.empty()
        && state.pendingRewards.front().kind == ChessRewardKind::ForcedBan;
    if (state.phase != ChessSessionPhase::Management && !forcedBan)
    {
        return ChessRuleErrorCode::WrongPhase;
    }
    if (forcedBan
        && action.type != ChessActionType::AddBan
        && action.type != ChessActionType::SkipForcedBans)
    {
        return ChessRuleErrorCode::WrongPhase;
    }
    const auto& balance = content.balance();
    switch (action.type)
    {
    case ChessActionType::RefreshShop:
    {
        const int cost = state.freeShopRefreshAvailable ? 0 : balance.refreshCost;
        return state.money < cost ? ChessRuleErrorCode::InsufficientGold : ChessRuleErrorCode::None;
    }
    case ChessActionType::SetShopLocked:
        return ChessRuleErrorCode::None;
    case ChessActionType::BuyShopSlot:
    {
        if (action.shopSlot < 0 || action.shopSlot >= static_cast<int>(state.shop.size()))
        {
            return ChessRuleErrorCode::InvalidShopSlot;
        }
        const auto& slot = state.shop[action.shopSlot];
        if (slot.roleId < 0)
        {
            return ChessRuleErrorCode::EmptyShopSlot;
        }
        if (!content.role(slot.roleId))
        {
            return ChessRuleErrorCode::InvalidRole;
        }
        if (state.money < pieceValue(content, slot.roleId, 1))
        {
            return ChessRuleErrorCode::InsufficientGold;
        }
        if (!canGrantPiece(state, content, slot.roleId))
        {
            return ChessRuleErrorCode::BenchFull;
        }
        return ChessRuleErrorCode::None;
    }
    case ChessActionType::SellChess:
        return state.roster.contains(action.chessInstanceId)
            ? ChessRuleErrorCode::None
            : ChessRuleErrorCode::UnknownChessInstance;
    case ChessActionType::BuyExp:
        if (state.level >= balance.maxLevel)
        {
            return ChessRuleErrorCode::MaximumLevel;
        }
        return state.money < balance.buyExpCost ? ChessRuleErrorCode::InsufficientGold : ChessRuleErrorCode::None;
    case ChessActionType::SetDeployment:
    {
        auto ids = action.chessInstanceIds;
        std::ranges::sort(ids);
        if (std::ranges::adjacent_find(ids) != ids.end())
        {
            return ChessRuleErrorCode::DuplicateIdentifier;
        }
        if (ids.size() > static_cast<std::size_t>(maximumDeployment(state, content)))
        {
            return ChessRuleErrorCode::DeploymentLimitExceeded;
        }
        for (const int id : ids)
        {
            if (!state.roster.contains(id))
            {
                return ChessRuleErrorCode::UnknownChessInstance;
            }
        }
        return ChessRuleErrorCode::None;
    }
    case ChessActionType::AddBan:
        if (!content.role(action.roleId))
        {
            return ChessRuleErrorCode::InvalidRole;
        }
        if (state.bannedRoleIds.contains(action.roleId))
        {
            return ChessRuleErrorCode::RoleAlreadyBanned;
        }
        if (forcedBan)
        {
            const auto& options = state.pendingRewards.front().options;
            return std::ranges::contains(options, action.roleId, &ChessRewardOption::value)
                ? ChessRuleErrorCode::None
                : ChessRuleErrorCode::InvalidRole;
        }
        if (!state.seenRoleIds.contains(action.roleId))
        {
            return ChessRuleErrorCode::RoleNotSeen;
        }
        return state.bannedRoleIds.size() >= static_cast<std::size_t>(maximumBanCount(state, content))
            ? ChessRuleErrorCode::BanLimitReached
            : ChessRuleErrorCode::None;
    case ChessActionType::SkipForcedBans:
        return forcedBan ? ChessRuleErrorCode::None : ChessRuleErrorCode::WrongPhase;
    case ChessActionType::Equip:
    {
        const auto equipment = state.equipmentInventory.find(action.equipmentInstanceId);
        if (equipment == state.equipmentInventory.end())
        {
            return ChessRuleErrorCode::UnknownEquipmentInstance;
        }
        if (!state.roster.contains(action.targetChessInstanceId))
        {
            return ChessRuleErrorCode::UnknownChessInstance;
        }
        return equipmentDefinition(content, equipment->second.itemId)
            ? ChessRuleErrorCode::None
            : ChessRuleErrorCode::InvalidEquipment;
    }
    case ChessActionType::BuyLegendaryEquipment:
    {
        if (balance.legendaryShop.unlockFight <= 0
            || state.fight < balance.legendaryShop.unlockFight)
        {
            return ChessRuleErrorCode::LegendaryShopLocked;
        }
        const auto* equipment = equipmentDefinition(content, action.itemId);
        if (!equipment || equipment->tier != 4)
        {
            return ChessRuleErrorCode::InvalidEquipment;
        }
        return state.money < balance.legendaryShop.price
            ? ChessRuleErrorCode::InsufficientGold
            : ChessRuleErrorCode::None;
    }
    case ChessActionType::SetPositionSwapEnabled:
        return ChessRuleErrorCode::None;
    case ChessActionType::RerollEnemySeed:
        return state.money < balance.enemyRerollCost
            ? ChessRuleErrorCode::InsufficientGold
            : ChessRuleErrorCode::None;
    default:
        return ChessRuleErrorCode::UnsupportedAction;
    }
}

void ChessManagementRules::apply(
    ChessSessionState& state,
    const ChessGameContent& content,
    ChessRunRandom& random,
    const ChessAction& action,
    std::vector<ChessSemanticEvent>& events)
{
    assert(validate(state, content, action) == ChessRuleErrorCode::None);
    const bool forcedBan = state.phase == ChessSessionPhase::RewardChoice
        && !state.pendingRewards.empty()
        && state.pendingRewards.front().kind == ChessRewardKind::ForcedBan;
    const auto& balance = content.balance();
    switch (action.type)
    {
    case ChessActionType::RefreshShop:
    {
        const bool free = state.freeShopRefreshAvailable;
        const int cost = free ? 0 : balance.refreshCost;
        state.money -= cost;
        if (free)
        {
            state.freeShopRefreshAvailable = false;
            state.freeShopRefreshGrantedFight = -1;
            events.push_back({ChessSemanticEventType::FreeShopRefreshConsumed});
        }
        refreshShop(state, content, random);
        events.push_back({ChessSemanticEventType::ShopRefreshed, {}, {}, cost, {}});
        return;
    }
    case ChessActionType::SetShopLocked:
        state.shopLocked = action.value;
        events.push_back({ChessSemanticEventType::ShopLockChanged, {}, {}, action.value ? 1 : 0, {}});
        return;
    case ChessActionType::BuyShopSlot:
    {
        auto& slot = state.shop[action.shopSlot];
        const int roleId = slot.roleId;
        const int cost = pieceValue(content, roleId, 1);
        state.money -= cost;
        slot.roleId = -1;
        grantPiece(state, content, roleId, events, cost);
        return;
    }
    case ChessActionType::SellChess:
    {
        const auto piece = state.roster.at(action.chessInstanceId);
        const int price = pieceValue(content, piece.roleId, piece.star);
        state.roster.erase(action.chessInstanceId);
        if (piece.weaponInstanceId >= 0)
        {
            state.equipmentInventory.at(piece.weaponInstanceId).assignedChessInstanceId = -1;
        }
        if (piece.armorInstanceId >= 0)
        {
            state.equipmentInventory.at(piece.armorInstanceId).assignedChessInstanceId = -1;
        }
        state.money += price;
        events.push_back({ChessSemanticEventType::ChessSold, action.chessInstanceId, piece.roleId, price, {}});
        return;
    }
    case ChessActionType::BuyExp:
    {
        state.money -= balance.buyExpCost;
        const bool leveled = gainExperience(state, content, balance.buyExpAmount);
        events.push_back({ChessSemanticEventType::ExperiencePurchased, {}, {}, balance.buyExpAmount, {}});
        if (leveled)
        {
            events.push_back({ChessSemanticEventType::LevelChanged, {}, {}, state.level, {}});
        }
        return;
    }
    case ChessActionType::SetDeployment:
    {
        std::set<int> selected(action.chessInstanceIds.begin(), action.chessInstanceIds.end());
        for (auto& [id, piece] : state.roster)
        {
            piece.deployed = selected.contains(id);
        }
        events.push_back({ChessSemanticEventType::DeploymentChanged, {}, {}, static_cast<int>(selected.size()), {}});
        return;
    }
    case ChessActionType::AddBan:
        state.bannedRoleIds.insert(action.roleId);
        events.push_back({ChessSemanticEventType::RoleBanned, action.roleId, {}, {}, {}});
        if (forcedBan)
        {
            auto& pending = state.pendingRewards.front();
            --pending.parameter;
            const bool candidateRemaining = std::ranges::any_of(
                pending.options,
                [&](const ChessRewardOption& option) {
                    return !state.bannedRoleIds.contains(option.value);
                });
            if (pending.parameter == 0 || !candidateRemaining)
            {
                ChessRewardRules::completePendingReward(state, events);
            }
        }
        return;
    case ChessActionType::SkipForcedBans:
        ChessRewardRules::completePendingReward(state, events);
        events.push_back({ChessSemanticEventType::ForcedBansSkipped});
        return;
    case ChessActionType::Equip:
    {
        auto& equipment = state.equipmentInventory.at(action.equipmentInstanceId);
        const auto* definition = equipmentDefinition(content, equipment.itemId);
        assert(definition);
        if (equipment.assignedChessInstanceId >= 0)
        {
            auto& oldPiece = state.roster.at(equipment.assignedChessInstanceId);
            auto& oldSlot = definition->equipType == 0
                ? oldPiece.weaponInstanceId
                : oldPiece.armorInstanceId;
            oldSlot = -1;
        }
        auto& target = state.roster.at(action.targetChessInstanceId);
        auto& targetSlot = definition->equipType == 0
            ? target.weaponInstanceId
            : target.armorInstanceId;
        if (targetSlot >= 0)
        {
            state.equipmentInventory.at(targetSlot).assignedChessInstanceId = -1;
        }
        targetSlot = equipment.instanceId;
        equipment.assignedChessInstanceId = target.instanceId;
        events.push_back({
            ChessSemanticEventType::EquipmentAssigned,
            equipment.instanceId,
            target.instanceId,
            equipment.itemId,
        });
        return;
    }
    case ChessActionType::BuyLegendaryEquipment:
    {
        state.money -= balance.legendaryShop.price;
        const int instanceId = grantEquipment(state, action.itemId, events);
        events.push_back({
            ChessSemanticEventType::LegendaryEquipmentPurchased,
            instanceId,
            action.itemId,
            balance.legendaryShop.price,
        });
        return;
    }
    case ChessActionType::SetPositionSwapEnabled:
        state.options.positionSwapEnabled = action.value;
        events.push_back({ChessSemanticEventType::PositionSwapOptionChanged, {}, {}, action.value ? 1 : 0, {}});
        return;
    case ChessActionType::RerollEnemySeed:
    {
        const auto previousEnemyPlanKey = random.enemyPlanIdentity();
        state.money -= balance.enemyRerollCost;
        random.rerollEnemySeed();
        ChessSemanticEvent event{
            ChessSemanticEventType::EnemyPlanRerolled,
            {},
            {},
            balance.enemyRerollCost,
        };
        event.enemyPlanReroll = ChessEnemyPlanRerollEventDetail{
            balance.enemyRerollCost,
            previousEnemyPlanKey,
            random.enemyPlanIdentity(),
        };
        events.push_back(std::move(event));
        return;
    }
    default:
        std::unreachable();
    }
}

int ChessManagementRules::grantPiece(
    ChessSessionState& state,
    const ChessGameContent& content,
    int roleId,
    std::vector<ChessSemanticEvent>& events,
    int eventValue)
{
    assert(content.role(roleId));
    ChessSessionPiece piece;
    piece.instanceId = state.nextChessInstanceId++;
    piece.roleId = roleId;
    state.roster.emplace(piece.instanceId, piece);
    events.push_back({ChessSemanticEventType::ChessPurchased, piece.instanceId, roleId, eventValue});
    mergeAvailablePieces(state, content, roleId, events);
    return piece.instanceId;
}

bool ChessManagementRules::canGrantPiece(
    const ChessSessionState& state,
    const ChessGameContent& content,
    int roleId)
{
    assert(content.role(roleId));
    return benchCount(state) < content.balance().benchSize
        || wouldGrantPieceMerge(state, roleId);
}

bool ChessManagementRules::wouldGrantPieceMerge(const ChessSessionState& state, int roleId)
{
    return matchingCount(state, roleId, 1) >= 2;
}

int ChessManagementRules::grantEquipment(
    ChessSessionState& state,
    int itemId,
    std::vector<ChessSemanticEvent>& events)
{
    ChessEquipmentInstance equipment;
    equipment.instanceId = state.nextEquipmentInstanceId++;
    equipment.itemId = itemId;
    state.equipmentInventory.emplace(equipment.instanceId, equipment);
    events.push_back({ChessSemanticEventType::EquipmentAcquired, equipment.instanceId, itemId});
    return equipment.instanceId;
}

void ChessManagementRules::upgradePiece(
    ChessSessionState& state,
    const ChessGameContent& content,
    int chessInstanceId,
    int newStar,
    std::vector<ChessSemanticEvent>& events)
{
    auto& piece = state.roster.at(chessInstanceId);
    piece.star = newStar;
    ChessSemanticEvent event{ChessSemanticEventType::ChessMerged, chessInstanceId, piece.roleId, newStar};
    ChessMergeEventDetail detail;
    detail.consumedInstanceIds = {chessInstanceId};
    detail.inheritedFightsWon = piece.fightsWon;
    detail.deployed = piece.deployed;
    if (piece.weaponInstanceId >= 0) detail.transferredEquipmentInstanceIds.push_back(piece.weaponInstanceId);
    if (piece.armorInstanceId >= 0) detail.transferredEquipmentInstanceIds.push_back(piece.armorInstanceId);
    event.merge = std::move(detail);
    events.push_back(std::move(event));
    mergeAvailablePieces(state, content, piece.roleId, events);
}

}
