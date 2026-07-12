#include "BattleReportCollector.h"
#include "BattleSetupFactory.h"
#include "BattleSummaryBuilder.h"
#include "BattlefieldData.h"
#include "ChessCombo.h"
#include "ChessGuiBattleFlow.h"
#include "ChessMagicEffectDisplay.h"
#include "HeadlessBattleRunner.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess;

namespace
{

std::shared_ptr<const ChessGameContent> parityContent()
{
    ChessGameContentData data;
    for (int id : {10, 11, 20})
    {
        ChessRoleDefinition role;
        role.ID = id;
        role.Name = std::to_string(id);
        role.Cost = 1;
        role.MaxHP = 100;
        role.Attack = 10;
        role.Defence = 5;
        data.roles.emplace(id, role);
    }
    return std::make_shared<const ChessGameContent>(std::move(data));
}

}

TEST_CASE("initial battle camera converts the integer allied grid average to world coordinates",
          "[chess][battle][gui][camera][parity]")
{
    struct Unit
    {
        int id{};
        int team{};
        Point grid;
    };

    const std::vector<Unit> units{
        {2, 0, {10, 10}},
        {7, 0, {11, 12}},
        {20, 1, {40, 40}},
    };
    const BattlefieldData terrain;
    Point convertedGrid;
    const auto center = chessGuiInitialBattleCameraCenter(
        units,
        [&terrain, &convertedGrid](int x, int y) {
            convertedGrid = {x, y};
            return terrain.worldPosition(x, y);
        });

    CHECK(center.grid.x == 10);
    CHECK(center.grid.y == 11);
    CHECK(convertedGrid.x == center.grid.x);
    CHECK(convertedGrid.y == center.grid.y);
    CHECK(center.world.x == terrain.worldPosition(10, 11).x);
    CHECK(center.world.y == terrain.worldPosition(10, 11).y);

    const auto averagedWorld = (
        terrain.worldPosition(10, 10)
        + terrain.worldPosition(11, 12)) * 0.5;
    CHECK(center.world.x != averagedWorld.x);
    CHECK(center.world.y != averagedWorld.y);
}

TEST_CASE("prebattle summary preloads after its first rendered frame before dismissal",
          "[chess][battle][gui][preview][lifecycle]")
{
    ChessGuiPreBattleFlow flow;
    int preloadCount{};
    const auto preload = [&preloadCount] { ++preloadCount; };

    CHECK(flow.loadingVisible());
    CHECK_FALSE(flow.canDismiss());
    CHECK_FALSE(flow.runPreloadAfterFirstFrame(preload));
    CHECK(preloadCount == 0);
    CHECK_FALSE(flow.shouldDismiss(ChessGuiPreBattleDismissInput::KeyboardRelease));

    flow.markFirstFramePresented();
    CHECK_FALSE(flow.shouldDismiss(ChessGuiPreBattleDismissInput::PointerRelease));

    REQUIRE(flow.runPreloadAfterFirstFrame(preload));

    CHECK(preloadCount == 1);
    CHECK_FALSE(flow.loadingVisible());
    CHECK(flow.canDismiss());
    CHECK(flow.shouldDismiss(ChessGuiPreBattleDismissInput::KeyboardRelease));
    CHECK(flow.shouldDismiss(ChessGuiPreBattleDismissInput::GamepadRelease));
    CHECK(flow.shouldDismiss(ChessGuiPreBattleDismissInput::PointerRelease));
    CHECK_FALSE(flow.shouldDismiss(ChessGuiPreBattleDismissInput::None));
    CHECK_FALSE(flow.runPreloadAfterFirstFrame(preload));
    CHECK(preloadCount == 1);
}

TEST_CASE("GUI battle captures completion after terminal presentation sets its result",
          "[chess][battle][gui][completion][lifecycle]")
{
    const auto verify = [](Battle::BattleOutcome outcome, int initialResult, int expectedResult) {
        ChessActionResult action;
        action.accepted = true;
        action.replaySequence = 42;
        std::optional<ChessActionResult> source{action};
        std::optional<ChessActionResult> destination;
        int presentationResult = initialResult;

        REQUIRE(captureChessGuiBattleCompletion(
            destination,
            source,
            outcome,
            presentationResult));

        CHECK_FALSE(source);
        REQUIRE(destination);
        CHECK(destination->accepted);
        CHECK(destination->replaySequence == 42);
        CHECK(presentationResult == expectedResult);
        CHECK_FALSE(captureChessGuiBattleCompletion(
            destination,
            source,
            outcome,
            presentationResult));
    };

    SECTION("terminal frame already set victory")
    {
        verify(Battle::BattleOutcome::PlayerVictory, 0, 0);
    }
    SECTION("terminal frame already set defeat")
    {
        verify(Battle::BattleOutcome::PlayerDefeat, 1, 1);
    }
    SECTION("timeout normalizes an unset presentation result to defeat")
    {
        verify(Battle::BattleOutcome::Timeout, -1, 1);
    }

    SECTION("aborted scene skips postbattle processing")
    {
        const std::optional<ChessActionResult> noCompletion;
        CHECK_FALSE(chessGuiBattleReadyForPostBattle(false, noCompletion));
    }

    SECTION("external exit after terminal action still skips postbattle processing")
    {
        ChessActionResult completed;
        completed.accepted = true;
        CHECK_FALSE(chessGuiBattleReadyForPostBattle(false, completed));
    }

    SECTION("completed scene enters postbattle processing")
    {
        ChessActionResult completed;
        completed.accepted = true;
        CHECK(chessGuiBattleReadyForPostBattle(true, completed));
    }
}

