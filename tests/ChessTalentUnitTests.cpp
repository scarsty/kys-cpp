#include "ChessGameSessionTestHelpers.h"
#include "ChessBattlePlanner.h"
#include "ChessRewardRules.h"
#include "ChessProgressionRules.h"
#include "ChessPreparationModel.h"
#include "ChessGameQueries.h"
#include "ChessReplayVerifier.h"
#include "ChessSessionCheckpoint.h"
#include "ChessPvp.h"
#include "ChessStandaloneBattle.h"
#include "ChessPreparedBattleAnalysis.h"
#include "ChessJsonCodec.h"
#include "BattleSetupFactory.h"
#include "BattleStarStats.h"
#include "battle/BattleDamageSystem.h"

#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>

using namespace KysChess;

namespace
{
std::shared_ptr<const ChessGameContent> talentContent(int luckPerRefresh = 1)
{
    ChessGameContentData data;
    data.difficulty = Difficulty::Hard;
    // 測試自有天賦數值，不讀取頂層設定檔。
    data.balance.initialMoney = 1000;
    data.balance.shopSlotCount = 6;
    data.balance.banBaseCount = 2;
    data.balance.legendaryShop = {.unlockFight = 5, .price = 40};
    data.balance.availableTalents.assign(
        kChessTalentIds.begin(),
        kChessTalentIds.end());
    data.balance.talents.insert_or_assign(
        ChessTalentId::DivineArms,
        ChessTalentDefinition{.description = "測試神兵", .legendaryShop = true});
    data.balance.talents.insert_or_assign(
        ChessTalentId::LateBloomer,
        ChessTalentDefinition{.description = "測試晚成", .evolutionWins = 30,
            .evolutionWarningBattles = 35, .evolutionStatusShields = {810, 410, 210, 0, 0}});
    data.balance.talents.insert_or_assign(
        ChessTalentId::Gambler,
        ChessTalentDefinition{
            .description = "測試賭徒",
            .openingBans = 3,
            .banMinTier = 1,
            .banMaxTier = 2,
            .luckLastFight = 20,
            .luckMinTier = 1,
            .luckMaxTier = 3,
            .luckPerRefresh = luckPerRefresh,
            .luckChancePerStack = 15,
            .luckStackCap = 5,
            .luckSurvivalHp = 30,
            .luckInvincibleFrames = 60,
        });
    data.balance.talents.insert_or_assign(
        ChessTalentId::Backbone,
        ChessTalentDefinition{
            .description = "測試中堅",
            .targetTier = 3,
            .mpPerExtraStar = 15,
            .strengtheningChargesPerExtraStar = 2,
            .strengtheningDamagePercent = 37,
            .extraStarCap = 2,
            .guaranteeStar = 2,
            .guaranteeCount = 1,
        });
    data.balance.playerEquipmentRewards = {{35, 2, 2, 4, 1}, {40, 3, 2, 4, 1}};
    data.balance.talentEquipmentRewards[ChessTalentId::DivineArms] = {{30, 4, 2, 4, 3}};
    data.balance.talents.emplace(ChessTalentId::Alliance, ChessTalentDefinition{
        .description = "測試合縱", .selectedComboBonus = 1,
        .reducedThresholdMinimum = 6, .thresholdReduction = 1});
    for (int tier = 1; tier <= 5; ++tier)
        for (int number = 0; number < 8; ++number)
        {
            ChessRoleDefinition role;
            role.ID = tier * 100 + number;
            role.Cost = tier;
            role.Name = std::to_string(role.ID);
            role.MaxHP = 200;
            role.MaxMP = 100;
            role.Attack = 40;
            Test::enableFastTestBattle(data, role);
            data.roles.emplace(role.ID, role);
            data.poolRoleIds.push_back(role.ID);
        }
    for (int tier = 1; tier <= 4; ++tier)
        for (int number = 0; number < 8; ++number)
            data.equipment.push_back({tier * 100 + number, tier, 0});
    for (int id = 0; id < 4; ++id)
    {
        ComboDef combo;
        combo.id = id;
        combo.name = std::format("結盟測試{}", id);
        combo.memberRoleIds = {100, 101, 102, 103, 104, 105, 106, 107};
        combo.isAntiCombo = id == 2;
        combo.isNpcOnly = id == 3;
        for (int count : {2, 4, 6, 8}) combo.thresholds.push_back({count, std::to_string(count)});
        if (id == 0)
        {
            EffectRule rule;
            rule.event = EffectEvent::BattleInitialized;
            ModifyAttributeAction effect;
            effect.attribute = BattleAttribute::Attack;
            effect.amount.flat = 17;
            effect.operation = AttributeOperation::FlatAdd;
            rule.actions.push_back({EffectActionValue{effect}});
            combo.thresholds.back().rules.push_back(std::move(rule));
        }
        data.combos.push_back(std::move(combo));
    }
    ChessBattleMapDefinition map;
    map.id = 7;
    map.battlefieldId = 7;
    map.teammateRoleIds = std::vector<int>(10, -1);
    map.enemyRoleIds = std::vector<int>(20, -1);
    for (int index = 0; index < 10; ++index)
    {
        map.teammateX.push_back(20 + index);
        map.teammateY.push_back(20);
    }
    for (int index = 0; index < 20; ++index)
    {
        map.enemyX.push_back(40 - index);
        map.enemyY.push_back(40);
    }
    data.battleMaps.emplace(map.id, std::move(map));
    return std::make_shared<const ChessGameContent>(std::move(data));
}

ChessAction action(ChessActionType type) { return {.type = type}; }
int eventCount(const std::vector<ChessSemanticEvent>& events, ChessSemanticEventType type)
{
    return static_cast<int>(std::ranges::count(events, type, &ChessSemanticEvent::type));
}
}

