#include "ChessProgressionRules.h"
#include "ChessRewardRules.h"
#include "ChessRuntimeConstants.h"
#include "ChessSaveStore.h"
#include "ChessGameSessionTestHelpers.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <format>

using namespace KysChess;
using namespace KysChess::Test;

TEST_CASE("campaign victory derives progression from runtime survivors", "[chess][progression]")
{
    auto contentData = ChessGameContentData{};
    contentData.balance.totalFights = 1;
    contentData.balance.rewardBase = 3;
    contentData.balance.rewardGrowth = 0;
    contentData.balance.interestPercent = 10;
    contentData.balance.interestMax = 3;
    contentData.balance.battleExp = 4;
    ChessGameContent content(std::move(contentData));
    ChessSessionState state;
    state.money = 20;
    state.roster.emplace(1, ChessSessionPiece{1, 10, 1, true});
    PreparedChessBattle prepared;
    prepared.kind = PreparedChessBattleKind::Campaign;
    prepared.stableBattleId = "campaign:1";
    state.preparedBattle = prepared;
    ChessRunRandom random(1);
    HeadlessBattleResult battle;
    battle.summary.outcome = Battle::BattleOutcome::PlayerVictory;
    battle.summary.endFrame = 12;
    battle.summary.survivors.push_back({1, 1, 10, 0, 20, 0});
    battle.digest[0] = 9;
    std::vector<ChessSemanticEvent> events;

    ChessProgressionRules::applyBattleResult(state, content, random, battle, events);

    CHECK(state.fight == 1);
    CHECK(state.campaignComplete);
    CHECK(state.money == 25);
    CHECK(state.roster.at(1).fightsWon == 1);
    CHECK(state.lastBattleDigest == battle.digest);
    CHECK(state.phase == ChessSessionPhase::Management);
}

TEST_CASE("configured victory economy grants combo gold and one persisted free refresh",
          "[chess][progression][combo][shop]")
{
    ChessGameContentData data;
    data.balance.totalFights = 2;
    data.balance.rewardBase = 3;
    data.balance.rewardGrowth = 0;
    data.balance.interestPercent = 0;
    data.balance.battleExp = 0;
    data.balance.shopSlotCount = 0;
    ChessRoleDefinition comboMember;
    comboMember.ID = 10;
    comboMember.Name = "羈絆成員";
    comboMember.Cost = 1;
    data.roles.emplace(comboMember.ID, comboMember);
    ChessRoleDefinition outsider = comboMember;
    outsider.ID = 20;
    outsider.Name = "高星生還者";
    data.roles.emplace(outsider.ID, outsider);
    ComboDef combo;
    combo.id = 7;
    combo.name = "配置經濟羈絆";
    combo.memberRoleIds = {comboMember.ID};
    combo.thresholds.push_back({
        1,
        "配置門檻",
        {
            {EffectType::GoldCoefficient, 2},
            {EffectType::FreeRefresh, 1},
        },
    });
    data.combos.push_back(std::move(combo));
    ChessGameContent content(std::move(data));

    ChessSessionState state;
    state.roster.emplace(1, ChessSessionPiece{1, comboMember.ID, 1, true});
    state.roster.emplace(2, ChessSessionPiece{2, outsider.ID, 3, true});
    PreparedChessBattle prepared;
    prepared.kind = PreparedChessBattleKind::Campaign;
    prepared.stableBattleId = "campaign:1";
    state.preparedBattle = prepared;
    ChessRunRandom random(77);
    HeadlessBattleResult battle;
    battle.summary.outcome = Battle::BattleOutcome::PlayerVictory;
    battle.summary.survivors = {
        {1, 1, comboMember.ID, 0, 10, 0},
        {2, 2, outsider.ID, 0, 10, 0},
    };
    std::vector<ChessSemanticEvent> events;

    ChessProgressionRules::applyBattleResult(state, content, random, battle, events);

    CHECK(state.money == 9);
    CHECK(state.freeShopRefreshAvailable);
    CHECK(state.freeShopRefreshGrantedFight == 1);
    const auto gold = std::ranges::find(events, ChessSemanticEventType::GoldAwarded, &ChessSemanticEvent::type);
    REQUIRE(gold != events.end());
    CHECK(gold->primaryId == 3);
    CHECK(gold->secondaryId == 0);
    CHECK(gold->value == 9);
    CHECK(gold->stableId == "combo:7");

    state.money = 0;
    ChessAction refresh;
    refresh.type = ChessActionType::RefreshShop;
    REQUIRE(ChessManagementRules::validate(state, content, refresh) == ChessRuleErrorCode::None);
    std::vector<ChessSemanticEvent> refreshEvents;
    ChessManagementRules::apply(state, content, random, refresh, refreshEvents);
    CHECK(state.money == 0);
    CHECK_FALSE(state.freeShopRefreshAvailable);
    CHECK(state.freeShopRefreshGrantedFight == -1);
    REQUIRE(refreshEvents.size() == 2);
    CHECK(refreshEvents.front().type == ChessSemanticEventType::FreeShopRefreshConsumed);
    CHECK(refreshEvents.back().type == ChessSemanticEventType::ShopRefreshed);
    CHECK(refreshEvents.back().value == 0);
}

