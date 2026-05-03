#include "battle/BattleAttackSystem.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace KysChess::Battle;

namespace
{
constexpr double SceneTileWidth = 36.0;
constexpr double SceneHitRadius = SceneTileWidth * 2.0;
constexpr double SceneBounceSpawnDistance = SceneTileWidth * 1.5;
constexpr double SceneProjectileSpeed = SceneTileWidth / 3.0;
constexpr double LegacyMinimumVectorNorm = 0.0001;
constexpr double TightTrackingHitRadius = SceneTileWidth / 8.0;

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
    state.state.attackerUnitId = attackerId;
    state.state.totalFrame = 30;
    state.state.position = { static_cast<float>(x), static_cast<float>(y), 0.0f };
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

BattleAttackSpawnRequest spawnRequest()
{
    BattleAttackSpawnRequest request;
    request.initial.attackerUnitId = 1;
    request.initial.skillId = 101;
    request.initial.operationType = 2;
    request.initial.visualEffectId = 33;
    request.initial.preferredTargetUnitId = 2;
    request.initial.position = { 10, 20, 0 };
    request.initial.velocity = { 3, 4, 0 };
    request.initial.totalFrame = 30;
    return request;
}

BattleAttackWorld attackWorld()
{
    BattleAttackWorld world;
    world.hitRadius = SceneHitRadius;
    world.minimumVectorNorm = LegacyMinimumVectorNorm;
    world.bounceSpawnDistance = SceneBounceSpawnDistance;
    world.defaultProjectileSpeed = SceneProjectileSpeed;
    return world;
}
}  // namespace

TEST_CASE("BattleAttackSystem_WorldGeometryStartsEmptyUntilSupplied", "[battle][attack][unit]")
{
    BattleAttackWorld world;

    CHECK(world.hitRadius == Catch::Approx(0.0));
    CHECK(world.minimumVectorNorm == Catch::Approx(0.0));
    CHECK(world.bounceSpawnDistance == Catch::Approx(0.0));
    CHECK(world.defaultProjectileSpeed == Catch::Approx(0.0));
}

TEST_CASE("BattleAttackSystem_DefaultAttackPayloadHasNoCastSubrequestKind", "[battle][attack][unit]")
{
    BattleAttackSpawnRequest request;
    BattleAttackInstance instance;

    CHECK(request.initial.castSubrequestKind == BattleAttackCastSubrequestKind::None);
    CHECK(instance.state.castSubrequestKind == BattleAttackCastSubrequestKind::None);
}

TEST_CASE("BattleAttackSystem_SpawnAssignsDeterministicAttackIds", "[battle][attack][unit]")
{
    auto world = attackWorld();
    world.nextAttackId = 40;

    auto first = BattleAttackSystem().spawn(world, spawnRequest());
    auto second = BattleAttackSystem().spawn(world, spawnRequest());

    REQUIRE(world.attacks.size() == 2);
    CHECK(world.attacks[0].id == 40);
    CHECK(world.attacks[1].id == 41);
    CHECK(first.attackId == 40);
    CHECK(second.attackId == 41);
    CHECK(world.nextAttackId == 42);
}

TEST_CASE("BattleAttackSystem_SpawnStoresCoreAttackPayload", "[battle][attack][unit]")
{
    auto world = attackWorld();
    BattleAttackSpawnRequest request = spawnRequest();
    request.initial.through = true;
    request.initial.track = true;
    request.initial.sharedHitGroupId = 7;
    request.initial.requirePreferredTarget = true;
    request.initial.bounceRemaining = 2;
    request.initial.bounceRange = 120;
    request.initial.bounceChancePct = 80;
    request.initial.bounceRollPct = 30;
    request.initial.executeCanHitInvincible = true;
    request.initial.ignoreProjectileCancel = true;
    request.initial.hiddenWeaponItemId = 501;
    request.initial.scriptedDamage = 33;
    request.initial.scriptedStunFrames = 12;
    request.initial.scriptedBleedStacks = 4;

    BattleAttackSystem().spawn(world, request);

    REQUIRE(world.attacks.size() == 1);
    const auto& attack = world.attacks[0];
    CHECK(attack.state.attackerUnitId == 1);
    CHECK(attack.state.skillId == 101);
    CHECK(attack.state.operationType == 2);
    CHECK(attack.state.visualEffectId == 33);
    CHECK(attack.state.preferredTargetUnitId == 2);
    CHECK(attack.state.position.x == 10.0f);
    CHECK(attack.state.position.y == 20.0f);
    CHECK(attack.state.velocity.x == 3.0f);
    CHECK(attack.state.velocity.y == 4.0f);
    CHECK(attack.state.totalFrame == 30);
    CHECK(attack.state.through);
    CHECK(attack.state.track);
    CHECK(attack.state.sharedHitGroupId == 7);
    CHECK(attack.state.requirePreferredTarget);
    CHECK(attack.state.bounceRemaining == 2);
    CHECK(attack.state.bounceRange == 120);
    CHECK(attack.state.bounceChancePct == 80);
    CHECK(attack.state.bounceRollPct == 30);
    CHECK(attack.state.executeCanHitInvincible);
    CHECK(attack.state.ignoreProjectileCancel);
    CHECK(attack.state.hiddenWeaponItemId == 501);
    CHECK(attack.state.scriptedDamage == 33);
    CHECK(attack.state.scriptedStunFrames == 12);
    CHECK(attack.state.scriptedBleedStacks == 4);
    CHECK(attack.state.castSubrequestKind == BattleAttackCastSubrequestKind::None);
}

