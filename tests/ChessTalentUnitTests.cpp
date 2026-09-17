#include "ChessGameSessionTestHelpers.h"
#include "ChessBattlePlanner.h"
#include "ChessRewardRules.h"
#include "ChessReplayVerifier.h"
#include "ChessSessionCheckpoint.h"
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
        ChessTalentDefinition{.description = "測試晚成", .amplifiedGrowthPercent = 100});
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
            .extraStarCap = 2,
            .guaranteeStar = 2,
            .guaranteeCount = 1,
        });
    data.balance.playerEquipmentRewards = {{35, 2, 2, 4, 1}, {40, 3, 2, 4, 1}};
    data.balance.talentEquipmentRewards[ChessTalentId::DivineArms] = {{30, 4, 2, 4, 3}};
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
    return std::make_shared<const ChessGameContent>(std::move(data));
}

ChessAction action(ChessActionType type) { return {.type = type}; }
int eventCount(const std::vector<ChessSemanticEvent>& events, ChessSemanticEventType type)
{
    return static_cast<int>(std::ranges::count(events, type, &ChessSemanticEvent::type));
}
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
            for (int ratio : {0, 33, 50, 100})
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
        CHECK(battle.units[1].openingMp == 30);
        CHECK(battle.units[2].openingMp == 0);
        CHECK(battle.units[3].openingMp == 0);
    }
    state.talent = ChessTalentId::LateBloomer;
    auto battle = ChessBattlePlanner::prepareChallenge(state, *content, random, challenge);
    CHECK(battle.units[0].amplifiedGrowthPercent == 100);
    CHECK(battle.units.back().amplifiedGrowthPercent == 0);
    state.talent = ChessTalentId::Gambler;
    state.roster.at(1).luckStacks = 5;
    battle = ChessBattlePlanner::prepareChallenge(state, *content, random, challenge);
    REQUIRE(battle.units[0].lethalRecovery);
    CHECK(battle.units[0].lethalRecovery->chancePercent == 75);
    CHECK_FALSE(battle.units.back().lethalRecovery);
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