TEST_CASE("experience gain uses one shared legacy level step", "[chess][progression][experience]")
{
    auto data = ChessGameContentData{};
    data.balance.expTable = {4, 4, 4};
    data.balance.maxLevel = 3;
    ChessGameContent content(std::move(data));
    ChessSessionState state;

    CHECK(ChessManagementRules::gainExperience(state, content, 12));
    CHECK(state.level == 1);
    CHECK(state.experience == 8);

    CHECK(ChessManagementRules::gainExperience(state, content, 0));
    CHECK(state.level == 2);
    CHECK(state.experience == 4);
}

TEST_CASE("reward choice and reroll are explicit deterministic boundaries", "[chess][reward][determinism]")
{
    auto data = ChessGameContentData{};
    data.balance.playerEquipmentRewards.push_back({1, 4, 2, 1});
    data.equipment.push_back({101, 1, 0});
    data.equipment.push_back({102, 2, 1});
    data.equipment.push_back({103, 3, 0});
    ChessGameContent content(std::move(data));
    ChessSessionState state;
    state.money = 10;
    ChessRunRandom random(7);
    std::vector<ChessSemanticEvent> events;
    ChessRewardRules::enqueueCampaignRewards(state, content, random, 1, events);
    REQUIRE(state.phase == ChessSessionPhase::RewardChoice);
    REQUIRE(state.pendingRewards.size() == 1);
    const auto firstOptions = state.pendingRewards.front().options;

    ChessAction reroll;
    reroll.type = ChessActionType::RerollReward;
    REQUIRE(ChessRewardRules::validate(state, content, reroll) == ChessRuleErrorCode::None);
    ChessRewardRules::apply(state, content, random, reroll, events);
    CHECK(state.money == 9);
    CHECK(state.pendingRewards.front().rerolled);

    ChessAction choose;
    choose.type = ChessActionType::ChooseReward;
    choose.rewardId = state.pendingRewards.front().options.front().id;
    ChessRewardRules::apply(state, content, random, choose, events);
    CHECK(state.phase == ChessSessionPhase::Management);
    REQUIRE(state.equipmentInventory.size() == 1);
}

TEST_CASE("boss internal-skill rewards use the exact configured boss tier set", "[chess][reward][config]")
{
    ChessGameContentData data;
    data.balance.bossInterval = 4;
    data.neigongConfig.choiceCount = 5;
    data.neigongConfig.tiersByBoss = {
        {0, {2}},
        {1, {3}},
    };
    data.neigong = {
        {101, 1, 1, "一階"},
        {102, 2, 2, "二階"},
        {103, 3, 3, "三階"},
    };
    ChessGameContent content(std::move(data));
    ChessSessionState state;
    ChessRunRandom random(100);
    std::vector<ChessSemanticEvent> events;

    ChessRewardRules::enqueueCampaignRewards(state, content, random, 4, events);

    REQUIRE(state.pendingRewards.size() == 1);
    CHECK(state.pendingRewards.front().eligibleTiers == std::vector<int>{2});
    REQUIRE(state.pendingRewards.front().options.size() == 1);
    CHECK(state.pendingRewards.front().options.front().value == 102);
}

