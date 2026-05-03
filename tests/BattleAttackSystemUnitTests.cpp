#include "battle/BattleAttackSystem.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess::Battle;

namespace
{
BattleAttackUnit unit(int id, int team, double x, double y)
{
    BattleAttackUnit state;
    state.id = id;
    state.team = team;
    state.position = { static_cast<float>(x), static_cast<float>(y), 0.0f };
    return state;
}

BattleAttackInstance attack(int id, int attackerId, double x, double y)
{
    BattleAttackInstance state;
    state.id = id;
    state.attackerUnitId = attackerId;
    state.totalFrame = 30;
    state.position = { static_cast<float>(x), static_cast<float>(y), 0.0f };
    return state;
}

bool hasEvent(const std::vector<BattleAttackEvent>& events, BattleAttackEventType type, int attackId, int unitId = -1)
{
    for (const auto& event : events)
    {
        if (event.type == type && event.attackId == attackId && event.unitId == unitId)
        {
            return true;
        }
    }
    return false;
}
}  // namespace

TEST_CASE("BattleAttackSystem_MovesAndExpiresProjectiles", "[battle][attack][unit]")
{
    BattleAttackWorld world;
    world.units = { unit(1, 0, 0, 0), unit(2, 1, 500, 0) };
    auto projectile = attack(10, 1, 0, 0);
    projectile.velocity = { 3, 4, 0 };
    projectile.totalFrame = 1;
    world.attacks.push_back(projectile);

    auto events = BattleAttackSystem().tick(world);

    REQUIRE(world.attacks.size() == 1);
    CHECK(world.attacks[0].frame == 1);
    CHECK(world.attacks[0].position.x == 3.0f);
    CHECK(world.attacks[0].position.y == 4.0f);
    CHECK(hasEvent(events, BattleAttackEventType::Moved, 10));
    CHECK(hasEvent(events, BattleAttackEventType::Expired, 10));
}

TEST_CASE("BattleAttackSystem_HitsNearestEnemyOnceAndMarksNonThroughSpent", "[battle][attack][unit]")
{
    BattleAttackWorld world;
    world.hitRadius = 50.0;
    world.units = { unit(1, 0, 0, 0), unit(2, 1, 40, 0), unit(3, 1, 45, 0) };
    world.attacks.push_back(attack(10, 1, 0, 0));

    auto events = BattleAttackSystem().tick(world);

    REQUIRE(world.attacks[0].hitUnitIds.size() == 1);
    CHECK(world.attacks[0].hitUnitIds[0] == 2);
    CHECK(world.attacks[0].noHurt);
    CHECK(hasEvent(events, BattleAttackEventType::Hit, 10, 2));

    auto secondEvents = BattleAttackSystem().tick(world);
    CHECK(!hasEvent(secondEvents, BattleAttackEventType::Hit, 10, 2));
}

TEST_CASE("BattleAttackSystem_ThroughProjectileCanHitDifferentEnemiesButNotSameTargetTwice", "[battle][attack][unit]")
{
    BattleAttackWorld world;
    world.hitRadius = 50.0;
    world.units = { unit(1, 0, 0, 0), unit(2, 1, 30, 0), unit(3, 1, 120, 0) };
    auto projectile = attack(10, 1, 0, 0);
    projectile.through = true;
    world.attacks.push_back(projectile);

    auto firstEvents = BattleAttackSystem().tick(world);
    CHECK(hasEvent(firstEvents, BattleAttackEventType::Hit, 10, 2));
    CHECK_FALSE(world.attacks[0].noHurt);

    world.attacks[0].velocity = { 90, 0, 0 };
    auto secondEvents = BattleAttackSystem().tick(world);
    CHECK(hasEvent(secondEvents, BattleAttackEventType::Hit, 10, 3));
    CHECK(!hasEvent(secondEvents, BattleAttackEventType::Hit, 10, 2));
}

TEST_CASE("BattleAttackSystem_SharedHitGroupPreventsDuplicateHitsAcrossProjectiles", "[battle][attack][unit]")
{
    BattleAttackWorld world;
    world.hitRadius = 50.0;
    world.units = { unit(1, 0, 0, 0), unit(2, 1, 30, 0) };
    auto first = attack(10, 1, 0, 0);
    first.sharedHitGroupId = 7;
    first.through = true;
    auto second = attack(11, 1, 0, 0);
    second.sharedHitGroupId = 7;
    second.through = true;
    world.attacks = { first, second };

    auto events = BattleAttackSystem().tick(world);

    CHECK(hasEvent(events, BattleAttackEventType::Hit, 10, 2));
    CHECK(!hasEvent(events, BattleAttackEventType::Hit, 11, 2));
    REQUIRE(world.sharedHitGroupTargets[7].size() == 1);
    CHECK(world.sharedHitGroupTargets[7][0] == 2);
}

