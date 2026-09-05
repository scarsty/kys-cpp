#include "battle/BattleAttackSystem.h"
#include "battle/BattleCore.h"
#include "BattleRuntimeRecordTestHelpers.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cassert>
#include <initializer_list>
#include <iterator>
#include <span>
#include <utility>

using namespace KysChess::Battle;

namespace
{
constexpr double SceneTileWidth = 36.0;
constexpr double SceneHitRadius = SceneTileWidth * 2.0;
constexpr double SceneBounceSpawnDistance = SceneTileWidth * 1.5;
constexpr double SceneProjectileSpeed = SceneTileWidth / 3.0;
constexpr double TestMinimumVectorNorm = 0.0001;
constexpr double TightTrackingHitRadius = SceneTileWidth / 8.0;

BattleRuntimeUnit unit(int id, int team, double x, double y)
{
    BattleRuntimeUnit state;
    state.id = id;
    state.team = team;
    state.motion.position = { static_cast<float>(x), static_cast<float>(y), 0.0f };
    return state;
}

BattleRuntimeUnits runtimeUnits(std::initializer_list<BattleRuntimeUnit> unitList)
{
    return KysChess::Battle::Test::runtimeRecords(unitList);
}

BattleAttackInstance attack(int id, int attackerId, double x, double y)
{
    BattleAttackInstance state{ BattleAttackPayload(
        BattleAttackDelivery::projectile(),
        BattleProjectilePayloadClass::combat(),
        BattleAttackReflectionLineageKind::Ordinary) };
    state.id = id;
    state.state.attackSourceUnitId = attackerId;
    state.state.totalFrame = 30;
    state.state.position = { static_cast<float>(x), static_cast<float>(y), 0.0f };
    return state;
}

struct TestAttackWorld : BattleAttackState
{
    using BattleAttackState::spawn;
    using BattleAttackState::tick;

    BattleCastLifecycle lifecycle;

    BattleAttackEvent spawn(BattleAttackSpawnRequest request)
    {
        assert(!request.provenance.valid());
        assert(!request.castWork.valid());
        const auto cast = lifecycle.beginRootCast({
            request.initial.attackSourceUnitId,
            request.initial.skillId,
            request.provenance.cast.ultimate,
            request.provenance.cast.ultimate
                ? CastOriginKind::Ultimate
                : CastOriginKind::Normal,
        });
        const auto reservation = lifecycle.reserveAttack(cast.provenance.castId, {
            .rootAttack = true,
            .mainProjectile = request.provenance.mainProjectile,
            .sharedHitGroupId = request.provenance.sharedHitGroupId,
            .propagation = request.provenance.propagation,
        });
        request.provenance = reservation.provenance;
        request.castWork = reservation.work;
        auto event = BattleAttackState::spawn(std::move(request), lifecycle);
        lifecycle.completeWork(cast.commitBarrier);
        return event;
    }

    std::pmr::vector<BattleAttackEvent> tick(
        const BattleRuntimeUnits& units,
        std::pmr::memory_resource* memoryResource = std::pmr::get_default_resource())
    {
        auto events = BattleAttackState::tick(units, lifecycle, memoryResource);
        std::vector<BattleAttackEvent> hits;
        std::ranges::copy_if(
            events,
            std::back_inserter(hits),
            [](const BattleAttackEvent& event)
            {
                return event.type == BattleAttackEventType::Hit;
            });
        for (const auto& hit : hits)
        {
            auto settled = settleHit(
                {
                    .attackId = hit.attackId,
                    .targetUnitId = hit.unitId,
                    .accepted = true,
                    .continuation = BattleHitContinuation::Normal,
                },
                units,
                lifecycle);
            events.insert(
                events.end(),
                std::make_move_iterator(settled.events.begin()),
                std::make_move_iterator(settled.events.end()));
        }
        appendProjectileCancelEvents(units, events);
        return events;
    }

    void addAttack(
        BattleAttackInstance instance,
        BattlePendingAttackProvenance pending = {})
    {
        assert(instance.id >= 0);
        assert(!instance.provenance.valid());
        assert(!instance.castWork.valid());
        const bool bounce = pending.origin == BattleAttackOriginKind::Bounce;
        const auto cast = lifecycle.beginRootCast({
            instance.state.attackSourceUnitId,
            instance.state.skillId,
            pending.cast.ultimate,
            pending.cast.ultimate ? CastOriginKind::Ultimate : CastOriginKind::Normal,
        });
        BattleAttackReservationRequest reservationRequest;
        if (bounce)
        {
            reservationRequest.origin = pending.origin;
        }
        reservationRequest.rootAttack = !bounce;
        reservationRequest.mainProjectile = pending.mainProjectile;
        reservationRequest.sharedHitGroupId = pending.sharedHitGroupId;
        reservationRequest.propagation = pending.propagation;
        const auto reservation = lifecycle.reserveAttack(
            cast.provenance.castId,
            reservationRequest);
        instance.provenance = completeAttackProvenance(
            reservation.provenance,
            battleAttackIdFromRuntimeId(instance.id));
        instance.castWork = reservation.work;
        lifecycle.transferToLiveAttack(
            reservation.work,
            instance.provenance.attackId);
        lifecycle.completeWork(cast.commitBarrier);
        attacks.push_back(std::move(instance));
    }

    void setAttacks(std::initializer_list<BattleAttackInstance> instances)
    {
        assert(attacks.empty());
        for (auto instance : instances)
        {
            addAttack(std::move(instance));
        }
    }
};

bool hasEvent(std::span<const BattleAttackEvent> events, BattleAttackEventType type, int attackId, int unitId = -1)
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
    BattleAttackSpawnRequest request{ BattleAttackPayload(
        BattleAttackDelivery::projectile(),
        BattleProjectilePayloadClass::combat(),
        BattleAttackReflectionLineageKind::Ordinary) };
    request.initial.attackSourceUnitId = 1;
    request.initial.skillId = 101;
    request.initial.operationType = BattleOperationType::RangedProjectile;
    request.initial.visualEffectId = 33;
    request.initial.preferredTargetUnitId = 2;
    request.initial.position = { 10, 20, 0 };
    request.initial.velocity = { 3, 4, 0 };
    request.initial.totalFrame = 30;
    return request;
}

TestAttackWorld attackWorld()
{
    TestAttackWorld world;
    world.hitRadius = SceneHitRadius;
    world.minimumVectorNorm = TestMinimumVectorNorm;
    world.bounceSpawnDistance = SceneBounceSpawnDistance;
    world.defaultProjectileSpeed = SceneProjectileSpeed;
    return world;
}
}  // namespace

TEST_CASE("BattleAttackSystem_WorldGeometryStartsEmptyUntilSupplied", "[battle][attack][unit]")
{
    BattleAttackState world;

    CHECK(world.hitRadius == Catch::Approx(0.0));
    CHECK(world.minimumVectorNorm == Catch::Approx(0.0));
    CHECK(world.bounceSpawnDistance == Catch::Approx(0.0));
    CHECK(world.defaultProjectileSpeed == Catch::Approx(0.0));
}

TEST_CASE("BattleAttackSystem_ExplicitAttackPayloadHasNoCastSubrequestKind", "[battle][attack][unit]")
{
    BattleAttackSpawnRequest request{ BattleAttackPayload(
        BattleAttackDelivery::contact(),
        BattleProjectilePayloadClass::combat(),
        BattleAttackReflectionLineageKind::Ordinary) };
    BattleAttackInstance instance{ BattleAttackPayload(
        BattleAttackDelivery::contact(),
        BattleProjectilePayloadClass::combat(),
        BattleAttackReflectionLineageKind::Ordinary) };

    CHECK(request.initial.castSubrequestKind == BattleAttackCastSubrequestKind::None);
    CHECK(instance.state.castSubrequestKind == BattleAttackCastSubrequestKind::None);
}

TEST_CASE("BattleAttackSystem_DelayedSpawnElapsesWithoutEnteringAttackWorldEarly", "[battle][attack][unit]")
{
    BattleCastLifecycle lifecycle;
    const auto cast = lifecycle.beginRootCast({ 1, 101, false });
    const auto reservation = lifecycle.reserveAttack(cast.provenance.castId, {
        .rootAttack = true,
    });
    BattleAttackSpawnRequest request{ BattleAttackPayload(
        BattleAttackDelivery::projectile(),
        BattleProjectilePayloadClass::combat(),
        BattleAttackReflectionLineageKind::Ordinary) };
    request.provenance = reservation.provenance;
    request.castWork = reservation.work;
    request.spawnDelayFrames = 3;

    CHECK_FALSE(attackSpawnDelayElapsed(request));
    CHECK(request.spawnDelayFrames == 2);
    CHECK_FALSE(attackSpawnDelayElapsed(request));
    CHECK(request.spawnDelayFrames == 1);
    CHECK_FALSE(attackSpawnDelayElapsed(request));
    CHECK(request.spawnDelayFrames == 0);
    CHECK(attackSpawnDelayElapsed(request));
}