TEST_CASE("BattleAttackSystem_SpawnStoresCastSubrequestMetadata", "[battle][attack][unit]")
{
    auto world = attackWorld();
    BattleAttackSpawnRequest request = spawnRequest();
    request.initialFrame = 6;
    request.initial.castSubrequestKind = BattleAttackCastSubrequestKind::DashHit;
    request.initial.strengthMultiplier = 2.0f;

    BattleAttackSystem().spawn(world, request);

    REQUIRE(world.attacks.size() == 1);
    CHECK(world.attacks[0].frame == 6);
    CHECK(world.attacks[0].state.castSubrequestKind == BattleAttackCastSubrequestKind::DashHit);
    CHECK(world.attacks[0].state.strengthMultiplier == Catch::Approx(2.0f));
}

TEST_CASE("BattleAttackSystem_SpawnStoresUltimateFlagFromRequest", "[battle][attack][unit]")
{
    auto world = attackWorld();
    BattleAttackSpawnRequest request = spawnRequest();
    request.initial.ultimate = true;

    BattleAttackSystem().spawn(world, request);

    REQUIRE(world.attacks.size() == 1);
    CHECK(world.attacks[0].state.ultimate);
}

TEST_CASE("BattleAttackSystem_SpawnEmitsVisualPayloadWithoutScenePointers", "[battle][attack][unit]")
{
    auto world = attackWorld();

    auto event = BattleAttackSystem().spawn(world, spawnRequest());

    CHECK(event.type == BattleAttackEventType::AttackSpawned);
    CHECK(event.attackId == 0);
    CHECK(event.sourceUnitId == 1);
    CHECK(event.unitId == 2);
    CHECK(event.skillId == 101);
    CHECK(event.operationType == 2);
    CHECK(event.visualEffectId == 33);
    CHECK(event.position.x == 10.0f);
    CHECK(event.position.y == 20.0f);
    CHECK(event.velocity.x == 3.0f);
    CHECK(event.velocity.y == 4.0f);
    CHECK(event.totalFrame == 30);
}

TEST_CASE("BattleAttackSystem_HitEventCarriesDamageRequestPayload", "[battle][attack][unit]")
{
    auto world = attackWorld();
    world.hitRadius = SceneHitRadius;
    world.units = { unit(1, 0, 0, 0), unit(2, 1, 40, 0) };
    auto projectile = attack(10, 1, 0, 0);
    projectile.state.skillId = 101;
    projectile.state.operationType = 2;
    projectile.state.hiddenWeaponItemId = 501;
    projectile.state.scriptedDamage = 33;
    projectile.state.scriptedStunFrames = 12;
    projectile.state.scriptedBleedStacks = 4;
    projectile.state.executeCanHitInvincible = true;
    world.attacks.push_back(projectile);

    auto events = BattleAttackSystem().tick(world);

    auto hit = std::find_if(events.begin(), events.end(), [](const BattleAttackEvent& event) {
        return event.type == BattleAttackEventType::Hit;
    });
    REQUIRE(hit != events.end());
    CHECK(hit->attackId == 10);
    CHECK(hit->sourceUnitId == 1);
    CHECK(hit->unitId == 2);
    CHECK(hit->skillId == 101);
    CHECK(hit->operationType == 2);
    CHECK(hit->hiddenWeaponItemId == 501);
    CHECK(hit->scriptedDamage == 33);
    CHECK(hit->scriptedStunFrames == 12);
    CHECK(hit->scriptedBleedStacks == 4);
    CHECK(hit->executeCanHitInvincible);
}

TEST_CASE("BattleAttackSystem_MeleeHitOnlyEmitsAfterHitVolumeReachesTarget", "[battle][attack][unit]")
{
    auto world = attackWorld();
    world.units = {
        unit(1, 0, 0, 0),
        unit(2, 1, SceneHitRadius + 1.0, 0),
    };
    auto melee = attack(10, 1, 0, 0);
    melee.state.operationType = 0;
    world.attacks.push_back(melee);

    auto beforeReach = BattleAttackSystem().tick(world);

    CHECK(!hasEvent(beforeReach, BattleAttackEventType::Hit, 10, 2));

    world.attacks[0].state.position = { static_cast<float>(SceneHitRadius), 0.0f, 0.0f };
    auto atReach = BattleAttackSystem().tick(world);

    CHECK(hasEvent(atReach, BattleAttackEventType::Hit, 10, 2));
}