TEST_CASE("GUI flow maps an enclosing run-owner exit to an abort",
          "[chess][battle][gui][lifecycle][exit]")
{
    CHECK(chessGuiFlowResult(false) == ChessGuiFlowResult::Continue);
    CHECK(chessGuiFlowResult(true) == ChessGuiFlowResult::Aborted);
}

TEST_CASE("prebattle presentation facts come from the shared initialized battle setup", "[chess][battle][gui][preview][parity]")
{
    ChessGameContentData data;
    data.balance.starHPMult = 0.5;
    data.balance.starAtkMult = 0.5;
    data.balance.starDefMult = 0.5;
    data.balance.starSpdMult = 0.25;
    data.balance.starFlatHP = 10;
    data.balance.starFlatAtk = 5;
    data.balance.starFlatDef = 2;

    ChessRoleDefinition ally;
    ally.ID = 10;
    ally.HeadID = 77;
    ally.Name = "測試俠客";
    ally.Cost = 3;
    ally.MaxHP = 100;
    ally.MaxMP = 50;
    ally.Attack = 20;
    ally.Defence = 10;
    ally.Speed = 8;
    ally.MagicID[2] = 102;
    ally.MagicPower[2] = 200;
    ally.MagicID[3] = 101;
    ally.MagicPower[3] = 100;
    data.roles.emplace(ally.ID, ally);

    ChessRoleDefinition enemy;
    enemy.ID = 20;
    enemy.HeadID = 88;
    enemy.Name = "測試敵手";
    enemy.Cost = 1;
    enemy.MaxHP = 100;
    enemy.Attack = 10;
    enemy.Defence = 5;
    enemy.Speed = 5;
    data.roles.emplace(enemy.ID, enemy);

    ChessMagicDefinition normal;
    normal.ID = 101;
    normal.Name = "平招";
    data.magics.emplace(normal.ID, normal);
    ChessMagicDefinition ultimate;
    ultimate.ID = 102;
    ultimate.Name = "絕招";
    data.magics.emplace(ultimate.ID, ultimate);

    ComboDef combo;
    combo.id = 7;
    combo.name = "星級羈絆";
    combo.memberRoleIds = {10};
    combo.starSynergyBonus = true;
    combo.thresholds.push_back({2, "初階", {}});
    combo.thresholds.push_back({3, "高階", {}});
    data.combos.push_back(combo);

    const ChessGameContent content(std::move(data));
    const auto selectedMagics = chessRoleMagicsForStar(content, *content.role(10), 2);
    REQUIRE(selectedMagics.size() == 2);
    CHECK(selectedMagics[0].first->ID == 101);
    CHECK(selectedMagics[1].first->ID == 102);

    PreparedChessBattle prepared;
    prepared.chosenMapId = -1;
    prepared.battleSeed = 123;
    prepared.units.push_back({1, 1, 10, 0, 2, 501, 502});
    prepared.units.push_back({2, -1, 20, 1});
    const auto input = BattleSetupFactory::build(prepared, content, {}, 36000);
    REQUIRE(input.units.size() == 2);
    CHECK(input.units[0].headId == 77);
    CHECK(input.units[0].weaponId == 501);
    CHECK(input.units[0].armorId == 502);
    CHECK(input.units[0].normalSkill.id == 101);
    CHECK(input.units[0].ultimateSkill.id == 102);
    CHECK(input.units[0].skillNames == "平招 絕招");

    const auto resolved = Battle::resolveBattleSetupCombos(input.setup.allyRoster, input.setup);
    REQUIRE(resolved.size() == 1);
    const auto progress = chessComboProgress(content.combos().front(), resolved.front());
    CHECK(progress.physicalCount == 1);
    CHECK(progress.effectiveCount == 2);
    CHECK(progress.activeThresholdIndex == 0);
    CHECK(progress.nextThresholdIndex == 1);
    CHECK(formatChessComboProgressCount(progress) == "1+1/3 ✓");

    auto initialized = Battle::BattleRuntimeSession::createInitialized(input);
    const auto& runtime = initialized.session.requireRuntimeUnit(1);
    CHECK(runtime.headId == 77);
    CHECK(runtime.skillNames == "平招 絕招");
    CHECK(runtime.weaponId == 501);
    CHECK(runtime.armorId == 502);
    CHECK(runtime.vitals.maxHp == 160);
    CHECK(runtime.stats.attack == 35);
    CHECK(runtime.stats.defence == 17);
    CHECK(runtime.stats.speed == 10);
}

