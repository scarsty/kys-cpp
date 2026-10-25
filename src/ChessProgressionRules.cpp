#include "ChessProgressionRules.h"

#include "ChessCombo.h"
#include "ChessManagementRules.h"
#include "ChessRewardRules.h"

#include <algorithm>
#include <cassert>
#include <format>
#include <set>

namespace KysChess
{
namespace
{

const BalanceConfig::ChallengeDef& requireChallenge(
    const ChessGameContent& content,
    const std::string& name)
{
    const auto found = std::ranges::find(content.balance().challenges, name, &BalanceConfig::ChallengeDef::name);
    assert(found != content.balance().challenges.end());
    return *found;
}

void updateDeployedWins(ChessSessionState& state)
{
    for (auto& entry : state.roster)
    {
        auto& piece = entry.second;
        if (piece.deployed)
        {
            ++piece.fightsWon;
        }
    }
}

}

int ChessProgressionRules::remainingVictoryBattles(const ChessSessionState& state, const ChessGameContent& content)
{
    return std::max(0, content.balance().totalFights - state.fight)
        + static_cast<int>(std::ranges::count_if(content.balance().challenges, [&](const auto& challenge) {
            return !state.completedChallengeNames.contains(challenge.name)
                && std::ranges::none_of(state.pendingRewards, [&](const auto& reward) {
                    return reward.challengeName == challenge.name;
                });
        }));
}

std::string ChessProgressionRules::lateBloomerReminder(const ChessSessionState& state, const ChessGameContent& content)
{
    if (state.talent != ChessTalentId::LateBloomer) return {};
    const auto& talent = content.balance().talent(state.talent);
    const int remaining = remainingVictoryBattles(state, content);
    if (remaining >= talent.evolutionWarningBattles) return {};
    if (!state.lateBloomerSelectionUsed)
        return std::format("晚成提醒：尚餘{}場主線與未通關遠征；請在棋子面板選定培養對象並讓其上陣，累積{}勝且達到三星後突破四星", remaining, talent.evolutionWins);
    const auto chosen = std::ranges::find_if(state.roster, [](const auto& entry) { return entry.second.lateBloomerChosen; });
    if (chosen == state.roster.end() || chosen->second.star == 4) return {};
    const auto& piece = chosen->second;
    const auto& name = content.role(piece.roleId)->Name;
    const int needed = std::max(0, talent.evolutionWins - piece.fightsWon);
    if (needed > remaining)
        return std::format("晚成提醒：{}還需{}勝，但主線與未通關遠征只餘{}場，已不足以完成培養", name, needed, remaining);
    if (needed == 0)
        return std::format("晚成提醒：{}已達{}勝，合成至三星即可突破四星", name, talent.evolutionWins);
    if (!piece.deployed)
        return std::format("晚成提醒：尚餘{}場主線與未通關遠征；請將{}上陣，還需{}勝才能完成培養", remaining, name, needed);
    return {};
}

int ChessProgressionRules::baseVictoryGold(
    const ChessSessionState& state,
    const ChessGameContent& content)
{
    const auto& balance = content.balance();
    const int completedFight = state.fight + 1;
    const int growthDivisor = std::max(1, balance.totalFights - 1);
    const bool boss = balance.bossInterval > 0 && completedFight % balance.bossInterval == 0;
    return balance.rewardBase
        + state.fight * balance.rewardGrowth / growthDivisor
        + (boss ? balance.bossRewardBonus : 0);
}

int ChessProgressionRules::interestGold(
    const ChessSessionState& state,
    const ChessGameContent& content)
{
    const auto& balance = content.balance();
    return std::min(state.money * balance.interestPercent / 100, balance.interestMax);
}

std::optional<int> ChessProgressionRules::nextInterestThreshold(
    const ChessSessionState& state,
    const ChessGameContent& content)
{
    const auto& balance = content.balance();
    const int current = interestGold(state, content);
    if (balance.interestPercent <= 0 || current >= balance.interestMax)
    {
        return std::nullopt;
    }
    const int numerator = (current + 1) * 100;
    return numerator / balance.interestPercent
        + (numerator % balance.interestPercent != 0 ? 1 : 0);
}

int ChessProgressionRules::projectedVictoryIncome(
    const ChessSessionState& state,
    const ChessGameContent& content)
{
    return baseVictoryGold(state, content) + interestGold(state, content);
}

void ChessProgressionRules::applyBattleResult(
    ChessSessionState& state,
    const ChessGameContent& content,
    ChessRunRandom& random,
    const HeadlessBattleResult& battle,
    std::vector<ChessSemanticEvent>& events)
{
    assert(state.preparedBattle);
    const auto prepared = *state.preparedBattle;
    state.lastBattleOutcome = battle.summary.outcome;
    state.lastBattleEndFrame = battle.summary.endFrame;
    state.lastBattleDigest = battle.digest;
    events.push_back({
        ChessSemanticEventType::BattleEnded,
        {},
        {},
        static_cast<int>(battle.summary.outcome),
        prepared.stableBattleId,
    });

    if (prepared.kind == PreparedChessBattleKind::Standalone)
    {
        state.preparedBattle.reset();
        state.phase = ChessSessionPhase::Management;
        return;
    }

    if (Battle::hasDefeatProgressionSemantics(battle.summary.outcome))
    {
        random.restorePreparation(prepared.preparationCheckpoint);
        state.preparedBattle.reset();
        state.phase = ChessSessionPhase::Management;
        return;
    }

    if (prepared.kind == PreparedChessBattleKind::Challenge)
    {
        const bool repeated = state.completedChallengeNames.contains(prepared.stableBattleId);
        if (repeated)
        {
            random.restorePreparation(prepared.preparationCheckpoint);
        }
        else
        {
            updateDeployedWins(state);
            ChessRewardRules::enqueueChallengeReward(
                state,
                content,
                requireChallenge(content, prepared.stableBattleId),
                events);
            ChessManagementRules::resolveLateBloomerEvolution(state, content, events);
        }
        state.preparedBattle.reset();
        if (state.pendingRewards.empty())
        {
            state.phase = ChessSessionPhase::Management;
        }
        return;
    }

    updateDeployedWins(state);
    const auto& balance = content.balance();
    std::set<int> survivingChessInstanceIds;
    for (const auto& survivor : battle.summary.survivors)
    {
        if (survivor.team == 0 && survivor.chessInstanceId >= 0)
        {
            survivingChessInstanceIds.insert(survivor.chessInstanceId);
        }
    }
    const auto comboGoldBonus = resolveChessComboGoldBonus(
        state,
        content,
        survivingChessInstanceIds);
    const bool grantsFreeRefresh = chessRosterHasActiveManagementRule(
        state,
        content,
        ChessNonBattleRuleKind::FreeShopRefresh);
    const int completedFight = state.fight + 1;
    const bool boss = balance.bossInterval > 0 && completedFight % balance.bossInterval == 0;
    const int reward = baseVictoryGold(state, content);
    const int interest = interestGold(state, content);
    const int goldAwarded = reward + interest + comboGoldBonus.amount;
    state.money += goldAwarded;
    const int experienceAwarded = boss ? balance.bossBattleExp : balance.battleExp;
    const bool leveled = ChessManagementRules::gainExperience(
        state,
        content,
        experienceAwarded);
    state.fight = completedFight;
    events.push_back({
        ChessSemanticEventType::GoldAwarded,
        reward,
        interest,
        goldAwarded,
        comboGoldBonus.sourceComboId >= 0
            ? std::format("combo:{}", comboGoldBonus.sourceComboId)
            : std::string{},
    });
    events.push_back({ChessSemanticEventType::ExperienceAwarded, {}, {}, experienceAwarded});
    if (leveled)
    {
        events.push_back({ChessSemanticEventType::LevelChanged, {}, {}, state.level});
    }
    events.push_back({ChessSemanticEventType::FightAdvanced, {}, {}, state.fight});
    if (grantsFreeRefresh && !state.freeShopRefreshAvailable)
    {
        state.freeShopRefreshAvailable = true;
        state.freeShopRefreshGrantedFight = state.fight;
        events.push_back({ChessSemanticEventType::FreeShopRefreshGranted, {}, {}, state.fight});
    }

    ChessRewardRules::enqueueCampaignRewards(state, content, random, completedFight, events);
    for (const auto& unlock : balance.banUnlocks)
    {
        if (completedFight < balance.totalFights
            && unlock.afterFight == completedFight)
        {
            ChessRewardRules::enqueueForcedBan(state, content, unlock.slots, unlock.maxTier, events);
        }
    }
    if (!state.shopLocked)
    {
        ChessManagementRules::refreshShop(state, content, random, &events);
    }
    state.campaignComplete = state.fight >= balance.totalFights;
    ChessManagementRules::resolveLateBloomerEvolution(state, content, events);
    if (state.campaignComplete)
    {
        events.push_back({ChessSemanticEventType::CampaignCompleted});
    }
    state.preparedBattle.reset();
    state.phase = state.pendingRewards.empty()
        ? ChessSessionPhase::Management
        : ChessSessionPhase::RewardChoice;
}

}
