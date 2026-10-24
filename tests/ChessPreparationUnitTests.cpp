#include "ChessPreparationModel.h"
#include "ChessGameSessionTestHelpers.h"
#include "ChessSessionCheckpoint.h"
#include "ChessGameQueries.h"
#include "ChessMenuFormatting.h"
#include "ChessBattlePlanner.h"
#include "BattleSetupFactory.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess;
using namespace KysChess::Test;

namespace
{
ChessGameContentData capacityTestData()
{
    ChessGameContentData data;
    data.balance.initialMoney = 100;
    data.balance.minBattleSize = 4;
    data.balance.maxLevel = 9;
    data.balance.expTable = std::vector<int>(10, 5);
    data.balance.buyExpAmount = 5;
    data.balance.enemyTable = {{{1, 1}}};
    data.balance.talents.emplace(ChessTalentId::DivineArms, ChessTalentDefinition{});
    data.balance.talents.emplace(ChessTalentId::Alliance,
        ChessTalentDefinition{.selectedComboBonus = 1, .reducedThresholdMinimum = 6, .thresholdReduction = 1});
    for (auto& weights : data.balance.shopWeights) weights = {100, 0, 0, 0, 0};
    for (int id = 1; id <= 30; ++id)
    {
        ChessRoleDefinition role;
        role.ID = id;
        role.Name = std::format("容量測試{}", id);
        role.Cost = 1;
        role.MaxHP = 100;
        data.roles.emplace(id, role);
        data.poolRoleIds.push_back(id);
    }
    ComboDef combo;
    combo.id = 0;
    combo.name = "容量測試羈絆";
    combo.memberRoleIds = {1, 2, 3, 4, 5};
    combo.thresholds.push_back({.count = 5, .managementRules = {DeploymentCapacityRule{}}});
    data.combos.push_back(std::move(combo));
    data.items.emplace(500, ChessItemDefinition{.id = 500});
    data.items.emplace(501, ChessItemDefinition{.id = 501});
    data.equipment.push_back({500, 1, 0, {}, {CountsAsComboRule{"容量測試羈絆"}}});
    data.equipment.push_back({501, 2, 0});
    BalanceConfig::ChallengeDef challenge;
    challenge.name = "容量測試遠征";
    challenge.enemies.push_back({1, 1});
    data.balance.challenges.push_back(std::move(challenge));
    addCampaignTestMaps(data);
    return data;
}

ChessSessionState capacityTestState(int count)
{
    ChessSessionState state;
    state.options.talent = state.talent;
    state.level = count > 5 ? 9 : 3;
    state.money = 100;
    for (int id = 1; id <= count; ++id)
        state.roster.emplace(id, ChessSessionPiece{.instanceId = id, .roleId = id, .deployed = true});
    state.nextChessInstanceId = count + 1;
    ChessManagementRules::maintainFormation(state);
    return state;
}
}

TEST_CASE("deployment capacity uses the resulting lineup and alliance points", "[chess][preparation][capacity]")
{
    const ChessGameContent content(capacityTestData());
    auto state = capacityTestState(4);
    state.roster.emplace(5, ChessSessionPiece{.instanceId = 5, .roleId = 5});
    state.roster.emplace(6, ChessSessionPiece{.instanceId = 6, .roleId = 6});
    CHECK(ChessManagementRules::maximumDeployment(state, content) == 4);
    CHECK(ChessManagementRules::validate(state, content, chessPreparationDeployment(state, 6)) == ChessRuleErrorCode::DeploymentLimitExceeded);
    const auto fifth = chessPreparationDeployment(state, 5);
    REQUIRE(ChessManagementRules::validate(state, content, fifth) == ChessRuleErrorCode::None);
    ChessRunRandom random(1);
    std::vector<ChessSemanticEvent> events;
    ChessManagementRules::apply(state, content, random, fifth, events);
    CHECK(ChessManagementRules::maximumDeployment(state, content) == 5);
    CHECK(ChessManagementRules::deploymentReadiness(state, content) == ChessRuleErrorCode::None);
    const auto xp = buildChessBuyExpDeploymentPreview(state, content);
    REQUIRE(xp.next);
    CHECK(xp.current == 5);
    CHECK(*xp.next == 6);
    ChessManagementRules::gainExperience(state, content, 5);
    CHECK(ChessManagementRules::maximumDeployment(state, content) == *xp.next);
    ChessManagementRules::apply(state, content, random, chessPreparationDeployment(state, 5), events);
    CHECK(ChessManagementRules::maximumDeployment(state, content) == 5);
    state.talent = ChessTalentId::Alliance;
    state.selectedTalentComboId = 0;
    CHECK(evaluateChessComboProgress(state, content, content.combos()[0]).effectiveCount == 5);
    CHECK(ChessManagementRules::maximumDeployment(state, content) == 6);
}