TEST_CASE("alliance opening choice is mandatory free and survives checkpoint replay", "[chess][talent][alliance][replay]")
{
    const auto content = talentContent();
    ChessGameSession session(content, 51, {.talent = ChessTalentId::Alliance});
    ChessGameSession ordinary(content, 51);
    REQUIRE(session.state().phase == ChessSessionPhase::RewardChoice);
    REQUIRE(session.state().pendingRewards.front().kind == ChessRewardKind::Synergy);
    REQUIRE(session.state().pendingRewards.front().options.size() == 2);
    CHECK(session.state().shop == ordinary.state().shop);
    CHECK(session.random().state() == ordinary.random().state());
    const auto money = session.state().money;
    const auto legal = session.legalActions();
    REQUIRE(legal.size() == 1);
    CHECK(legal.front().type == ChessActionType::ChooseReward);
    CHECK_FALSE(session.submitAndDrain(Test::buySlot(0)).accepted);
    CHECK_FALSE(session.submitAndDrain({.type = ChessActionType::ChooseReward, .rewardId = "synergy:2"}).accepted);
    CHECK_FALSE(session.submitAndDrain({.type = ChessActionType::ChooseReward, .rewardId = "synergy:3"}).accepted);
    CHECK_FALSE(ProtocolDetail::inspectComboDto(session, "結盟測試3", ProtocolDetail::ComboInspectionDetail::Full));
    CHECK_FALSE(ProtocolDetail::inspectCatalogDto(session,
        {.combo_names = {"結盟測試3"}}, ProtocolDetail::CatalogDetail::Full));
    CHECK(std::ranges::none_of(session.observe().combos, [](const auto& combo) { return combo.comboId == 3; }));
    auto checkpoint = ChessSessionCheckpoint::capture(session, 1);
    ChessCheckpointError error;
    auto parsed = ChessSessionCheckpoint::parseJson(checkpoint.serializeJson(), error);
    REQUIRE(parsed);
    ChessGameSession restored(content, 52);
    REQUIRE(parsed->restore(restored) == ChessCheckpointError::None);
    const ChessAction choose{.type = ChessActionType::ChooseReward, .rewardId = "synergy:0"};
    const auto chosen = session.submitAndDrain(choose);
    REQUIRE(chosen.accepted);
    CHECK(eventCount(chosen.events, ChessSemanticEventType::TalentComboChosen) == 1);
    REQUIRE(restored.submitAndDrain(choose).accepted);
    CHECK(restored.state() == session.state());
    CHECK(session.state().phase == ChessSessionPhase::Management);
    CHECK(session.state().selectedTalentComboId == 0);
    CHECK(session.state().money == money);
    CHECK_FALSE(session.submitAndDrain(choose).accepted);
    const auto progress = evaluateChessComboProgress(session.state(), *content, content->combos()[0]);
    CHECK(progress.physicalCount == 0);
    CHECK(progress.effectiveCount == 1);
    CHECK(formatChessComboProgressCount(progress) == "0+1/2");
    CHECK(session.observe().combos[0].requiredCounts == std::vector<int>{2, 4, 5, 7});
    const auto dto = ProtocolDetail::observationDto(session.observe(), *content,
        ProtocolDetail::ObservationDetail::Full);
    REQUIRE(dto.talent_synergy);
    CHECK(dto.talent_synergy->id == 0);
    REQUIRE(dto.combos.size() == 1);
    CHECK(dto.combos.front().physical_count == 0);
    CHECK(dto.combos.front().effective_count == 1);
    const auto inspected = ProtocolDetail::inspectComboDto(session, "結盟測試0", ProtocolDetail::ComboInspectionDetail::Full);
    REQUIRE(inspected);
    CHECK(inspected->thresholds->back().required_count == 7);
    checkpoint = ChessSessionCheckpoint::capture(session, 2);
    parsed = ChessSessionCheckpoint::parseJson(checkpoint.serializeJson(), error);
    REQUIRE(parsed);
    REQUIRE(parsed->restore(restored) == ChessCheckpointError::None);
    CHECK(restored.state() == session.state());
    CHECK(ChessReplayVerifier::verify(content, *session.exportReplay()).valid);
    auto changed = session.state();
    changed.selectedTalentComboId = 1;
    CHECK(chessStateHash(changed, session.random()) != chessStateHash(session.state(), session.random()));
    checkpoint.state.selectedTalentComboId = 2;
    CHECK(checkpoint.restore(restored) == ChessCheckpointError::UnrepresentableSnapshot);
}

TEST_CASE("alliance lowers high thresholds for all synergies and stacks only the chosen bonus", "[chess][talent][alliance]")
{
    const auto content = talentContent();
    ChessSessionState state;
    state.talent = ChessTalentId::Alliance;
    state.selectedTalentComboId = 0;
    for (int count = 0; count <= 8; ++count)
    {
        if (count > 0) state.roster.emplace(count, ChessSessionPiece{count, 99 + count, 1, true});
        const auto all = evaluateChessComboProgresses(state, *content);
        for (int id : {0, 1})
        {
            const auto& progress = all[id];
            CHECK(progress.requiredCounts == std::vector<int>{2, 4, 5, 7});
            CHECK(progress.physicalCount == count);
            CHECK(progress.effectiveCount == count + (id == 0 ? 1 : 0));
            int expected = -1;
            for (int index = 0; index < 4; ++index)
                if (progress.effectiveCount >= progress.requiredCounts[index]) expected = index;
            CHECK(progress.activeThresholdIndex == expected);
            CHECK(evaluateChessComboProgress(state, *content, content->combos()[id]).activeThresholdIndex == expected);
        }
        CHECK(all[2].effectiveCount == (count > 0 ? 1 : 0));
        CHECK(all[2].requiredCounts == std::vector<int>{2, 4, 6, 8});
    }
    state.talent = ChessTalentId::DivineArms;
    const auto ordinary = evaluateChessComboProgresses(state, *content);
    CHECK(ordinary[0].effectiveCount == 8);
    CHECK(ordinary[0].requiredCounts == std::vector<int>{2, 4, 6, 8});
}

