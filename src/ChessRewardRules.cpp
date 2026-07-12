#include "ChessRewardRules.h"

#include "ChessManagementRules.h"

#include <algorithm>
#include <cassert>
#include <format>
#include <set>
#include <utility>

namespace KysChess
{
namespace
{

template <class T, class Projection>
std::vector<T> chooseStable(
    std::vector<T> candidates,
    int count,
    ChessRunRandom& random,
    Projection projection)
{
    std::ranges::sort(candidates, {}, projection);
    for (int index = static_cast<int>(candidates.size()) - 1; index > 0; --index)
    {
        const int other = random.nextInt(ChessRngStream::Reward, index + 1);
        std::swap(candidates[index], candidates[other]);
    }
    candidates.resize(std::min(count, static_cast<int>(candidates.size())));
    std::ranges::sort(candidates, {}, projection);
    return candidates;
}

void exposeRewardBoundary(ChessSessionState& state, std::vector<ChessSemanticEvent>& events)
{
    if (!state.pendingRewards.empty() && state.phase != ChessSessionPhase::RewardChoice)
    {
        state.phase = ChessSessionPhase::RewardChoice;
        events.push_back({
            ChessSemanticEventType::RewardOffered,
            {},
            {},
            static_cast<int>(state.pendingRewards.front().options.size()),
            state.pendingRewards.front().id,
        });
    }
}

void popReward(
    ChessSessionState& state,
    std::vector<ChessSemanticEvent>& events,
    bool exposeNext = true)
{
    state.pendingRewards.erase(state.pendingRewards.begin());
    state.phase = ChessSessionPhase::Management;
    if (exposeNext)
    {
        exposeRewardBoundary(state, events);
    }
}

std::vector<ChessRewardOption> equipmentOptions(
    const ChessGameContent& content,
    ChessRunRandom& random,
    int maximumTier,
    int count)
{
    std::vector<const EquipmentDef*> candidates;
    for (const auto& equipment : content.equipment())
    {
        if (equipment.tier <= maximumTier)
        {
            candidates.push_back(&equipment);
        }
    }
    candidates = chooseStable(std::move(candidates), count, random, [](const auto* value) {
        return value->itemId;
    });
    std::vector<ChessRewardOption> result;
    for (const auto* equipment : candidates)
    {
        result.push_back({
            std::format("equipment:{}", equipment->itemId),
            ChessRewardKind::Equipment,
            equipment->itemId,
        });
    }
    return result;
}

std::vector<ChessRewardOption> neigongOptions(
    const ChessSessionState& state,
    const ChessGameContent& content,
    ChessRunRandom& random,
    const std::vector<int>& eligibleTiers,
    int count)
{
    std::vector<const NeigongDef*> candidates;
    for (const auto& neigong : content.neigong())
    {
        if (std::ranges::contains(eligibleTiers, neigong.tier)
            && !state.obtainedNeigongIds.contains(neigong.magicId))
        {
            candidates.push_back(&neigong);
        }
    }
    candidates = chooseStable(std::move(candidates), count, random, [](const auto* value) {
        return value->magicId;
    });
    std::vector<ChessRewardOption> result;
    for (const auto* neigong : candidates)
    {
        result.push_back({
            std::format("neigong:{}", neigong->magicId),
            ChessRewardKind::InternalSkill,
            neigong->magicId,
        });
    }
    return result;
}

ChessPendingReward makeEquipmentReward(
    const ChessGameContent& content,
    ChessRunRandom& random,
    int maximumTier,
    int count,
    int rerollCost,
    std::string id)
{
    ChessPendingReward pending;
    pending.id = std::move(id);
    pending.kind = ChessRewardKind::Equipment;
    pending.rerollCost = rerollCost;
    pending.parameter = maximumTier;
    pending.choiceCount = count;
    pending.options = equipmentOptions(content, random, maximumTier, count);
    return pending;
}

ChessPendingReward makeNeigongReward(
    const ChessSessionState& state,
    const ChessGameContent& content,
    ChessRunRandom& random,
    std::vector<int> eligibleTiers,
    int count,
    int rerollCost,
    std::string id)
{
    ChessPendingReward pending;
    pending.id = std::move(id);
    pending.kind = ChessRewardKind::InternalSkill;
    pending.rerollCost = rerollCost;
    pending.eligibleTiers = std::move(eligibleTiers);
    pending.choiceCount = count;
    pending.options = neigongOptions(
        state,
        content,
        random,
        pending.eligibleTiers,
        count);
    return pending;
}

bool challengeRewardAvailable(
    const ChessSessionState& state,
    const ChessGameContent& content,
    const BalanceConfig::ChallengeReward& reward)
{
    using RT = BalanceConfig::ChallengeRewardType;
    switch (reward.type)
    {
    case RT::Gold:
        return true;
    case RT::GetPiece:
        return std::ranges::any_of(content.poolRoleIds(), [&](int roleId) {
            const auto* role = content.role(roleId);
            return role
                && role->Cost <= reward.value
                && ChessManagementRules::canGrantPiece(state, content, roleId);
        });
    case RT::GetNeigong:
        return std::ranges::any_of(content.neigong(), [&](const NeigongDef& neigong) {
            return neigong.tier <= reward.value
                && !state.obtainedNeigongIds.contains(neigong.magicId);
        });
    case RT::StarUp1to2:
    case RT::StarUp2to3:
    {
        const int fromStar = reward.type == RT::StarUp1to2 ? 1 : 2;
        return std::ranges::any_of(state.roster, [&](const auto& entry) {
            const auto* role = content.role(entry.second.roleId);
            assert(role);
            return entry.second.star == fromStar && role->Cost <= reward.value;
        });
    }
    case RT::GetEquipment:
        return std::ranges::any_of(content.equipment(), [&](const EquipmentDef& equipment) {
            return equipment.tier <= reward.value;
        });
    case RT::GetSpecificEquipment:
        return std::ranges::contains(content.equipment(), reward.value, &EquipmentDef::itemId);
    }
    std::unreachable();
}

void completeChallengeReward(
    ChessSessionState& state,
    std::string challengeName,
    std::vector<ChessSemanticEvent>& events)
{
    if (challengeName.empty())
    {
        return;
    }
    state.completedChallengeNames.insert(challengeName);
    events.push_back({ChessSemanticEventType::ChallengeCompleted, {}, {}, {}, std::move(challengeName)});
}

bool enqueueNestedChallengeChoice(
    ChessSessionState& state,
    const ChessGameContent& content,
    const ChessRewardOption& selected,
    const std::string& challengeName,
    std::vector<ChessSemanticEvent>& events)
{
    using RT = BalanceConfig::ChallengeRewardType;
    ChessPendingReward pending;
    pending.id = selected.id;
    pending.challengeName = challengeName;
    const auto type = static_cast<RT>(selected.value);
    const int parameter = selected.value2;
    if (type == RT::GetPiece)
    {
        pending.kind = ChessRewardKind::Piece;
        for (const int roleId : content.poolRoleIds())
        {
            const auto* role = content.role(roleId);
            if (role
                && role->Cost <= parameter
                && ChessManagementRules::canGrantPiece(state, content, roleId))
            {
                pending.options.push_back({std::format("piece:{}", roleId), ChessRewardKind::Piece, roleId});
            }
        }
    }
    else if (type == RT::GetNeigong)
    {
        pending.kind = ChessRewardKind::InternalSkill;
        for (const auto& neigong : content.neigong())
        {
            if (neigong.tier <= parameter && !state.obtainedNeigongIds.contains(neigong.magicId))
            {
                pending.options.push_back({
                    std::format("neigong:{}", neigong.magicId),
                    ChessRewardKind::InternalSkill,
                    neigong.magicId,
                });
            }
        }
    }
    else if (type == RT::StarUp1to2 || type == RT::StarUp2to3)
    {
        const int fromStar = type == RT::StarUp1to2 ? 1 : 2;
        pending.kind = ChessRewardKind::StarUpgrade;
        for (const auto& [instanceId, piece] : state.roster)
        {
            const auto* role = content.role(piece.roleId);
            if (piece.star == fromStar && role->Cost <= parameter)
            {
                pending.options.push_back({
                    std::format("star:{}", instanceId),
                    ChessRewardKind::StarUpgrade,
                    instanceId,
                    fromStar + 1,
                });
            }
        }
    }
    else if (type == RT::GetEquipment)
    {
        pending.kind = ChessRewardKind::Equipment;
        for (const auto& equipment : content.equipment())
        {
            if (equipment.tier <= parameter)
            {
                pending.options.push_back({
                    std::format("equipment:{}", equipment.itemId),
                    ChessRewardKind::Equipment,
                    equipment.itemId,
                });
            }
        }
    }
    std::ranges::sort(pending.options, {}, &ChessRewardOption::id);
    if (!pending.options.empty())
    {
        state.pendingRewards.insert(state.pendingRewards.begin(), std::move(pending));
        exposeRewardBoundary(state, events);
        return true;
    }
    return false;
}

}

std::string chessChallengeRewardDescription(
    const ChessGameContent& content,
    const BalanceConfig::ChallengeReward& reward)
{
    using Type = BalanceConfig::ChallengeRewardType;
    switch (reward.type)
    {
    case Type::Gold:
        return std::format("獲取{}金幣", reward.value);
    case Type::GetPiece:
        return std::format("獲取棋子(最高{}費)", reward.value);
    case Type::GetNeigong:
        return std::format("獲取內功(最高{}階)", reward.value);
    case Type::StarUp1to2:
        return std::format("升星★→★★(最高{}費)", reward.value);
    case Type::StarUp2to3:
        return std::format("升星★★→★★★(最高{}費)", reward.value);
    case Type::GetEquipment:
        return std::format("獲取裝備(最高{}階)", reward.value);
    case Type::GetSpecificEquipment:
        if (const auto* item = content.item(reward.value))
        {
            return std::format("獲取指定裝備: {}", item->name);
        }
        return "獲取指定裝備";
    }
    std::unreachable();
}

void ChessRewardRules::enqueueCampaignRewards(
    ChessSessionState& state,
    const ChessGameContent& content,
    ChessRunRandom& random,
    int completedFight,
    std::vector<ChessSemanticEvent>& events)
{
    if (content.balance().bossInterval > 0 && completedFight % content.balance().bossInterval == 0)
    {
        const int bossIndex = (completedFight - 1) / content.balance().bossInterval;
        std::vector<int> eligibleTiers;
        for (auto found = content.neigongConfig().tiersByBoss.rbegin();
             found != content.neigongConfig().tiersByBoss.rend();
             ++found)
        {
            if (found->first <= bossIndex)
            {
                eligibleTiers = found->second;
                break;
            }
        }
        if (eligibleTiers.empty())
        {
            eligibleTiers = {1};
        }
        auto pending = makeNeigongReward(
            state,
            content,
            random,
            std::move(eligibleTiers),
            content.neigongConfig().choiceCount,
            content.neigongConfig().rerollCost,
            std::format("boss_neigong:{}", completedFight));
        if (!pending.options.empty())
        {
            state.pendingRewards.push_back(std::move(pending));
        }
    }
    for (const auto& reward : content.balance().playerEquipmentRewards)
    {
        if (reward.fight == completedFight)
        {
            auto pending = makeEquipmentReward(
                content,
                random,
                reward.maxTier,
                reward.choices,
                reward.refreshCost,
                std::format("fight_equipment:{}", completedFight));
            if (!pending.options.empty())
            {
                state.pendingRewards.push_back(std::move(pending));
            }
        }
    }
    exposeRewardBoundary(state, events);
}

void ChessRewardRules::enqueueChallengeReward(
    ChessSessionState& state,
    const ChessGameContent& content,
    const BalanceConfig::ChallengeDef& challenge,
    std::vector<ChessSemanticEvent>& events)
{
    ChessPendingReward pending;
    pending.id = std::format("遠征獎勵：{}", challenge.name);
    pending.kind = ChessRewardKind::ChallengeReward;
    pending.challengeName = challenge.name;
    for (const auto& reward : challenge.rewards)
    {
        if (!challengeRewardAvailable(state, content, reward))
        {
            continue;
        }
        pending.options.push_back({
            chessChallengeRewardDescription(content, reward),
            ChessRewardKind::ChallengeReward,
            static_cast<int>(reward.type),
            reward.value,
        });
    }
    if (!pending.options.empty())
    {
        state.pendingRewards.push_back(std::move(pending));
    }
    exposeRewardBoundary(state, events);
}

void ChessRewardRules::enqueueForcedBan(
    ChessSessionState& state,
    const ChessGameContent& content,
    int slots,
    int maximumTier,
    std::vector<ChessSemanticEvent>& events)
{
    ChessPendingReward pending;
    pending.id = std::format("forced_ban:{}", state.fight);
    pending.kind = ChessRewardKind::ForcedBan;
    pending.parameter = slots;
    pending.choiceCount = slots;
    pending.eligibleTiers = {maximumTier};
    for (const int roleId : content.poolRoleIds())
    {
        const auto* role = content.role(roleId);
        if (role && role->Cost <= maximumTier && !state.bannedRoleIds.contains(roleId))
        {
            pending.options.push_back({std::format("ban:{}", roleId), ChessRewardKind::ForcedBan, roleId});
        }
    }
    if (!pending.options.empty())
    {
        state.pendingRewards.push_back(std::move(pending));
    }
    exposeRewardBoundary(state, events);
}

void ChessRewardRules::completePendingReward(
    ChessSessionState& state,
    std::vector<ChessSemanticEvent>& events)
{
    assert(!state.pendingRewards.empty());
    popReward(state, events);
}

ChessRuleErrorCode ChessRewardRules::validate(
    const ChessSessionState& state,
    const ChessGameContent&,
    const ChessAction& action)
{
    if (state.phase != ChessSessionPhase::RewardChoice || state.pendingRewards.empty())
    {
        return ChessRuleErrorCode::WrongPhase;
    }
    const auto& pending = state.pendingRewards.front();
    if (pending.kind == ChessRewardKind::ForcedBan)
    {
        return ChessRuleErrorCode::UnsupportedAction;
    }
    if (action.type == ChessActionType::RerollReward)
    {
        if (pending.rerolled || pending.rerollCost <= 0)
        {
            return ChessRuleErrorCode::RewardRerollUnavailable;
        }
        return state.money < pending.rerollCost
            ? ChessRuleErrorCode::InsufficientGold
            : ChessRuleErrorCode::None;
    }
    if (action.type != ChessActionType::ChooseReward)
    {
        return ChessRuleErrorCode::UnsupportedAction;
    }
    return std::ranges::contains(pending.options, action.rewardId, &ChessRewardOption::id)
        ? ChessRuleErrorCode::None
        : ChessRuleErrorCode::InvalidReward;
}

void ChessRewardRules::apply(
    ChessSessionState& state,
    const ChessGameContent& content,
    ChessRunRandom& random,
    const ChessAction& action,
    std::vector<ChessSemanticEvent>& events)
{
    assert(validate(state, content, action) == ChessRuleErrorCode::None);
    auto& pending = state.pendingRewards.front();
    if (action.type == ChessActionType::RerollReward)
    {
        state.money -= pending.rerollCost;
        pending.rerolled = true;
        pending.options = pending.kind == ChessRewardKind::Equipment
            ? equipmentOptions(content, random, pending.parameter, pending.choiceCount)
            : neigongOptions(state, content, random, pending.eligibleTiers, pending.choiceCount);
        events.push_back({ChessSemanticEventType::RewardRerolled, {}, {}, pending.rerollCost, pending.id});
        return;
    }

    const auto selected = *std::ranges::find(pending.options, action.rewardId, &ChessRewardOption::id);
    const auto pendingKind = pending.kind;
    const auto challengeName = pending.challengeName;
    const bool challengeNeedsNestedChoice = pendingKind == ChessRewardKind::ChallengeReward
        && static_cast<BalanceConfig::ChallengeRewardType>(selected.value)
            != BalanceConfig::ChallengeRewardType::Gold
        && static_cast<BalanceConfig::ChallengeRewardType>(selected.value)
            != BalanceConfig::ChallengeRewardType::GetSpecificEquipment;
    popReward(state, events, !challengeNeedsNestedChoice);
    bool nestedChallengeChoice = false;
    if (pendingKind == ChessRewardKind::Equipment)
    {
        ChessManagementRules::grantEquipment(state, selected.value, events);
    }
    else if (pendingKind == ChessRewardKind::InternalSkill)
    {
        state.obtainedNeigongIds.insert(selected.value);
        events.push_back({ChessSemanticEventType::InternalSkillAcquired, selected.value});
    }
    else if (pendingKind == ChessRewardKind::Piece)
    {
        ChessManagementRules::grantPiece(state, content, selected.value, events);
    }
    else if (pendingKind == ChessRewardKind::StarUpgrade)
    {
        ChessManagementRules::upgradePiece(
            state,
            content,
            selected.value,
            selected.value2,
            events);
    }
    else if (pendingKind == ChessRewardKind::ChallengeReward)
    {
        using RT = BalanceConfig::ChallengeRewardType;
        const auto type = static_cast<RT>(selected.value);
        if (type == RT::Gold)
        {
            state.money += selected.value2;
            events.push_back({ChessSemanticEventType::GoldAwarded, {}, {}, selected.value2});
        }
        else if (type == RT::GetSpecificEquipment)
        {
            ChessManagementRules::grantEquipment(state, selected.value2, events);
        }
        else
        {
            nestedChallengeChoice = enqueueNestedChallengeChoice(
                state,
                content,
                selected,
                challengeName,
                events);
            assert(nestedChallengeChoice);
        }
    }
    if (!nestedChallengeChoice)
    {
        completeChallengeReward(state, challengeName, events);
    }
    events.push_back({ChessSemanticEventType::RewardChosen, {}, {}, {}, action.rewardId});
}

}