TEST_CASE("full bench replacement preserves its slot and overflow can be saved and repaired", "[chess][preparation][capacity][save]")
{
    const auto content = std::make_shared<const ChessGameContent>(capacityTestData());
    ChessGameSession session(content, 42);
    auto checkpoint = ChessSessionCheckpoint::capture(session, 1);
    checkpoint.state = capacityTestState(11);
    checkpoint.state.difficulty = content->difficulty();
    for (int id = 20; id < 30; ++id)
        checkpoint.state.roster.emplace(id, ChessSessionPiece{.instanceId = id, .roleId = id, .star = 3});
    checkpoint.state.nextChessInstanceId = 30;
    std::swap(checkpoint.state.formationSlots[4], checkpoint.state.formationSlots[10]);
    REQUIRE(checkpoint.restore(session) == ChessCheckpointError::None);
    const auto beforeSlots = session.state().formationSlots;
    const auto replace = chessPreparationReplacement(session.state(), 20, 5);
    REQUIRE(session.submitAndDrain(replace).accepted);
    CHECK(session.observe().deployedCount == 11);
    CHECK(session.observe().deploymentOverflow == 1);
    CHECK(session.state().formationSlots[10] == 20);
    for (int i = 0; i < 10; ++i) CHECK(session.state().formationSlots[i] == beforeSlots[i]);
    CHECK(session.validateAction({ChessActionType::PrepareBattle}) == ChessRuleErrorCode::DeploymentLimitExceeded);
    ChessAction challenge;
    challenge.type = ChessActionType::StartChallenge;
    challenge.challengeName = "容量測試遠征";
    CHECK(session.validateAction(challenge) == ChessRuleErrorCode::DeploymentLimitExceeded);
    const auto saved = ChessSessionCheckpoint::capture(session, 2);
    ChessCheckpointError error;
    const auto parsed = ChessSessionCheckpoint::parseJson(saved.serializeJson(), error);
    REQUIRE(parsed);
    REQUIRE(parsed->restore(session) == ChessCheckpointError::None);
    CHECK(session.observe().deploymentOverflow == 1);
    REQUIRE(session.submitAndDrain(chessPreparationReplacement(session.state(), 5, 20)).accepted);
    CHECK(session.observe().deploymentOverflow == 0);
    REQUIRE(session.submitAndDrain({ChessActionType::PrepareBattle}).accepted);
    const auto prepared = session.state().preparedBattle;
    REQUIRE(prepared);
    REQUIRE(prepared->chosenMapId >= 0);
    CHECK(std::ranges::count(prepared->units, 0, &PreparedChessBattleUnit::team) == 11);
    CHECK(std::ranges::count(prepared->units, 1, &PreparedChessBattleUnit::team) == 1);
    const auto frozen = ChessSessionCheckpoint::capture(session, 3);
    const auto loaded = ChessSessionCheckpoint::parseJson(frozen.serializeJson(), error);
    REQUIRE(loaded);
    REQUIRE(loaded->restore(session) == ChessCheckpointError::None);
    CHECK(session.state().preparedBattle == prepared);
    CHECK(session.validateAction({ChessActionType::StartBattle}) == ChessRuleErrorCode::None);
}