TEST_CASE("alliance configurable bonuses combine with equipment and stars without extra bodies", "[chess][talent][alliance]")
{
    const std::vector<ChessComboResolverUnit> units{{10, 2, 1, -1, -1, 1}, {20, 1, 2, 50, -1, 2},
        {20, 1, 2, 50, -1, 3}};
    const std::vector<ChessComboResolverEquipmentRule> equipment{{50, {}, {"測試"}}};
    const std::vector<ChessComboResolverDefinition> definitions{{0, "測試", {10}, {3, 5, 7}, false, true}};
    const auto resolved = resolveChessCombos(units, equipment, definitions, {0, 1, 5, 2});
    REQUIRE(resolved.size() == 1);
    CHECK(resolved[0].physicalMemberCount == 2);
    CHECK(resolved[0].effectiveMemberCount == 4);
    CHECK(resolved[0].requiredCounts == std::vector<int>{3, 3, 5});
    CHECK(resolved[0].activeThresholdIndex == 1);
    CHECK(resolved[0].contributions.size() == 2);
}

TEST_CASE("alliance campaign challenge and PvP battles preserve team scoped synergy rules", "[chess][talent][alliance][battle][pvp]")
{
    const auto content = talentContent();
    ChessGameSession session(content, 88, {.talent = ChessTalentId::Alliance});
    REQUIRE(session.submitAndDrain({.type = ChessActionType::ChooseReward, .rewardId = "synergy:0"}).accepted);
    auto checkpoint = ChessSessionCheckpoint::capture(session, 1);
    for (int id = 1; id <= 6; ++id)
        checkpoint.state.roster.emplace(id, ChessSessionPiece{id, 99 + id, 1, true});
    checkpoint.state.nextChessInstanceId = 7;
    ChessManagementRules::maintainFormation(checkpoint.state);
    REQUIRE(checkpoint.restore(session) == ChessCheckpointError::None);
    const auto& state = session.state();
    ChessRunRandom random(8);
    BalanceConfig::ChallengeDef challenge;
    challenge.name = "合縱測試";
    challenge.enemies = {{100, 1}, {101, 1}, {102, 1}, {103, 1}, {104, 1}, {105, 1}};
    for (const auto& battle : {ChessBattlePlanner::prepareCampaign(state, *content, random),
        ChessBattlePlanner::prepareChallenge(state, *content, random, challenge)})
    {
        CHECK(battle.comboModifiersByTeam[0] == (ChessComboModifiers{0, 1, 6, 1}));
        CHECK(battle.comboModifiersByTeam[1] == ChessComboModifiers{});
        const auto setup = BattleSetupFactory::build(battle, *content, kChessBattleFrameLimit).setup;
        const auto allies = Battle::resolveBattleSetupCombos(setup.allyRoster, setup);
        CHECK(allies[0].effectiveMemberCount == 7);
        CHECK(allies[0].activeThresholdIndex == 3);
        CHECK(allies[1].activeThresholdIndex == 2);
        const auto enemies = Battle::resolveBattleSetupCombos(setup.enemyRoster, setup);
        CHECK(enemies[0].requiredCounts == std::vector<int>{2, 4, 6, 8});
        const auto analysis = analyzePreparedChessBattle(battle, *content, kChessBattleFrameLimit);
        REQUIRE_FALSE(analysis.allySynergies.empty());
        CHECK(analysis.allySynergies[0].thresholds.back().requiredCount == 7);
        for (const auto& unit : analysis.units)
        {
            REQUIRE(unit.initializedStatDelta);
            CHECK(unit.initializedStatDelta->attack == (unit.team == 0 ? 17 : 0));
        }
    }
    const auto composition = extractChessPvpComposition(session);
    CHECK(composition.talent == session.state().talent);
    CHECK(composition.comboModifiers == (ChessComboModifiers{0, 1, 6, 1}));
    const auto team = chessStandaloneBattleTeam(composition);
    CHECK(team.comboModifiers == composition.comboModifiers);
    ChessStandaloneBattleRequest request;
    request.teams = {team, team};
    request.teams[1].comboModifiers.selectedComboId = 1;
    std::string error;
    const auto built = ChessStandaloneBattle::prepare(content, request, error);
    REQUIRE(built);
    const auto setup = BattleSetupFactory::build(built->preparedBattle, *content, kChessBattleFrameLimit).setup;
    CHECK(Battle::resolveBattleSetupCombos(setup.allyRoster, setup)[0].activeThresholdIndex == 3);
    CHECK(Battle::resolveBattleSetupCombos(setup.enemyRoster, setup)[0].activeThresholdIndex == 2);
    CHECK(Battle::resolveBattleSetupCombos(setup.enemyRoster, setup)[1].activeThresholdIndex == 3);
}

TEST_CASE("talent identity round trips checkpoints replay headers and hashes", "[chess][talent][replay]")
{
    const auto content = talentContent();
    for (auto talent : kChessTalentIds)
    {
        ChessGameSession session(content, 9, {.talent = talent});
        CHECK(session.state().talent == talent);
        CHECK(session.observe().talent == talent);
        const auto checkpoint = ChessSessionCheckpoint::capture(session, 1, "天賦");
        ChessCheckpointError error;
        const auto decoded = ChessSessionCheckpoint::parseJson(checkpoint.serializeJson(), error);
        REQUIRE(decoded);
        ChessGameSession restored(content, 10);
        REQUIRE(decoded->restore(restored) == ChessCheckpointError::None);
        CHECK(restored.state() == session.state());
        CHECK(restored.random().state() == session.random().state());
        const auto replay = session.exportReplay();
        REQUIRE(replay);
        CHECK(replay->header.talent == talent);
        CHECK(ChessReplayVerifier::verify(content, *replay).valid);
        auto changed = session.state();
        changed.shopGuarantees = {301, 302};
        const auto first = chessStateHash(changed, session.random());
        changed.shopGuarantees = {302, 301};
        CHECK(chessStateHash(changed, session.random()) != first);
        changed.roster.emplace(1, ChessSessionPiece{1, 100});
        const auto beforeLuck = chessStateHash(changed, session.random());
        changed.roster.at(1).luckStacks = 1;
        CHECK(chessStateHash(changed, session.random()) != beforeLuck);
    }
    const auto easy = Test::managementContent(10, Difficulty::Easy);
    CHECK_THROWS_AS(ChessGameSession(easy, 1, {.talent = ChessTalentId::Gambler}), std::invalid_argument);
}

