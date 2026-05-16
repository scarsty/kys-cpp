#include "BattleSceneReportPlayer.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>

namespace
{
BattleSceneUnit reportPlayerUnit(int unitId, int battleId, int team, std::string name)
{
    BattleSceneUnit unit;
    unit.unitId = unitId;
    unit.identity = { battleId, battleId, team, 0, std::move(name) };
    return unit;
}
}  // namespace

TEST_CASE("BattleSceneReportPlayer_RecordsDamageBeforeDeathAndBattleEnd", "[battle][scene_report]")
{
    BattleSceneUnitStore units;
    units.initialize({
        reportPlayerUnit(0, 101, 0, "段譽"),
        reportPlayerUnit(1, 202, 1, "岳不群"),
    });
    BattleReportBuilder builder;
    BattleSceneReportPlayer player;

    std::vector<KysChess::Battle::BattleLogEvent> logs = {
        { KysChess::Battle::BattleLogEventType::Damage, 221, 0, 1, 152, "", "六脈神劍" },
        { KysChess::Battle::BattleLogEventType::UnitDied, 221, 0, 1 },
        { KysChess::Battle::BattleLogEventType::BattleEnded, 221, -1, -1, 0 },
    };

    player.playLogs(logs, { &builder, &units });

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
    BattleSceneUnitStore units;
    units.initialize({
        reportPlayerUnit(0, 101, 0, "段譽"),
        reportPlayerUnit(1, 202, 1, "岳不群"),
    });
    BattleReportBuilder builder;
    BattleSceneReportPlayer player;

    std::vector<KysChess::Battle::BattleProjectileCancelDamageCommand> commands = {
        { 34, 29, 0, 1, 35, 20 },
    };

    player.playProjectileCancelDamageCommands(commands, { &builder, &units });

    CHECK(builder.report().cancelDamageForUnit(0) == 35);
    CHECK(builder.report().cancelDamageForUnit(1) == 20);
}