TEST_CASE("purchase preview and commit share self enabling placement and inherited capacity loss", "[chess][preparation][capacity][shop]")
{
    const ChessGameContent content(capacityTestData());
    auto state = capacityTestState(4);
    state.shop = {{5, 1}};
    const auto preview = queryChessShopSlot(state, content, 0);
    REQUIRE(preview.purchaseError == ChessRuleErrorCode::None);
    REQUIRE(preview.synergies.size() == 1);
    CHECK(preview.synergies[0].afterPurchase == 5);
    std::vector<ChessSemanticEvent> events;
    int id{};
    REQUIRE(ChessManagementRules::resolvePurchase(state, content, 0, events, &id) == ChessRuleErrorCode::None);
    CHECK(state.roster.at(id).deployed);
    CHECK(ChessManagementRules::maximumDeployment(state, content) == 5);

    state = capacityTestState(11);
    state.roster.erase(5);
    state.roster.emplace(12, ChessSessionPiece{.instanceId = 12, .roleId = 12, .deployed = true});
    state.roster.at(6).weaponInstanceId = 1;
    state.equipmentInventory.emplace(1, ChessEquipmentInstance{1, 500, 6});
    state.roster.emplace(13, ChessSessionPiece{.instanceId = 13, .roleId = 6, .weaponInstanceId = 2});
    state.equipmentInventory.emplace(2, ChessEquipmentInstance{2, 501, 13});
    state.nextChessInstanceId = 14;
    state.shop = {{6, 1}};
    ChessManagementRules::maintainFormation(state);
    REQUIRE(ChessManagementRules::maximumDeployment(state, content) == 11);
    events.clear();
    REQUIRE(ChessManagementRules::resolvePurchase(state, content, 0, events, &id) == ChessRuleErrorCode::None);
    CHECK(state.roster.at(id).star == 2);
    CHECK(state.roster.at(id).deployed);
    CHECK(state.roster.at(id).weaponInstanceId == 2);
    CHECK(state.equipmentInventory.at(1).assignedChessInstanceId == -1);
    CHECK(ChessManagementRules::deploymentReadiness(state, content) == ChessRuleErrorCode::DeploymentLimitExceeded);
    for (int n = 20; n < 30; ++n)
        state.roster.emplace(n, ChessSessionPiece{.instanceId = n, .roleId = n, .star = 3});
    state.nextChessInstanceId = 30;
    state.shop = {{30, 1}};
    const auto before = state;
    events.clear();
    CHECK(ChessManagementRules::resolvePurchase(state, content, 0, events) == ChessRuleErrorCode::BenchFull);
    CHECK(state == before);
    CHECK(events.empty());
}

TEST_CASE("no fitting campaign map rejects preparation without consuming state or random", "[chess][preparation][map]")
{
    auto data = capacityTestData();
    data.battleMaps.erase(17);
    data.balance.enemyTable = {std::vector<BalanceConfig::EnemySlot>(12, {1, 1})};
    auto raw = data.battleMaps.at(6);
    raw.id = 701;
    raw.teammateX.resize(11);
    raw.teammateY.resize(11);
    data.battleMaps.emplace(raw.id, raw);
    const auto content = std::make_shared<const ChessGameContent>(std::move(data));
    ChessGameSession session(content, 70);
    auto checkpoint = ChessSessionCheckpoint::capture(session, 1);
    checkpoint.state = capacityTestState(1);
    checkpoint.state.difficulty = content->difficulty();
    std::swap(checkpoint.state.formationSlots[0], checkpoint.state.formationSlots[10]);
    REQUIRE(checkpoint.restore(session) == ChessCheckpointError::None);
    const auto before = session.state();
    const auto random = session.random().state();
    CHECK(session.submitAndDrain({ChessActionType::PrepareBattle}).error == ChessRuleErrorCode::InvalidMap);
    CHECK(session.state() == before);
    CHECK(session.random().state() == random);
    CHECK(ChessBattlePlanner::fittingMaps(session.state(), *content).empty());
    CHECK(BattleSetupFactory::requiredFormationSlots(ChessBattlePlanner::prepareRosterPreview(session.state(), *content), 0) == 11);
}