TEST_CASE("BattleAttackSystem_RequiredPreferredTargetExpiresWhenTargetInvalid", "[battle][attack][unit]")
{
    BattleAttackWorld world;
    world.units = { unit(1, 0, 0, 0), unit(2, 1, 30, 0) };
    world.units[1].alive = false;
    auto projectile = attack(10, 1, 0, 0);
    projectile.preferredTargetUnitId = 2;
    projectile.requirePreferredTarget = true;
    projectile.totalFrame = 20;
    world.attacks.push_back(projectile);

    auto events = BattleAttackSystem().tick(world);

    CHECK(world.attacks[0].noHurt);
    CHECK(world.attacks[0].frame == 15);
    CHECK(hasEvent(events, BattleAttackEventType::TargetLost, 10));
}

TEST_CASE("BattleAttackSystem_TrackingPreservesSpeedWhileTurningTowardTarget", "[battle][attack][unit]")
{
    BattleAttackWorld world;
    world.hitRadius = 5.0;
    world.units = { unit(1, 0, 0, 0), unit(2, 1, 100, 100) };
    auto projectile = attack(10, 1, 0, 0);
    projectile.track = true;
    projectile.velocity = { 10, 0, 0 };
    world.attacks.push_back(projectile);

    BattleAttackSystem().tick(world);

    CHECK(world.attacks[0].velocity.y > 0.0f);
    CHECK(world.attacks[0].velocity.x > 0.0f);
}

TEST_CASE("BattleAttackSystem_ProjectileCancelEventsAreDeterministic", "[battle][attack][unit]")
{
    BattleAttackWorld world;
    world.hitRadius = 50.0;
    world.projectileGraceFrames = 5;
    world.units = { unit(1, 0, -1000, 0), unit(2, 1, 1000, 0) };
    auto lhs = attack(10, 1, 0, 0);
    lhs.frame = 5;
    auto rhs = attack(11, 2, 20, 0);
    rhs.frame = 5;
    world.attacks = { lhs, rhs };

    auto events = BattleAttackSystem().tick(world);

    REQUIRE(events.back().type == BattleAttackEventType::ProjectileCancel);
    CHECK(events.back().attackId == 10);
    CHECK(events.back().otherAttackId == 11);
}

TEST_CASE("BattleAttackSystem_BounceSpawnsTrackingProjectileAtNearestEligibleTarget", "[battle][attack][unit]")
{
    BattleAttackWorld world;
    world.hitRadius = 50.0;
    world.nextAttackId = 20;
    world.bounceSpawnDistance = 30.0;
    world.units = {
        unit(1, 0, 0, 0),
        unit(2, 1, 20, 0),
        unit(3, 1, 80, 0),
        unit(4, 1, 130, 0),
        unit(5, 1, 260, 0),
    };
    auto projectile = attack(10, 1, 0, 0);
    projectile.velocity = { 10, 0, 0 };
    projectile.bounceRemaining = 2;
    projectile.bounceRange = 120;
    projectile.bounceChancePct = 100;
    projectile.bounceRollPct = 0;
    projectile.hitUnitIds = { 4 };
    world.attacks.push_back(projectile);

    auto events = BattleAttackSystem().tick(world);

    REQUIRE(world.attacks.size() == 2);
    const auto& source = world.attacks[0];
    CHECK(source.noHurt);
    CHECK(source.bounceRemaining == 0);

    const auto& bounce = world.attacks[1];
    CHECK(bounce.id == 20);
    CHECK(bounce.attackerUnitId == 1);
    CHECK(bounce.preferredTargetUnitId == 3);
    CHECK(bounce.requirePreferredTarget);
    CHECK(bounce.track);
    CHECK_FALSE(bounce.through);
    CHECK(bounce.ignoreProjectileCancel);
    CHECK(bounce.bounceRemaining == 1);
    CHECK(bounce.position.x == 50.0f);
    CHECK(bounce.velocity.x > 0.0f);

    REQUIRE(events.back().type == BattleAttackEventType::Bounce);
    CHECK(events.back().attackId == 10);
    CHECK(events.back().otherAttackId == 20);
    CHECK(events.back().unitId == 3);
}

TEST_CASE("BattleAttackSystem_BounceChanceMissConsumesSourceWithoutSpawning", "[battle][attack][unit]")
{
    BattleAttackWorld world;
    world.hitRadius = 50.0;
    world.units = { unit(1, 0, 0, 0), unit(2, 1, 20, 0), unit(3, 1, 80, 0) };
    auto projectile = attack(10, 1, 0, 0);
    projectile.bounceRemaining = 1;
    projectile.bounceRange = 120;
    projectile.bounceChancePct = 30;
    projectile.bounceRollPct = 30;
    world.attacks.push_back(projectile);

    auto events = BattleAttackSystem().tick(world);

    REQUIRE(world.attacks.size() == 1);
    CHECK(world.attacks[0].noHurt);
    CHECK(world.attacks[0].bounceRemaining == 0);
    CHECK(!hasEvent(events, BattleAttackEventType::Bounce, 10, 3));
}
