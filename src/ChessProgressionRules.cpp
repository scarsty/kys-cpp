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
    const std::string& id)
{
    const auto found = std::ranges::find(content.balance().challenges, id, &BalanceConfig::ChallengeDef::id);
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
        const bool repeated = state.completedChallengeIds.contains(prepared.stableBattleId);
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
    const int growthDivisor = std::max(1, balance.totalFights - 1);
    const bool boss = balance.bossInterval > 0 && completedFight % balance.bossInterval == 0;
    const int reward = balance.rewardBase
        + state.fight * balance.rewardGrowth / growthDivisor
        + (boss ? balance.bossRewardBonus : 0);
    const int interest = std::min(state.money * balance.interestPercent / 100, balance.interestMax);
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