TEST_CASE("BattleAttackSystem_RangedHitOnlyEmitsAfterProjectileReachesTarget", "[battle][attack][unit]")
{
    auto world = attackWorld();
    world.units = {
        unit(1, 0, 0, 0),
        unit(2, 1, SceneHitRadius + SceneProjectileSpeed + 1.0, 0),
    };
    auto projectile = attack(10, 1, 0, 0);
    projectile.state.operationType = 2;
    projectile.state.velocity = { static_cast<float>(SceneProjectileSpeed), 0.0f, 0.0f };
    world.attacks.push_back(projectile);

    auto beforeReach = BattleAttackSystem().tick(world);

    CHECK(!hasEvent(beforeReach, BattleAttackEventType::Hit, 10, 2));

    auto atReach = BattleAttackSystem().tick(world);

    CHECK(hasEvent(atReach, BattleAttackEventType::Hit, 10, 2));
}

TEST_CASE("BattleAttackSystem_MovesAndExpiresProjectiles", "[battle][attack][unit]")
{
    auto world = attackWorld();
    world.units = { unit(1, 0, 0, 0), unit(2, 1, 500, 0) };
    auto projectile = attack(10, 1, 0, 0);
    projectile.state.velocity = { 3, 4, 0 };
    projectile.state.totalFrame = 1;
    world.attacks.push_back(projectile);

    auto events = BattleAttackSystem().tick(world);

    REQUIRE(world.attacks.size() == 1);
    CHECK(world.attacks[0].frame == 1);
    CHECK(world.attacks[0].state.position.x == 3.0f);
    CHECK(world.attacks[0].state.position.y == 4.0f);
    CHECK(hasEvent(events, BattleAttackEventType::Moved, 10));
    CHECK(hasEvent(events, BattleAttackEventType::Expired, 10));
}

TEST_CASE("BattleAttackSystem_HitsNearestEnemyOnceAndMarksNonThroughSpent", "[battle][attack][unit]")
{
    auto world = attackWorld();
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
    auto world = attackWorld();
    world.units = { unit(1, 0, 0, 0), unit(2, 1, 30, 0), unit(3, 1, 120, 0) };
    auto projectile = attack(10, 1, 0, 0);
    projectile.state.through = true;
    world.attacks.push_back(projectile);

    auto firstEvents = BattleAttackSystem().tick(world);
    CHECK(hasEvent(firstEvents, BattleAttackEventType::Hit, 10, 2));
    CHECK_FALSE(world.attacks[0].noHurt);

    world.attacks[0].state.velocity = { 90, 0, 0 };
    auto secondEvents = BattleAttackSystem().tick(world);
    CHECK(hasEvent(secondEvents, BattleAttackEventType::Hit, 10, 3));
    CHECK(!hasEvent(secondEvents, BattleAttackEventType::Hit, 10, 2));
}

TEST_CASE("BattleAttackSystem_SharedHitGroupPreventsDuplicateHitsAcrossProjectiles", "[battle][attack][unit]")
{
    auto world = attackWorld();
    world.units = { unit(1, 0, 0, 0), unit(2, 1, 30, 0) };
    auto first = attack(10, 1, 0, 0);
    first.state.sharedHitGroupId = 7;
    first.state.through = true;
    auto second = attack(11, 1, 0, 0);
    second.state.sharedHitGroupId = 7;
    second.state.through = true;
    world.attacks = { first, second };

    auto events = BattleAttackSystem().tick(world);

    CHECK(hasEvent(events, BattleAttackEventType::Hit, 10, 2));
    CHECK(!hasEvent(events, BattleAttackEventType::Hit, 11, 2));
    REQUIRE(world.sharedHitGroupTargets[7].size() == 1);
    CHECK(world.sharedHitGroupTargets[7][0] == 2);
}

TEST_CASE("BattleAttackSystem_RequiredPreferredTargetExpiresWhenTargetInvalid", "[battle][attack][unit]")
{
    auto world = attackWorld();
    world.units = { unit(1, 0, 0, 0), unit(2, 1, 30, 0) };
    world.units[1].alive = false;
    auto projectile = attack(10, 1, 0, 0);
    projectile.state.preferredTargetUnitId = 2;
    projectile.state.requirePreferredTarget = true;
    projectile.state.totalFrame = 20;
    world.attacks.push_back(projectile);

    auto events = BattleAttackSystem().tick(world);

    CHECK(world.attacks[0].noHurt);
    CHECK(world.attacks[0].frame == 15);
    CHECK(hasEvent(events, BattleAttackEventType::TargetLost, 10));
}

