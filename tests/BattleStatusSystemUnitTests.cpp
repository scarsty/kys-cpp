#include "battle/BattleStatusSystem.h"
#include "ChessBattleEffectTypes.h"
#include "BattleCoreTestHelpers.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess;
using namespace KysChess::Battle;
using namespace KysChess::Battle::Test;
using namespace BattlePresentationTest;

TEST_CASE("BattleStatusSystem_CopiesStatusEffectsAsACluster", "[battle][status]")
{
    BattleStatusUnitState source;
    source.id = 7;
    source.effects.statuses = {
        {
            .kind = BattleStatusKind::Poison,
            .sourceUnitId = 3,
            .remainingFrames = 9,
            .stacks = 2,
            .potency = 5,
        },
        { .kind = BattleStatusKind::Bleed, .stacks = 2 },
        { .kind = BattleStatusKind::Stun, .remainingFrames = 4 },
        { .kind = BattleStatusKind::MpBlocked, .remainingFrames = 6 },
    };

    auto runtime = makeBattleStatusRuntimeUnit(source);

    CHECK(runtime.effects == source.effects);
}

TEST_CASE("BattleFrameRunner_NextAttackMissIsConsumedFromTheDefender", "[battle][core][status][suppression]")
{
    auto state = attackSuppressionFrameState();
    addAttackSuppressionStatus(state, 0, BattleStatusKind::NextAttackMiss);
    addAttackSuppressionStatus(state, 1, BattleStatusKind::NextAttackMiss);
    const auto spawned = spawnTrackedAttack(state, attackSuppressionRequest());

    advanceUntilAttackContacts(state, spawned.attackId, 2);

    CHECK(state.units.requireCore(1).vitals.hp == 100);
    CHECK(state.units.requireCore(2).vitals.hp == 75);
    CHECK(hasAttackSuppressionStatus(state, 0, BattleStatusKind::NextAttackMiss));
    CHECK_FALSE(hasAttackSuppressionStatus(state, 1, BattleStatusKind::NextAttackMiss));
    CHECK_FALSE(state.attacks.contactsSuppressed(spawned.attackId));
    CHECK(requireById(state.attacks.attacks, spawned.attackId).hitUnitIds
          == std::vector<int>{ 1, 2 });
}

TEST_CASE("BattleFrameRunner_BlindedSuppressesEveryContactOfOneAttack", "[battle][core][status][suppression]")
{
    auto state = attackSuppressionFrameState();
    addAttackContactShieldRule(state, 9);
    addAttackSuppressionStatus(state, 0, BattleStatusKind::Blinded);
    const auto blindedAttack = spawnTrackedAttack(state, attackSuppressionRequest());

    advanceUntilAttackContacts(state, blindedAttack.attackId, 2);

    CHECK(state.units.requireCore(1).vitals.hp == 100);
    CHECK(state.units.requireCore(2).vitals.hp == 100);
    CHECK(state.units.requireCore(1).shield == 0);
    CHECK(state.units.requireCore(2).shield == 0);
    CHECK_FALSE(hasAttackSuppressionStatus(state, 0, BattleStatusKind::Blinded));
    CHECK(state.attacks.contactsSuppressed(blindedAttack.attackId));

    auto nextRequest = attackSuppressionRequest();
    nextRequest.initial.preferredTargetUnitId = 1;
    nextRequest.initial.through = false;
    nextRequest.initial.position = state.units.requireCore(1).motion.position;
    nextRequest.initial.velocity = {};
    const auto nextAttack = spawnTrackedAttack(state, std::move(nextRequest));
    runBattleFrame(state);

    CHECK(state.units.requireCore(1).vitals.hp == 84);
    CHECK(state.units.requireCore(1).shield == 0);
    CHECK_FALSE(state.attacks.contactsSuppressed(nextAttack.attackId));
}

TEST_CASE("BattleFrameRunner_NeutralizeForceSuppressesTheAttackAndShieldsItsOriginalTargetOnce", "[battle][core][status][suppression]")
{
    auto state = attackSuppressionFrameState();
    addAttackContactShieldRule(state, 9);
    addAttackSuppressionStatus(state, 0, BattleStatusKind::NeutralizeForce, 37);
    const auto tracked = spawnTrackedAttack(
        state,
        attackSuppressionRequest(),
        2);

    advanceUntilAttackContacts(state, tracked.attackId, 1);

    CHECK(state.units.requireCore(1).shield == 0);
    CHECK(state.units.requireCore(2).shield == 37);
    CHECK_FALSE(hasAttackSuppressionStatus(state, 0, BattleStatusKind::NeutralizeForce));
    CHECK(state.attacks.contactsSuppressed(tracked.attackId));

    advanceUntilAttackContacts(state, tracked.attackId, 2);

    CHECK(state.units.requireCore(1).vitals.hp == 100);
    CHECK(state.units.requireCore(2).vitals.hp == 100);
    CHECK(state.units.requireCore(1).shield == 0);
    CHECK(state.units.requireCore(2).shield == 37);
    CHECK(state.castLifecycle.runtime(tracked.castId).aggregate.distinctHitUnitIds.empty());
}