TEST_CASE("preparation keeps active synergy overflow ahead of incomplete synergies", "[chess][preparation]")
{
    ChessGameContentData data;
    ChessSessionState state;
    for (int id = 1; id <= 3; ++id)
    {
        ChessRoleDefinition role;
        role.ID = id;
        data.roles.emplace(id, role);
        state.roster.emplace(id, ChessSessionPiece{.instanceId = id, .roleId = id, .deployed = true});
    }
    for (int id = 0; id < 12; ++id)
    {
        ComboDef combo;
        combo.id = id;
        combo.memberRoleIds = id == 0 ? std::vector<int>{1, 2, 3} : std::vector<int>{1};
        combo.thresholds.push_back({.count = id == 0 ? 4 : 1});
        combo.isNpcOnly = id == 11;
        data.combos.push_back(std::move(combo));
    }
    const ChessGameContent content(std::move(data));
    const auto combos = chessPreparationCombos(state, content);
    REQUIRE(combos.size() == 11);
    for (int i = 0; i < 10; ++i)
    {
        CHECK(combos[i].definition->id == i + 1);
        CHECK(combos[i].progress.active);
    }
    CHECK(combos.back().definition->id == 0);
    CHECK_FALSE(combos.back().progress.active);
}

TEST_CASE("equipment hover preview cannot replace a pinned item", "[chess][preparation]")
{
    ChessPreparationEquipmentPopover popover;
    popover.preview(10);
    popover.scrollBy(90, 200);
    popover.preview(10);
    CHECK(popover.scroll == 90);
    popover.preview(20);
    CHECK(popover.equipmentId == 20);
    CHECK(popover.scroll == 0);
    popover.pin(20);
    popover.scrollBy(80, 200);
    popover.preview(30);
    CHECK(popover.equipmentId == 20);
    CHECK(popover.scroll == 80);
    popover.dismiss();
    CHECK_FALSE(popover.pinned);
    CHECK(popover.equipmentId == -1);
    CHECK(popover.scroll == 0);
}

TEST_CASE("equipment popover scrolling stays within readable content", "[chess][preparation]")
{
    ChessPreparationEquipmentPopover popover;
    popover.pin(11);
    popover.scrollBy(500, 180);
    CHECK(popover.scroll == 180);
    popover.scrollBy(-500, 180);
    CHECK(popover.scroll == 0);
    popover.scrollBy(150, 180);
    popover.scrollBy(0, 40);
    CHECK(popover.scroll == 40);
    popover.scrollBy(100, 0);
    CHECK(popover.scroll == 0);
}

TEST_CASE("preparation selection does not follow a removed roster index", "[chess][preparation]")
{
    ChessSessionState state;
    state.roster.emplace(4, ChessSessionPiece{.instanceId = 4, .roleId = 10});
    state.roster.emplace(9, ChessSessionPiece{.instanceId = 9, .roleId = 20});
    ChessPreparationSelection selection;
    selection.selectPiece(4);
    state.roster.erase(4);
    selection.reconcile(state);
    CHECK(selection.instanceId == -1);
    CHECK(selection.shopSlot == -1);

    state.shop = {{10, 1}, {20, 2}};
    selection.selectShop(1, 20);
    selection.reconcile(state);
    CHECK(selection.shopSlot == 1);
    state.shop[1] = {30, 3};
    selection.reconcile(state);
    CHECK(selection.shopSlot == -1);
}

TEST_CASE("preparation follows actual recursive merge events and inherited equipment", "[chess][preparation]")
{
    const auto content = configuredMapChoiceContent();
    ChessSessionState state;
    state.money = 100;
    state.shop = {{10, 1}};
    for (int id = 1; id <= 4; ++id)
        state.roster.emplace(id, ChessSessionPiece{.instanceId = id, .roleId = 10, .star = id <= 2 ? 1 : 2,
            .deployed = id == 1, .weaponInstanceId = id == 1 ? 1 : -1});
    state.equipmentInventory.emplace(1, ChessEquipmentInstance{1, 500, 1});
    state.nextChessInstanceId = 5;
    ChessManagementRules::maintainFormation(state);
    ChessAction action;
    action.type = ChessActionType::BuyShopSlot;
    action.shopSlot = 0;
    REQUIRE(ChessManagementRules::validate(state, *content, action) == ChessRuleErrorCode::None);
    ChessRunRandom random(42);
    ChessActionResult result;
    result.accepted = true;
    ChessManagementRules::apply(state, *content, random, action, result.events);
    const auto destination = chessPreparationPurchaseDestination(state, result);
    REQUIRE(destination);
    REQUIRE(state.roster.size() == 1);
    CHECK(state.roster.at(*destination).star == 3);
    CHECK(state.roster.at(*destination).deployed);
    CHECK(state.roster.at(*destination).weaponInstanceId == 1);
    CHECK(state.equipmentInventory.at(1).assignedChessInstanceId == *destination);
    CHECK(std::ranges::count(result.events, ChessSemanticEventType::ChessMerged, &ChessSemanticEvent::type) == 2);
}

