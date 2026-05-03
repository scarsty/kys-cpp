#include "battle/BattlePresentation.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess::Battle;

TEST_CASE("BattlePresentationRecorder_BeginsFrameWithSnapshotAndRecordsEvents", "[battle][presentation][unit]")
{
    BattlePresentationSnapshot snapshot;
    snapshot.frame = 42;
    snapshot.units.push_back({ 1, 101, "unit", 0, true, 50, 100, 20, 100, 3, 0, { 10, 20, 0 }, { 1, 0, 0 } });

    BattlePresentationRecorder recorder;
    recorder.beginFrame(snapshot);
    recorder.record({ BattlePresentationEventType::StatusLog, BattlePresentationCurrentFrame, 1, 2, 0, 0, -1, 0, 0, "status" });
    recorder.record({ BattlePresentationEventType::HealLog, 43, 1, 1, 12, 0, -1, 0, 0, "heal" });

    const auto& frame = recorder.frame();
    REQUIRE(frame.snapshot.units.size() == 1);
    REQUIRE(frame.events.size() == 2);
    CHECK(frame.snapshot.frame == 42);
    CHECK(frame.events[0].frame == 42);
    CHECK(frame.events[0].type == BattlePresentationEventType::StatusLog);
    CHECK(frame.events[0].text == "status");
    CHECK(frame.events[1].frame == 43);
    CHECK(frame.events[1].amount == 12);
}

TEST_CASE("BattlePresentationRecorder_ConsumeFrameMovesCurrentFrameAndResets", "[battle][presentation][unit]")
{
    BattlePresentationRecorder recorder;
    recorder.beginFrame({ 7, {} });
    recorder.record({ BattlePresentationEventType::FloatingText, BattlePresentationCurrentFrame, -1, 3, 0, 0, -1, 24, 1, "text" });

    auto consumed = recorder.consumeFrame();

    CHECK(consumed.snapshot.frame == 7);
    REQUIRE(consumed.events.size() == 1);
    CHECK(consumed.events[0].targetUnitId == 3);
    CHECK(recorder.frame().snapshot.frame == 0);
    CHECK(recorder.frame().events.empty());
}

TEST_CASE("BattlePresentationRecorder_PreservesExplicitFrameZero", "[battle][presentation][unit]")
{
    BattlePresentationRecorder recorder;
    recorder.beginFrame({ 9, {} });
    recorder.record({ BattlePresentationEventType::StatusLog, 0, -1, -1, 0, 0, -1, 0, 0, "frame-zero" });

    const auto& frame = recorder.frame();
    REQUIRE(frame.events.size() == 1);
    CHECK(frame.events[0].frame == 0);
}