TEST_CASE("opening bans preserve first shop until finished then refresh once", "[chess][talent][gambler]")
{
    const auto content = talentContent();
    ChessGameSession gambler(content, 21, {.talent = ChessTalentId::Gambler});
    ChessGameSession ordinary(content, 21);
    CHECK(gambler.state().shop == ordinary.state().shop);
    CHECK(gambler.random().streamState(ChessRngStream::Shop) == ordinary.random().streamState(ChessRngStream::Shop));
    CHECK(gambler.state().phase == ChessSessionPhase::RewardChoice);
    REQUIRE_FALSE(gambler.state().pendingRewards.empty());
    CHECK(gambler.state().pendingRewards.front().parameter == 3);
    CHECK_FALSE(gambler.submitAndDrain(Test::buySlot(0)).accepted);
    for (const auto& option : gambler.state().pendingRewards.front().options)
        CHECK(content->role(option.value)->Cost <= 2);
    const auto before = gambler.random().streamState(ChessRngStream::Shop);
    ChessAction ban{.type = ChessActionType::AddBan, .roleId = 100};
    REQUIRE(gambler.submitAndDrain(ban).accepted);
    CHECK(gambler.random().streamState(ChessRngStream::Shop) == before);
    const auto skipped = gambler.submitAndDrain(action(ChessActionType::SkipForcedBans));
    REQUIRE(skipped.accepted);
    CHECK(eventCount(skipped.events, ChessSemanticEventType::OpeningTalentShopRefreshed) == 1);
    CHECK(gambler.state().selectedForcedBanCount == 1);
    CHECK(gambler.observe().maximumBanCount == 3);
    CHECK(gambler.state().phase == ChessSessionPhase::Management);
    CHECK_FALSE(std::ranges::contains(gambler.state().shop, 100, &ChessSessionShopSlot::roleId));
    CHECK(gambler.random().streamState(ChessRngStream::TalentManagement).rawDrawCount == 0);
    ChessGameSession skippedAll(content, 21, {.talent = ChessTalentId::Gambler});
    REQUIRE(skippedAll.submitAndDrain(action(ChessActionType::SkipForcedBans)).accepted);
    CHECK(skippedAll.state().shop == ordinary.state().shop);
    CHECK(skippedAll.random().streamState(ChessRngStream::Shop) == before);
}

TEST_CASE("paid refresh luck cutoff eligibility inheritance and isolated RNG", "[chess][talent][gambler]")
{
    const auto content = talentContent();
    ChessSessionState state;
    state.talent = ChessTalentId::Gambler;
    state.money = 100;
    state.fight = content->balance().talent(state.talent).luckLastFight - 1;
    state.roster.emplace(1, ChessSessionPiece{1, 300, 1, true});
    state.roster.emplace(2, ChessSessionPiece{2, 300, 1, false});
    state.roster.emplace(3, ChessSessionPiece{3, 400, 1, false});
    state.nextChessInstanceId = 4;
    ChessRunRandom random(5), baseline(5);
    std::vector<ChessSemanticEvent> events;
    auto ordinary = state;
    ordinary.talent = ChessTalentId::DivineArms;
    ChessManagementRules::apply(state, *content, random, action(ChessActionType::RefreshShop), events);
    ChessManagementRules::apply(ordinary, *content, baseline, action(ChessActionType::RefreshShop), events);
    CHECK(state.roster.at(1).luckStacks == 1);
    CHECK(state.roster.at(2).luckStacks == 0);
    CHECK(state.roster.at(3).luckStacks == 0);
    CHECK(random.streamState(ChessRngStream::Shop) == baseline.streamState(ChessRngStream::Shop));
    const auto talentRng = random.streamState(ChessRngStream::TalentManagement);
    state.fight = content->balance().talent(state.talent).luckLastFight;
    ChessManagementRules::apply(state, *content, random, action(ChessActionType::RefreshShop), events);
    CHECK(random.streamState(ChessRngStream::TalentManagement) == talentRng);
    state.fight = content->balance().talent(state.talent).luckLastFight - 1;
    state.freeShopRefreshAvailable = true;
    ChessManagementRules::apply(state, *content, random, action(ChessActionType::RefreshShop), events);
    CHECK(random.streamState(ChessRngStream::TalentManagement) == talentRng);
    state.roster.at(1).luckStacks = 2;
    state.roster.at(2).luckStacks = 4;
    ChessManagementRules::grantPiece(state, *content, 300, events);
    const auto merged = std::ranges::find_if(state.roster, [](const auto& entry) { return entry.second.roleId == 300; });
    REQUIRE(merged != state.roster.end());
    CHECK(merged->second.star == 2);
    CHECK(merged->second.luckStacks == 5);
    ChessManagementRules::apply(state, *content, random,
        {.type = ChessActionType::SellChess, .chessInstanceId = merged->first}, events);
    CHECK(state.roster.size() == 1);
    CHECK(content->balance().talent(state.talent).luckChance(100) == 75);
}

TEST_CASE("paid refresh prioritizes deployed uncapped pieces and stops at the stack cap", "[chess][talent][gambler]")
{
    const auto content = talentContent(3);
    const auto& talent = content->balance().talent(ChessTalentId::Gambler);
    ChessSessionState state;
    state.talent = ChessTalentId::Gambler;
    state.money = 100;
    state.roster.emplace(1, ChessSessionPiece{1, 100, 1, true});
    state.roster.emplace(2, ChessSessionPiece{2, 200, 1, true});
    state.roster.emplace(3, ChessSessionPiece{3, 300, 1, false});
    state.roster.emplace(4, ChessSessionPiece{4, 400, 1, true});
    state.roster.at(1).luckStacks = talent.luckStackCap;
    state.roster.at(2).luckStacks = talent.luckStackCap - 1;
    ChessRunRandom random(5);
    std::vector<ChessSemanticEvent> events;
    const auto refresh = [&] {
        events.clear();
        ChessManagementRules::apply(state, *content, random, action(ChessActionType::RefreshShop), events);
    };
    refresh();
    CHECK(state.roster.at(1).luckStacks == talent.luckStackCap);
    CHECK(state.roster.at(2).luckStacks == talent.luckStackCap);
    CHECK(state.roster.at(3).luckStacks == 0);
    const auto granted = std::ranges::find(events, ChessSemanticEventType::LuckGranted, &ChessSemanticEvent::type);
    REQUIRE(granted != events.end());
    CHECK(granted->value == 1);
    refresh();
    CHECK(state.roster.at(3).luckStacks == 3);
    refresh();
    CHECK(state.roster.at(3).luckStacks == talent.luckStackCap);
    const auto talentRng = random.streamState(ChessRngStream::TalentManagement);
    refresh();
    CHECK(eventCount(events, ChessSemanticEventType::LuckGranted) == 0);
    CHECK(random.streamState(ChessRngStream::TalentManagement) == talentRng);
    CHECK(state.roster.at(4).luckStacks == 0);
}