TEST_CASE("purchase places directly on the board with a full bench", "[chess][preparation]")
{
    const auto content = managementContent();
    ChessSessionState state;
    state.money = 100;
    state.shop = {{10, 1}};
    for (int id = 1; id <= content->balance().benchSize; ++id)
        state.roster.emplace(id, ChessSessionPiece{.instanceId = id, .roleId = 10, .star = 3});
    ChessAction action;
    action.type = ChessActionType::BuyShopSlot;
    CHECK(ChessManagementRules::maximumDeployment(state, *content) > 0);
    CHECK(ChessManagementRules::validate(state, *content, action) == ChessRuleErrorCode::None);
    state.nextChessInstanceId = content->balance().benchSize + 1;
    std::vector<ChessSemanticEvent> events;
    int id{};
    REQUIRE(ChessManagementRules::resolvePurchase(state, *content, 0, events, &id) == ChessRuleErrorCode::None);
    CHECK(state.roster.at(id).deployed);
    CHECK(state.roster.size() == content->balance().benchSize + 1);
}

TEST_CASE("preparation rejects movement into a full destination without changing the session", "[chess][preparation][session]")
{
    const auto content = managementContent();
    ChessGameSession session(content, 42);
    auto checkpoint = ChessSessionCheckpoint::capture(session, 1);
    const int deployment = ChessManagementRules::maximumDeployment(checkpoint.state, *content);
    const int total = deployment + content->balance().benchSize;
    for (int id = 1; id <= total; ++id)
        checkpoint.state.roster.emplace(id, ChessSessionPiece{.instanceId = id, .roleId = 10, .star = 3, .deployed = id <= deployment});
    checkpoint.state.nextChessInstanceId = total + 1;
    ChessManagementRules::maintainFormation(checkpoint.state);
    REQUIRE(checkpoint.restore(session) == ChessCheckpointError::None);
    const auto before = session.state();
    const auto journalSize = session.journal().decisions().size();

    const auto withdrawal = session.submitAndDrain(chessPreparationDeployment(session.state(), 1));
    CHECK_FALSE(withdrawal.accepted);
    CHECK(withdrawal.error == ChessRuleErrorCode::BenchFull);
    CHECK(session.state() == before);
    CHECK(session.journal().decisions().size() == journalSize);
    const auto deploymentAttempt = session.submitAndDrain(chessPreparationDeployment(session.state(), total));
    CHECK_FALSE(deploymentAttempt.accepted);
    CHECK(deploymentAttempt.error == ChessRuleErrorCode::DeploymentLimitExceeded);
    CHECK(session.state() == before);
    CHECK(session.journal().decisions().size() == journalSize);

    ChessAction clear;
    clear.type = ChessActionType::SetDeployment;
    CHECK(session.submitAndDrain(clear).error == ChessRuleErrorCode::BenchFull);
}

TEST_CASE("withdrawal uses a free bench slot and deployment repairs an overflowing bench", "[chess][preparation]")
{
    const auto content = managementContent();
    ChessSessionState state;
    state.roster.emplace(1, ChessSessionPiece{.instanceId = 1, .roleId = 10, .deployed = true});
    CHECK(ChessManagementRules::validate(state, *content, chessPreparationDeployment(state, 1)) == ChessRuleErrorCode::None);
    for (int id = 2; id <= content->balance().benchSize + 2; ++id)
        state.roster.emplace(id, ChessSessionPiece{.instanceId = id, .roleId = 10, .star = 3});
    CHECK(ChessManagementRules::validate(state, *content, chessPreparationDeployment(state, 2)) == ChessRuleErrorCode::None);
}