TEST_CASE("equal-power magic tie uses the same highest-ID ultimate in runtime and UI",
          "[chess][battle][gui][preview][parity][magic]")
{
    ChessGameContentData data;
    ChessRoleDefinition role;
    role.ID = 10;
    role.Cost = 1;
    role.MaxHP = 100;
    role.MagicID[0] = 101;
    role.MagicPower[0] = 200;
    role.MagicID[1] = 102;
    role.MagicPower[1] = 200;
    data.roles.emplace(role.ID, role);

    ChessMagicDefinition lowerId;
    lowerId.ID = 101;
    lowerId.Name = "較低識別碼";
    data.magics.emplace(lowerId.ID, lowerId);
    ChessMagicDefinition higherId;
    higherId.ID = 102;
    higherId.Name = "較高識別碼";
    data.magics.emplace(higherId.ID, higherId);

    const ChessGameContent content(std::move(data));
    const auto selected = chessRoleMagicsForStar(content, *content.role(10), 1);
    REQUIRE(selected.size() == 2);
    CHECK(selected.front().first->ID == 101);
    CHECK(selected.back().first->ID == 102);

    PreparedChessBattle prepared;
    prepared.chosenMapId = -1;
    prepared.units.push_back({1, 1, 10, 0, 1});
    const auto input = BattleSetupFactory::build(prepared, content, {}, 36000);
    REQUIRE(input.units.size() == 1);
    CHECK(input.units.front().normalSkill.id == 101);
    CHECK(input.units.front().ultimateSkill.id == 102);

    std::vector<const MagicSave*> magics;
    for (const auto& selection : selected)
    {
        magics.push_back(selection.first);
    }
    const auto rows = buildChessMagicEffectDisplayRows(
        magics,
        {},
        selected.back().first->ID);
    REQUIRE(rows.size() == 2);
    CHECK_FALSE(rows.front().ultimate);
    CHECK(rows.back().ultimate);
    CHECK(rows.back().magic->ID == input.units.front().ultimateSkill.id);
}

TEST_CASE("incremental GUI drain and headless runner consume identical shared input", "[chess][battle][gui][parity][determinism]")
{
    const auto content = parityContent();
    PreparedChessBattle prepared;
    prepared.chosenMapId = -1;
    prepared.battleSeed = 123;
    prepared.units.push_back({1, 1, 10, 0});
    prepared.units.push_back({2, 2, 11, 0});
    prepared.units.push_back({3, -1, 20, 1});
    prepared.formationSwaps.emplace_back(1, 2);
    const auto input = BattleSetupFactory::build(prepared, *content, {}, 1);

    const auto headless = HeadlessBattleRunner::run(input);
    auto creation = Battle::BattleRuntimeSession::createInitialized(input);
    BattleReportCollector collector;
    collector.consumeInitialization(creation.initialization, creation.session);
    std::vector<Battle::BattleDigestEvent> guiDigest;
    while (!creation.session.runtime().result.ended)
    {
        const auto frame = creation.session.runFrame();
        collector.consumeFrame(frame, creation.session);
        const auto digest = Battle::battleDigestEvents(frame);
        guiDigest.insert(guiDigest.end(), digest.begin(), digest.end());
    }
    const auto guiSummary = BattleSummaryBuilder::build(creation.session, collector.report());

    CHECK(guiDigest == headless.digestEvents);
    CHECK(guiSummary.outcome == headless.summary.outcome);
    CHECK(guiSummary.endFrame == headless.summary.endFrame);
    CHECK(guiSummary.survivors == headless.summary.survivors);
    CHECK(collector.report().battleResult() == headless.report.battleResult());
    CHECK(collector.report().battleEndFrame() == headless.report.battleEndFrame());
    CHECK(collector.report().events().size() == headless.report.events().size());
    HeadlessBattleResult gui;
    gui.digestEvents = std::move(guiDigest);
    gui.report = collector.report();
    gui.summary = guiSummary;
    gui.finalRuntime = creation.session.runtime();
    CHECK(HeadlessBattleRunner::digest(gui) == headless.digest);
}
