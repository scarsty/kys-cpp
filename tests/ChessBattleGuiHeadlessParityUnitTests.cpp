#include "BattleReportCollector.h"
#include "BattleSetupFactory.h"
#include "BattleSummaryBuilder.h"
#include "BattlefieldData.h"
#include "ChessCombo.h"
#include "ChessGuiBattleFlow.h"
#include "ChessBattleOverviewLayout.h"
#include "ChessCatalogQueries.h"
#include "HeadlessBattleRunner.h"
#include "ScenePreloader.h"

#include <catch2/catch_approx.hpp>
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

TEST_CASE("prebattle overview yields between load batches and enables input after a ready frame",
          "[chess][battle][gui][preview][lifecycle]")
{
    ChessGuiPreBattleFlow flow;
    int batches{};
    const auto load = [&] { return ++batches == 3; };
    CHECK_FALSE(flow.runLoadBatch(load));
    CHECK_FALSE(flow.interactive());
    flow.markFramePresented();
    REQUIRE(flow.runLoadBatch(load));
    CHECK_FALSE(flow.assetsLoaded());
    CHECK_FALSE(flow.interactive());
    flow.markFramePresented();
    REQUIRE(flow.runLoadBatch(load));
    CHECK_FALSE(flow.interactive());
    REQUIRE(flow.runLoadBatch(load));
    CHECK(flow.assetsLoaded());
    CHECK_FALSE(flow.interactive());
    CHECK_FALSE(flow.runLoadBatch(load));
    flow.markFramePresented();
    CHECK(flow.interactive());
    CHECK_FALSE(flow.runLoadBatch(load));
    CHECK(batches == 3);
}

TEST_CASE("prebattle overview fits 13 pieces per side without overlapping synergies or inspector",
          "[chess][battle][gui][preview][layout]")
{
    namespace Layout = ChessBattleOverviewLayout;
    for (int side = 0; side < 2; ++side)
    {
        const auto team = Layout::team(side);
        for (int row = 0; row < 13; ++row)
        {
            const auto frame = Layout::row(side, row);
            CHECK(frame.x >= team.x);
            CHECK(frame.x + frame.w <= team.x + team.w);
            CHECK(frame.y + frame.h <= Layout::synergyTop);
            if (row < 12) CHECK(frame.y + frame.h <= Layout::row(side, row + 1).y);
            for (int slot = 0; slot < 2; ++slot)
            {
                const auto equipment = Layout::equipment(side, row, slot);
                CHECK(equipment.x >= frame.x);
                CHECK(equipment.x + equipment.w <= frame.x + frame.w);
                CHECK(equipment.y + equipment.h <= frame.y + frame.h);
            }
        }
        const auto last = Layout::synergy(side, Layout::synergiesPerPage - 1);
        CHECK(last.y + last.h <= Layout::details.y);
        CHECK(last.x + last.w <= team.x + team.w);
    }
    CHECK(Layout::details.y + Layout::details.h <= Layout::proceed.y);
    CHECK(Layout::proceed.y + Layout::proceed.h <= 720);
}

TEST_CASE("prebattle loading credits cached assets ahead of pending work and skips their loaders",
          "[chess][battle][gui][preview][loading]")
{
    ScenePreloader::LoadQueue queue;
    std::vector<int> loaded;
    const auto load = [&](int id) {
        return [&, id] { loaded.push_back(id); return ScenePreloader::LoadProgress{1, 1}; };
    };
    queue.add("cached portrait", load(1), {1, 1});
    queue.add("missing equipment", load(2));
    queue.add("cached synergy", load(3), {1, 1});
    queue.add("missing animation", load(4));
    queue.add("cached audio", load(5), {1, 1});

    CHECK(loaded.empty());
    CHECK(queue.fraction() == Catch::Approx(3.0 / 5));
    CHECK(queue.currentLabel() == "missing equipment");
    queue.advance();
    CHECK(loaded == std::vector<int>{2});
    CHECK(queue.fraction() == Catch::Approx(4.0 / 5));
    CHECK(queue.currentLabel() == "missing animation");
    queue.advance();
    CHECK(loaded == std::vector<int>{2, 4});
    CHECK(queue.complete());
    CHECK(queue.fraction() == 1.0);
}

TEST_CASE("prebattle loading advances within partially cached animation groups",
          "[chess][battle][gui][preview][loading]")
{
    ScenePreloader::LoadQueue queue;
    std::size_t frame = 2;
    queue.add("animation", [&] { return ScenePreloader::LoadProgress{++frame, 4}; }, {frame, 4});
    queue.add("cached portrait", [] { FAIL("Cached portrait must not be loaded again"); return ScenePreloader::LoadProgress{}; }, {1, 1});

    CHECK(queue.fraction() == Catch::Approx(0.75));
    queue.advance();
    CHECK_FALSE(queue.complete());
    CHECK(queue.fraction() == Catch::Approx(0.875));
    queue.advance();
    CHECK(queue.complete());
    CHECK(queue.fraction() == 1.0);
}