TEST_CASE("BattleFrameRunner_SourceSuppressionCoversDelayedSiblingAttacksUntilCastSettles", "[battle][core][status][suppression][cast]")
{
    auto state = attackSuppressionFrameState();
    addAttackContactShieldRule(state, 9);
    addAttackSuppressionStatus(state, 0, BattleStatusKind::NeutralizeForce, 37);

    auto firstRequest = attackSuppressionRequest();
    firstRequest.initial.preferredTargetUnitId = 1;
    firstRequest.initial.requirePreferredTarget = true;
    firstRequest.initial.through = false;
    firstRequest.initial.totalFrame = 2;
    auto siblingRequest = attackSuppressionRequest();
    siblingRequest.initial.preferredTargetUnitId = 2;
    siblingRequest.initial.requirePreferredTarget = true;
    siblingRequest.initial.through = false;
    siblingRequest.initial.totalFrame = 2;
    siblingRequest.initial.position = { 120, 100, 0 };

    std::vector<BattleAttackSpawnRequest> requests;
    requests.push_back(std::move(firstRequest));
    requests.push_back(std::move(siblingRequest));
    auto tracked = reserveTrackedAttackCast(state, std::move(requests), 2);
    const auto firstSpawned = state.attacks.spawn(
        std::move(tracked.requests[0]),
        state.castLifecycle);

    runBattleFrame(state);

    CHECK(state.units.requireCore(1).vitals.hp == 100);
    CHECK(state.units.requireCore(1).shield == 0);
    CHECK(state.units.requireCore(2).shield == 37);
    CHECK_FALSE(hasAttackSuppressionStatus(
        state,
        0,
        BattleStatusKind::NeutralizeForce));
    CHECK(state.attacks.castContactsSuppressed(tracked.castId));
    CHECK(state.attacks.contactsSuppressed(firstSpawned.attackId));

    const auto siblingSpawned = state.attacks.spawn(
        std::move(tracked.requests[1]),
        state.castLifecycle);
    CHECK(state.attacks.contactsSuppressed(siblingSpawned.attackId));

    runBattleFrame(state);

    CHECK(state.units.requireCore(2).vitals.hp == 100);
    CHECK(state.units.requireCore(2).shield == 37);
    CHECK(state.castLifecycle.runtime(tracked.castId)
          .aggregate.distinctHitUnitIds.empty());

    runBattleFrame(state);

    CHECK_FALSE(state.attacks.castContactsSuppressed(tracked.castId));

    auto nextRequest = attackSuppressionRequest();
    nextRequest.initial.preferredTargetUnitId = 1;
    nextRequest.initial.requirePreferredTarget = true;
    nextRequest.initial.through = false;
    const auto nextAttack = spawnTrackedAttack(state, std::move(nextRequest));
    runBattleFrame(state);

    CHECK_FALSE(state.attacks.contactsSuppressed(nextAttack.attackId));
    CHECK(state.units.requireCore(1).vitals.hp == 84);
    CHECK(state.units.requireCore(1).shield == 0);
}

TEST_CASE("BattleFrameRunner_NoContactCancelledCastLeavesSourceSuppressionForNextHit", "[battle][core][status][suppression][cast]")
{
    auto state = attackSuppressionFrameState();
    addAttackSuppressionStatus(state, 0, BattleStatusKind::Blinded);
    const auto cancelled = state.castLifecycle.beginRootCast({
        .sourceUnitId = 0,
        .magicId = 101,
    });

    state.castLifecycle.cancelPlannedCast(cancelled, state.movement.frame);

    CHECK(hasAttackSuppressionStatus(state, 0, BattleStatusKind::Blinded));
    CHECK_FALSE(state.attacks.castContactsSuppressed(
        cancelled.provenance.castId));

    auto request = attackSuppressionRequest();
    request.initial.preferredTargetUnitId = 1;
    request.initial.requirePreferredTarget = true;
    request.initial.through = false;
    const auto tracked = spawnTrackedAttack(state, std::move(request));
    runBattleFrame(state);

    CHECK(state.units.requireCore(1).vitals.hp == 100);
    CHECK_FALSE(hasAttackSuppressionStatus(state, 0, BattleStatusKind::Blinded));
    CHECK(state.attacks.castContactsSuppressed(tracked.castId));
    CHECK(state.attacks.contactsSuppressed(tracked.attackId));
}

