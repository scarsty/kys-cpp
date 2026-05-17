#include "BattleSceneReportPlayer.h"
#include "BattleSceneTestRuntimeFixture.h"

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("BattleSceneReportPlayer_RecordsDamageBeforeDeathAndBattleEnd", "[battle][scene_report]")
{
    auto first = BattleSceneTest::makeSetupUnit(0, 0, 0, 0, { 0, 0, 0 });
    first.realRoleId = 101;
    first.name = "段譽";
    auto second = BattleSceneTest::makeSetupUnit(1, 1, 1, 0, { 10, 0, 0 });
    second.realRoleId = 202;
    second.name = "岳不群";
    BattleSceneTest::StoreFixture fixture({
        first,
        second,
    });
    BattleReportBuilder builder;
    BattleSceneReportPlayer player;

    std::vector<KysChess::Battle::BattleLogEvent> logs = {
        { KysChess::Battle::BattleLogEventType::Damage, 221, 0, 1, 152, KysChess::Battle::BattleLogCategory::Status, KysChess::Battle::BattleLogPerspective::Targeted, {}, "六脈神劍" },
        { KysChess::Battle::BattleLogEventType::UnitDied, 221, 0, 1 },
        { KysChess::Battle::BattleLogEventType::BattleEnded, 221, -1, -1, 0 },
    };

    player.playLogs(logs, { &builder, &fixture.store });

    const auto& events = builder.report().events();
    REQUIRE(events.size() == 4);
    CHECK(events[0].type == BattleReportEventType::Damage);
    CHECK(events[0].sourceName == "段譽");
    CHECK(events[0].targetName == "岳不群");
    CHECK(events[0].value == 152);
    CHECK(events[1].type == BattleReportEventType::Kill);
    CHECK(events[1].sourceName == "段譽");
    CHECK(events[1].targetName == "岳不群");
    CHECK(events[2].type == BattleReportEventType::Death);
    CHECK(events[2].targetName == "岳不群");
    CHECK(events[3].type == BattleReportEventType::BattleEnd);
}

TEST_CASE("BattleSceneReportPlayer_RecordsProjectileCancelStats", "[battle][scene_report]")
{
    auto first = BattleSceneTest::makeSetupUnit(0, 0, 0, 0, { 0, 0, 0 });
    first.realRoleId = 101;
    first.name = "段譽";
    auto second = BattleSceneTest::makeSetupUnit(1, 1, 1, 0, { 10, 0, 0 });
    second.realRoleId = 202;
    second.name = "岳不群";
    BattleSceneTest::StoreFixture fixture({
        first,
        second,
    });
    BattleReportBuilder builder;
    BattleSceneReportPlayer player;

    std::vector<KysChess::Battle::BattleProjectileCancelDamageCommand> commands = {
        { 34, 29, 0, 1, 35, 20 },
    };

    player.playProjectileCancelDamageCommands(commands, { &builder, &fixture.store });

    CHECK(builder.report().cancelDamageForUnit(0) == 35);
    CHECK(builder.report().cancelDamageForUnit(1) == 20);
}
