#include "ChessPreparationModel.h"
#include "ChessGameSessionTestHelpers.h"
#include "ChessSessionCheckpoint.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess;
using namespace KysChess::Test;

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

TEST_CASE("expanded inspector recovers when refresh purchase or merge removes its target", "[chess][preparation]")
{
    ChessSessionState state;
    state.shop = {{10, 1}};
    ChessPreparationSelection selection;
    selection.selectShop(0, 10);
    auto view = ChessPreparationView::Attributes;
    reconcileChessPreparationView(selection, view, state);
    CHECK(view == ChessPreparationView::Attributes);
    state.shop[0].roleId = -1;
    reconcileChessPreparationView(selection, view, state);
    CHECK(view == ChessPreparationView::Piece);
    CHECK(selection.shopRoleId == -1);
    state.roster.emplace(4, ChessSessionPiece{.instanceId = 4, .roleId = 10});
    selection.selectPiece(4);
    view = ChessPreparationView::Ability;
    state.roster.erase(4);
    reconcileChessPreparationView(selection, view, state);
    CHECK(view == ChessPreparationView::Piece);
    CHECK(selection.instanceId == -1);
}

TEST_CASE("replacement previews use real equipment and star synergy without mutating state", "[chess][preparation]")
{
    const auto content = configuredMapChoiceContent();
    ChessSessionState state;
    state.money = 37;
    state.roster.emplace(1, ChessSessionPiece{.instanceId = 1, .roleId = 10, .star = 2, .deployed = true});
    state.roster.emplace(2, ChessSessionPiece{.instanceId = 2, .roleId = 30, .weaponInstanceId = 1});
    state.equipmentInventory.emplace(1, ChessEquipmentInstance{1, 500, 2});
    ChessManagementRules::maintainFormation(state);
    const auto original = state;
    const auto changes = chessPreparationReplacementPreview(state, *content, 2, 1);
    CHECK(state == original);
    REQUIRE(changes.size() == 1);
    CHECK(changes.front().name == "配置選圖羈絆");
    CHECK(changes.front().before.effectiveCount == 2);
    CHECK(changes.front().after.effectiveCount == 1);
    CHECK(changes.front().before.active);
    CHECK_FALSE(changes.front().after.active);
    const auto action = chessPreparationDeployment(state, 2, 1);
    CHECK(action.chessInstanceIds == std::vector<int>{2});
    CHECK(ChessManagementRules::validate(state, *content, action) == ChessRuleErrorCode::None);
    CHECK(state == original);
}

TEST_CASE("preparation purchase does not bypass bench capacity for automatic deployment", "[chess][preparation]")
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
    CHECK(ChessManagementRules::validate(state, *content, action) == ChessRuleErrorCode::BenchFull);
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
    confirmation.cancelUnless("inspector-ban:10");
    CHECK_FALSE(confirmation.confirm("inspector-ban:10"));
    CHECK(confirmation.confirm("inspector-ban:10"));
    CHECK(confirmation.armedKey.empty());
}

TEST_CASE("portrait shortcuts dispatch once and survive selection rebuilds", "[chess][preparation]")
{
    ChessPreparationPortraitClicks clicks;
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

TEST_CASE("slow clicks and different portrait identities only inspect", "[chess][preparation]")
{
    ChessPreparationPortraitClicks clicks;
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
    std::array<int, kChessFormationSlotCount> formation;
    formation.fill(-1);
    formation[0] = 4;
    formation[8] = 9;
    formation[9] = 12;
    CHECK(chessPreparationVisibleFormation(formation, 4) == std::vector<int>{4, 9, 12, -1});
    CHECK(formation[8] == 9);
    CHECK(formation[9] == 12);
}