TEST_CASE("prebattle loading discovers uncached groups incrementally without reducing progress",
          "[chess][battle][gui][preview][loading]")
{
    ScenePreloader::LoadQueue queue;
    std::size_t calls{};
    queue.add("cached icon", [] { return ScenePreloader::LoadProgress{1, 1}; }, {1, 1});
    queue.add("undiscovered group", [&] { return ScenePreloader::LoadProgress{calls++, 3}; });
    queue.add("empty group", [] { return ScenePreloader::LoadProgress{0, 0}; });

    CHECK(queue.fraction() == Catch::Approx(1.0 / 3));
    queue.advance();
    CHECK(queue.fraction() == Catch::Approx(1.0 / 3));
    for (int frame = 1; frame <= 3; ++frame)
    {
        queue.advance();
        CHECK(queue.fraction() == Catch::Approx((1.0 + frame / 3.0) / 3));
    }
    CHECK(queue.currentLabel() == "empty group");
    queue.advance();
    CHECK(queue.complete());
    CHECK(queue.fraction() == 1.0);
}

TEST_CASE("an entirely cached preload still waits for the ready frame before accepting input",
          "[chess][battle][gui][preview][loading][lifecycle]")
{
    ScenePreloader::LoadQueue queue;
    CHECK(queue.complete());
    CHECK(queue.fraction() == 1.0);
    queue.add("cached resources", [] { FAIL("Cached resources must not be loaded again"); return ScenePreloader::LoadProgress{}; }, {1, 1});
    CHECK(queue.complete());

    ChessGuiPreBattleFlow flow;
    const auto load = [&] { return queue.complete(); };
    CHECK_FALSE(flow.runLoadBatch(load));
    flow.markFramePresented();
    REQUIRE(flow.runLoadBatch(load));
    CHECK(flow.assetsLoaded());
    CHECK_FALSE(flow.interactive());
    flow.markFramePresented();
    CHECK(flow.interactive());
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
    prepared.kind = PreparedChessBattleKind::Standalone;
    prepared.chosenMapId = -1;
    prepared.battleSeed = 123;
    prepared.units.push_back({1, 1, 10, 0, 2, 501, 502});
    prepared.units.push_back({2, -1, 20, 1});
    const auto input = BattleSetupFactory::build(prepared, content, kChessBattleFrameLimit);
    REQUIRE(input.units.size() == 2);
    CHECK(input.units[0].headId == 77);
    CHECK(input.units[0].weaponId == 501);
    CHECK(input.units[0].armorId == 502);
    REQUIRE(input.units[0].actionPlan);
    CHECK(input.units[0].actionPlan->normalSkill.id == 101);
    CHECK(input.units[0].actionPlan->ultimateSkill.id == 102);
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
    prepared.kind = PreparedChessBattleKind::Standalone;
    prepared.chosenMapId = -1;
    prepared.units.push_back({1, 1, 10, 0, 1});
    const auto input = BattleSetupFactory::build(prepared, content, kChessBattleFrameLimit);
    REQUIRE(input.units.size() == 1);
    REQUIRE(input.units.front().actionPlan);
    CHECK(input.units.front().actionPlan->normalSkill.id == 101);
    CHECK(input.units.front().actionPlan->ultimateSkill.id == 102);

    const auto abilities = chessAbilitiesForRoleStar(content, *content.role(10), 1);
    REQUIRE(abilities.size() == 2);
    CHECK(abilities.front().magicId == input.units.front().actionPlan->normalSkill.id);
    CHECK(abilities.back().magicId == input.units.front().actionPlan->ultimateSkill.id);
}

TEST_CASE("incremental GUI drain and headless runner consume identical shared input", "[chess][battle][gui][parity][determinism]")
{
    const auto content = parityContent();
    PreparedChessBattle prepared;
    prepared.kind = PreparedChessBattleKind::Standalone;
    prepared.chosenMapId = -1;
    prepared.battleSeed = 123;
    prepared.units.push_back({1, 1, 10, 0});
    prepared.units.push_back({2, 2, 11, 0});
    prepared.units.push_back({3, -1, 20, 1});
    prepared.formationSwaps.emplace_back(1, 2);
    const auto input = BattleSetupFactory::build(prepared, *content, 1);

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