TEST_CASE("preparation moves one piece down and back up when slots are available", "[chess][preparation][session]")
{
    const auto content = managementContent();
    ChessGameSession session(content, 42);
    auto checkpoint = ChessSessionCheckpoint::capture(session, 1);
    for (int id = 1; id <= 3; ++id)
        checkpoint.state.roster.emplace(id, ChessSessionPiece{.instanceId = id, .roleId = 10, .star = 3, .deployed = id < 3});
    checkpoint.state.nextChessInstanceId = 4;
    ChessManagementRules::maintainFormation(checkpoint.state);
    REQUIRE(checkpoint.restore(session) == ChessCheckpointError::None);
    const auto before = session.state();
    REQUIRE(session.submitAndDrain(chessPreparationDeployment(session.state(), 1)).accepted);
    CHECK_FALSE(session.state().roster.at(1).deployed);
    CHECK(session.state().roster.at(2) == before.roster.at(2));
    CHECK(session.state().roster.at(3) == before.roster.at(3));
    REQUIRE(session.submitAndDrain(chessPreparationDeployment(session.state(), 1)).accepted);
    CHECK(session.state().roster == before.roster);
    CHECK(session.state().money == before.money);
}

TEST_CASE("touch spending requires a second tap on the same action", "[chess][preparation]")
{
    ChessPreparationConfirmation confirmation;
    CHECK_FALSE(confirmation.confirm("buy:0"));
    CHECK(confirmation.confirm("buy:0"));
    CHECK_FALSE(confirmation.confirm("buy:0"));
    CHECK_FALSE(confirmation.confirm("refresh"));
    CHECK(confirmation.confirm("refresh"));
    CHECK_FALSE(confirmation.confirm("experience"));
    confirmation.reset();
    CHECK_FALSE(confirmation.confirm("experience"));
}

TEST_CASE("preparation confirmation cancels outside its exact piece action", "[chess][preparation]")
{
    ChessPreparationConfirmation confirmation;
    CHECK_FALSE(confirmation.confirm("sell:1"));
    confirmation.cancelUnless("sell:1");
    CHECK(confirmation.confirm("sell:1"));
    CHECK_FALSE(confirmation.confirm("sell:1"));
    confirmation.cancelUnless("");
    CHECK(confirmation.armedKey.empty());
    CHECK_FALSE(confirmation.confirm("sell:1"));
    confirmation.cancelUnless("sell:2");
    CHECK_FALSE(confirmation.confirm("sell:2"));
    confirmation.cancelUnless("sell:10");
    CHECK_FALSE(confirmation.confirm("sell:10"));
    CHECK(confirmation.confirm("sell:10"));
    CHECK(confirmation.armedKey.empty());
}

TEST_CASE("portrait shortcuts dispatch once and survive selection rebuilds", "[chess][preparation]")
{
    ChessPortraitClicks clicks;
    CHECK_FALSE(clicks.release("offer:0:10", 1, 1000000000, false));
    // 單擊重建選取不清除序列；第二擊只派送一次。
    CHECK(clicks.release("offer:0:10", 2, 1100000000, false));
    CHECK_FALSE(clicks.release("offer:0:20", 3, 1200000000, false));
    CHECK_FALSE(clicks.release("piece:4:deployed", 4, 1300000000, false));
    CHECK_FALSE(clicks.release("piece:4:deployed", 1, 2100000000, false));
    CHECK(clicks.release("piece:4:deployed", 2, 2200000000, false));
    CHECK_FALSE(clicks.release("piece:4", 1, 2300000000, false));
    CHECK_FALSE(clicks.release("piece:5", 2, 2400000000, false));
}

