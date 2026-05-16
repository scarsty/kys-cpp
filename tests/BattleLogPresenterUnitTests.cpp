#include "BattleLogPresenter.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string_view>

namespace
{
BattlePostBattleUnitSummary summaryUnit(int battleId, int team, const std::string& name)
{
    BattlePostBattleUnitSummary unit;
    unit.identity = { battleId, battleId, team, 10 + battleId, name };
    unit.hpRemaining = 50;
    unit.maxHpRemaining = 100;
    return unit;
}
}  // namespace

TEST_CASE("BattleLogPresenter_BuildsRowsAndFormattedEntries", "[battle][log_presenter]")
{
    BattleUnitIdentity attacker{ 101, 1, 0, 11, "段譽" };
    BattleUnitIdentity defender{ 202, 2, 1, 12, "岳不群" };
    BattleReportBuilder builder;
    builder.recordDamage(&attacker, &defender, 20, "六脈神劍", 12, "連擊增傷 +20%");
    builder.recordBattleEnd(30, 0);

    BattlePostBattleSummary summary;
    summary.battleResult = 0;
    summary.allies.push_back(summaryUnit(101, 0, "段譽"));
    summary.enemies.push_back(summaryUnit(202, 1, "岳不群"));

    auto model = BattleLogPresenter().present(summary, builder.report());

    CHECK(model.title == "本場戰鬥日誌");
    CHECK(model.resultText == "戰鬥勝利");
    CHECK(model.totalFrames == 30);
    REQUIRE(model.allies.size() == 1);
    CHECK(model.allies[0].damageDealt == 20);
    REQUIRE(model.enemies.size() == 1);
    CHECK(model.enemies[0].damageTaken == 20);
    REQUIRE(model.entries.size() == 2);
    CHECK(model.entries[0].category == BattleLogEntryCategory::Damage);
    CHECK(model.entries[0].plainText() == "[  12F] 段譽 施放 六脈神劍 命中 岳不群，造成 20 點傷害（連擊增傷 +20%）");
}

TEST_CASE("BattleLogPresenter_DisambiguatesDuplicateNames", "[battle][log_presenter]")
{
    BattleUnitIdentity first{ 201, 2, 1, 12, "弟子" };
    BattleUnitIdentity second{ 202, 3, 1, 13, "弟子" };
    BattleReportBuilder builder;
    builder.recordStatus(&first, &second, "測試狀態", 10);

    BattlePostBattleSummary summary;
    summary.battleResult = 0;
    summary.enemies.push_back(summaryUnit(201, 1, "弟子"));
    summary.enemies.push_back(summaryUnit(202, 1, "弟子"));

    auto model = BattleLogPresenter().present(summary, builder.report());

    REQUIRE(model.entries.size() == 1);
    CHECK(model.entries[0].plainText() == "[  10F] 弟子 [1] 對 弟子 [2]：測試狀態");
}

TEST_CASE("BattleLogPresenter_FiltersByParticipantAndCategory", "[battle][log_presenter]")
{
    BattleUnitIdentity attacker{ 101, 1, 0, 11, "段譽" };
    BattleUnitIdentity defender{ 202, 2, 1, 12, "岳不群" };
    BattleReportBuilder builder;
    builder.recordDamage(&attacker, &defender, 20, "六脈神劍", 12);
    builder.recordStatus(&attacker, &defender, "眩暈（12幀）", 13);

    BattlePostBattleSummary summary;
    summary.allies.push_back(summaryUnit(101, 0, "段譽"));
    summary.enemies.push_back(summaryUnit(202, 1, "岳不群"));
    auto model = BattleLogPresenter().present(summary, builder.report());

    BattleLogFilter filter;
    filter.allyUnitId = 101;
    filter.category = BattleLogEntryCategory::Damage;

    REQUIRE(countVisibleBattleLogEntries(model, filter) == 1);
    CHECK(battleLogEntryMatchesFilter(model.entries[0], filter));
    CHECK_FALSE(battleLogEntryMatchesFilter(model.entries[1], filter));
}

