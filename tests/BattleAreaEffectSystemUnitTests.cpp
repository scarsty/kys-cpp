#include "battle/BattleAreaEffectSystem.h"
#include "BattleRuntimeRecordTestHelpers.h"
#include "BattleRuntimeStateTestHelpers.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

using namespace KysChess;
using namespace KysChess::Battle;
using namespace KysChess::Battle::Test;

TEST_CASE("BattleAreaEffectSystem_GuardianUsesRangeLifetimeAndStrongestReduction", "[battle][area][ultimate]")
{
    const BattleGridTransform transform{10.0, 64};
    const Pointf center{100, 100, 0};
    auto units = runtimeRecords({
        runtimeUnitSnapshot(0, 0, 100, center),
        runtimeUnitSnapshot(1, 0, 100, {110, 100, 0}),
        runtimeUnitSnapshot(2, 1, 100, center),
        runtimeUnitSnapshot(3, 0, 100, {500, 100, 0}),
        runtimeUnitSnapshot(4, 0, 100, center),
    });
    BattleAreaEffectState state;
    auto request = fixedCircleAreaRequest(0, 0, {1}, center, 10, 100);
    request.modifiers = {{.kind = AreaModifierKind::DamageRedirect,
        .relation = EffectTeamFilter::Ally, .percent = 40}};
    BattleAreaEffectSystem::create(state, request);
    const auto redirect = [&](int id, int frame = 20) {
        return BattleAreaEffectSystem::damageRedirect(state, transform, units, id, frame);
    };
    REQUIRE(redirect(1));
    CHECK(redirect(1)->guardianUnitId == 0);
    CHECK(redirect(1)->reductionPct == 40);
    CHECK_FALSE(redirect(0));
    CHECK_FALSE(redirect(2));
    CHECK_FALSE(redirect(3));
    CHECK_FALSE(redirect(1, 110));
    request.source.ownerUnitId = 4;
    request.modifiers.front().percent = 60;
    BattleAreaEffectSystem::create(state, request);
    CHECK(redirect(1)->guardianUnitId == 4);
    units.requireCore(4).alive = false;
    CHECK(redirect(1)->guardianUnitId == 0);
    units.requireCore(0).alive = false;
    CHECK_FALSE(redirect(1));
}

TEST_CASE("BattleAreaEffectSystem_RefreshReplaceAndIndependentMergesHaveStableIds", "[battle][area]")
{
    BattleAreaEffectState state;
    const Pointf firstCenter{ 100.0f, 100.0f, 0.0f };
    const Pointf refreshedCenter{ 240.0f, 180.0f, 0.0f };

    const auto created = BattleAreaEffectSystem::create(
        state,
        fixedCircleAreaRequest(0, 0, { 10 }, firstCenter, 10, 100));
    REQUIRE(created.events.size() == 1);
    CHECK(created.events[0].type == BattleAreaLifecycleEventType::Created);

    const auto refreshed = BattleAreaEffectSystem::create(
        state,
        fixedCircleAreaRequest(0, 0, { 10 }, refreshedCenter, 25, 100));
    CHECK(refreshed.areaId == created.areaId);
    REQUIRE(refreshed.events.size() == 1);
    CHECK(refreshed.events[0].type == BattleAreaLifecycleEventType::Refreshed);
    REQUIRE(state.areas.size() == 1);
    CHECK(state.areas[0].createdFrame == 10);
    CHECK(state.areas[0].expiresFrameExclusive == 125);
    CHECK(state.areas[0].anchor.fixedPosition.x == refreshedCenter.x);
    CHECK(state.areas[0].anchor.fixedPosition.y == refreshedCenter.y);

    const auto otherSource = BattleAreaEffectSystem::create(
        state,
        fixedCircleAreaRequest(1, 0, { 10 }, firstCenter, 25, 100));
    CHECK(otherSource.areaId != created.areaId);
    CHECK(state.areas.size() == 2);

    const auto firstReplacement = BattleAreaEffectSystem::create(
        state,
        fixedCircleAreaRequest(
            0,
            0,
            { 20 },
            firstCenter,
            30,
            20,
            AreaMergePolicy::ReplaceSameSource));
    const auto secondReplacement = BattleAreaEffectSystem::create(
        state,
        fixedCircleAreaRequest(
            0,
            0,
            { 20 },
            refreshedCenter,
            31,
            20,
            AreaMergePolicy::ReplaceSameSource));
    CHECK(secondReplacement.areaId != firstReplacement.areaId);
    REQUIRE(secondReplacement.events.size() == 2);
    CHECK(secondReplacement.events[0].type == BattleAreaLifecycleEventType::Removed);
    CHECK(secondReplacement.events[0].areaId == firstReplacement.areaId);
    CHECK(secondReplacement.events[0].removalReason == BattleAreaRemovalReason::Replaced);
    CHECK(secondReplacement.events[1].type == BattleAreaLifecycleEventType::Created);

    auto independent = fixedCircleAreaRequest(
        0,
        0,
        { 30 },
        firstCenter,
        40,
        20,
        AreaMergePolicy::Independent);
    const auto firstIndependent = BattleAreaEffectSystem::create(state, independent);
    const auto secondIndependent = BattleAreaEffectSystem::create(state, std::move(independent));
    CHECK(firstIndependent.areaId != secondIndependent.areaId);
}