TEST_CASE("forced bans use configured pool candidates and reject direct overrides", "[chess][reward][ban][config]")
{
    ChessGameContentData data;
    for (const auto [roleId, cost] : std::vector<std::pair<int, int>>{{10, 1}, {20, 2}, {30, 3}})
    {
        ChessRoleDefinition role;
        role.ID = roleId;
        role.Name = std::format("角色{}", roleId);
        role.Cost = cost;
        data.roles.emplace(roleId, role);
        data.poolRoleIds.push_back(roleId);
    }
    ChessGameContent content(std::move(data));
    ChessSessionState state;
    state.fight = 1;
    state.seenRoleIds = {10};
    std::vector<ChessSemanticEvent> events;
    ChessRewardRules::enqueueForcedBan(state, content, 1, 2, events);
    REQUIRE(state.phase == ChessSessionPhase::RewardChoice);

    ChessAction unseenConfigured;
    unseenConfigured.type = ChessActionType::AddBan;
    unseenConfigured.roleId = 20;
    CHECK(ChessManagementRules::validate(state, content, unseenConfigured) == ChessRuleErrorCode::None);

    ChessAction illegalTier = unseenConfigured;
    illegalTier.roleId = 30;
    CHECK(ChessManagementRules::validate(state, content, illegalTier) == ChessRuleErrorCode::InvalidRole);

    ChessRunRandom random(2);
    ChessManagementRules::apply(state, content, random, unseenConfigured, events);
    CHECK(state.bannedRoleIds == std::set<int>{20});
    CHECK(state.phase == ChessSessionPhase::Management);
}

TEST_CASE("challenge rewards filter unavailable choices and complete only after a real grant",
          "[chess][reward][challenge]")
{
    ChessGameContentData data;
    data.balance.benchSize = 1;
    ChessRoleDefinition role;
    role.ID = 10;
    role.Name = "獎勵棋子";
    role.Cost = 1;
    data.roles.emplace(role.ID, role);
    data.poolRoleIds = {role.ID};
    BalanceConfig::ChallengeDef challenge;
    challenge.name = "可用性";
    challenge.rewards = {
        {BalanceConfig::ChallengeRewardType::GetPiece, 1},
        {BalanceConfig::ChallengeRewardType::GetNeigong, 1},
        {BalanceConfig::ChallengeRewardType::StarUp1to2, 1},
        {BalanceConfig::ChallengeRewardType::Gold, 5},
    };
    ChessGameContent content(std::move(data));
    ChessSessionState state;
    state.roster.emplace(1, ChessSessionPiece{1, role.ID, 3, false});
    std::vector<ChessSemanticEvent> events;

    ChessRewardRules::enqueueChallengeReward(state, content, challenge, events);

    REQUIRE(state.pendingRewards.size() == 1);
    REQUIRE(state.pendingRewards.front().options.size() == 1);
    CHECK(state.pendingRewards.front().options.front().id == "獲取5金幣");
    CHECK_FALSE(state.completedChallengeNames.contains(challenge.name));

    ChessAction choose;
    choose.type = ChessActionType::ChooseReward;
    choose.rewardId = "獲取5金幣";
    ChessRunRandom random(3);
    ChessRewardRules::apply(state, content, random, choose, events);
    CHECK(state.money == 5);
    CHECK(state.completedChallengeNames.contains(challenge.name));
}

TEST_CASE("challenge star rewards reuse cascading merge rules", "[chess][reward][challenge][merge]")
{
    ChessGameContentData data;
    ChessRoleDefinition role;
    role.ID = 10;
    role.Name = "升星棋子";
    role.Cost = 1;
    data.roles.emplace(role.ID, role);
    BalanceConfig::ChallengeDef challenge;
    challenge.name = "連鎖升星";
    challenge.rewards = {{BalanceConfig::ChallengeRewardType::StarUp1to2, 1}};
    ChessGameContent content(std::move(data));
    ChessSessionState state;
    state.nextChessInstanceId = 4;
    state.roster.emplace(1, ChessSessionPiece{1, role.ID, 1});
    state.roster.emplace(2, ChessSessionPiece{2, role.ID, 2});
    state.roster.emplace(3, ChessSessionPiece{3, role.ID, 2});
    std::vector<ChessSemanticEvent> events;
    ChessRewardRules::enqueueChallengeReward(state, content, challenge, events);
    ChessRunRandom random(4);

    ChessAction outer;
    outer.type = ChessActionType::ChooseReward;
    outer.rewardId = "升星★→★★(最高1費)";
    ChessRewardRules::apply(state, content, random, outer, events);
    REQUIRE(state.pendingRewards.front().kind == ChessRewardKind::StarUpgrade);
    ChessAction inner;
    inner.type = ChessActionType::ChooseReward;
    inner.rewardId = "star:1";
    ChessRewardRules::apply(state, content, random, inner, events);

    REQUIRE(state.roster.size() == 1);
    CHECK(state.roster.begin()->second.star == 3);
    CHECK(state.completedChallengeNames.contains(challenge.name));
}