TEST_CASE("gambler checkpoint preserves invested luck and deterministic continuation", "[chess][talent][replay]")
{
    const auto content = talentContent();
    ChessGameSession session(content, 19, {.talent = ChessTalentId::Gambler});
    REQUIRE(session.submitAndDrain({.type = ChessActionType::AddBan, .roleId = 100}).accepted);
    auto checkpoint = ChessSessionCheckpoint::capture(session, 1, "開局禁棋中");
    ChessCheckpointError error;
    auto parsed = ChessSessionCheckpoint::parseJson(checkpoint.serializeJson(), error);
    REQUIRE(parsed);
    ChessGameSession restored(content, 20, {.talent = ChessTalentId::Gambler});
    REQUIRE(parsed->restore(restored) == ChessCheckpointError::None);
    REQUIRE(session.submitAndDrain(action(ChessActionType::SkipForcedBans)).accepted);
    REQUIRE(restored.submitAndDrain(action(ChessActionType::SkipForcedBans)).accepted);
    CHECK(restored.state() == session.state());
    REQUIRE(session.submitAndDrain(Test::buySlot(0)).accepted);
    for (int i = 0; i < 4; ++i)
        REQUIRE(session.submitAndDrain(action(ChessActionType::RefreshShop)).accepted);
    REQUIRE(session.state().roster.size() == 1);
    CHECK(session.state().roster.begin()->second.luckStacks == 4);
    checkpoint = ChessSessionCheckpoint::capture(session, 2, "賭運累積後");
    parsed = ChessSessionCheckpoint::parseJson(checkpoint.serializeJson(), error);
    REQUIRE(parsed);
    REQUIRE(parsed->restore(restored) == ChessCheckpointError::None);
    REQUIRE(session.submitAndDrain(action(ChessActionType::RefreshShop)).accepted);
    REQUIRE(restored.submitAndDrain(action(ChessActionType::RefreshShop)).accepted);
    CHECK(restored.state() == session.state());
    CHECK(restored.random().state() == session.random().state());
    const auto replay = restored.exportReplay();
    REQUIRE(replay);
    CHECK(ChessReplayVerifier::verify(content, *replay).valid);
}

TEST_CASE("backbone grants follow star milestones and preserve six shop draws", "[chess][talent][backbone]")
{
    const auto content = talentContent();
    ChessSessionState state;
    state.talent = ChessTalentId::Backbone;
    state.money = 100;
    std::vector<ChessSemanticEvent> events;
    const int id = ChessManagementRules::grantPiece(state, *content, 300, events);
    ChessManagementRules::upgradePiece(state, *content, id, 2, events);
    REQUIRE(state.shopGuarantees == std::vector<int>{300});
    CHECK(state.shop.empty());
    state.bannedRoleIds.insert(300);
    for (int i = 1; i <= 7; ++i) ChessManagementRules::grantPiece(state, *content, 300 + i, events, 0, 2);
    CHECK(state.shopGuarantees.size() == 8);
    auto ordinary = state;
    ordinary.shopGuarantees.clear();
    ChessRunRandom random(8), baseline(8);
    ChessManagementRules::refreshShop(state, *content, random, &events);
    ChessManagementRules::refreshShop(ordinary, *content, baseline);
    REQUIRE(state.shop.size() == 6);
    for (int i = 0; i < 6; ++i) CHECK(state.shop[i].roleId == 300 + i);
    CHECK(state.shopGuarantees == std::vector<int>{306, 307});
    CHECK(random.streamState(ChessRngStream::Shop) == baseline.streamState(ChessRngStream::Shop));
    CHECK(ChessManagementRules::pieceValue(*content, state.shop[0].roleId, 1) == 3);
    ChessManagementRules::refreshShop(state, *content, random, &events);
    CHECK(state.shopGuarantees.empty());
    CHECK(state.shop[0].roleId == 306);
    CHECK(state.shop[1].roleId == 307);
}

TEST_CASE("backbone suppresses guarantees after full chain reaches three stars", "[chess][talent][backbone]")
{
    const auto content = talentContent();
    ChessSessionState state;
    state.talent = ChessTalentId::Backbone;
    std::vector<ChessSemanticEvent> events;
    for (int i = 0; i < 8; ++i) ChessManagementRules::grantPiece(state, *content, 300, events);
    REQUIRE(state.shopGuarantees.size() == 2);
    events.clear();
    ChessManagementRules::grantPiece(state, *content, 300, events);
    REQUIRE(state.roster.size() == 1);
    CHECK(state.roster.begin()->second.star == 3);
    CHECK(state.shopGuarantees.empty());
    CHECK(eventCount(events, ChessSemanticEventType::ShopGuaranteeQueued) == 0);
    CHECK(eventCount(events, ChessSemanticEventType::ShopGuaranteeCancelled) == 2);
}