TEST_CASE("BattleAreaEffectSystem_CircleAndGridSquareIncludeTheirBoundaries", "[battle][area][geometry]")
{
    const BattleGridTransform transform{ 10.0, 64 };
    const Pointf center = areaGridWorldPosition(transform, 10, 10);
    auto units = runtimeRecords({
        runtimeUnitSnapshot(0, 0, 100, center),
        runtimeUnitSnapshot(1, 1, 100, { center.x + 60.0f, center.y, 0.0f }),
        runtimeUnitSnapshot(2, 1, 100, { center.x + 60.1f, center.y, 0.0f }),
        runtimeUnitSnapshot(3, 1, 100, areaGridWorldPosition(transform, 12, 12)),
        runtimeUnitSnapshot(4, 1, 100, areaGridWorldPosition(transform, 13, 12)),
    });

    BattleAreaEffectState circleState;
    BattleAreaEffectSystem::create(
        circleState,
        fixedCircleAreaRequest(0, 0, { 1 }, center, 10, 100));
    const auto& circle = circleState.areas.front();
    CHECK(BattleAreaEffectSystem::containsUnit(circle, transform, units, 0, 10));
    CHECK(BattleAreaEffectSystem::containsUnit(circle, transform, units, 1, 109));
    CHECK_FALSE(BattleAreaEffectSystem::containsUnit(circle, transform, units, 2, 109));
    CHECK_FALSE(BattleAreaEffectSystem::containsUnit(circle, transform, units, 1, 110));
    CHECK(BattleAreaEffectSystem::removeExpired(circleState, 109).empty());
    CHECK(BattleAreaEffectSystem::removeExpired(circleState, 110).size() == 1);
    CHECK(circleState.areas.empty());

    BattleAreaEffectState squareState;
    auto squareRequest = fixedCircleAreaRequest(0, 0, { 2 }, center, 0, 10);
    squareRequest.geometry = { AreaShape::GridSquare, 0, 5 };
    BattleAreaEffectSystem::create(squareState, std::move(squareRequest));
    const auto& square = squareState.areas.front();
    CHECK(BattleAreaEffectSystem::containsUnit(square, transform, units, 3, 0));
    CHECK_FALSE(BattleAreaEffectSystem::containsUnit(square, transform, units, 4, 0));
}

TEST_CASE("BattleAreaEffectSystem_FollowAnchorAndSourceDeathPoliciesStayLive", "[battle][area]")
{
    const BattleGridTransform transform{ 10.0, 64 };
    const Pointf firstCenter = areaGridWorldPosition(transform, 10, 10);
    const Pointf movedCenter = areaGridWorldPosition(transform, 30, 30);
    auto units = runtimeRecords({
        runtimeUnitSnapshot(0, 0, 100, firstCenter),
        runtimeUnitSnapshot(1, 0, 100, firstCenter),
    });

    BattleAreaEffectState state;
    auto followRequest = fixedCircleAreaRequest(0, 0, { 1 }, firstCenter, 0, 100);
    followRequest.anchor = { BattleAreaAnchorKind::FollowSourceUnit, {}, 0 };
    followRequest.sourceDeath = AreaSourceDeathPolicy::RemoveImmediately;
    const auto follow = BattleAreaEffectSystem::create(state, std::move(followRequest));

    const auto persistent = BattleAreaEffectSystem::create(
        state,
        fixedCircleAreaRequest(0, 0, { 2 }, firstCenter, 0, 100));
    CHECK(BattleAreaEffectSystem::containsUnit(state.areas[0], transform, units, 1, 0));

    units.setPosition(0, movedCenter, transform);
    CHECK(BattleAreaEffectSystem::center(state.areas[0], units).x == movedCenter.x);
    CHECK(BattleAreaEffectSystem::center(state.areas[0], units).y == movedCenter.y);
    CHECK_FALSE(BattleAreaEffectSystem::containsUnit(state.areas[0], transform, units, 1, 0));

    const auto removed = BattleAreaEffectSystem::removeForSourceDeath(state, 0);
    REQUIRE(removed.size() == 1);
    CHECK(removed[0].areaId == follow.areaId);
    REQUIRE(state.areas.size() == 1);
    CHECK(state.areas[0].id == persistent.areaId);
}

