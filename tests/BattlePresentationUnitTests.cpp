#include "battle/BattlePresentation.h"
#include "BattleStatsView.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess::Battle;

TEST_CASE("BattlePresentationRecorder_BeginsFrameWithSnapshotAndRecordsLogEvents", "[battle][presentation][unit]")
{
    BattlePresentationSnapshot snapshot;
    snapshot.frame = 42;
    snapshot.units.push_back({ 1, 101, "unit", 0, true, 50, 100, 20, 100, 3, 0, { 10, 20, 0 }, { 1, 0, 0 } });

    BattlePresentationRecorder recorder;
    recorder.beginFrame(snapshot);
    recorder.recordLog({ KysChess::Battle::BattleLogEventType::Status, BattlePresentationCurrentFrame, 1, 2, 0, "status" });
    recorder.recordLog({ KysChess::Battle::BattleLogEventType::Heal, 43, 1, 1, 12, "heal" });

    const auto& frame = recorder.frame();
    REQUIRE(frame.snapshot.units.size() == 1);
    REQUIRE(frame.logEvents.size() == 2);
    CHECK(frame.visualEvents.empty());
    CHECK(frame.gameplayEvents.empty());
    CHECK(frame.snapshot.frame == 42);
    CHECK(frame.logEvents[0].frame == 42);
    CHECK(frame.logEvents[0].type == KysChess::Battle::BattleLogEventType::Status);
    CHECK(frame.logEvents[0].text == "status");
    CHECK(frame.logEvents[1].frame == 43);
    CHECK(frame.logEvents[1].amount == 12);
}

TEST_CASE("BattlePresentationRecorder_BeginFrameWithFrameOnlyStampsEvents", "[battle][presentation][unit]")
{
    BattlePresentationRecorder recorder;

    recorder.beginFrame(17);
    recorder.recordLog({ KysChess::Battle::BattleLogEventType::Status, BattlePresentationCurrentFrame, -1, -1, 0, "status" });
    recorder.recordVisual({ BattleVisualEventType::FloatingText, BattlePresentationCurrentFrame, -1, 1, 0, 0, -1, 16, 0, "text" });
    recorder.recordGameplay({ BattleGameplayEventType::BattleEnded });

    const auto& frame = recorder.frame();
    CHECK(frame.snapshot.frame == 17);
    CHECK(frame.snapshot.units.empty());
    REQUIRE(frame.logEvents.size() == 1);
    CHECK(frame.logEvents[0].frame == 17);
    REQUIRE(frame.visualEvents.size() == 1);
    CHECK(frame.visualEvents[0].frame == 17);
    REQUIRE(frame.gameplayEvents.size() == 1);
    CHECK(frame.gameplayEvents[0].frame == 17);
}

TEST_CASE("BattlePresentationRecorder_StampsAndPreservesGameplayEventOrder", "[battle][presentation][unit]")
{
    BattlePresentationRecorder recorder;
    recorder.beginFrame({ 42, {} });

    recorder.recordGameplay({ BattleGameplayEventType::CastStarted, BattlePresentationCurrentFrame, 1, 2, 0, -1 });
    recorder.recordGameplay({ BattleGameplayEventType::DamageApplied, 43, 1, 2, 12, -1 });
    recorder.recordGameplay({ BattleGameplayEventType::UnitDied, BattlePresentationCurrentFrame, -1, 2, 0, -1 });
    recorder.recordGameplay({ BattleGameplayEventType::ProjectileCancelled, BattlePresentationCurrentFrame, 1, -1, 0, 10, {}, "", 20 });

    const auto& frame = recorder.frame();
    REQUIRE(frame.gameplayEvents.size() == 4);
    CHECK(frame.logEvents.empty());
    CHECK(frame.visualEvents.empty());
    CHECK(frame.gameplayEvents[0].frame == 42);
    CHECK(frame.gameplayEvents[0].type == BattleGameplayEventType::CastStarted);
    CHECK(frame.gameplayEvents[1].frame == 43);
    CHECK(frame.gameplayEvents[1].type == BattleGameplayEventType::DamageApplied);
    CHECK(frame.gameplayEvents[1].amount == 12);
    CHECK(frame.gameplayEvents[2].frame == 42);
    CHECK(frame.gameplayEvents[2].type == BattleGameplayEventType::UnitDied);
    CHECK(frame.gameplayEvents[2].targetUnitId == 2);
    CHECK(frame.gameplayEvents[3].type == BattleGameplayEventType::ProjectileCancelled);
    CHECK(frame.gameplayEvents[3].effectId == 10);
    CHECK(frame.gameplayEvents[3].otherAttackId == 20);
}