TEST_CASE("BattleAttackSystem_AppliesAuthorizedBouncePrimeToSpawnRequest", "[battle][attack][unit]")
{
    BattleAttackSpawnRequest request = spawnRequest();

    applyProjectileBouncePrime(request, { 2, 80, 30, 120 });

    CHECK(request.initial.bounceRemaining == 2);
    CHECK(request.initial.bounceChancePct == 80);
    CHECK(request.initial.bounceRollPct == 30);
    CHECK(request.initial.bounceRange == 120);
}

TEST_CASE("BattleAttackSystem_AppliesBouncePrimeOnlyToEligibleRequests", "[battle][attack][unit]")
{
    BattleAttackSpawnRequest request = spawnRequest();
    request.initial.scriptedDamage = 10;
    request.initial.payloadClass = BattleProjectilePayloadClass::scriptedDamage();

    CHECK_FALSE(tryApplyProjectileBouncePrime(request, { 2, 80, 30, 120 }));

    CHECK(request.initial.bounceRemaining == 0);
    CHECK(request.initial.bounceChancePct == 0);
    CHECK(request.initial.bounceRollPct == 0);
    CHECK(request.initial.bounceRange == 0);
}

TEST_CASE("BattleAttackSystem_SpawnAssignsDeterministicAttackIds", "[battle][attack][unit]")
{
    auto world = attackWorld();
    world.nextAttackId = 40;

    auto first = world.spawn(spawnRequest());
    auto second = world.spawn(spawnRequest());

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
    request.initial.requirePreferredTarget = true;
    request.initial.bounceRemaining = 2;
    request.initial.bounceRange = 120;
    request.initial.bounceChancePct = 80;
    request.initial.bounceRollPct = 30;
    request.initial.executeCanHitInvincible = true;
    request.initial.ignoreProjectileCancel = true;
    request.initial.scriptedDamage = 33;
    request.initial.scriptedDamageAppliesModifiers = true;
    request.initial.scriptedDamageTriggersDefenseEffects = true;
    request.initial.scriptedStunFrames = 12;
    request.initial.scriptedBleedStacks = 4;
    request.initial.scriptedBleedProducer = makeBattleStatusProducerProvenance(
        { .kind = KysChess::EffectSourceKind::Combo, .sourceId = 71, .ownerUnitId = 3 },
        KysChess::EffectRuleId{ 71 },
        5);
    request.initial.payloadClass = BattleProjectilePayloadClass::scriptedControl();
    request.initial.projectileCancelDamage = 90;
    request.initial.projectileCancelWeaken = 13;
    request.initial.projectilePressurePct = 65;
    request.initial.strengthPct = 200;
    request.initial.suppressNearbyTrackingProjectileProc = true;
    request.initialFrame = 4;
    request.acceleration = { 1, 2, 3 };
    request.spiralMotion = true;
    request.spiralCenter = { 7, 8, 9 };
    request.spiralRadius = 10.0f;
    request.spiralRadiusGrowth = 0.5f;
    request.spiralAngle = 1.25f;
    request.spiralAngularVelocity = 0.75f;
    request.provenance.mainProjectile = false;
    request.provenance.sharedHitGroupId = 7;

    world.spawn(request);

    REQUIRE(world.attacks.size() == 1);
    const auto& attack = world.attacks[0];
    CHECK(attack.state.attackSourceUnitId == 1);
    CHECK(attack.state.skillId == 101);
    CHECK(attack.state.operationType == BattleOperationType::RangedProjectile);
    CHECK(attack.state.visualEffectId == 33);
    CHECK(attack.state.preferredTargetUnitId == 2);
    CHECK(attack.state.position.x == 10.0f);
    CHECK(attack.state.position.y == 20.0f);
    CHECK(attack.state.velocity.x == 3.0f);
    CHECK(attack.state.velocity.y == 4.0f);
    CHECK(attack.state.totalFrame == 30);
    CHECK(attack.state.through);
    CHECK(attack.state.track);
    CHECK(attack.provenance.sharedHitGroupId == 7);
    CHECK(attack.state.requirePreferredTarget);
    CHECK(attack.state.bounceRemaining == 2);
    CHECK(attack.state.bounceRange == 120);
    CHECK(attack.state.bounceChancePct == 80);
    CHECK(attack.state.bounceRollPct == 30);
    CHECK(attack.state.executeCanHitInvincible);
    CHECK(attack.state.ignoreProjectileCancel);
    CHECK(attack.state.scriptedDamage == 33);
    CHECK(attack.state.scriptedDamageAppliesModifiers);
    CHECK(attack.state.scriptedDamageTriggersDefenseEffects);
    CHECK(attack.state.scriptedStunFrames == 12);
    CHECK(attack.state.scriptedBleedStacks == 4);
    CHECK(attack.state.payloadClass.kind()
        == BattleProjectilePayloadKind::ScriptedControl);
    CHECK(attack.state.projectileCancelDamage == 90);
    CHECK(attack.state.projectileCancelWeaken == 13);
    CHECK(attack.state.projectilePressurePct == 65);
    CHECK(attack.state.strengthPct == 200);
    CHECK(attack.state.suppressNearbyTrackingProjectileProc);
    CHECK_FALSE(attack.provenance.mainProjectile);
    CHECK(attack.frame == 4);
    CHECK(attack.acceleration.x == 1.0f);
    CHECK(attack.acceleration.y == 2.0f);
    CHECK(attack.acceleration.z == 3.0f);
    CHECK(attack.spiralMotion);
    CHECK(attack.spiralCenter.x == 7.0f);
    CHECK(attack.spiralCenter.y == 8.0f);
    CHECK(attack.spiralCenter.z == 9.0f);
    CHECK(attack.spiralRadius == 10.0f);
    CHECK(attack.spiralRadiusGrowth == 0.5f);
    CHECK(attack.spiralAngle == 1.25f);
    CHECK(attack.spiralAngularVelocity == 0.75f);
    CHECK(attack.state.castSubrequestKind == BattleAttackCastSubrequestKind::None);
}

TEST_CASE("BattleAttackSystem_SpawnStoresCastSubrequestMetadata", "[battle][attack][unit]")
{
    auto world = attackWorld();
    BattleAttackSpawnRequest request = spawnRequest();
    request.initialFrame = 6;
    request.initial.castSubrequestKind = BattleAttackCastSubrequestKind::DashHit;
    request.initial.strengthPct = 200;

    world.spawn(request);

    REQUIRE(world.attacks.size() == 1);
    CHECK(world.attacks[0].frame == 6);
    CHECK(world.attacks[0].state.castSubrequestKind == BattleAttackCastSubrequestKind::DashHit);
    CHECK(world.attacks[0].state.strengthPct == 200);
}

TEST_CASE("BattleAttackSystem_SpawnStoresUltimateOnlyInProvenance", "[battle][attack][unit]")
{
    auto world = attackWorld();
    BattleAttackSpawnRequest request = spawnRequest();
    request.provenance.cast.ultimate = true;

    world.spawn(request);

    REQUIRE(world.attacks.size() == 1);
    CHECK(world.attacks[0].provenance.cast.ultimate);
}

TEST_CASE("BattleAttackSystem_SpawnEmitsVisualPayloadWithoutScenePointers", "[battle][attack][unit]")
{
    auto world = attackWorld();

    auto event = world.spawn(spawnRequest());

    CHECK(event.type == BattleAttackEventType::AttackSpawned);
    CHECK(event.attackId == 0);
    CHECK(event.sourceUnitId == 1);
    CHECK(event.unitId == 2);
    CHECK(event.preferredTargetUnitId == 2);
    CHECK(event.skillId == 101);
    CHECK(event.operationType == BattleOperationType::RangedProjectile);
    CHECK(event.visualEffectId == 33);
    CHECK(event.position.x == 10.0f);
    CHECK(event.position.y == 20.0f);
    CHECK(event.velocity.x == 3.0f);
    CHECK(event.velocity.y == 4.0f);
    CHECK(event.totalFrame == 30);
}

TEST_CASE("BattleAttackSystem_SpawnAllowsDerivedProjectileAttackerToDifferFromCastOwner",
          "[battle][attack][unit][attack_source]")
{
    auto world = attackWorld();
    BattleCastLifecycle lifecycle;
    const auto cast = lifecycle.beginRootCast({
        .sourceUnitId = 1,
        .magicId = 62,
        .ultimate = false,
        .origin = CastOriginKind::Normal,
    });
    const auto reservation = lifecycle.reserveAttack(cast.provenance.castId, {
        .origin = BattleAttackOriginKind::CastDerived,
        .rootAttack = false,
    });
    auto request = spawnRequest();
    request.initial.attackSourceUnitId = 7;
    request.provenance = reservation.provenance;
    request.castWork = reservation.work;

    const auto event = world.spawn(std::move(request), lifecycle);

    CHECK(event.sourceUnitId == 7);
    CHECK(event.provenance.cast.sourceUnitId == 1);
    CHECK(event.preferredTargetUnitId == 2);
}

