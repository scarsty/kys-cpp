#include "BattleLogImGuiView.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("BattleLogImGuiViewState_ShowResetsFiltersAndOpens", "[battle][log_imgui]")
{
    BattleLogWindowState state;
    state.filter.allyUnitId = 101;
    state.filter.enemyUnitId = 202;
    state.filter.category = BattleLogEntryCategory::Damage;
    state.dragging = true;

    BattleLogViewModel model;
    model.title = "本場戰鬥日誌";

    BattleLogImGuiView().show(state, model);

    CHECK(state.model.open);
    CHECK(state.model.title == "本場戰鬥日誌");
    CHECK(state.inputGuardFrames == 10);
    CHECK(state.hoverGuard);
    CHECK_FALSE(state.dragging);
    CHECK(state.filter.allyUnitId == -1);
    CHECK(state.filter.enemyUnitId == -1);
    CHECK(state.filter.category == BattleLogEntryCategory::Any);
}

TEST_CASE("BattleLogImGuiViewState_HideClosesAndClearsInteractionState", "[battle][log_imgui]")
{
    BattleLogWindowState state;
    state.model.open = true;
    state.inputGuardFrames = 5;
    state.hoverGuard = true;
    state.dragging = true;

    BattleLogImGuiView().hide(state);

    CHECK_FALSE(state.model.open);
    CHECK(state.inputGuardFrames == 0);
    CHECK_FALSE(state.hoverGuard);
    CHECK_FALSE(state.dragging);
}

TEST_CASE("BattleLogImGuiView_CategoryFilterLabelsUseTraditionalChinese", "[battle][log_imgui]")
{
    CHECK(battleLogCategoryFilterLabel(BattleLogEntryCategory::Any) == "全部類型");
    CHECK(battleLogCategoryFilterLabel(BattleLogEntryCategory::Damage) == "傷害");
    CHECK(battleLogCategoryFilterLabel(BattleLogEntryCategory::Heal) == "治療");
    CHECK(battleLogCategoryFilterLabel(BattleLogEntryCategory::Status) == "狀態");
    CHECK(battleLogCategoryFilterLabel(BattleLogEntryCategory::Cast) == "施放");
    CHECK(battleLogCategoryFilterLabel(BattleLogEntryCategory::ProjectileCancel) == "彈道取消");
    CHECK(battleLogCategoryFilterLabel(BattleLogEntryCategory::Lifecycle) == "擊倒");
    CHECK(battleLogCategoryFilterLabel(BattleLogEntryCategory::BattleEnd) == "戰鬥結束");
}

TEST_CASE("live battle log updates preserve reading position controls and never reopen a closed view", "[battle][log_imgui][live]")
{
    BattleLogWindowState state;
    BattleLogImGuiView view;
    BattleLogViewModel initial;
    initial.live = true;
    int pauseRequests{};
    view.show(state, initial, [&] { ++pauseRequests; });
    state.filter.allyUnitId = 101;
    state.filter.category = BattleLogEntryCategory::Damage;
    state.followLatest = false;
    state.inputGuardFrames = 0;
    state.dragging = true;
    const int child = state.childFlip;

    BattleLogViewModel next;
    next.live = true;
    next.totalFrames = 80;
    next.entries.push_back({.category = BattleLogEntryCategory::Damage, .sourceId = 101});
    view.update(state, next);
    CHECK(state.model.open);
    CHECK(state.model.totalFrames == 80);
    REQUIRE(state.model.entries.size() == 1);
    CHECK(state.filter.allyUnitId == 101);
    CHECK(state.filter.category == BattleLogEntryCategory::Damage);
    CHECK_FALSE(state.followLatest);
    CHECK(state.dragging);
    CHECK(state.inputGuardFrames == 0);
    CHECK(state.childFlip == child);
    state.togglePause();
    CHECK(pauseRequests == 1);

    view.hide(state);
    view.update(state, next);
    CHECK_FALSE(state.model.open);
    CHECK_FALSE(state.togglePause);
}