TEST_CASE("growth amplification uses integer G S U for every stat", "[chess][talent][growth]")
{
    const BattleStarStatInputs base{101, 31, 17, 21, 11, 12, 13, 14, 15};
    BattleStarGrowthConfig config;
    config.hpPerWin = 1.7;
    config.attackPerWin = 0.6;
    config.defencePerWin = 0.5;
    config.speedPerWin = 0.5;
    config.weaponSkillPerWin = 0.5;
    for (int star = 1; star <= 3; ++star)
        for (int wins : {0, 1, 3, 17})
            for (int ratio : {0, 33, 50, 100, 120})
            {
                const auto result = computeStarBoostedStats(base, config, star, wins, 2, 3, 4, ratio);
                const auto expected = [&](int b, double growth, double mult, int flat) {
                    const int g = static_cast<int>(std::floor(wins * growth));
                    const int s = g * ratio / 100;
                    return static_cast<int>(std::floor((b + s) * (1 + mult * (star - 1)))) + flat * (star - 1) + g - s;
                };
                CHECK(result.hp == expected(101, 3.7, config.hpMultiplierPerStar, config.flatHpPerStar));
                CHECK(result.atk == expected(31, 3.6, config.attackMultiplierPerStar, config.flatAttackPerStar));
                CHECK(result.def == expected(17, 4.5, config.defenceMultiplierPerStar, config.flatDefencePerStar));
                CHECK(result.spd == expected(21, 0.5, config.speedMultiplierPerStar, 0));
                CHECK(result.fist == expected(11, 0.5, config.martialMultiplierPerStar, 15));
                CHECK(result.sword == expected(12, 0.5, config.martialMultiplierPerStar, 15));
                CHECK(result.knife == expected(13, 0.5, config.martialMultiplierPerStar, 15));
                CHECK(result.unusual == expected(14, 0.5, config.martialMultiplierPerStar, 15));
                CHECK(result.hidden == expected(15, 0.5, config.martialMultiplierPerStar, 15));
            }
}

TEST_CASE("lethal recovery follows death protection and consumes exactly one independent draw", "[chess][talent][damage]")
{
    Battle::BattleDamageUnitState unit;
    unit.id = 1;
    unit.vitals = {100, 100, 23, 100};
    unit.deathPrevention = true;
    unit.deathPreventionFrames = 30;
    unit.lethalRecovery = Battle::BattleLethalRecovery{100, 1, 120};
    Battle::BattleRuntimeRandom random(17);
    Battle::BattleDamageSystem system;
    auto first = system.applyDamageTaken(unit, 100, true, &random);
    CHECK(first.deathPrevented);
    CHECK_FALSE(first.recoveryTested);
    CHECK(first.defender.lethalRecovery->used == false);
    CHECK(random.rawDrawCount() == 0);
    first.defender.invincible = 0;
    auto second = system.applyDamageTaken(first.defender, 100, true, &random);
    CHECK(second.recoverySucceeded);
    CHECK_FALSE(second.deathPrevented);
    CHECK(second.defender.vitals.hp == 1);
    CHECK(second.defender.vitals.mp == unit.vitals.mp);
    CHECK(second.defender.invincible == 120);
    CHECK(random.rawDrawCount() == 1);
    auto third = system.applyDamageTaken(second.defender, 100, true, &random);
    CHECK(third.died);
    CHECK_FALSE(third.recoveryTested);
    CHECK(random.rawDrawCount() == 1);
    unit.deathPrevention = false;
    unit.lethalRecovery->chancePercent = 0;
    const auto failed = system.applyDamageTaken(unit, 100, true, &random);
    CHECK(failed.recoveryTested);
    CHECK_FALSE(failed.recoverySucceeded);
    CHECK(failed.died);
}

TEST_CASE("campaign and challenge lower talents from their deployed lineup", "[chess][talent][battle]")
{
    const auto content = talentContent();
    ChessSessionState state;
    state.talent = ChessTalentId::Backbone;
    state.roster.emplace(1, ChessSessionPiece{1, 300, 3, true});
    state.roster.emplace(2, ChessSessionPiece{2, 301, 2, true});
    state.roster.emplace(3, ChessSessionPiece{3, 100, 3, true});
    state.roster.emplace(4, ChessSessionPiece{4, 101, 3, false});
    ChessManagementRules::maintainFormation(state);
    ChessRunRandom random(3);
    BalanceConfig::ChallengeDef challenge;
    challenge.name = "天賦測試";
    challenge.enemies = {{200, 1}};
    for (const auto& battle : {ChessBattlePlanner::prepareCampaign(state, *content, random),
        ChessBattlePlanner::prepareChallenge(state, *content, random, challenge)})
    {
        CHECK(battle.units[0].openingMp == 30);
        CHECK(battle.units[0].amplifiedGrowthPercent == 0);
        CHECK(battle.units[1].amplifiedGrowthPercent == 0);
        CHECK(battle.units[2].amplifiedGrowthPercent == 0);
        CHECK(battle.units[3].amplifiedGrowthPercent == 0);
        CHECK(battle.units[1].openingMp == 30);
        CHECK(battle.units[2].openingMp == 0);
        CHECK(battle.units[3].openingMp == 0);
        CHECK(battle.units[0].openingStrengthening == (Battle::BattleStrengthening{4, 4, 37}));
        CHECK(battle.units[1].openingStrengthening == (Battle::BattleStrengthening{4, 4, 37}));
        CHECK(battle.units[2].openingStrengthening == Battle::BattleStrengthening{});
        CHECK(battle.units[3].openingStrengthening == Battle::BattleStrengthening{});
    }
    state.talent = ChessTalentId::LateBloomer;
    auto battle = ChessBattlePlanner::prepareChallenge(state, *content, random, challenge);
    CHECK(battle.units[0].amplifiedGrowthPercent == 0);
    CHECK(battle.units.back().amplifiedGrowthPercent == 0);
    CHECK(battle.units[0].openingStrengthening == Battle::BattleStrengthening{});
    state.talent = ChessTalentId::Gambler;
    state.roster.at(1).luckStacks = 5;
    battle = ChessBattlePlanner::prepareChallenge(state, *content, random, challenge);
    REQUIRE(battle.units[0].lethalRecovery);
    CHECK(battle.units[0].lethalRecovery->chancePercent == 75);
    CHECK_FALSE(battle.units.back().lethalRecovery);
}