TEST_CASE("BattleAttackSystem_HitEventCarriesDamageRequestPayload", "[battle][attack][unit]")
{
    auto world = attackWorld();
    world.hitRadius = SceneHitRadius;
    auto units = runtimeUnits({ unit(1, 0, 0, 0), unit(2, 1, 40, 0) });
    auto projectile = attack(10, 1, 0, 0);
    projectile.state.skillId = 101;
    projectile.state.operationType = BattleOperationType::RangedProjectile;
    projectile.state.scriptedDamage = 33;
    projectile.state.scriptedDamageAppliesModifiers = true;
    projectile.state.scriptedDamageTriggersDefenseEffects = true;
    projectile.state.scriptedStunFrames = 12;
    projectile.state.scriptedBleedStacks = 4;
    projectile.state.scriptedBleedProducer = makeBattleStatusProducerProvenance(
        { .kind = KysChess::EffectSourceKind::Combo, .sourceId = 71, .ownerUnitId = 3 },
        KysChess::EffectRuleId{ 71 },
        5);
    projectile.state.payloadClass = BattleProjectilePayloadClass::scriptedControl();
    projectile.state.executeCanHitInvincible = true;
    projectile.state.projectileCancelWeaken = 6;
    projectile.state.strengthPct = 175;
    projectile.state.suppressNearbyTrackingProjectileProc = true;
    projectile.state.track = true;
    projectile.state.through = true;
    projectile.state.position = { 12, 5, 0 };
    projectile.state.velocity = { 9, 0, 0 };
    projectile.frame = 7;
    world.addAttack(std::move(projectile), {
        .cast = { .ultimate = true },
        .mainProjectile = false,
        .sharedHitGroupId = 17,
    });

    auto events = world.tick(units);

    auto hit = std::find_if(events.begin(), events.end(), [](const BattleAttackEvent& event) {
        return event.type == BattleAttackEventType::Hit;
    });
    REQUIRE(hit != events.end());
    CHECK(hit->attackId == 10);
    CHECK(hit->sourceUnitId == 1);
    CHECK(hit->unitId == 2);
    CHECK(hit->skillId == 101);
    CHECK(hit->operationType == BattleOperationType::RangedProjectile);
    CHECK(hit->scriptedDamage == 33);
    CHECK(hit->scriptedDamageAppliesModifiers);
    CHECK(hit->scriptedDamageTriggersDefenseEffects);
    CHECK(hit->scriptedStunFrames == 12);
    CHECK(hit->scriptedBleedStacks == 4);
    CHECK(hit->executeCanHitInvincible);
    CHECK(hit->projectileCancelDamage == 6);
    CHECK(hit->strengthPct == 175);
    CHECK(hit->suppressNearbyTrackingProjectileProc);
    CHECK_FALSE(hit->provenance.mainProjectile);
    CHECK(hit->track);
    CHECK(hit->provenance.sharedHitGroupId == 17);
    CHECK(hit->through);
    CHECK(hit->provenance.cast.ultimate);
    CHECK(hit->frame == 8);
    CHECK(hit->totalFrame == 30);
    CHECK(hit->position.x == Catch::Approx(21.0f));
    CHECK(hit->velocity.x == Catch::Approx(9.0f).margin(0.01));
}

TEST_CASE("BattleAttackSystem_HitEventKeepsPreferredTargetSeparateFromContact", "[battle][attack][unit][preferred_target]")
{
    auto world = attackWorld();
    auto units = runtimeUnits({ unit(1, 0, 0, 0), unit(2, 1, 40, 0), unit(9, 1, 200, 0) });
    units.requireCore(9).alive = false;
    auto projectile = attack(10, 1, 0, 0);
    projectile.state.preferredTargetUnitId = 9;
    world.addAttack(std::move(projectile));

    const auto events = world.tick(units);
    const auto hit = std::ranges::find_if(events, [](const BattleAttackEvent& event)
    {
        return event.type == BattleAttackEventType::Hit;
    });

    REQUIRE(hit != events.end());
    CHECK(hit->unitId == 2);
    CHECK(hit->preferredTargetUnitId == 9);
}

TEST_CASE("BattleAttackSystem_InvincibleContactEmitsNonDamagingBlockOnce", "[battle][attack][unit]")
{
    auto world = attackWorld();
    world.hitRadius = SceneHitRadius;
    auto units = runtimeUnits({ unit(1, 0, 0, 0), unit(2, 1, 40, 0) });
    units.requireCore(2).invincible = 1;
    auto projectile = attack(10, 1, 0, 0);
    projectile.state.operationType = BattleOperationType::RangedProjectile;
    projectile.state.velocity = { 10, 0, 0 };
    world.addAttack(std::move(projectile));

    auto events = world.tick(units);

    CHECK(hasEvent(events, BattleAttackEventType::BlockedByInvincible, 10, 2));
    CHECK(!hasEvent(events, BattleAttackEventType::Hit, 10, 2));

    auto repeated = world.tick(units);

    CHECK(!hasEvent(repeated, BattleAttackEventType::BlockedByInvincible, 10, 2));
    CHECK(!hasEvent(repeated, BattleAttackEventType::Hit, 10, 2));

    units.requireCore(2).invincible = 0;
    auto vulnerable = world.tick(units);

    CHECK(hasEvent(vulnerable, BattleAttackEventType::Hit, 10, 2));
}

TEST_CASE("BattleAttackSystem_MeleeHitOnlyEmitsAfterHitVolumeReachesTarget", "[battle][attack][unit]")
{
    auto world = attackWorld();
    auto units = runtimeUnits({
        unit(1, 0, 0, 0),
        unit(2, 1, SceneHitRadius + 1.0, 0),
    });
    auto melee = attack(10, 1, 0, 0);
    melee.state.operationType = BattleOperationType::Melee;
    world.addAttack(std::move(melee));

    auto beforeReach = world.tick(units);

    CHECK(!hasEvent(beforeReach, BattleAttackEventType::Hit, 10, 2));

    world.attacks[0].state.position = { static_cast<float>(SceneHitRadius), 0.0f, 0.0f };
    auto atReach = world.tick(units);

    CHECK(hasEvent(atReach, BattleAttackEventType::Hit, 10, 2));
}

TEST_CASE("BattleAttackSystem_RangedHitOnlyEmitsAfterProjectileReachesTarget", "[battle][attack][unit]")
{
    auto world = attackWorld();
    auto units = runtimeUnits({
        unit(1, 0, 0, 0),
        unit(2, 1, SceneHitRadius + SceneProjectileSpeed + 1.0, 0),
    });
    auto projectile = attack(10, 1, 0, 0);
    projectile.state.operationType = BattleOperationType::RangedProjectile;
    projectile.state.velocity = { static_cast<float>(SceneProjectileSpeed), 0.0f, 0.0f };
    world.addAttack(std::move(projectile));

    auto beforeReach = world.tick(units);

    CHECK(!hasEvent(beforeReach, BattleAttackEventType::Hit, 10, 2));

    auto atReach = world.tick(units);

    CHECK(hasEvent(atReach, BattleAttackEventType::Hit, 10, 2));
}

TEST_CASE("BattleAttackSystem_FastProjectileHitsTargetCrossedBetweenFrames", "[battle][attack][unit]")
{
    auto world = attackWorld();
    world.hitRadius = 10.0;
    auto units = runtimeUnits({
        unit(1, 0, 0, 0),
        unit(2, 1, 50, 0),
    });
    auto projectile = attack(10, 1, 0, 0);
    projectile.state.operationType = BattleOperationType::RangedProjectile;
    projectile.state.velocity = { 100, 0, 0 };
    world.addAttack(std::move(projectile));

    auto events = world.tick(units);

    CHECK(hasEvent(events, BattleAttackEventType::Hit, 10, 2));
}

TEST_CASE("BattleAttackSystem_OngoingProjectileCanHitAfterSourceDies", "[battle][attack][unit]")
{
    auto world = attackWorld();
    world.hitRadius = 10.0;
    auto units = runtimeUnits({
        unit(1, 0, 0, 0),
        unit(2, 1, 50, 0),
    });
    units.requireCore(1).alive = false;
    auto projectile = attack(10, 1, 0, 0);
    projectile.state.operationType = BattleOperationType::RangedProjectile;
    projectile.state.velocity = { 100, 0, 0 };
    world.addAttack(std::move(projectile));

    auto events = world.tick(units);

    CHECK(hasEvent(events, BattleAttackEventType::Hit, 10, 2));
}

TEST_CASE("BattleAttackSystem_FastPreferredProjectileCanHitCloseTargetBehindSpawnOffset", "[battle][attack][unit]")
{
    auto world = attackWorld();
    world.hitRadius = 10.0;
    auto units = runtimeUnits({
        unit(1, 0, 0, 0),
        unit(2, 1, 40, 0),
    });
    auto projectile = attack(10, 1, 72, 0);
    projectile.state.operationType = BattleOperationType::RangedProjectile;
    projectile.state.preferredTargetUnitId = 2;
    projectile.state.velocity = { 60, 0, 0 };
    world.addAttack(std::move(projectile));

    auto events = world.tick(units);

    CHECK(hasEvent(events, BattleAttackEventType::Hit, 10, 2));
}