TEST_CASE("BattleFrameRunner_NeutralizeForceShieldGrantSaturates", "[battle][core][status][suppression]")
{
    auto state = attackSuppressionFrameState();
    state.units.requireCore(2).shield = std::numeric_limits<int>::max() - 10;
    addAttackSuppressionStatus(state, 0, BattleStatusKind::NeutralizeForce, 37);
    const auto tracked = spawnTrackedAttack(
        state,
        attackSuppressionRequest(),
        2);

    advanceUntilAttackContacts(state, tracked.attackId, 1);

    CHECK(state.units.requireCore(2).shield == std::numeric_limits<int>::max());
    CHECK_FALSE(hasAttackSuppressionStatus(state, 0, BattleStatusKind::NeutralizeForce));
    CHECK(state.attacks.contactsSuppressed(tracked.attackId));
}

TEST_CASE("BattleFrameRunner_SourceSuppressorsApplyTogetherToTheSameAttack", "[battle][core][status][suppression]")
{
    const auto verify = [](bool neutralizeFirst)
    {
        auto state = attackSuppressionFrameState();
        if (neutralizeFirst)
        {
            addAttackSuppressionStatus(state, 0, BattleStatusKind::NeutralizeForce, 37);
            addAttackSuppressionStatus(state, 0, BattleStatusKind::Blinded);
        }
        else
        {
            addAttackSuppressionStatus(state, 0, BattleStatusKind::Blinded);
            addAttackSuppressionStatus(state, 0, BattleStatusKind::NeutralizeForce, 37);
        }
        addAttackSuppressionStatus(state, 1, BattleStatusKind::NextAttackMiss);
        const auto tracked = spawnTrackedAttack(
            state,
            attackSuppressionRequest(),
            2);

        advanceUntilAttackContacts(state, tracked.attackId, 2);

        CHECK(state.units.requireCore(1).vitals.hp == 100);
        CHECK(state.units.requireCore(2).vitals.hp == 100);
        CHECK(state.units.requireCore(1).shield == 0);
        CHECK(state.units.requireCore(2).shield == 37);
        CHECK_FALSE(hasAttackSuppressionStatus(state, 0, BattleStatusKind::Blinded));
        CHECK_FALSE(hasAttackSuppressionStatus(state, 0, BattleStatusKind::NeutralizeForce));
        CHECK(hasAttackSuppressionStatus(state, 1, BattleStatusKind::NextAttackMiss));
        CHECK(state.attacks.contactsSuppressed(tracked.attackId));
    };

    SECTION("先套用化勁")
    {
        verify(true);
    }
    SECTION("先套用刺目")
    {
        verify(false);
    }
}

TEST_CASE("BattleFrameRunner_SourceSuppressionPropagatesThroughBounceDescendants", "[battle][core][status][suppression][bounce]")
{
    auto state = attackSuppressionFrameState();
    addAttackSuppressionStatus(state, 0, BattleStatusKind::Blinded);
    auto request = attackSuppressionRequest();
    request.initial.preferredTargetUnitId = 1;
    request.initial.requirePreferredTarget = true;
    request.initial.through = false;
    request.initial.bounceRemaining = 1;
    request.initial.bounceRange = 120;
    request.initial.bounceChancePct = 100;
    request.initial.bounceRollPct = 0;
    const auto source = spawnTrackedAttack(state, std::move(request));

    advanceUntilAttackContacts(state, source.attackId, 1);
    const auto bounce = std::ranges::find_if(
        state.attacks.attacks,
        [&source](const BattleAttackInstance& attack)
        {
            return attack.provenance.parentAttackId
                == battleAttackIdFromRuntimeId(source.attackId);
        });
    REQUIRE(bounce != state.attacks.attacks.end());
    const int bounceAttackId = bounce->id;
    CHECK(state.attacks.contactsSuppressed(source.attackId));
    CHECK(state.attacks.contactsSuppressed(bounceAttackId));

    advanceUntilAttackContacts(state, bounceAttackId, 1);

    CHECK(state.units.requireCore(1).vitals.hp == 100);
    CHECK(state.units.requireCore(2).vitals.hp == 100);
    CHECK_FALSE(hasAttackSuppressionStatus(state, 0, BattleStatusKind::Blinded));
    CHECK(state.attacks.contactsSuppressed(bounceAttackId));
}