TEST_CASE("single click movement consumes rapid repeats even after pieces change slots", "[chess][preparation]")
{
    for (const bool touch : {false, true})
    {
        ChessPortraitClicks clicks;
        CHECK(clicks.release("piece:4", 1, 1000000000, touch, true));
        CHECK_FALSE(clicks.release("piece:4:deployed", 2, 1100000000, touch, true));
        CHECK_FALSE(clicks.release("piece:5", 3, 1200000000, touch, true));
        CHECK(clicks.release("piece:4:deployed", 1, 2000000000, touch, true));
        clicks.reset();
        CHECK_FALSE(clicks.release("offer:0:10", 1, 3000000000, touch));
        CHECK(clicks.release("offer:0:10", 2, 3100000000, touch));
    }
}

TEST_CASE("slow clicks and different portrait identities only inspect", "[chess][preparation]")
{
    ChessPortraitClicks clicks;
    CHECK_FALSE(clicks.release("piece:4", 1, 1000000000, false));
    CHECK_FALSE(clicks.release("piece:4", 1, 1800000000, false));
    CHECK_FALSE(clicks.release("piece:5", 2, 1900000000, false));
    clicks.reset(); // 裝備／價格等獨立控制項與輸入擁有權轉移會打斷序列。
    CHECK_FALSE(clicks.release("piece:5", 2, 2000000000, false));
    clicks.reset();
    CHECK_FALSE(clicks.release("piece:4", 0, 3000000000, true));
    CHECK(clicks.release("piece:4", 0, 3100000000, true));
    CHECK_FALSE(clicks.release("piece:4:deployed", 0, 3200000000, true));
    CHECK_FALSE(clicks.release("piece:5", 0, 3300000000, true));
}

TEST_CASE("ban portraits require two releases on the same character", "[chess][preparation]")
{
    for (const bool touch : {false, true})
    {
        ChessPortraitClicks clicks;
        CHECK_FALSE(clicks.release("role:10", 1, 1000000000, touch));
        CHECK_FALSE(clicks.release("role:11", 2, 1100000000, touch));
        CHECK(clicks.release("role:11", touch ? 0 : 2, 1200000000, touch));
        CHECK_FALSE(clicks.release("role:12", 3, 1300000000, touch));
        clicks.reset(); // 換頁、篩選或取消不能沿用上一個角色的點擊。
        CHECK_FALSE(clicks.release("role:11", 2, 1400000000, touch));
    }
}

TEST_CASE("touch preview consumes long presses but preserves quick actions", "[chess][preparation]")
{
    ChessPressPreview preview;
    CHECK_FALSE(preview.visible(1000));
    CHECK_FALSE(preview.release(1000));
    preview.start("buy:0", 1000);
    CHECK_FALSE(preview.visible(1349));
    CHECK_FALSE(preview.release(1349));
    preview.start("equipment", 2000);
    CHECK(preview.visible(2350));
    CHECK(preview.release(2350));
    CHECK_FALSE(preview.visible(2400));
    preview.start("refresh", 3000);
    // 沒有繪製說明的影格也不能讓長按誤觸付費操作。
    CHECK(preview.release(4000));
    preview.start("save", 5000);
    preview.reset();
    CHECK_FALSE(preview.visible(6000));
}

TEST_CASE("sliding away cancels touch preview and activation even on reentry", "[chess][preparation]")
{
    ChessPressPreview preview;
    preview.start("load", 1000);
    preview.move("load");
    CHECK(preview.visible(1350));
    preview.move("");
    CHECK_FALSE(preview.visible(1400));
    preview.move("load");
    CHECK_FALSE(preview.visible(1450));
    CHECK(preview.release(1450));
    preview.start("load", 2000);
    preview.move("save");
    CHECK(preview.release(2010));
    preview.start("save", 3000);
    CHECK_FALSE(preview.release(3100));
}