TEST_CASE("BattleAttackSystem_MovesAndExpiresProjectiles", "[battle][attack][unit]")
{
    auto world = attackWorld();
    auto units = runtimeUnits({ unit(1, 0, 0, 0), unit(2, 1, 500, 0) });
    auto projectile = attack(10, 1, 0, 0);
    projectile.state.velocity = { 3, 4, 0 };
    projectile.state.totalFrame = 1;
    world.addAttack(std::move(projectile));

    auto events = world.tick(units);

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
    auto units = runtimeUnits({ unit(1, 0, 0, 0), unit(2, 1, 40, 0), unit(3, 1, 45, 0) });
    world.addAttack(attack(10, 1, 0, 0));

    auto events = world.tick(units);

    REQUIRE(world.attacks[0].hitUnitIds.size() == 1);
    CHECK(world.attacks[0].hitUnitIds[0] == 2);
    CHECK(world.attacks[0].noHurt);
    CHECK(hasEvent(events, BattleAttackEventType::Hit, 10, 2));

    auto secondEvents = world.tick(units);
    CHECK(!hasEvent(secondEvents, BattleAttackEventType::Hit, 10, 2));
}

TEST_CASE("BattleAttackSystem_ThroughProjectileCanHitDifferentEnemiesButNotSameTargetTwice", "[battle][attack][unit]")
{
    auto world = attackWorld();
    auto units = runtimeUnits({ unit(1, 0, 0, 0), unit(2, 1, 30, 0), unit(3, 1, 120, 0) });
    auto projectile = attack(10, 1, 0, 0);
    projectile.state.through = true;
    world.addAttack(std::move(projectile));

    auto firstEvents = world.tick(units);
    CHECK(hasEvent(firstEvents, BattleAttackEventType::Hit, 10, 2));
    CHECK_FALSE(world.attacks[0].noHurt);

    world.attacks[0].state.velocity = { 90, 0, 0 };
    auto secondEvents = world.tick(units);
    CHECK(hasEvent(secondEvents, BattleAttackEventType::Hit, 10, 3));
    CHECK(!hasEvent(secondEvents, BattleAttackEventType::Hit, 10, 2));
}

TEST_CASE("BattleAttackSystem_SharedHitGroupPreventsDuplicateHitsAcrossProjectiles", "[battle][attack][unit]")
{
    auto world = attackWorld();
    auto units = runtimeUnits({ unit(1, 0, 0, 0), unit(2, 1, 30, 0) });
    auto first = attack(10, 1, 0, 0);
    first.state.through = true;
    auto second = attack(11, 1, 0, 0);
    second.state.through = true;
    world.addAttack(std::move(first), { .sharedHitGroupId = 7 });
    world.addAttack(std::move(second), { .sharedHitGroupId = 7 });

    auto events = world.tick(units);

    CHECK(hasEvent(events, BattleAttackEventType::Hit, 10, 2));
    CHECK(!hasEvent(events, BattleAttackEventType::Hit, 11, 2));
    REQUIRE(world.sharedHitGroupTargets[7].size() == 1);
    CHECK(world.sharedHitGroupTargets[7][0] == 2);
}

TEST_CASE("BattleAttackSystem_RequiredPreferredTargetExpiresWhenTargetInvalid", "[battle][attack][unit]")
{
    auto world = attackWorld();
    auto units = runtimeUnits({ unit(1, 0, 0, 0), unit(2, 1, 30, 0) });
    units.requireCore(2).alive = false;
    auto projectile = attack(10, 1, 0, 0);
    projectile.state.preferredTargetUnitId = 2;
    projectile.state.requirePreferredTarget = true;
    projectile.state.totalFrame = 20;
    world.addAttack(std::move(projectile));

    auto events = world.tick(units);

    CHECK(world.attacks[0].noHurt);
    CHECK(world.attacks[0].frame == 15);
    CHECK(hasEvent(events, BattleAttackEventType::TargetLost, 10));
}

TEST_CASE("BattleAttackSystem_TrackingPreservesSpeedWhileTurningTowardTarget", "[battle][attack][unit]")
{
    auto world = attackWorld();
    world.hitRadius = TightTrackingHitRadius;
    auto units = runtimeUnits({ unit(1, 0, 0, 0), unit(2, 1, 100, 100) });
    auto projectile = attack(10, 1, 0, 0);
    projectile.state.track = true;
    projectile.state.velocity = { 10, 0, 0 };
    world.addAttack(std::move(projectile));

    world.tick(units);

    CHECK(world.attacks[0].state.velocity.y > 0.0f);
    CHECK(world.attacks[0].state.velocity.x > 0.0f);
}

TEST_CASE("BattleAttackSystem_TrackingProjectileStopsSteeringAfterFirstThroughHit", "[battle][attack][unit]")
{
    auto world = attackWorld();
    world.hitRadius = TightTrackingHitRadius;
    auto units = runtimeUnits({ unit(1, 0, 0, 0), unit(2, 1, 100, 100) });
    auto projectile = attack(10, 1, 0, 0);
    projectile.state.track = true;
    projectile.state.through = true;
    projectile.state.preferredTargetUnitId = 2;
    projectile.state.velocity = { 10, 0, 0 };
    projectile.hitUnitIds.push_back(2);
    world.addAttack(std::move(projectile));

    world.tick(units);

    CHECK(world.attacks[0].state.velocity.x == Catch::Approx(10.0f));
    CHECK(world.attacks[0].state.velocity.y == Catch::Approx(0.0f));
}

TEST_CASE("BattleAttackSystem_ProjectileSweepClearsEveryEnemyBeforeContact",
          "[battle][attack][unit][projectile_sweep]")
{
    auto world = attackWorld();
    world.hitRadius = 10;
    const auto units = runtimeUnits({ unit(1, 0, 50, 0), unit(2, 1, 75, 0) });
    auto sweep = attack(10, 1, 0, 0);
    sweep.state.velocity = { 100, 0, 0 };
    sweep.state.through = true;
    sweep.state.projectileClearRadiusPct = 150;
    world.addAttack(std::move(sweep));

    for (int id = 11; id <= 14; ++id)
    {
        auto target = attack(id, 2, 20 * (id - 10), 0);
        target.state.projectileCancelDamage = 10000;
        target.state.ignoreProjectileCancel = true;
        if (id == 12)
        {
            target.state.position = { 50, -100, 0 };
            target.state.velocity = { 0, 200, 0 };
        }
        if (id == 13) target.state.payloadClass = BattleProjectilePayloadClass::scriptedControl();
        if (id == 14) target.state.payloadClass = BattleProjectilePayloadClass::scriptedDamage();
        BattlePendingAttackProvenance pending;
        pending.cast.ultimate = id == 12;
        world.addAttack(std::move(target), pending);
    }
    auto friendly = attack(15, 1, 50, 0);
    world.addAttack(std::move(friendly));
    auto outside = attack(16, 2, 50, 16);
    world.addAttack(std::move(outside));
    auto contact = attack(17, 2, 50, 0);
    contact.state.delivery = BattleAttackDelivery::contact();
    world.addAttack(std::move(contact));
    auto effect = attack(18, 2, 50, 0);
    effect.state.delivery = BattleAttackDelivery::effect();
    world.addAttack(std::move(effect));

    const auto events = world.tick(units);
    CHECK(hasEvent(events, BattleAttackEventType::Hit, 10, 2));
    CHECK_FALSE(world.attacks.front().noHurt);
    for (int id = 11; id <= 14; ++id)
    {
        CHECK_FALSE(hasEvent(events, BattleAttackEventType::Hit, id, 1));
        CHECK(hasEvent(events, BattleAttackEventType::Expired, id));
        const auto& cleared = world.attacks[id - 10];
        CHECK(cleared.noHurt);
        CHECK(cleared.scheduledFinishReason == AttackFinishReason::ProjectileCancelled);
        CHECK_FALSE(cleared.pendingContact);
    }
    CHECK_FALSE(world.attacks[5].noHurt);
    CHECK_FALSE(world.attacks[6].noHurt);
    CHECK(hasEvent(events, BattleAttackEventType::Hit, 17, 1));
    CHECK(hasEvent(events, BattleAttackEventType::Hit, 18, 1));
    world.completeFinished(world.lifecycle);
    for (int i = 1; i <= 4; ++i)
    {
        CHECK(world.attacks[i].finishReason == AttackFinishReason::ProjectileCancelled);
        CHECK_FALSE(world.attacks[i].castWork.valid());
    }
    world.eraseFinished();
    CHECK(std::ranges::none_of(world.attacks, [](const auto& value)
    {
        return value.id >= 11 && value.id <= 14;
    }));
}