TEST_CASE("backbone counts only deployed extra stars and caps both opening benefits", "[chess][talent][backbone]")
{
    const auto content = talentContent();
    ChessSessionState state;
    state.talent = ChessTalentId::Backbone;
    state.roster.emplace(1, ChessSessionPiece{1, 300, 3, true});
    state.roster.emplace(2, ChessSessionPiece{2, 100, 1, true});
    state.roster.emplace(3, ChessSessionPiece{3, 200, 1, true});
    state.roster.emplace(4, ChessSessionPiece{4, 400, 3, false});
    ChessManagementRules::maintainFormation(state);

    SECTION("one-star supporters and bench pieces provide no charges") {}
    SECTION("each extra star contributes") { state.roster.at(2).star = 2; }
    SECTION("contributions from different costs are summed")
    {
        state.roster.at(2).star = 2;
        state.roster.at(3).star = 2;
    }
    SECTION("the extra-star cap applies to MP and charges together")
    {
        state.roster.at(2).star = 3;
        state.roster.at(3).star = 3;
    }
    const int stars = std::min(2, state.roster.at(2).star + state.roster.at(3).star - 2);
    PreparedChessBattle battle;
    for (const int id : {1, 2, 3})
    {
        const auto& piece = state.roster.at(id);
        battle.units.push_back({.chessInstanceId = id, .roleId = piece.roleId,
            .team = 0, .star = piece.star});
    }
    battle.units.push_back({.roleId = 300, .team = 1, .star = 3});
    ChessBattlePlanner::applyPlayerTalents(battle, state, *content);

    CHECK(battle.units[0].openingMp == stars * 15);
    CHECK(battle.units[0].openingStrengthening == (Battle::BattleStrengthening{stars * 2, stars * 2, 37}));
    for (const int index : {1, 2, 3})
    {
        CHECK(battle.units[index].openingMp == 0);
        CHECK(battle.units[index].openingStrengthening == Battle::BattleStrengthening{});
    }
}

TEST_CASE("PvP extraction preserves backbone opening strengthening", "[chess][talent][backbone][pvp]")
{
    const auto content = talentContent();
    ChessGameSession session(content, 88, {.talent = ChessTalentId::Backbone});
    auto checkpoint = ChessSessionCheckpoint::capture(session, 1);
    checkpoint.state.roster.emplace(1, ChessSessionPiece{1, 300, 2, true});
    checkpoint.state.roster.emplace(2, ChessSessionPiece{2, 100, 3, true});
    checkpoint.state.nextChessInstanceId = 3;
    ChessManagementRules::maintainFormation(checkpoint.state);
    REQUIRE(checkpoint.restore(session) == ChessCheckpointError::None);

    const auto composition = extractChessPvpComposition(session);
    CHECK(composition.talent == session.state().talent);
    REQUIRE(composition.pieces.size() == 2);
    const auto team = chessStandaloneBattleTeam(composition);
    REQUIRE(team.pieces.size() == 2);
    const auto target = std::ranges::find(composition.pieces, 300, &ChessPvpPiece::roleId);
    REQUIRE(target != composition.pieces.end());
    CHECK(target->openingMp == 30);
    CHECK(target->openingStrengthening == (Battle::BattleStrengthening{4, 4, 37}));
    const auto transferred = std::ranges::find(team.pieces, 300, &ChessStandaloneBattlePiece::roleId);
    REQUIRE(transferred != team.pieces.end());
    CHECK(transferred->openingStrengthening == target->openingStrengthening);
}

TEST_CASE("equipment rewards respect both tier bounds and talent shop capability", "[chess][talent][reward]")
{
    const auto content = talentContent();
    for (auto talent : kChessTalentIds)
    {
        ChessSessionState state;
        state.talent = talent;
        state.fight = 31;
        state.money = 100;
        const ChessAction buy{.type = ChessActionType::BuyLegendaryEquipment, .itemId = 400};
        CHECK((ChessManagementRules::validate(state, *content, buy) == ChessRuleErrorCode::None)
            == (talent == ChessTalentId::DivineArms));
        ChessRunRandom random(3);
        std::vector<ChessSemanticEvent> events;
        ChessRewardRules::enqueueCampaignRewards(state, *content, random, 30, events);
        CHECK(state.pendingRewards.size() == (talent == ChessTalentId::DivineArms ? 1 : 0));
        for (const auto& pending : state.pendingRewards)
        {
            std::set<int> chosen;
            for (const auto& option : pending.options)
            {
                CHECK(option.value / 100 >= 3);
                CHECK(option.value / 100 <= 4);
                CHECK(chosen.insert(option.value).second);
            }
        }
    }
}

TEST_CASE("late bloomer chooses one owned instance at any tier or star and preserves existing wins", "[chess][talent][late-bloomer]")
{
    const auto content = talentContent();
    for (int tier = 1; tier <= 5; ++tier)
        for (int star = 1; star <= 3; ++star)
        {
            ChessSessionState state;
            state.talent = ChessTalentId::LateBloomer;
            state.roster.emplace(1, ChessSessionPiece{.instanceId = 1, .roleId = tier * 100,
                .star = star, .fightsWon = 30});
            state.roster.emplace(2, ChessSessionPiece{.instanceId = 2, .roleId = tier * 100,
                .star = star, .deployed = true, .fightsWon = 40});
            ChessRunRandom random(1);
            std::vector<ChessSemanticEvent> events;
            const ChessAction choose{.type = ChessActionType::ChooseLateBloomer, .chessInstanceId = 1};
            REQUIRE(ChessManagementRules::validate(state, *content, choose) == ChessRuleErrorCode::None);
            ChessManagementRules::apply(state, *content, random, choose, events);
            CHECK(state.lateBloomerSelectionUsed);
            CHECK(state.roster.at(1).lateBloomerChosen);
            CHECK(state.roster.at(1).fightsWon == 30);
            CHECK(state.roster.at(1).star == (star == 3 ? 4 : star));
            CHECK_FALSE(state.roster.at(2).lateBloomerChosen);
            CHECK(state.roster.at(2).star == star);
            CHECK(ChessManagementRules::validate(state, *content,
                {.type = ChessActionType::ChooseLateBloomer, .chessInstanceId = 2}) == ChessRuleErrorCode::TalentSelectionUnavailable);
            ChessManagementRules::apply(state, *content, random,
                {.type = ChessActionType::SellChess, .chessInstanceId = 1}, events);
            CHECK(ChessManagementRules::validate(state, *content,
                {.type = ChessActionType::ChooseLateBloomer, .chessInstanceId = 2}) == ChessRuleErrorCode::TalentSelectionUnavailable);
        }
    ChessSessionState state;
    state.talent = ChessTalentId::LateBloomer;
    CHECK(ChessManagementRules::validate(state, *content,
        {.type = ChessActionType::ChooseLateBloomer, .chessInstanceId = 99}) == ChessRuleErrorCode::UnknownChessInstance);
}