TEST_CASE("BattleAreaEffectSystem_QueriesSortByIdAndApplyStrongestOrSemantics", "[battle][area]")
{
    const BattleGridTransform transform{ 10.0, 64 };
    const Pointf center = areaGridWorldPosition(transform, 10, 10);
    auto units = runtimeRecords({
        runtimeUnitSnapshot(0, 0, 100, center),
        runtimeUnitSnapshot(1, 0, 100, center),
        runtimeUnitSnapshot(2, 0, 100, center),
        runtimeUnitSnapshot(3, 1, 100, center),
    });

    const auto modifiers = [](int blockPct, int outgoingPct, int projectilePct)
    {
        AreaModifier block;
        block.kind = AreaModifierKind::Attribute;
        block.relation = EffectTeamFilter::Ally;
        block.attribute = BattleAttribute::BlockChance;
        block.amount.flat = blockPct;
        block.overlap = AreaOverlapPolicy::KeepStrongest;

        AreaModifier outgoing;
        outgoing.kind = AreaModifierKind::OutgoingDamage;
        outgoing.relation = EffectTeamFilter::Enemy;
        outgoing.percent = outgoingPct;
        outgoing.overlap = AreaOverlapPolicy::KeepStrongest;

        AreaModifier tracking;
        tracking.kind = AreaModifierKind::AttackSpawn;
        tracking.relation = EffectTeamFilter::Enemy;
        tracking.tracking = false;
        tracking.overlap = AreaOverlapPolicy::Any;
        tracking.trackingOverlap = AreaOverlapPolicy::Any;

        AreaModifier projectileSpeed;
        projectileSpeed.kind = AreaModifierKind::AttackSpawn;
        projectileSpeed.relation = EffectTeamFilter::Enemy;
        projectileSpeed.speedPct = projectilePct;
        projectileSpeed.overlap = AreaOverlapPolicy::KeepStrongest;
        projectileSpeed.speedOverlap = AreaOverlapPolicy::KeepStrongest;

        AreaModifier projectilePressure;
        projectilePressure.kind = AreaModifierKind::AttackSpawn;
        projectilePressure.relation = EffectTeamFilter::Enemy;
        projectilePressure.projectilePressurePct = projectilePct;
        projectilePressure.overlap = AreaOverlapPolicy::KeepStrongest;
        projectilePressure.projectilePressureOverlap = AreaOverlapPolicy::KeepStrongest;

        AreaModifier forceMove;
        forceMove.kind = AreaModifierKind::ForcedMoveImmunity;
        forceMove.relation = EffectTeamFilter::Ally;
        forceMove.blockedDirection = ForceMoveDirection::AwayFromSource;
        forceMove.overlap = AreaOverlapPolicy::Any;
        return std::vector{
            block,
            outgoing,
            tracking,
            projectileSpeed,
            projectilePressure,
            forceMove,
        };
    };

    BattleAreaEffectState state;
    auto strongest = fixedCircleAreaRequest(0, 0, { 1 }, center, 0, 100, AreaMergePolicy::Independent);
    strongest.modifiers = modifiers(25, -20, 65);
    const auto strongestArea = BattleAreaEffectSystem::create(state, std::move(strongest));
    auto weaker = fixedCircleAreaRequest(1, 0, { 2 }, center, 0, 100, AreaMergePolicy::Independent);
    weaker.modifiers = modifiers(15, -10, 80);
    const auto weakerArea = BattleAreaEffectSystem::create(state, std::move(weaker));

    std::ranges::reverse(state.areas);
    const auto containing = BattleAreaEffectSystem::areasContainingUnit(
        state,
        transform,
        units,
        2,
        0,
        BattleAreaQueryPhase::UnitAttribute);
    REQUIRE(containing.size() == 2);
    CHECK(containing[0].id == strongestArea.areaId);
    CHECK(containing[1].id == weakerArea.areaId);

    const auto block = BattleAreaEffectSystem::collectAreaUnitModifiers(
        state,
        transform,
        units,
        2,
        0,
        BattleAreaQueryPhase::UnitAttribute);
    REQUIRE(block.modifiers.size() == 1);
    CHECK(block.modifiers[0].areaId == strongestArea.areaId);
    CHECK(block.modifiers[0].modifier.amount.flat == 25);

    const auto outgoing = BattleAreaEffectSystem::collectAreaUnitModifiers(
        state,
        transform,
        units,
        3,
        0,
        BattleAreaQueryPhase::OutgoingDamage);
    REQUIRE(outgoing.modifiers.size() == 1);
    CHECK(outgoing.modifiers[0].areaId == strongestArea.areaId);
    CHECK(outgoing.modifiers[0].modifier.percent == -20);

    const auto attackSpawn = BattleAreaEffectSystem::collectAreaAttackSpawnModifiers(
        state,
        transform,
        units,
        3,
        0);
    REQUIRE(attackSpawn.tracking.has_value());
    CHECK_FALSE(*attackSpawn.tracking);
    CHECK(attackSpawn.speedPct == 65);
    CHECK(attackSpawn.projectilePressurePct == 65);
    CHECK(BattleAreaEffectSystem::blocksForcedMovement(
        state,
        transform,
        units,
        2,
        0,
        ForceMoveDirection::AwayFromSource));
    CHECK_FALSE(BattleAreaEffectSystem::blocksForcedMovement(
        state,
        transform,
        units,
        2,
        0,
        ForceMoveDirection::TowardSource));
}
