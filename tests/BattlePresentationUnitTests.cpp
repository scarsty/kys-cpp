#include "battle/BattlePresentation.h"
#include "battle/BattleLogSegments.h"
#include "BattleLogTestHelpers.h"
#include "BattleStatsView.h"

#include <catch2/catch_test_macros.hpp>

#include <utility>

using namespace KysChess::Battle;

TEST_CASE("BattlePresentationRecorder_BeginsFrameAndRecordsLogEvents", "[battle][presentation][unit]")
{
    BattlePresentationRecorder recorder;
    recorder.beginFrame(42);
    recorder.recordLog({ KysChess::Battle::BattleLogEventType::Status, BattlePresentationCurrentFrame, 1, 2, 0, BattleLogCategory::Status, BattleLogPerspective::Targeted, battleLogText("status") });
    recorder.recordLog({ KysChess::Battle::BattleLogEventType::Heal, 43, 1, 1, 12, BattleLogCategory::Status, BattleLogPerspective::Targeted, battleLogText("heal") });

    const auto& frame = recorder.frame();
    REQUIRE(frame.logEvents.size() == 2);
    CHECK(frame.visualEvents.empty());
    CHECK(frame.gameplayEvents.empty());
    CHECK(frame.frame == 42);
    CHECK(frame.logEvents[0].frame == 42);
    CHECK(frame.logEvents[0].type == KysChess::Battle::BattleLogEventType::Status);
    CHECK(BattleLogTest::textOf(frame.logEvents[0]) == "status");
    CHECK(frame.logEvents[1].frame == 43);
    CHECK(frame.logEvents[1].amount == 12);
}

TEST_CASE("BattlePresentationRecorder_BeginFrameWithFrameOnlyStampsEvents", "[battle][presentation][unit]")
{
    BattlePresentationRecorder recorder;

    recorder.beginFrame(17);
    recorder.recordLog({ KysChess::Battle::BattleLogEventType::Status, BattlePresentationCurrentFrame, -1, -1, 0, BattleLogCategory::Status, BattleLogPerspective::Targeted, battleLogText("status") });
    BattleVisualEvent text;
    text.type = BattleVisualEventType::FloatingText;
    text.targetUnitId = 1;
    text.textSize = 16;
    text.textMotionType = 0;
    text.text = "text";
    recorder.recordVisual(std::move(text));
    recorder.recordGameplay({ BattleGameplayEventType::BattleEnded });

    const auto& frame = recorder.frame();
    CHECK(frame.frame == 17);
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
    recorder.beginFrame(42);

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
    recorder.beginFrame(7);
    recorder.recordGameplay({ BattleGameplayEventType::ProjectileExpired, BattlePresentationCurrentFrame, 1, 3, 0, 11 });
    BattleVisualEvent text;
    text.type = BattleVisualEventType::FloatingText;
    text.targetUnitId = 3;
    text.textSize = 24;
    text.textMotionType = 1;
    text.text = "text";
    recorder.recordVisual(std::move(text));

    auto consumed = recorder.consumeFrame();

    CHECK(consumed.frame == 7);
    REQUIRE(consumed.gameplayEvents.size() == 1);
    REQUIRE(consumed.visualEvents.size() == 1);
    CHECK(consumed.gameplayEvents[0].effectId == 11);
    CHECK(consumed.visualEvents[0].targetUnitId == 3);
    CHECK(recorder.frame().frame == 0);
    CHECK(recorder.frame().gameplayEvents.empty());
    CHECK(recorder.frame().logEvents.empty());
    CHECK(recorder.frame().visualEvents.empty());
}

TEST_CASE("BattlePresentationRecorder_PreservesExplicitFrameZero", "[battle][presentation][unit]")
{
    BattlePresentationRecorder recorder;
    recorder.beginFrame(9);
    recorder.recordGameplay({ BattleGameplayEventType::BattleEnded, 0 });
    recorder.recordLog({ KysChess::Battle::BattleLogEventType::Status, 0, -1, -1, 0, BattleLogCategory::Status, BattleLogPerspective::Targeted, battleLogText("frame-zero") });

    const auto& frame = recorder.frame();
    REQUIRE(frame.gameplayEvents.size() == 1);
    REQUIRE(frame.logEvents.size() == 1);
    CHECK(frame.gameplayEvents[0].frame == 0);
    CHECK(frame.logEvents[0].frame == 0);
}
