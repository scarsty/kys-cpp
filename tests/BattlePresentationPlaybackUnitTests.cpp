#include "battle/BattlePresentationPlayback.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess::Battle;

TEST_CASE("BattlePresentationPlaybackPlanner_MapsEveryCommittedPresentationEvent", "[battle][presentation][unit]")
{
    BattlePresentationFrame frame;
    frame.snapshot.frame = 18;
    frame.snapshot.units.push_back({ 1, 11, "source", 0, true, 100, 100, 20, 100, 0, 0, { 1, 2, 0 }, {} });
    frame.events = {
        { BattlePresentationEventType::DamageLog, 18, 1, 2, 30, 0, -1, 0, 0, "", "skill", "detail" },
        { BattlePresentationEventType::HealLog, 18, 1, 1, 12, 0, -1, 0, 0, "heal" },
        { BattlePresentationEventType::StatusLog, 18, 1, 2, 0, 0, -1, 0, 0, "status" },
        { BattlePresentationEventType::FloatingText, 18, -1, 2, 0, 0, -1, 24, 1, "float" },
        { BattlePresentationEventType::RoleEffect, 18, -1, 2, 0, 48, 7 },
        { BattlePresentationEventType::DamageNumber, 18, -1, 2, 15, 0, -1, 30 },
        { BattlePresentationEventType::CameraFocus, 18, -1, -1, 0, 10, -1, 0, 0, "", "", "", {}, { 9, 8, 0 } },
        { BattlePresentationEventType::ProjectileMoved, 18, 1, 2, 0, 0, 10, 0, 0, "", "", "", {}, { 11, 12, 0 } },
        { BattlePresentationEventType::ProjectileHit, 18, 1, 2, 0, 0, 10 },
        { BattlePresentationEventType::ProjectileExpired, 18, 1, -1, 0, 0, 10 },
        { BattlePresentationEventType::ProjectileCancelled, 18, 1, 2, 0, 0, 10 },
        { BattlePresentationEventType::ProjectileBounced, 18, 1, 3, 0, 0, 10 },
    };

    BattlePresentationPlaybackPlanner planner;
    auto plan = planner.build(frame);

    REQUIRE(plan.snapshot.frame == 18);
    REQUIRE(plan.snapshot.units.size() == 1);
    REQUIRE(plan.commands.size() == frame.events.size());
    CHECK(plan.commands[0].type == BattlePresentationCommandType::RecordDamage);
    CHECK(plan.commands[0].amount == 30);
    CHECK(plan.commands[0].skillName == "skill");
    CHECK(plan.commands[0].detailText == "detail");
    CHECK(plan.commands[1].type == BattlePresentationCommandType::RecordHeal);
    CHECK(plan.commands[1].text == "heal");
    CHECK(plan.commands[2].type == BattlePresentationCommandType::RecordStatus);
    CHECK(plan.commands[2].text == "status");
    CHECK(plan.commands[3].type == BattlePresentationCommandType::SpawnFloatingText);
    CHECK(plan.commands[3].textSize == 24);
    CHECK(plan.commands[3].textMotionType == 1);
    CHECK(plan.commands[4].type == BattlePresentationCommandType::SpawnRoleEffect);
    CHECK(plan.commands[4].effectId == 7);
    CHECK(plan.commands[4].durationFrames == 48);
    CHECK(plan.commands[5].type == BattlePresentationCommandType::SpawnDamageNumber);
    CHECK(plan.commands[5].amount == 15);
    CHECK(plan.commands[6].type == BattlePresentationCommandType::FocusCamera);
    CHECK(plan.commands[6].position.x == 9);
    CHECK(plan.commands[7].type == BattlePresentationCommandType::MoveProjectile);
    CHECK(plan.commands[7].effectId == 10);
    CHECK(plan.commands[7].position.x == 11);
    CHECK(plan.commands[8].type == BattlePresentationCommandType::HitProjectile);
    CHECK(plan.commands[9].type == BattlePresentationCommandType::ExpireProjectile);
    CHECK(plan.commands[10].type == BattlePresentationCommandType::CancelProjectile);
    CHECK(plan.commands[11].type == BattlePresentationCommandType::BounceProjectile);
}

TEST_CASE("BattlePresentationPlaybackPlanner_PreservesCommandOrder", "[battle][presentation][unit]")
{
    BattlePresentationFrame frame;
    frame.snapshot.frame = 3;
    frame.events = {
        { BattlePresentationEventType::StatusLog, 3, -1, 1, 0, 0, -1, 0, 0, "first" },
        { BattlePresentationEventType::FloatingText, 3, -1, 1, 0, 0, -1, 24, 0, "second" },
        { BattlePresentationEventType::DamageLog, 3, 1, 2, 9 },
    };

    BattlePresentationPlaybackPlanner planner;
    auto plan = planner.build(frame);

    REQUIRE(plan.commands.size() == 3);
    CHECK(plan.commands[0].type == BattlePresentationCommandType::RecordStatus);
    CHECK(plan.commands[0].text == "first");
    CHECK(plan.commands[1].type == BattlePresentationCommandType::SpawnFloatingText);
    CHECK(plan.commands[1].text == "second");
    CHECK(plan.commands[2].type == BattlePresentationCommandType::RecordDamage);
    CHECK(plan.commands[2].amount == 9);
}