TEST_CASE("late bloomer follows recursive merges and reward upgrades without spreading to a second copy", "[chess][talent][late-bloomer]")
{
    const auto content = talentContent();
    ChessSessionState state;
    state.talent = ChessTalentId::LateBloomer;
    state.lateBloomerSelectionUsed = true;
    state.nextChessInstanceId = 10;
    state.roster.emplace(1, ChessSessionPiece{.instanceId = 1, .roleId = 100, .fightsWon = 35});
    state.roster.emplace(2, ChessSessionPiece{.instanceId = 2, .roleId = 100, .deployed = true,
        .fightsWon = 12, .lateBloomerChosen = true});
    state.roster.emplace(3, ChessSessionPiece{.instanceId = 3, .roleId = 100, .star = 2});
    state.roster.emplace(4, ChessSessionPiece{.instanceId = 4, .roleId = 100, .star = 2});
    state.roster.emplace(5, ChessSessionPiece{.instanceId = 5, .roleId = 100, .star = 3, .fightsWon = 50});
    std::vector<ChessSemanticEvent> events;
    ChessManagementRules::grantPiece(state, *content, 100, events);
    REQUIRE(state.roster.size() == 2);
    const auto chosen = std::ranges::find_if(state.roster, [](const auto& entry) { return entry.second.lateBloomerChosen; });
    REQUIRE(chosen != state.roster.end());
    CHECK(chosen->second.star == 4);
    CHECK(chosen->second.fightsWon == 35);
    CHECK(chosen->second.deployed);
    CHECK(state.roster.at(5).star == 3);
    CHECK(eventCount(events, ChessSemanticEventType::LateBloomerEvolved) == 1);
    const auto copies = queryChessRoleCopies(state, 100);
    CHECK(copies.copiesByStar == std::array{0, 0, 1, 1});
    CHECK(copies.ownedCopies == 18);
    CHECK(ChessManagementRules::pieceValue(*content, 100, 4) == ChessManagementRules::pieceValue(*content, 100, 3));

    auto& piece = state.roster.at(chosen->first);
    piece.star = 2;
    events.clear();
    ChessManagementRules::upgradePiece(state, *content, piece.instanceId, 3, events);
    CHECK(piece.star == 4);
    CHECK(eventCount(events, ChessSemanticEventType::LateBloomerEvolved) == 1);
}

TEST_CASE("late bloomer selection round trips action protocol checkpoint and replay", "[chess][talent][late-bloomer][replay]")
{
    const auto content = talentContent();
    ChessGameSession session(content, 24, {.talent = ChessTalentId::LateBloomer});
    REQUIRE(session.submitAndDrain(Test::buySlot(0)).accepted);
    const int id = session.state().roster.begin()->first;
    const ChessAction choose{.type = ChessActionType::ChooseLateBloomer, .chessInstanceId = id};
    REQUIRE(parseChessActionJson(serializeChessActionJson(choose)) == choose);
    CHECK(std::ranges::any_of(session.legalActions(), [](const auto& action) {
        return action.type == ChessActionType::ChooseLateBloomer;
    }));
    const auto before = session.observe().stateHash;
    REQUIRE(session.submitAndDrain(choose).accepted);
    CHECK(session.observe().stateHash != before);
    CHECK(session.observe().lateBloomerSelectionUsed);
    CHECK_FALSE(session.submitAndDrain(choose).accepted);
    CHECK(ChessReplayVerifier::verify(content, *session.exportReplay()).valid);
    const auto checkpoint = ChessSessionCheckpoint::capture(session, 1);
    ChessCheckpointError error;
    const auto parsed = ChessSessionCheckpoint::parseJson(checkpoint.serializeJson(), error);
    REQUIRE(parsed);
    ChessGameSession restored(content, 25);
    REQUIRE(parsed->restore(restored) == ChessCheckpointError::None);
    CHECK(restored.state() == session.state());
    auto invalid = checkpoint;
    invalid.state.roster.emplace(99, ChessSessionPiece{.instanceId = 99, .roleId = 100, .lateBloomerChosen = true});
    CHECK(invalid.restore(restored) == ChessCheckpointError::UnrepresentableSnapshot);
}

TEST_CASE("late bloomer four-star opening shields survive PvP and standalone conversion", "[chess][talent][late-bloomer][pvp]")
{
    const auto content = talentContent();
    for (int tier = 1; tier <= 5; ++tier)
    {
        ChessGameSession session(content, 88, {.talent = ChessTalentId::LateBloomer});
        auto checkpoint = ChessSessionCheckpoint::capture(session, 1);
        checkpoint.state.lateBloomerSelectionUsed = true;
        checkpoint.state.roster.emplace(1, ChessSessionPiece{.instanceId = 1, .roleId = tier * 100, .star = 4,
            .deployed = true, .fightsWon = 30, .lateBloomerChosen = true});
        checkpoint.state.roster.emplace(2, ChessSessionPiece{.instanceId = 2, .roleId = tier * 100,
            .star = 3, .deployed = true, .fightsWon = 40});
        checkpoint.state.nextChessInstanceId = 3;
        ChessManagementRules::maintainFormation(checkpoint.state);
        REQUIRE(checkpoint.restore(session) == ChessCheckpointError::None);
        const auto composition = extractChessPvpComposition(session);
        const auto team = chessStandaloneBattleTeam(composition);
        const auto preview = ChessBattlePlanner::prepareRosterPreview(session.state(), *content);
        const auto shield = content->balance().talent(ChessTalentId::LateBloomer).evolutionStatusShields[tier - 1];
        CHECK(preview.units[0].openingStatusShield == shield);
        CHECK(preview.units[1].openingStatusShield == 0);
        CHECK(composition.pieces[0].star == 4);
        CHECK(composition.pieces[0].openingStatusShield == shield);
        CHECK(team.pieces[0].openingStatusShield == shield);
        CHECK(team.pieces[1].openingStatusShield == 0);
        CHECK(chessRoleMagicsForStar(*content, *content->role(tier * 100), 4)
            == chessRoleMagicsForStar(*content, *content->role(tier * 100), 3));
    }
}
