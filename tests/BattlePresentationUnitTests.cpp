#include "battle/BattlePresentation.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess::Battle;

TEST_CASE("BattlePresentationRecorder_BeginsFrameWithSnapshotAndRecordsLogEvents", "[battle][presentation][unit]")
{
    BattlePresentationSnapshot snapshot;
    snapshot.frame = 42;
    snapshot.units.push_back({ 1, 101, "unit", 0, true, 50, 100, 20, 100, 3, 0, { 10, 20, 0 }, { 1, 0, 0 } });

    BattlePresentationRecorder recorder;
    recorder.beginFrame(snapshot);
    recorder.recordLog({ BattleLogEventType::Status, BattlePresentationCurrentFrame, 1, 2, 0, "status" });
    recorder.recordLog({ BattleLogEventType::Heal, 43, 1, 1, 12, "heal" });

    const auto& frame = recorder.frame();
    REQUIRE(frame.snapshot.units.size() == 1);
    REQUIRE(frame.logEvents.size() == 2);
    CHECK(frame.visualEvents.empty());
    CHECK(frame.gameplayEvents.empty());
    CHECK(frame.snapshot.frame == 42);
    CHECK(frame.logEvents[0].frame == 42);
    CHECK(frame.logEvents[0].type == BattleLogEventType::Status);
    CHECK(frame.logEvents[0].text == "status");
    CHECK(frame.logEvents[1].frame == 43);
    CHECK(frame.logEvents[1].amount == 12);
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
    recorder.recordLog({ BattleLogEventType::Status, 0, -1, -1, 0, "frame-zero" });

    const auto& frame = recorder.frame();
    REQUIRE(frame.gameplayEvents.size() == 1);
    REQUIRE(frame.logEvents.size() == 1);
    CHECK(frame.gameplayEvents[0].frame == 0);
    CHECK(frame.logEvents[0].frame == 0);
}