TEST_CASE("BattleAttackSystem_ProjectileSweepRadiusAndContinuedTravel",
          "[battle][attack][unit][projectile_sweep]")
{
    int radiusPct = 100;
    SECTION("same hit radius") {}
    SECTION("larger hit radius") { radiusPct = 150; }
    auto world = attackWorld();
    world.hitRadius = 10;
    const auto units = runtimeUnits({ unit(1, 0, -500, 0), unit(2, 1, 500, 0) });
    auto sweep = attack(10, 1, 0, 0);
    sweep.state.velocity = { 100, 0, 0 };
    sweep.state.projectileClearRadiusPct = radiusPct;
    world.addAttack(std::move(sweep));
    world.addAttack(attack(11, 2, 50, radiusPct / 10.0));
    world.addAttack(attack(12, 2, 50, radiusPct / 10.0 + 1));
    world.addAttack(attack(13, 2, 175, 0));
    const auto first = world.tick(units);
    CHECK(hasEvent(first, BattleAttackEventType::Expired, 11));
    CHECK_FALSE(hasEvent(first, BattleAttackEventType::Expired, 12));
    CHECK_FALSE(hasEvent(first, BattleAttackEventType::Expired, 13));
    world.pruneFinished(world.lifecycle);
    const auto second = world.tick(units);
    CHECK(hasEvent(second, BattleAttackEventType::Expired, 13));
    CHECK_FALSE(world.attacks.front().noHurt);
}

TEST_CASE("BattleAttackSystem_OpposingProjectileSweepsClearEachOther",
          "[battle][attack][unit][projectile_sweep]")
{
    auto world = attackWorld();
    const auto units = runtimeUnits({ unit(1, 0, -500, 0), unit(2, 1, 500, 0) });
    for (int id = 1; id <= 2; ++id)
    {
        auto sweep = attack(id, id, 0, 0);
        sweep.state.projectileClearRadiusPct = 100;
        world.addAttack(std::move(sweep));
    }
    const auto events = world.tick(units);
    CHECK(hasEvent(events, BattleAttackEventType::Expired, 1));
    CHECK(hasEvent(events, BattleAttackEventType::Expired, 2));
    world.pruneFinished(world.lifecycle);
    CHECK(world.attacks.empty());
}

TEST_CASE("BattleAttackSystem_ProjectileCancelEventsAreDeterministic", "[battle][attack][unit]")
{
    auto world = attackWorld();
    world.projectileGraceFrames = 5;
    auto units = runtimeUnits({ unit(1, 0, -1000, 0), unit(2, 1, 1000, 0) });
    auto lhs = attack(10, 1, 0, 0);
    lhs.frame = 5;
    auto rhs = attack(11, 2, 20, 0);
    rhs.frame = 5;
    world.setAttacks({ lhs, rhs });

    auto events = world.tick(units);

    REQUIRE(events.back().type == BattleAttackEventType::ProjectileCancel);
    CHECK(events.back().attackId == 10);
    CHECK(events.back().otherAttackId == 11);
}

TEST_CASE("BattleAttackSystem_ProjectileCancelEventCarriesSourceIdsAndScaledDamage", "[battle][attack][unit]")
{
    auto world = attackWorld();
    world.projectileGraceFrames = 5;
    auto units = runtimeUnits({ unit(1, 0, -1000, 0), unit(2, 1, 1000, 0) });
    auto lhs = attack(10, 1, 0, 0);
    lhs.frame = 5;
    lhs.state.operationType = BattleOperationType::TrackingProjectile;
    lhs.state.projectileCancelDamage = 11;
    auto rhs = attack(11, 2, 20, 0);
    rhs.frame = 5;
    rhs.state.operationType = BattleOperationType::RangedProjectile;
    rhs.state.projectileCancelDamage = 10;
    world.setAttacks({ lhs, rhs });

    auto events = world.tick(units);

    REQUIRE(events.back().type == BattleAttackEventType::ProjectileCancel);
    CHECK(events.back().attackId == 10);
    CHECK(events.back().otherAttackId == 11);
    CHECK(events.back().sourceUnitId == 1);
    CHECK(events.back().otherSourceUnitId == 2);
    CHECK(events.back().projectileCancelDamage == 17);
    CHECK(events.back().otherProjectileCancelDamage == 10);
}

TEST_CASE("BattleAttackSystem_OngoingProjectilesCanCancelAfterSourceDies", "[battle][attack][unit]")
{
    auto world = attackWorld();
    auto units = runtimeUnits({ unit(1, 0, -1000, 0), unit(2, 1, 1000, 0) });
    units.requireCore(1).alive = false;
    auto first = attack(10, 1, 0, 0);
    first.state.operationType = BattleOperationType::RangedProjectile;
    first.frame = world.projectileGraceFrames;
    first.state.projectileCancelDamage = 10;
    auto second = attack(11, 2, 20, 0);
    second.state.operationType = BattleOperationType::RangedProjectile;
    second.frame = world.projectileGraceFrames;
    second.state.projectileCancelDamage = 10;
    world.setAttacks({ first, second });

    auto events = world.tick(units);

    CHECK(hasEvent(events, BattleAttackEventType::ProjectileCancel, 10));
}

TEST_CASE("BattleAttackSystem_ProjectileCancelUsesEachProjectileOncePerFrame", "[battle][attack][unit]")
{
    auto world = attackWorld();
    world.projectileGraceFrames = 5;
    auto units = runtimeUnits({ unit(1, 0, -1000, 0), unit(2, 1, 1000, 0) });
    for (int i = 0; i < 3; ++i)
    {
        auto lhs = attack(10 + i, 1, 0, 0);
        lhs.frame = 5;
        lhs.state.projectileCancelDamage = 100 - i;
        world.addAttack(std::move(lhs));

        auto rhs = attack(20 + i, 2, 0, 0);
        rhs.frame = 5;
        rhs.state.projectileCancelDamage = 90 - i;
        world.addAttack(std::move(rhs));
    }

    auto events = world.tick(units);

    std::vector<int> usedAttackIds;
    int cancelCount = 0;
    for (const auto& event : events)
    {
        if (event.type != BattleAttackEventType::ProjectileCancel)
        {
            continue;
        }
        ++cancelCount;
        CHECK(std::count(usedAttackIds.begin(), usedAttackIds.end(), event.attackId) == 0);
        usedAttackIds.push_back(event.attackId);
        CHECK(std::count(usedAttackIds.begin(), usedAttackIds.end(), event.otherAttackId) == 0);
        usedAttackIds.push_back(event.otherAttackId);
    }
    CHECK(cancelCount == 3);
}

TEST_CASE("BattleAttackSystem_ProjectileCancelMatchesHighestStrengthPairsFirst", "[battle][attack][unit]")
{
    auto world = attackWorld();
    world.projectileGraceFrames = 5;
    auto units = runtimeUnits({ unit(1, 0, -1000, 0), unit(2, 1, 1000, 0) });
    auto weakLeft = attack(10, 1, 0, 0);
    weakLeft.frame = 5;
    weakLeft.state.projectileCancelDamage = 10;
    auto strongLeft = attack(11, 1, 0, 0);
    strongLeft.frame = 5;
    strongLeft.state.projectileCancelDamage = 100;
    auto strongRight = attack(20, 2, 0, 0);
    strongRight.frame = 5;
    strongRight.state.projectileCancelDamage = 90;
    auto weakRight = attack(21, 2, 0, 0);
    weakRight.frame = 5;
    weakRight.state.projectileCancelDamage = 20;
    world.setAttacks({ weakLeft, strongLeft, strongRight, weakRight });

    auto events = world.tick(units);

    std::vector<BattleAttackEvent> cancels;
    std::copy_if(
        events.begin(),
        events.end(),
        std::back_inserter(cancels),
        [](const BattleAttackEvent& event)
        {
            return event.type == BattleAttackEventType::ProjectileCancel;
        });
    REQUIRE(cancels.size() == 2);
    CHECK(cancels[0].attackId == 11);
    CHECK(cancels[0].otherAttackId == 20);
    CHECK(cancels[1].attackId == 10);
    CHECK(cancels[1].otherAttackId == 21);
}

TEST_CASE("BattleAttackSystem_FastProjectilesCancelWhenCrossingBetweenFrames", "[battle][attack][unit]")
{
    auto world = attackWorld();
    world.hitRadius = 10.0;
    world.projectileGraceFrames = 5;
    auto units = runtimeUnits({ unit(1, 0, -1000, 0), unit(2, 1, 1000, 0) });
    auto lhs = attack(10, 1, 0, 0);
    lhs.frame = 5;
    lhs.state.velocity = { 100, 0, 0 };
    auto rhs = attack(11, 2, 120, 0);
    rhs.frame = 5;
    rhs.state.velocity = { -100, 0, 0 };
    world.setAttacks({ lhs, rhs });

    auto events = world.tick(units);

    REQUIRE(events.back().type == BattleAttackEventType::ProjectileCancel);
    CHECK(events.back().attackId == 10);
    CHECK(events.back().otherAttackId == 11);
}