TEST_CASE("BattleLogPresenter_ClassifiesCastCancelAndLifecycleCategories", "[battle][log_presenter]")
{
    BattleUnitIdentity attacker{ 101, 1, 0, 11, "段譽" };
    BattleUnitIdentity defender{ 202, 2, 1, 12, "岳不群" };
    BattleReportBuilder builder;
    builder.recordStatus(&attacker, &defender, "施放六脈神劍", 10);
    builder.recordStatus(&attacker, &defender, "抵消彈道 #34 vs #29（100 - 65 = 35）", 11);
    builder.recordKill(&attacker, &defender, 12);
    builder.recordDeath(&defender, 12);

    BattlePostBattleSummary summary;
    summary.allies.push_back(summaryUnit(101, 0, "段譽"));
    summary.enemies.push_back(summaryUnit(202, 1, "岳不群"));

    auto model = BattleLogPresenter().present(summary, builder.report());

    REQUIRE(model.entries.size() == 4);
    CHECK(model.entries[0].category == BattleLogEntryCategory::Cast);
    CHECK(model.entries[1].category == BattleLogEntryCategory::ProjectileCancel);
    CHECK(model.entries[2].category == BattleLogEntryCategory::Lifecycle);
    CHECK(model.entries[3].category == BattleLogEntryCategory::Lifecycle);

    BattleLogFilter castFilter;
    castFilter.category = BattleLogEntryCategory::Cast;
    CHECK(battleLogEntryMatchesFilter(model.entries[0], castFilter));
    CHECK_FALSE(battleLogEntryMatchesFilter(model.entries[1], castFilter));

    BattleLogFilter cancelFilter;
    cancelFilter.category = BattleLogEntryCategory::ProjectileCancel;
    CHECK_FALSE(battleLogEntryMatchesFilter(model.entries[0], cancelFilter));
    CHECK(battleLogEntryMatchesFilter(model.entries[1], cancelFilter));

    BattleLogFilter lifecycleFilter;
    lifecycleFilter.category = BattleLogEntryCategory::Lifecycle;
    CHECK_FALSE(battleLogEntryMatchesFilter(model.entries[0], lifecycleFilter));
    CHECK_FALSE(battleLogEntryMatchesFilter(model.entries[1], lifecycleFilter));
    CHECK(battleLogEntryMatchesFilter(model.entries[2], lifecycleFilter));
    CHECK(battleLogEntryMatchesFilter(model.entries[3], lifecycleFilter));
}

TEST_CASE("BattleLogPresenter_ColorsFramesDurationsProjectileIdsAndFormulas", "[battle][log_presenter]")
{
    constexpr auto durationTone = static_cast<BattleLogFieldTone>(6);
    constexpr auto frameTone = static_cast<BattleLogFieldTone>(7);
    constexpr auto formulaTone = static_cast<BattleLogFieldTone>(8);
    constexpr auto projectileTone = static_cast<BattleLogFieldTone>(9);

    BattleUnitIdentity attacker{ 101, 1, 0, 11, "段譽" };
    BattleUnitIdentity defender{ 202, 2, 1, 12, "岳不群" };
    BattleReportBuilder builder;
    builder.recordStatus(&attacker, &defender, "眩暈（12幀）", 13);
    builder.recordStatus(&attacker, &defender, "抵消彈道 #34 vs #29（100 - 65 = 35）", 14);

    BattlePostBattleSummary summary;
    summary.allies.push_back(summaryUnit(101, 0, "段譽"));
    summary.enemies.push_back(summaryUnit(202, 1, "岳不群"));

    auto model = BattleLogPresenter().present(summary, builder.report());

    REQUIRE(model.entries.size() == 2);
    CHECK(model.entries[0].plainText() == "[  13F] 段譽 對 岳不群：眩暈（12幀）");
    CHECK(model.entries[1].plainText() == "[  14F] 段譽 對 岳不群：抵消彈道 #34 vs #29（100 - 65 = 35）");

    auto hasSegment = [](const BattleLogEntryView& entry, std::string_view text, BattleLogFieldTone tone)
    {
        return std::any_of(
            entry.segments.begin(),
            entry.segments.end(),
            [&](const BattleLogSegment& segment)
            {
                return segment.text == text && segment.tone == tone;
            });
    };

    CHECK(hasSegment(model.entries[0], "  13F", frameTone));
    CHECK(hasSegment(model.entries[0], "12幀", durationTone));
    CHECK(hasSegment(model.entries[1], "#34", projectileTone));
    CHECK(hasSegment(model.entries[1], "#29", projectileTone));
    CHECK(hasSegment(model.entries[1], "100 - 65 = 35", formulaTone));
}
