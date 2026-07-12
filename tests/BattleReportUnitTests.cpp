#include "BattleReport.h"
#include "battle/BattleLogSegments.h"
#include "BattleLogTestHelpers.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("BattleReportBuilder_RecordsDamageStatsAndOrderedEvents", "[battle][report]")
{
    auto attacker = BattleLogTest::reportUnit(101, 1, 0, 11, "段譽");
    auto defender = BattleLogTest::reportUnit(202, 2, 1, 12, "岳不群");

    BattleReportBuilder builder;
    builder.recordDamage(&attacker, &defender, 152, "六脈神劍", 221, KysChess::Battle::battleLogText("連擊增傷 +42%（3層）"), 91);
    builder.recordBattleEnd(221, 0);

    const auto& report = builder.report();
    REQUIRE(report.stats().contains(101));
    CHECK(report.stats().at(101).damageDealt == 152);
    CHECK(report.stats().at(101).damagePerSkillId.at(91) == 152);
    REQUIRE(report.stats().contains(202));
    CHECK(report.stats().at(202).damageTaken == 152);
    CHECK(report.battleEndFrame() == 221);
    CHECK(report.battleResult() == 0);

    const auto& events = report.events();
    REQUIRE(events.size() == 2);
    CHECK(events[0].type == BattleReportEventType::Damage);
    CHECK(events[0].sourceName == "段譽");
    CHECK(events[0].targetName == "岳不群");
    CHECK(events[0].value == 152);
    CHECK(events[0].skillId == 91);
    CHECK(events[0].resourceId == KysChess::Battle::BattleResourceSemanticId::HitPoints);
    CHECK(BattleLogTest::joinSegments(events[0].segments) == "連擊增傷 +42%（3層）");
    CHECK(events[1].type == BattleReportEventType::BattleEnd);
}

TEST_CASE("BattleReportBuilder_BattleEndDoesNotSuppressAlreadyOrderedLaterEvents", "[battle][report]")
{
    auto attacker = BattleLogTest::reportUnit(101, 1, 0, 11, "段譽");
    auto defender = BattleLogTest::reportUnit(202, 2, 1, 12, "岳不群");

    BattleReportBuilder builder;
    builder.recordBattleEnd(221, 0);
    builder.recordDamage(&attacker, &defender, 1, "收尾", 221);

    const auto& events = builder.report().events();
    REQUIRE(events.size() == 2);
    CHECK(events[0].type == BattleReportEventType::BattleEnd);
    CHECK(events[1].type == BattleReportEventType::Damage);
}

TEST_CASE("BattleReportBuilder_AggregatesSkillDamageByStableIdNotLocalizedName", "[battle][report][determinism]")
{
    auto attacker = BattleLogTest::reportUnit(101, 1, 0, 11, "段譽");
    auto defender = BattleLogTest::reportUnit(202, 2, 1, 12, "岳不群");
    BattleReportBuilder builder;

    builder.recordDamage(&attacker, &defender, 20, "顯示名稱甲", 10, {}, 91);
    builder.recordDamage(&attacker, &defender, 30, "顯示名稱乙", 11, {}, 91);

    const auto& bySkill = builder.report().stats().at(101).damagePerSkillId;
    REQUIRE(bySkill.size() == 1);
    CHECK(bySkill.at(91) == 50);
}

TEST_CASE("BattleReportBuilder_RecordsLifecycleAndProjectileCancelStats", "[battle][report]")
{
    auto killer = BattleLogTest::reportUnit(101, 1, 0, 11, "段譽");
    auto victim = BattleLogTest::reportUnit(202, 2, 1, 12, "岳不群");

    BattleReportBuilder builder;
    builder.recordKill(&killer, &victim, 20);
    builder.recordDeath(&victim, 20);
    builder.recordProjectileCancel(101, 77);
    builder.recordProjectileCancel(101, 23);

    const auto& report = builder.report();
    REQUIRE(report.stats().contains(101));
    CHECK(report.stats().at(101).kills == 1);
    CHECK(report.cancelDamageForUnit(101) == 100);
    CHECK(report.cancelDamageForUnit(202) == 0);

    const auto& events = report.events();
    REQUIRE(events.size() == 2);
    CHECK(events[0].type == BattleReportEventType::Kill);
    CHECK(events[1].type == BattleReportEventType::Death);
    CHECK(events[1].targetName == "岳不群");
}