TEST_CASE("BattleAttackSystem_ApplyProjectileCancelDamageCommitsWeakenRules", "[battle][attack][unit]")
{
    auto world = attackWorld();
    auto lhs = attack(10, 1, 0, 0);
    lhs.frame = 8;
    lhs.state.totalFrame = 30;
    lhs.state.projectileCancelWeaken = 6;
    auto rhs = attack(11, 2, 20, 0);
    rhs.frame = 9;
    rhs.state.totalFrame = 35;
    world.setAttacks({ lhs, rhs });

    BattleAttackEvent event;
    event.type = BattleAttackEventType::ProjectileCancel;
    event.attackId = 10;
    event.otherAttackId = 11;
    event.projectileCancelDamage = 6;
    event.otherProjectileCancelDamage = 7;

    world.applyProjectileCancelDamage(event);

    REQUIRE(world.attacks.size() == 2);
    CHECK(world.attacks[0].state.projectileCancelWeaken == 13);
    CHECK(world.attacks[0].frame == 25);
    CHECK(world.attacks[0].noHurt);
    CHECK(world.attacks[1].state.projectileCancelWeaken == 6);
    CHECK(world.attacks[1].frame == 9);
    CHECK_FALSE(world.attacks[1].noHurt);
}

TEST_CASE("BattleAttackSystem_UltimateProjectileDoesNotCancel", "[battle][attack][unit]")
{
    auto world = attackWorld();
    world.projectileGraceFrames = 5;
    auto units = runtimeUnits({ unit(1, 0, -1000, 0), unit(2, 1, 1000, 0) });
    auto lhs = attack(10, 1, 0, 0);
    lhs.frame = 5;
    auto rhs = attack(11, 2, 20, 0);
    rhs.frame = 5;
    world.addAttack(std::move(lhs), {
        .cast = { .ultimate = true },
    });
    world.addAttack(std::move(rhs));

    auto events = world.tick(units);

    CHECK(!hasEvent(events, BattleAttackEventType::ProjectileCancel, 10));
}

TEST_CASE("BattleAttackSystem_IgnoredProjectileDoesNotCancel", "[battle][attack][unit]")
{
    auto world = attackWorld();
    world.projectileGraceFrames = 5;
    auto units = runtimeUnits({ unit(1, 0, -1000, 0), unit(2, 1, 1000, 0) });
    auto lhs = attack(10, 1, 0, 0);
    lhs.frame = 5;
    lhs.state.ignoreProjectileCancel = true;
    auto rhs = attack(11, 2, 20, 0);
    rhs.frame = 5;
    world.setAttacks({ lhs, rhs });

    auto events = world.tick(units);

    CHECK(!hasEvent(events, BattleAttackEventType::ProjectileCancel, 10));
}