TEST_CASE("BattlePresentationRecorder_ConsumeFrameMovesCurrentFrameAndResets", "[battle][presentation][unit]")
{
    BattlePresentationRecorder recorder;
    recorder.beginFrame({ 7, {} });
    recorder.recordGameplay({ BattleGameplayEventType::ProjectileExpired, BattlePresentationCurrentFrame, 1, 3, 0, 11 });
    recorder.recordVisual({ BattleVisualEventType::FloatingText, BattlePresentationCurrentFrame, -1, 3, 0, 0, -1, 24, 1, "text" });

    auto consumed = recorder.consumeFrame();

    CHECK(consumed.snapshot.frame == 7);
    REQUIRE(consumed.gameplayEvents.size() == 1);
    REQUIRE(consumed.visualEvents.size() == 1);
    CHECK(consumed.gameplayEvents[0].effectId == 11);
    CHECK(consumed.visualEvents[0].targetUnitId == 3);
    CHECK(recorder.frame().snapshot.frame == 0);
    CHECK(recorder.frame().gameplayEvents.empty());
    CHECK(recorder.frame().logEvents.empty());
    CHECK(recorder.frame().visualEvents.empty());
}

TEST_CASE("BattlePresentationRecorder_PreservesExplicitFrameZero", "[battle][presentation][unit]")
{
    BattlePresentationRecorder recorder;
    recorder.beginFrame({ 9, {} });
    recorder.recordGameplay({ BattleGameplayEventType::BattleEnded, 0 });
    recorder.recordLog({ KysChess::Battle::BattleLogEventType::Status, 0, -1, -1, 0, "frame-zero" });

    const auto& frame = recorder.frame();
    REQUIRE(frame.gameplayEvents.size() == 1);
    REQUIRE(frame.logEvents.size() == 1);
    CHECK(frame.gameplayEvents[0].frame == 0);
    CHECK(frame.logEvents[0].frame == 0);
}

TEST_CASE("BattleTracker_RecordsDamageFromRoleFreeIdentities", "[battle][presentation][unit]")
{
    BattleUnitIdentity attacker{ 101, 1, 0, 11, "令狐沖" };
    BattleUnitIdentity defender{ 202, 2, 1, 12, "任我行" };

    BattleTracker tracker;
    tracker.recordDamage(&attacker, &defender, 33, "獨孤九劍", 12, "破招");

    const auto& stats = tracker.getStats();
    REQUIRE(stats.contains(101));
    CHECK(stats.at(101).damageDealt == 33);
    CHECK(stats.at(101).damagePerSkill.at("獨孤九劍") == 33);
    REQUIRE(stats.contains(202));
    CHECK(stats.at(202).damageTaken == 33);

    const auto& events = tracker.getEvents();
    REQUIRE(events.size() == 1);
    CHECK(events[0].sourceId == 101);
    CHECK(events[0].sourceTeam == 0);
    CHECK(events[0].sourceName == "令狐沖");
    CHECK(events[0].targetId == 202);
    CHECK(events[0].targetTeam == 1);
    CHECK(events[0].targetName == "任我行");
    CHECK(events[0].skillName == "獨孤九劍");
    CHECK(events[0].detailText == "破招");
}

TEST_CASE("BattleTracker_RecordsLifecycleAndProjectileCancelFromIds", "[battle][presentation][unit]")
{
    BattleUnitIdentity killer{ 101, 1, 0, 11, "令狐沖" };
    BattleUnitIdentity victim{ 202, 2, 1, 12, "任我行" };

    BattleTracker tracker;
    tracker.recordKill(&killer, &victim, 20);
    tracker.recordDeath(&victim, 20);
    tracker.recordProjectileCancel(101, 77);
    tracker.recordProjectileCancel(101, 23);

    const auto& stats = tracker.getStats();
    REQUIRE(stats.contains(101));
    CHECK(stats.at(101).kills == 1);
    REQUIRE(stats.contains(202));
    CHECK(stats.at(202).lastActiveFrame == 20);
    CHECK(tracker.cancelDamageForUnit(101) == 100);
    CHECK(tracker.cancelDamageForUnit(202) == 0);

    const auto& events = tracker.getEvents();
    REQUIRE(events.size() == 2);
    CHECK(events[0].type == ::BattleLogEventType::Kill);
    CHECK(events[0].sourceId == 101);
    CHECK(events[0].targetId == 202);
    CHECK(events[1].type == ::BattleLogEventType::Death);
    CHECK(events[1].targetId == 202);
    CHECK(events[1].targetName == "任我行");
}