TEST_CASE("inspector history restores original piece and nested detail context", "[chess][preparation]")
{
    ChessPreparationHistory history;
    ChessPreparationNavigationContext piece;
    piece.view = ChessPreparationView::Piece;
    piece.selection.selectPiece(4);
    piece.focusKey = "piece:4";
    history.push(piece);
    auto gear = piece;
    gear.view = ChessPreparationView::Equipment;
    gear.equipmentId = 7;
    gear.equipmentType = 1;
    gear.listPage = 2;
    gear.textPage = 3;
    history.push(gear);
    auto catalog = gear;
    catalog.view = ChessPreparationView::Neigongs;
    catalog.listPage = 4;
    history.push(catalog);
    auto combo = catalog;
    combo.view = ChessPreparationView::Neigong;
    combo.neigongId = 11;
    history.push(combo);
    REQUIRE(history.pop()->neigongId == 11);
    const auto restoredCatalog = history.pop();
    REQUIRE(restoredCatalog);
    CHECK(restoredCatalog->view == ChessPreparationView::Neigongs);
    CHECK(restoredCatalog->listPage == 4);
    const auto restoredGear = history.pop();
    REQUIRE(restoredGear);
    CHECK(restoredGear->equipmentId == 7);
    CHECK(restoredGear->equipmentType == 1);
    CHECK(restoredGear->textPage == 3);
    CHECK(restoredGear->selection.instanceId == 4);
    const auto restoredPiece = history.pop();
    REQUIRE(restoredPiece);
    CHECK(restoredPiece->view == ChessPreparationView::Piece);
    CHECK(restoredPiece->selection.instanceId == 4);
    CHECK(restoredPiece->focusKey == "piece:4");
    CHECK_FALSE(history.pop());
}

TEST_CASE("compact formation includes pieces moved beyond the visible capacity", "[chess][preparation]")
{
    ChessSessionState state;
    state.formationSlots.assign(kChessFormationSlotCount, -1);
    state.formationSlots[0] = 4;
    state.formationSlots[8] = 9;
    state.formationSlots[9] = 12;
    for (int id : {4, 9, 12}) state.roster.emplace(id, ChessSessionPiece{.instanceId = id, .deployed = true});
    ChessPreparationRosterOrder order;
    order.reconcile(state);
    CHECK(order.deployed == std::vector<int>{4, 9, 12});
    CHECK(state.formationSlots[8] == 9);
    CHECK(state.formationSlots[9] == 12);
}

TEST_CASE("moving pieces append to the destination without reordering existing pieces", "[chess][preparation]")
{
    ChessSessionState state;
    for (int id : {1, 2, 3, 4}) state.roster.emplace(id, ChessSessionPiece{.instanceId = id, .deployed = id <= 2});
    ChessManagementRules::maintainFormation(state);
    ChessPreparationRosterOrder order;
    order.reconcile(state);
    CHECK(order.deployed == std::vector<int>{1, 2});
    CHECK(order.bench == std::vector<int>{3, 4});
    state.roster.at(1).deployed = false;
    ChessManagementRules::maintainFormation(state);
    order.reconcile(state);
    CHECK(order.deployed == std::vector<int>{2});
    CHECK(order.bench == std::vector<int>{3, 4, 1});
    state.roster.at(3).deployed = true;
    ChessManagementRules::maintainFormation(state);
    order.reconcile(state);
    CHECK(order.deployed == std::vector<int>{2, 3}); // 戰場空格填回前方，不影響整備的抵達順序。
    CHECK(order.bench == std::vector<int>{4, 1});
    order.reconcile(state); // 選取／檢視器重建不會重排。
    CHECK(order.deployed == std::vector<int>{2, 3});
    CHECK(order.bench == std::vector<int>{4, 1});
    state.roster.erase(4);
    state.roster.emplace(5, ChessSessionPiece{.instanceId = 5});
    order.reconcile(state);
    CHECK(order.bench == std::vector<int>{1, 5});
}

TEST_CASE("preparation card motion settles quickly without overshooting", "[chess][preparation]")
{
    ChessPreparationCardMotion motion{{28, 168, 166, 70}, {372, 384, 78, 60}, 1000};
    const auto start = motion.frame(1000);
    CHECK(start.x == 28);
    CHECK(start.y == 168);
    const auto middle = motion.frame(1110);
    CHECK(middle.x > start.x);
    CHECK(middle.x < 372);
    CHECK(middle.y > start.y);
    CHECK(middle.y < 384);
    CHECK(motion.active(1219));
    CHECK_FALSE(motion.active(1220));
    const auto end = motion.frame(2000);
    CHECK(end.x == 372);
    CHECK(end.y == 384);
    CHECK(end.w == 78);
    CHECK(end.h == 60);
}