TEST_CASE("BattleAttackSystem_BounceSpawnsTrackingProjectileAtNearestEligibleTarget", "[battle][attack][unit]")
{
    auto world = attackWorld();
    world.nextAttackId = 20;
    auto units = runtimeUnits({
        unit(1, 0, 0, 0),
        unit(2, 1, 20, 0),
        unit(3, 1, 80, 0),
        unit(4, 1, 130, 0),
        unit(5, 1, 260, 0),
    });
    auto projectile = attack(10, 1, 0, 0);
    projectile.state.velocity = { 10, 0, 0 };
    projectile.state.bounceRemaining = 2;
    projectile.state.bounceRange = 120;
    projectile.state.bounceChancePct = 100;
    projectile.state.bounceRollPct = 0;
    projectile.hitUnitIds = { 4 };
    world.addAttack(std::move(projectile));

    auto events = world.tick(units);

    REQUIRE(world.attacks.size() == 2);
    const auto& source = world.attacks[0];
    CHECK(source.noHurt);
    CHECK(source.state.bounceRemaining == 0);

    const auto& bounce = world.attacks[1];
    CHECK(bounce.id == 20);
    CHECK(bounce.state.attackSourceUnitId == 1);
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

TEST_CASE("BattleAttackSystem_HitContinuationWaitsForExplicitSettlement",
          "[battle][attack][unit][projectile_reflection]")
{
    auto world = attackWorld();
    world.nextAttackId = 20;
    auto units = runtimeUnits({
        unit(1, 0, 0, 0),
        unit(2, 1, 20, 0),
        unit(3, 1, 80, 0),
    });
    auto projectile = attack(10, 1, 0, 0);
    projectile.state.velocity = { 10, 0, 0 };
    projectile.state.bounceRemaining = 2;
    projectile.state.bounceRange = 120;
    projectile.state.bounceChancePct = 100;
    projectile.state.bounceRollPct = 0;
    world.addAttack(std::move(projectile));

    auto events = world.BattleAttackState::tick(
        units,
        world.lifecycle,
        std::pmr::get_default_resource());

    CHECK(hasEvent(events, BattleAttackEventType::Hit, 10, 2));
    REQUIRE(world.attacks.size() == 1);
    const auto& pending = world.attacks.front();
    REQUIRE(pending.pendingContact);
    CHECK(pending.pendingContact->targetUnitId == 2);
    CHECK_FALSE(pending.noHurt);
    CHECK(pending.state.bounceRemaining == 2);
    CHECK_FALSE(hasEvent(events, BattleAttackEventType::Bounce, 10, 3));

    const auto settlement = world.settleHit(
        {
            .attackId = 10,
            .targetUnitId = 2,
            .accepted = false,
            .continuation = BattleHitContinuation::Normal,
        },
        units,
        world.lifecycle);

    CHECK(hasEvent(settlement.events, BattleAttackEventType::Bounce, 10, 3));
    REQUIRE(world.attacks.size() == 2);
    CHECK_FALSE(world.attacks[0].pendingContact);
    CHECK(world.attacks[0].noHurt);
    CHECK(world.attacks[0].state.bounceRemaining == 0);
    const auto& bounce = world.attacks[1];
    CHECK(bounce.state.bounceRemaining == 1);
    CHECK(bounce.state.reflectionLineage == BattleAttackReflectionLineageKind::Ordinary);
}

TEST_CASE("BattleAttackSystem_ReflectedSettlementSnapshotsMaterializedProjectileProperties",
          "[battle][attack][unit][projectile_reflection]")
{
    auto world = attackWorld();
    auto units = runtimeUnits({ unit(1, 0, 0, 0), unit(2, 1, 20, 0) });
    auto projectile = attack(10, 1, 0, 0);
    projectile.state.operationType = BattleOperationType::TrackingProjectile;
    projectile.state.preferredTargetUnitId = 2;
    projectile.state.requirePreferredTarget = true;
    projectile.state.velocity = { 9, 0, 0 };
    projectile.state.totalFrame = 90;
    projectile.state.through = true;
    projectile.state.track = true;
    projectile.state.bounceRemaining = 3;
    projectile.state.bounceRange = 140;
    projectile.state.bounceChancePct = 75;
    projectile.state.bounceRollPct = 12;
    projectile.state.ignoreProjectileCancel = true;
    projectile.state.projectileCancelDamage = 88;
    projectile.state.projectileCancelWeaken = 17;
    projectile.state.projectilePressurePct = 64;
    projectile.state.strengthPct = 175;
    projectile.acceleration = { 1, 2, 3 };
    projectile.spiralMotion = true;
    projectile.spiralCenter = { 10, 0, 0 };
    projectile.spiralRadius = 1.0f;
    projectile.spiralRadiusGrowth = 0.5f;
    projectile.spiralAngle = 0.0f;
    projectile.spiralAngularVelocity = 0.0f;
    world.addAttack(std::move(projectile));

    const auto events = world.BattleAttackState::tick(
        units,
        world.lifecycle,
        std::pmr::get_default_resource());
    REQUIRE(hasEvent(events, BattleAttackEventType::Hit, 10, 2));
    REQUIRE(world.attacks.front().pendingContact);

    auto settlement = world.settleHit(
        {
            .attackId = 10,
            .targetUnitId = 2,
            .accepted = true,
            .continuation = BattleHitContinuation::Reflected,
        },
        units,
        world.lifecycle);

    REQUIRE(settlement.reflectedProjectile);
    CHECK(settlement.events.empty());
    REQUIRE(world.attacks.size() == 1);
    const auto& source = world.attacks.front();
    CHECK_FALSE(source.pendingContact);
    CHECK(source.noHurt);
    REQUIRE(source.scheduledFinishReason);
    CHECK(*source.scheduledFinishReason == AttackFinishReason::ReflectedAtHit);

    const auto& snapshot = *settlement.reflectedProjectile;
    CHECK(snapshot.payload.delivery.kind() == BattleAttackDeliveryKind::Projectile);
    CHECK(snapshot.payload.payloadClass.kind() == BattleProjectilePayloadKind::Combat);
    CHECK(snapshot.payload.reflectionLineage == BattleAttackReflectionLineageKind::Ordinary);
    CHECK(snapshot.payload.operationType == BattleOperationType::TrackingProjectile);
    CHECK(snapshot.payload.preferredTargetUnitId == 2);
    CHECK(snapshot.payload.requirePreferredTarget);
    CHECK(snapshot.payload.totalFrame == 90);
    CHECK(snapshot.payload.through);
    CHECK(snapshot.payload.track);
    CHECK(snapshot.payload.bounceRemaining == 3);
    CHECK(snapshot.payload.bounceRange == 140);
    CHECK(snapshot.payload.bounceChancePct == 75);
    CHECK(snapshot.payload.bounceRollPct == 12);
    CHECK(snapshot.payload.ignoreProjectileCancel);
    CHECK(snapshot.payload.projectileCancelDamage == 88);
    CHECK(snapshot.payload.projectileCancelWeaken == 17);
    CHECK(snapshot.payload.projectilePressurePct == 64);
    CHECK(snapshot.payload.strengthPct == 175);
    CHECK(snapshot.acceleration.x == Catch::Approx(1.0f));
    CHECK(snapshot.acceleration.y == Catch::Approx(2.0f));
    CHECK(snapshot.acceleration.z == Catch::Approx(3.0f));
    CHECK(snapshot.spiralMotion);
    CHECK(snapshot.spiralCenter.x == Catch::Approx(10.0f));
    CHECK(snapshot.spiralRadius == Catch::Approx(1.5f));
    CHECK(snapshot.spiralRadiusGrowth == Catch::Approx(0.5f));
    CHECK(snapshot.spiralAngle == Catch::Approx(0.0f));
    CHECK(snapshot.spiralAngularVelocity == Catch::Approx(0.0f));
}

TEST_CASE("BattleAttackSystem_ProjectileCancellationRunsAfterHitSettlement",
          "[battle][attack][unit][projectile_reflection][projectile_cancel]")
{
    bool through{};
    auto continuation = BattleHitContinuation::Normal;
    bool expectCancellation{};
    auto expectedFinishReason = AttackFinishReason::SpentOnHit;

    SECTION("普通非貫穿命中先消耗彈道")
    {
    }

    SECTION("普通貫穿命中仍可在同幀互消")
    {
        through = true;
        expectCancellation = true;
        expectedFinishReason = AttackFinishReason::ProjectileCancelled;
    }

    SECTION("反射結算停止原本可貫穿的來襲彈道")
    {
        through = true;
        continuation = BattleHitContinuation::Reflected;
        expectedFinishReason = AttackFinishReason::ReflectedAtHit;
    }

    auto world = attackWorld();
    world.hitRadius = 2.0;
    world.projectileGraceFrames = 0;
    auto units = runtimeUnits({
        unit(1, 0, -100, 0),
        unit(2, 1, 1, 0),
    });

    auto contacting = attack(10, 1, 0, 0);
    contacting.state.operationType = BattleOperationType::RangedProjectile;
    contacting.state.velocity = { 1, 0, 0 };
    contacting.state.through = through;
    contacting.state.projectileCancelDamage = 5;
    world.addAttack(std::move(contacting));

    auto overlapping = attack(11, 2, 0, 0);
    overlapping.state.operationType = BattleOperationType::RangedProjectile;
    overlapping.state.projectileCancelDamage = 20;
    world.addAttack(std::move(overlapping));

    auto events = world.BattleAttackState::tick(
        units,
        world.lifecycle,
        std::pmr::get_default_resource());
    REQUIRE(hasEvent(events, BattleAttackEventType::Hit, 10, 2));
    CHECK_FALSE(hasEvent(events, BattleAttackEventType::ProjectileCancel, 10));

    const auto settlement = world.settleHit(
        {
            .attackId = 10,
            .targetUnitId = 2,
            .accepted = true,
            .continuation = continuation,
        },
        units,
        world.lifecycle);
    CHECK(settlement.reflectedProjectile.has_value()
        == (continuation == BattleHitContinuation::Reflected));

    world.appendProjectileCancelEvents(units, events);
    const auto cancellation = std::ranges::find_if(
        events,
        [](const BattleAttackEvent& event)
        {
            return event.type == BattleAttackEventType::ProjectileCancel;
        });
    CHECK((cancellation != events.end()) == expectCancellation);
    if (cancellation != events.end())
    {
        world.applyProjectileCancelDamage(*cancellation);
    }

    const auto& source = world.attacks.front();
    CHECK(source.id == 10);
    REQUIRE(source.scheduledFinishReason);
    CHECK(*source.scheduledFinishReason == expectedFinishReason);
}

TEST_CASE("BattleAttackSystem_BounceChanceMissConsumesSourceWithoutSpawning", "[battle][attack][unit]")
{
    auto world = attackWorld();
    auto units = runtimeUnits({ unit(1, 0, 0, 0), unit(2, 1, 20, 0), unit(3, 1, 80, 0) });
    auto projectile = attack(10, 1, 0, 0);
    projectile.state.bounceRemaining = 1;
    projectile.state.bounceRange = 120;
    projectile.state.bounceChancePct = 30;
    projectile.state.bounceRollPct = 30;
    world.addAttack(std::move(projectile));

    auto events = world.tick(units);

    REQUIRE(world.attacks.size() == 1);
    CHECK(world.attacks[0].noHurt);
    CHECK(world.attacks[0].state.bounceRemaining == 0);
    CHECK(!hasEvent(events, BattleAttackEventType::Bounce, 10, 3));
}

TEST_CASE("BattleAttackSystem_BounceLastHitReportsChainEnded", "[battle][attack][unit]")
{
    auto world = attackWorld();
    auto units = runtimeUnits({ unit(1, 0, 0, 0), unit(2, 1, 20, 0) });
    auto projectile = attack(10, 1, 0, 0);
    projectile.state.bounceRemaining = 0;
    projectile.state.bounceRange = 120;
    projectile.state.bounceChancePct = 100;
    projectile.state.bounceRollPct = 0;
    world.addAttack(std::move(projectile), {
        .origin = BattleAttackOriginKind::Bounce,
    });

    auto events = world.tick(units);

    CHECK(hasEvent(events, BattleAttackEventType::Hit, 10, 2));
    CHECK(hasEvent(events, BattleAttackEventType::ChainEnded, 10, 2));
}

TEST_CASE("BattleAttackSystem_BounceReportsNoTargetInRangeBeforeChainEnds", "[battle][attack][unit]")
{
    auto world = attackWorld();
    auto units = runtimeUnits({ unit(1, 0, 0, 0), unit(2, 1, 20, 0), unit(3, 1, 260, 0) });
    auto projectile = attack(10, 1, 0, 0);
    projectile.state.bounceRemaining = 2;
    projectile.state.bounceRange = 120;
    projectile.state.bounceChancePct = 100;
    projectile.state.bounceRollPct = 0;
    world.addAttack(std::move(projectile));

    auto events = world.tick(units);

    CHECK(hasEvent(events, BattleAttackEventType::Hit, 10, 2));
    CHECK(!hasEvent(events, BattleAttackEventType::Bounce, 10, 3));
    CHECK(hasEvent(events, BattleAttackEventType::ChainNoTargetInRange, 10, 2));
}

TEST_CASE("BattleAttackSystem_BounceDoesNotSelectPreviouslyHitTarget", "[battle][attack][unit]")
{
    auto world = attackWorld();
    auto units = runtimeUnits({ unit(1, 0, 0, 0), unit(2, 1, 20, 0), unit(3, 1, 80, 0) });
    auto projectile = attack(10, 1, 0, 0);
    projectile.state.bounceRemaining = 2;
    projectile.state.bounceRange = 120;
    projectile.state.bounceChancePct = 100;
    projectile.state.bounceRollPct = 0;
    projectile.hitUnitIds = { 3 };
    world.addAttack(std::move(projectile));

    auto events = world.tick(units);

    CHECK(hasEvent(events, BattleAttackEventType::Hit, 10, 2));
    CHECK(!hasEvent(events, BattleAttackEventType::Bounce, 10, 3));
    CHECK(hasEvent(events, BattleAttackEventType::ChainNoTargetInRange, 10, 2));
}

TEST_CASE("BattleAttackSystem_BounceCannotTriggerWithoutHitEvent", "[battle][attack][unit]")
{
    auto world = attackWorld();
    auto units = runtimeUnits({
        unit(1, 0, 0, 0),
        unit(2, 1, SceneHitRadius + 1.0, 0),
        unit(3, 1, SceneHitRadius + SceneBounceSpawnDistance, 0),
    });
    auto projectile = attack(10, 1, 0, 0);
    projectile.state.bounceRemaining = 1;
    projectile.state.bounceRange = static_cast<int>(SceneTileWidth * 4);
    projectile.state.bounceChancePct = 100;
    projectile.state.bounceRollPct = 0;
    world.addAttack(std::move(projectile));

    auto events = world.tick(units);

    CHECK(!hasEvent(events, BattleAttackEventType::Hit, 10, 2));
    CHECK(!hasEvent(events, BattleAttackEventType::Bounce, 10, 3));
    REQUIRE(world.attacks.size() == 1);
    CHECK(world.attacks[0].state.bounceRemaining == 1);
    CHECK_FALSE(world.attacks[0].noHurt);
}

TEST_CASE("BattleAttackSystem_TrackedSpawnCopiesOneCompletedProvenanceValueToInstanceAndEvents", "[battle][attack][lineage]")
{
    BattleCastLifecycle lifecycle;
    const auto cast = lifecycle.beginRootCast({
        1,
        101,
        true,
        CastOriginKind::Ultimate,
        CastPropagationPolicy::SourceRules,
    });
    const auto reservation = lifecycle.reserveAttack(cast.provenance.castId, {
        .rootAttack = true,
        .mainProjectile = true,
        .sharedHitGroupId = 7,
    });
    auto request = spawnRequest();
    request.provenance = reservation.provenance;
    request.castWork = reservation.work;

    auto world = attackWorld();
    world.nextAttackId = 40;
    const auto spawned = world.spawn(std::move(request), lifecycle);

    CHECK_FALSE(request.provenance.valid());
    CHECK_FALSE(request.castWork.valid());
    REQUIRE(world.attacks.size() == 1);
    const auto& instance = world.attacks[0];
    CHECK(instance.provenance.valid());
    CHECK(instance.provenance.attackId == battleAttackIdFromRuntimeId(40));
    CHECK(instance.provenance.cast.castId == cast.provenance.castId);
    CHECK(instance.provenance.attackOrdinal == 0);
    CHECK(instance.provenance.rootAttack);
    CHECK(instance.provenance.mainProjectile);
    CHECK(instance.provenance.sharedHitGroupId == 7);
    CHECK(instance.castWork.id == reservation.work.id);
    CHECK(spawned.provenance.attackId == instance.provenance.attackId);
    CHECK(spawned.provenance.cast.castId == instance.provenance.cast.castId);
    CHECK(lifecycle.workKind(reservation.work) == CastWorkKind::LiveAttack);
}

TEST_CASE("BattleAttackSystem_BounceReservesDerivedLineageBeforePublishingAttack", "[battle][attack][lineage]")
{
    BattleCastLifecycle lifecycle;
    const auto cast = lifecycle.beginRootCast({ 1, 101, false });
    const auto root = lifecycle.reserveAttack(cast.provenance.castId, {
        .rootAttack = true,
    });
    auto request = spawnRequest();
    request.initial.preferredTargetUnitId = 2;
    request.initial.bounceRemaining = 1;
    request.initial.bounceRange = 120;
    request.initial.bounceChancePct = 100;
    request.initial.bounceRollPct = 0;
    request.provenance = root.provenance;
    request.castWork = root.work;

    auto world = attackWorld();
    const auto spawned = world.spawn(std::move(request), lifecycle);
    world.suppressContacts(spawned.attackId);
    lifecycle.completeWork(cast.commitBarrier);
    auto units = runtimeUnits({
        unit(1, 0, 0, 0),
        unit(2, 1, 20, 0),
        unit(3, 1, 80, 0),
    });

    auto events = world.BattleAttackState::tick(
        units,
        lifecycle,
        std::pmr::get_default_resource());
    std::vector<BattleAttackEvent> hits;
    std::ranges::copy_if(
        events,
        std::back_inserter(hits),
        [](const BattleAttackEvent& event)
        {
            return event.type == BattleAttackEventType::Hit;
        });
    for (const auto& hit : hits)
    {
        auto settled = world.settleHit(
            {
                .attackId = hit.attackId,
                .targetUnitId = hit.unitId,
                .accepted = true,
                .continuation = BattleHitContinuation::Normal,
            },
            units,
            lifecycle);
        events.insert(
            events.end(),
            std::make_move_iterator(settled.events.begin()),
            std::make_move_iterator(settled.events.end()));
    }

    REQUIRE(world.attacks.size() == 2);
    const auto& source = world.attacks[0];
    const auto& bounce = world.attacks[1];
    REQUIRE(source.provenance.valid());
    REQUIRE(bounce.provenance.valid());
    CHECK(source.contactsSuppressed);
    CHECK(bounce.contactsSuppressed);
    REQUIRE(bounce.provenance.parentAttackId);
    CHECK(*bounce.provenance.parentAttackId == source.provenance.attackId);
    CHECK(bounce.provenance.cast.castId == source.provenance.cast.castId);
    CHECK(bounce.provenance.cast.rootCastId == source.provenance.cast.rootCastId);
    CHECK(bounce.provenance.attackOrdinal == 1);
    CHECK_FALSE(bounce.provenance.rootAttack);
    CHECK(bounce.provenance.mainProjectile == source.provenance.mainProjectile);
    CHECK(bounce.provenance.propagation == CastPropagationPolicy::SourceHitRulesOnly);
    CHECK(lifecycle.workKind(bounce.castWork) == CastWorkKind::LiveAttack);
    CHECK(lifecycle.outstandingWork(cast.provenance.castId) == 2);

    const auto bounceEvent = std::find_if(events.begin(), events.end(), [](const auto& event) {
        return event.type == BattleAttackEventType::Bounce;
    });
    REQUIRE(bounceEvent != events.end());
    REQUIRE(bounceEvent->otherProvenance);
    CHECK(bounceEvent->provenance.attackId == source.provenance.attackId);
    CHECK(bounceEvent->otherProvenance->attackId == bounce.provenance.attackId);
}

TEST_CASE("BattleAttackSystem_FinishedAttackCompletesItsLiveWorkAtTheFinishBoundary",
          "[battle][attack][lineage]")
{
    auto world = attackWorld();
    auto projectile = attack(10, 1, 0, 0);
    projectile.state.totalFrame = 1;
    world.addAttack(std::move(projectile));
    const auto castId = world.attacks.front().provenance.cast.castId;
    auto units = runtimeUnits({ unit(1, 0, 0, 0), unit(2, 1, 500, 0) });

    const auto events = world.tick(units);

    CHECK(hasEvent(events, BattleAttackEventType::Expired, 10));
    REQUIRE(world.attacks.front().scheduledFinishReason);
    CHECK(*world.attacks.front().scheduledFinishReason == AttackFinishReason::Expired);
    CHECK_FALSE(world.attacks.front().finishReason);
    CHECK(world.lifecycle.outstandingWork(castId) == 1);

    world.completeFinished(world.lifecycle);

    REQUIRE(world.attacks.front().finishReason);
    CHECK(*world.attacks.front().finishReason == AttackFinishReason::Expired);
    CHECK(world.lifecycle.outstandingWork(castId) == 0);
    const auto& aggregate = world.lifecycle.runtime(castId).aggregate;
    REQUIRE(aggregate.attacksByOrdinal.at(0).finishReason);
    CHECK(*aggregate.attacksByOrdinal.at(0).finishReason == AttackFinishReason::Expired);

    world.eraseFinished();
    CHECK(world.attacks.empty());
}

TEST_CASE("BattleAttackSystem_BattleEndCancelsEveryLiveAttackAndClearsAttackState",
          "[battle][attack][lineage]")
{
    auto world = attackWorld();
    auto first = attack(10, 1, 0, 0);
    auto second = attack(11, 2, 20, 0);
    world.addAttack(std::move(first), { .sharedHitGroupId = 7 });
    world.addAttack(std::move(second));
    const std::vector castIds{
        world.attacks[0].provenance.cast.castId,
        world.attacks[1].provenance.cast.castId,
    };
    world.sharedHitGroupTargets[7] = { 3 };

    world.cancelAllForBattleEnd(world.lifecycle);

    CHECK(world.attacks.empty());
    CHECK(world.sharedHitGroupTargets.empty());
    CHECK(world.lifecycle.trackedWorkCount() == 0);
    for (const auto castId : castIds)
    {
        CHECK(world.lifecycle.outstandingWork(castId) == 0);
        const auto& aggregate = world.lifecycle.runtime(castId).aggregate;
        REQUIRE(aggregate.attacksByOrdinal.at(0).finishReason);
        CHECK(*aggregate.attacksByOrdinal.at(0).finishReason
            == AttackFinishReason::BattleEnded);
    }
}

TEST_CASE("BattleAttackSystem_BattleEndClearsAlreadyFinishedPresentationAttackWithoutCompletingTwice",
          "[battle][attack][lineage]")
{
    auto world = attackWorld();
    auto finished = attack(10, 1, 0, 0);
    finished.state.totalFrame = 1;
    world.addAttack(std::move(finished));
    const auto finishedCastId = world.attacks.front().provenance.cast.castId;
    auto units = runtimeUnits({ unit(1, 0, 0, 0), unit(2, 1, 500, 0) });
    world.tick(units);
    world.completeFinished(world.lifecycle);
    REQUIRE(world.attacks.front().finishReason);
    CHECK_FALSE(world.attacks.front().castWork.valid());

    auto live = attack(11, 1, 0, 0);
    world.addAttack(std::move(live));
    const auto liveCastId = world.attacks.back().provenance.cast.castId;

    world.cancelAllForBattleEnd(world.lifecycle);

    CHECK(world.attacks.empty());
    CHECK(*world.lifecycle.runtime(finishedCastId)
               .aggregate.attacksByOrdinal.at(0).finishReason
        == AttackFinishReason::Expired);
    CHECK(*world.lifecycle.runtime(liveCastId)
               .aggregate.attacksByOrdinal.at(0).finishReason
        == AttackFinishReason::BattleEnded);
}