TEST_CASE("BattleAttackSystem_TrackingPreservesSpeedWhileTurningTowardTarget", "[battle][attack][unit]")
{
    auto world = attackWorld();
    world.hitRadius = TightTrackingHitRadius;
    world.units = { unit(1, 0, 0, 0), unit(2, 1, 100, 100) };
    auto projectile = attack(10, 1, 0, 0);
    projectile.state.track = true;
    projectile.state.velocity = { 10, 0, 0 };
    world.attacks.push_back(projectile);

    BattleAttackSystem().tick(world);

    CHECK(world.attacks[0].state.velocity.y > 0.0f);
    CHECK(world.attacks[0].state.velocity.x > 0.0f);
}

TEST_CASE("BattleAttackSystem_ProjectileCancelEventsAreDeterministic", "[battle][attack][unit]")
{
    auto world = attackWorld();
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
    auto world = attackWorld();
    world.nextAttackId = 20;
    world.units = {
        unit(1, 0, 0, 0),
        unit(2, 1, 20, 0),
        unit(3, 1, 80, 0),
        unit(4, 1, 130, 0),
        unit(5, 1, 260, 0),
    };
    auto projectile = attack(10, 1, 0, 0);
    projectile.state.velocity = { 10, 0, 0 };
    projectile.state.bounceRemaining = 2;
    projectile.state.bounceRange = 120;
    projectile.state.bounceChancePct = 100;
    projectile.state.bounceRollPct = 0;
    projectile.hitUnitIds = { 4 };
    world.attacks.push_back(projectile);

    auto events = BattleAttackSystem().tick(world);

    REQUIRE(world.attacks.size() == 2);
    const auto& source = world.attacks[0];
    CHECK(source.noHurt);
    CHECK(source.state.bounceRemaining == 0);

    const auto& bounce = world.attacks[1];
    CHECK(bounce.id == 20);
    CHECK(bounce.state.attackerUnitId == 1);
    CHECK(bounce.state.preferredTargetUnitId == 3);
    CHECK(bounce.state.requirePreferredTarget);
    CHECK(bounce.state.track);
    CHECK_FALSE(bounce.state.through);
    CHECK(bounce.state.ignoreProjectileCancel);
    CHECK(bounce.state.bounceRemaining == 1);
    CHECK(bounce.state.position.x == Catch::Approx(74.0f));
    CHECK(bounce.state.velocity.x > 0.0f);

    REQUIRE(events.back().type == BattleAttackEventType::Bounce);
    CHECK(events.back().attackId == 10);
    CHECK(events.back().otherAttackId == 20);
    CHECK(events.back().unitId == 3);
}

TEST_CASE("BattleAttackSystem_BounceChanceMissConsumesSourceWithoutSpawning", "[battle][attack][unit]")
{
    auto world = attackWorld();
    world.units = { unit(1, 0, 0, 0), unit(2, 1, 20, 0), unit(3, 1, 80, 0) };
    auto projectile = attack(10, 1, 0, 0);
    projectile.state.bounceRemaining = 1;
    projectile.state.bounceRange = 120;
    projectile.state.bounceChancePct = 30;
    projectile.state.bounceRollPct = 30;
    world.attacks.push_back(projectile);

    auto events = BattleAttackSystem().tick(world);

    REQUIRE(world.attacks.size() == 1);
    CHECK(world.attacks[0].noHurt);
    CHECK(world.attacks[0].state.bounceRemaining == 0);
    CHECK(!hasEvent(events, BattleAttackEventType::Bounce, 10, 3));
}

TEST_CASE("BattleAttackSystem_BounceCannotTriggerWithoutHitEvent", "[battle][attack][unit]")
{
    auto world = attackWorld();
    world.units = {
        unit(1, 0, 0, 0),
        unit(2, 1, SceneHitRadius + 1.0, 0),
        unit(3, 1, SceneHitRadius + SceneBounceSpawnDistance, 0),
    };
    auto projectile = attack(10, 1, 0, 0);
    projectile.state.bounceRemaining = 1;
    projectile.state.bounceRange = static_cast<int>(SceneTileWidth * 4);
    projectile.state.bounceChancePct = 100;
    projectile.state.bounceRollPct = 0;
    world.attacks.push_back(projectile);

    auto events = BattleAttackSystem().tick(world);

    CHECK(!hasEvent(events, BattleAttackEventType::Hit, 10, 2));
    CHECK(!hasEvent(events, BattleAttackEventType::Bounce, 10, 3));
    REQUIRE(world.attacks.size() == 1);
    CHECK(world.attacks[0].state.bounceRemaining == 1);
    CHECK_FALSE(world.attacks[0].noHurt);
}
