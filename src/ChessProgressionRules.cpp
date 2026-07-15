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

void updateSurvivorWins(ChessSessionState& state, const BattleSummary& summary)
{
    std::set<int> rewardedInstanceIds;
    for (const auto& survivor : summary.survivors)
    {
        if (survivor.team == 0
            && survivor.chessInstanceId >= 0
            && rewardedInstanceIds.insert(survivor.chessInstanceId).second)
        {
            ++state.roster.at(survivor.chessInstanceId).fightsWon;
        }
    }
}

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
        ChessBattleEndedEventDetail{battle.summary.outcome, prepared.stableBattleId},
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
            updateSurvivorWins(state, battle.summary);
            ChessRewardRules::enqueueChallengeReward(
                state,
                content,
                requireChallenge(content, prepared.stableBattleId),
                events);
        }
        state.preparedBattle.reset();
        if (state.pendingRewards.empty())
        {
            state.phase = ChessSessionPhase::Management;
        }
        return;
    }

    updateSurvivorWins(state, battle.summary);
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
    const bool grantsFreeRefresh = chessRosterHasActiveComboEffect(
        state,
        content,
        EffectType::FreeRefresh);
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
        ChessGoldAwardedEventDetail{
            reward,
            interest,
            comboGoldBonus.amount,
            goldAwarded,
            comboGoldBonus.sourceComboId,
        },
    });
    events.push_back({
        ChessSemanticEventType::ExperienceAwarded,
        ChessExperienceAwardedEventDetail{experienceAwarded},
    });
    if (leveled)
    {
        events.push_back({ChessSemanticEventType::LevelChanged, ChessLevelChangedEventDetail{state.level}});
    }
    events.push_back({ChessSemanticEventType::FightAdvanced, ChessFightAdvancedEventDetail{state.fight}});
    if (grantsFreeRefresh && !state.freeShopRefreshAvailable)
    {
        state.freeShopRefreshAvailable = true;
        state.freeShopRefreshGrantedFight = state.fight;
        events.push_back({
            ChessSemanticEventType::FreeShopRefreshGranted,
            ChessFreeShopRefreshGrantedEventDetail{state.fight},
        });
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
        ChessManagementRules::refreshShop(state, content, random);
    }
    state.campaignComplete = state.fight >= balance.totalFights;
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