TEST_CASE("timeout progression rolls preparation streams back transactionally", "[chess][progression][battle][determinism]")
{
    const auto content = managementContent();
    ChessSessionState state;
    ChessRunRandom random(99);
    PreparedChessBattle prepared;
    prepared.kind = PreparedChessBattleKind::Campaign;
    prepared.stableBattleId = "campaign:1";
    prepared.preparationCheckpoint = random.checkpointPreparation();
    state.preparedBattle = prepared;
    state.phase = ChessSessionPhase::BattleResolution;
    random.nextRaw(ChessRngStream::EnemyLineup);
    random.nextRaw(ChessRngStream::EnemyEquipment);
    random.nextRaw(ChessRngStream::MapSelection);
    random.nextRaw(ChessRngStream::BattleSeed);
    HeadlessBattleResult battle;
    battle.summary.outcome = Battle::BattleOutcome::Timeout;
    battle.summary.endFrame = kChessBattleFrameLimit;
    std::vector<ChessSemanticEvent> events;

    ChessProgressionRules::applyBattleResult(state, *content, random, battle, events);

    CHECK(state.phase == ChessSessionPhase::Management);
    CHECK(state.fight == 0);
    CHECK(state.lastBattleOutcome == Battle::BattleOutcome::Timeout);
    CHECK(random.checkpointPreparation() == prepared.preparationCheckpoint);
}

TEST_CASE("battle transition advances incrementally and journals only at the boundary", "[chess][session][battle][incremental]")
{
    ChessGameSession session(managementContent(), 101);
    REQUIRE(session.submitAndDrain(buySlot(0)).accepted);
    REQUIRE(session.submitAndDrain(buySlot(1)).accepted);
    ChessAction deploy;
    deploy.type = ChessActionType::SetDeployment;
    for (const auto& [id, piece] : session.state().roster)
    {
        deploy.chessInstanceIds.push_back(id);
    }
    REQUIRE(session.submitAndDrain(deploy).accepted);
    ChessAction prepare;
    prepare.type = ChessActionType::PrepareBattle;
    REQUIRE(session.submitAndDrain(prepare).accepted);
    const auto journalSizeBeforeBattle = session.journal().decisions().size();
    ChessSaveStore saves;
    REQUIRE(saves.save("prepared", session) == ChessCheckpointError::None);

    ChessAction start;
    start.type = ChessActionType::StartBattle;
    const auto begun = session.beginAction(start);

    REQUIRE(begun.accepted);
    CHECK(begun.transitionPending);
    CHECK(session.transitionPending());
    CHECK(session.state().phase == ChessSessionPhase::BattleResolution);
    CHECK(session.journal().decisions().size() == journalSizeBeforeBattle);
    REQUIRE(session.pendingBattleInitialization());
    REQUIRE(session.pendingBattleRuntime());
    CHECK_FALSE(session.exportReplay());
    CHECK(saves.save("pending", session) == ChessCheckpointError::UnstableBoundary);
    ChessTimelineReplacement replacement;
    CHECK(saves.load("prepared", session, replacement) == ChessCheckpointError::UnstableBoundary);
    CHECK(session.transitionPending());
    CHECK(session.journal().decisions().size() == journalSizeBeforeBattle);

    ChessAction lock;
    lock.type = ChessActionType::SetShopLocked;
    const auto rejected = session.beginAction(lock);
    CHECK_FALSE(rejected.accepted);
    CHECK(rejected.error == ChessRuleErrorCode::TransitionPending);
    CHECK(session.journal().decisions().size() == journalSizeBeforeBattle);

    const auto firstFrame = session.advanceAutomatic(1);
    CHECK(firstFrame.framesAdvanced == 1);
    REQUIRE(firstFrame.frames.size() == 1);
    CHECK_FALSE(firstFrame.completedAction.has_value());
    CHECK(session.transitionPending());

    std::optional<ChessActionResult> completed;
    while (!completed)
    {
        completed = session.advanceAutomatic(1).completedAction;
    }
    REQUIRE(completed->accepted);
    CHECK_FALSE(completed->transitionPending);
    CHECK_FALSE(session.transitionPending());
    CHECK(session.journal().decisions().size() == journalSizeBeforeBattle + 1);
    CHECK(completed->replaySequence == session.journal().decisions().back().sequence);
}
