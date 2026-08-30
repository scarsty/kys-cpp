#include "battle/BattleCore.h"
#include "battle/BattleAreaEffectSystem.h"
#include "battle/BattleLogSegments.h"
#include "battle/BattleMovement.h"
#include "battle/BattleRuntimeSession.h"
#include "battle/BattleRuntimeRules.h"
#include "battle/BattleRuntimeUnitSpawn.h"
#include "ChessEftIds.h"
#include "ChessCombo.h"
#include "Find.h"
#include "BattleLogTestHelpers.h"
#include "BattleMovementTestHelpers.h"
#include "BattleRuntimeStateTestHelpers.h"
#include "BattlePresentationTestHelpers.h"
#include "BattleRuntimeRecordTestHelpers.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <filesystem>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "BattleCoreTestHelpers.h"

using namespace KysChess::Battle;
using namespace KysChess::Battle::Test;
using namespace KysChess;
using namespace BattlePresentationTest;

template <class T, class = void>
struct HasUnitIdMember : std::false_type
{
};

template <class T>
struct HasUnitIdMember<T, std::void_t<decltype(std::declval<T&>().unitId)>> : std::true_type
{
};

static_assert(!HasUnitIdMember<BattleRescueUnitRuntime>::value);


TEST_CASE("BattleFrameRunner_RoutesMovementPhysicsThroughRuntimeUnits", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({ unit(0, 0, { 100, 100, 0 }) }));
    state.attacks = attackWorld();
    state.gridTransform = { SceneTileWidth, 64 };
    state.units.requireCore(0).motion.velocity = { 5, 0, 0 };
    state.units.requireCore(0).motion.acceleration = { 0, 0, 0 };
    state.units.require(0).movement.physics.position = { 100, 100, 0 };
    state.units.require(0).movement.physics.velocity = { 5, 0, 0 };
    state.units.require(0).movement.physics.acceleration = { 0, 0, 0 };
    state.units.require(0).movement.physics.movementDashFrames = 1;
    state.movementPhysics.config.gravity = 0.0f;
    state.movementPhysics.config.friction = 0.0f;
    state.movementPhysics.config.postDashSpreadFrames = 6;
    state.movementPhysics.terrain.tileWidth = 100.0;
    state.movementPhysics.terrain.coordCount = 2;
    state.movementPhysics.terrain.defaultSeparationDistance = SceneTileWidth;
    state.movementPhysics.terrain.walkableByCell.assign(2 * 2, 1);

    runBattleFrame(state);

    const auto& unit = state.units.requireCore(0);
    CHECK(unit.motion.position.x == 105.0f);
    CHECK(unit.motion.velocity.x == 5.0f);
    CHECK(unit.motion.acceleration.x == 0.0f);
    CHECK(state.units.require(0).movement.physics.movementDashFrames == 0);
    CHECK(state.units.require(0).movement.physics.movementDashSpreadFrames == 6);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_CommitsMovementBeforeProjectileEvents", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 600, 100, 0 }),
    }));
    state.attacks = attackWorld();

    BattleAttackInstance projectile{ ordinaryProjectilePayload() };
    projectile.id = 10;
    projectile.state.attackSourceUnitId = 0;
    projectile.state.preferredTargetUnitId = 0;
    projectile.state.totalFrame = 30;
    projectile.state.visualEffectId = 33;
    projectile.state.operationType = BattleOperationType::RangedProjectile;
    projectile.state.position = { 0, 0, 0 };
    projectile.state.velocity = { 5, 0, 0 };

    seedRuntimeUnitsFromWorld(state);
    appendTrackedAttack(state, std::move(projectile));

    auto result = runBattleFrame(state);

    CHECK(state.movement.frame == 1);
    REQUIRE(result.logEvents.empty());
    REQUIRE(!result.visualEvents.empty());
    CHECK(result.frame == 1);
    const auto& firstProjectileEvent = result.visualEvents[0];
    CHECK(firstProjectileEvent.type == BattleVisualEventType::ProjectileMoved);
    CHECK(firstProjectileEvent.effectId == 10);
    CHECK(firstProjectileEvent.sourceUnitId == 0);
    CHECK(firstProjectileEvent.targetUnitId == 0);
    CHECK(firstProjectileEvent.durationFrames == 30);
    CHECK(firstProjectileEvent.visualEffectId == 33);
    CHECK(firstProjectileEvent.position.x == 5.0f);
    CHECK(firstProjectileEvent.velocity.x == 5.0f);
    CHECK(firstProjectileEvent.operationKind == 2);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_UsesGroupedRuntimeUnitState", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 600, 100, 0 }),
    }));
    state.attacks = attackWorld();

    auto grouped = runtimeUnitSnapshot(0, 0, 80, { 100, 100, 0 });
    grouped.vitals = { 80, 120, 4, 50 };
    grouped.motion = { { 100, 100, 0 }, { 7, 8, 0 }, { 0, 0, 0 }, { 1, 0, 0 } };
    grouped.animation = { 2, 60, 5, 13 };
    grouped.operationType = BattleOperationType::Dash;
    grouped.haveAction = true;

    auto target = runtimeUnitSnapshot(1, 1, 100, { 600, 100, 0 });
    seedRuntimeUnits(state, { grouped, target });
    state.units.require(0).status = statusRuntimeSnapshot(0, 80);
    state.units.require(1).status = statusRuntimeSnapshot(1, 100);

    runBattleFrame(state);

    const auto& updated = state.units.requireCore(0);
    CHECK(updated.animation.cooldown == 1);
    CHECK(updated.animation.cooldownMax == 60);
    CHECK(updated.animation.actFrame == 6);
    CHECK(updated.animation.actType == 13);
    CHECK(updated.vitals.mp == 5);
    CHECK(updated.motion.velocity.x == 7.0f);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_RecordsPendingAttackSpawnRequest", "[battle][core][presentation]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({}));
    state.attacks = attackWorld();
    state.attacks.hitRadius = 10.0;
    state.attacks.nextAttackId = 50;
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 0, 0, 0 }),
        runtimeUnitSnapshot(1, 1, 100, { 106, 120, 0 }),
    });
    queueTrackedAttack(state, attackSpawnRequest());

    auto result = runBattleFrame(state);

    CHECK(state.nextFrame.queuedAttacks().empty());
    REQUIRE(state.attacks.attacks.size() == 1);
    CHECK(state.attacks.attacks[0].id == 50);
    CHECK(state.attacks.attacks[0].state.position.x == 106.0f);
    CHECK(state.attacks.nextAttackId == 51);

    REQUIRE(result.gameplayEvents.size() >= 3);
    auto gameplayIt = std::find_if(
        result.gameplayEvents.begin(),
        result.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::AttackSpawned
                && event.effectId == 50;
        });
    REQUIRE(gameplayIt != result.gameplayEvents.end());
    CHECK(gameplayIt->sourceUnitId == 0);
    CHECK(gameplayIt->targetUnitId == 0);
    CHECK(gameplayIt->position.x == 100.0f);
    CHECK(gameplayIt->position.y == 120.0f);

    const auto* presentation = findVisualEvent(result, BattleVisualEventType::ProjectileSpawned, 50);
    REQUIRE(presentation);
    CHECK(presentation->sourceUnitId == 0);
    CHECK(presentation->targetUnitId == 0);
    CHECK(presentation->durationFrames == 30);
    CHECK(presentation->visualEffectId == 44);
    CHECK(presentation->position.x == 100.0f);
    CHECK(presentation->position.y == 120.0f);
    CHECK(presentation->velocity.x == 6.0f);
    CHECK(presentation->operationKind == 2);

    const auto* moved = findVisualEvent(result, BattleVisualEventType::ProjectileMoved, 50);
    REQUIRE(moved);
    CHECK(moved->position.x == 106.0f);
    REQUIRE(findVisualEvent(result, BattleVisualEventType::ProjectileHit, 50));
    auto hitGameplayIt = std::find_if(
        result.gameplayEvents.begin(),
        result.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::ProjectileHit
                && event.targetUnitId == 1;
        });
    CHECK(hitGameplayIt != result.gameplayEvents.end());

    CHECK(presentation->effectId == 50);
    CHECK(presentation->visualEffectId == 44);
    CHECK(presentation->position.x == 100.0f);
    CHECK(presentation->velocity.x == 6.0f);
    CHECK(presentation->durationFrames == 30);
    CHECK(presentation->operationKind == 2);
}


TEST_CASE("BattleFrameRunner_AdvanceFrame_ClearsRecoveredActionFrameUnitState", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    auto& unit = state.units.requireCore(0);
    unit.haveAction = true;
    unit.animation.actFrame = 11;
    unit.animation.actType = 2;
    unit.operationType = BattleOperationType::RangedProjectile;
    unit.animation.cooldown = 30;

    runBattleFrame(state);

    const auto& recovered = state.units.requireCore(0);
    CHECK(recovered.animation.actFrame == 12);
    CHECK_FALSE(recovered.haveAction);
    CHECK(recovered.animation.actType == -1);
    CHECK(recovered.operationType == BattleOperationType::None);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DeadUnitActionCleanupClearsAllActionOwners", "[battle][core][ownership]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 120, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 10, { 120, 100, 0 }),
});
    auto& deadBefore = state.units.requireCore(1);
    deadBefore.animation.cooldown = 4;
    deadBefore.animation.cooldownMax = 12;
    deadBefore.animation.actFrame = 2;
    deadBefore.animation.actType = 3;
    deadBefore.operationType = BattleOperationType::Melee;
    deadBefore.haveAction = true;
    BattlePendingCastAction pending;
    pending.castFrame = 6;
    setTrackedPendingCast(state, 1, std::move(pending));
    state.units.require(1).markUltimateCaster();
    queuePendingDamage(state, lethalDamageInput(0, 1));

    runBattleFrame(state);

    const auto& dead = state.units.requireCore(1);
    CHECK(dead.animation.cooldown == 0);
    CHECK(dead.animation.cooldownMax == 0);
    CHECK(dead.animation.actFrame == 0);
    CHECK(dead.animation.actType == -1);
    CHECK(dead.operationType == BattleOperationType::None);
    CHECK_FALSE(dead.haveAction);
    CHECK(state.units.require(1).pendingCast() == nullptr);
    CHECK_FALSE(state.units.require(1).isUltimateCaster());
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DamageDeathPrecedesBattleEndEvent", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 210, 100, 0 }),
    }));
    state.attacks = attackWorld();
    state.units.require(0).status = statusRuntimeSnapshot(0, 100);
    state.units.require(1).status = statusRuntimeSnapshot(1, 10);
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 10, { 210, 100, 0 }),
});
    queuePendingDamage(state, lethalDamageInput(0, 1));

    auto result = runBattleFrame(state);

    const auto damageLogs = damageLogsFor(result, 1);
    REQUIRE(damageLogs.size() == 1);
    CHECK(damageLogs[0].amount > 0);
    CHECK(state.units.requireCore(1).vitals.hp == 0);
    CHECK_FALSE(state.units.requireCore(1).alive);
    CHECK(state.result.ended);
    CHECK(state.result.winningTeam == 0);

    std::vector<BattleGameplayEventType> gameplayTypes;
    for (const auto& event : result.gameplayEvents)
    {
        gameplayTypes.push_back(event.type);
    }
    REQUIRE(gameplayTypes.size() >= 3);
    CHECK(gameplayTypes[gameplayTypes.size() - 3] == BattleGameplayEventType::DamageApplied);
    CHECK(gameplayTypes[gameplayTypes.size() - 2] == BattleGameplayEventType::UnitDied);
    CHECK(gameplayTypes[gameplayTypes.size() - 1] == BattleGameplayEventType::BattleEnded);
}

TEST_CASE("BattleFrameRunner_AcceptedHitResourcePresentationRequiresAppliedTransaction", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    state.gridTransform = { SceneTileWidth, 64 };
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 120, 100, 0 }),
    }));
    state.attacks = attackWorld();

    auto& attacker = state.units.require(0).core;
    attacker.vitals.hp = 60;
    auto& defender = state.units.require(1).core;
    defender.animation.cooldown = 20;
    defender.animation.cooldownMax = 20;
    defender.haveAction = true;
    defender.operationType = BattleOperationType::Melee;
    defender.animation.actType = 1;

    auto accepted = preResolvedDamageInput(0, 1, 100, 20);
    accepted.attacker.vitals = { 60, 100, 10, 50 };
    accepted.defender.vitals = { 100, 100, 0, 50 };
    accepted.request.acceptedHit = true;
    accepted.request.hpOnHit = 30;
    accepted.request.cooldownExtendPct = 50;
    queuePendingDamage(state, accepted);

    auto result = runBattleFrame(state);

    CHECK(state.units.requireCore(0).vitals.hp == 90);
    CHECK(state.units.requireCore(1).animation.cooldown == 29);
    CHECK(hasLogText(result, "命中回血"));
    CHECK(hasLogText(result, "冷卻延長（+10幀）"));
    CHECK(hasHealVisualEvent(result, 0));
}

TEST_CASE("BattleFrameRunner_BlockedAcceptedHitSuppressesResourcePresentation", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    state.gridTransform = { SceneTileWidth, 64 };
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 120, 100, 0 }),
    }));
    state.attacks = attackWorld();

    auto& attacker = state.units.require(0).core;
    attacker.vitals.hp = 60;
    auto& defender = state.units.require(1).core;
    defender.invincible = 10;
    defender.animation.cooldown = 20;
    defender.animation.cooldownMax = 20;
    defender.haveAction = true;
    defender.operationType = BattleOperationType::Melee;
    defender.animation.actType = 1;

    auto blocked = preResolvedDamageInput(0, 1, 100, 20);
    blocked.attacker.vitals = { 60, 100, 10, 50 };
    blocked.defender.vitals = { 100, 100, 0, 50 };
    blocked.defender.invincible = 10;
    blocked.defenderStatus.invincible = 10;
    blocked.request.acceptedHit = true;
    blocked.request.hpOnHit = 30;
    blocked.request.cooldownExtendPct = 50;
    queuePendingDamage(state, blocked);

    auto result = runBattleFrame(state);

    CHECK(state.units.requireCore(0).vitals.hp == 60);
    CHECK(state.units.requireCore(1).animation.cooldown == 19);
    CHECK_FALSE(hasLogText(result, "命中回血"));
    CHECK_FALSE(hasLogText(result, "冷卻延長（+10幀）"));
    CHECK_FALSE(hasHealVisualEvent(result, 0));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DeathClearsFrozenStatusAuthority", "[battle][core][ownership]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 120, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 10, { 120, 100, 0 }),
});
    state.units.require(0).status =
        makeBattleStatusRuntimeUnit(makeBattleStatusUnitState(state.units.requireCore(0)));
    state.units.require(1).status =
        makeBattleStatusRuntimeUnit(makeBattleStatusUnitState(state.units.requireCore(1)));
    queuePendingDamage(state, lethalDamageInput(0, 1));
    state.units.require(1).status.effects.setFrames(BattleStatusKind::Stun, 5, 8);

    runBattleFrame(state);

    CHECK_FALSE(state.units.require(1).frozen());
}

TEST_CASE("BattleFrameRunner_PublishesRenderComboFromRuntimeRecords", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 210, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 100, { 210, 100, 0 }),
});
    state.units.requireCore(1).shield = 33;

    BattleDamageRuntimeUnit damage;
    damage.dualWieldBlocksRemaining = 1;
    state.units.require(0).damage = {};
    state.units.require(1).damage = damage;

    runBattleFrame(state);

    CHECK(state.units.requireCore(1).shield == 33);
    CHECK(state.units.require(1).damage.dualWieldBlocksRemaining == 1);
}


TEST_CASE("BattleFrameRunner_AdvanceFrame_RunsMovementPhysicsInsideCore", "[battle][core][movement]")
{
    BattleRuntimeState state;
    state.gridTransform = { SceneTileWidth, 64 };
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 200, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 100, { 200, 100, 0 }),
});
    state.units.requireCore(0).motion.velocity = { 5, 0, 0 };
    state.units.requireCore(0).motion.acceleration = { 0, 0, -4 };
    state.units.requireCore(0).motion.velocity = { 5, 0, 0 };
    state.units.requireCore(0).motion.acceleration = { 0, 0, -4 };
    state.units.require(0).status = statusRuntimeSnapshot(0, 100);
    state.units.require(1).status = statusRuntimeSnapshot(1, 100);
    state.units.require(1).status.effects.setFrames(BattleStatusKind::Stun, 2);
    state.movementPhysics.config.gravity = -4.0f;
    state.movementPhysics.config.friction = 0.1f;
    state.movementPhysics.config.postDashSpreadFrames = 6;
    state.movementPhysics.terrain.tileWidth = 100.0;
    state.movementPhysics.terrain.coordCount = 2;
    state.movementPhysics.terrain.defaultSeparationDistance = SceneTileWidth * 1.5;
    state.movementPhysics.terrain.walkableByCell.assign(2 * 2, 1);
    state.units.require(0).movement.physics.position = { 100, 100, 0 };
    state.units.require(0).movement.physics.velocity = { 5, 0, 0 };
    state.units.require(0).movement.physics.acceleration = { 0, 0, -4 };
    state.units.require(0).movement.physics.movementDashFrames = 1;
    state.units.require(1).movement.physics.position = { 200, 100, 0 };
    state.units.require(1).movement.physics.velocity = { 5, 0, 0 };
    state.units.require(1).movement.physics.acceleration = { 0, 0, -4 };

    runBattleFrame(state);

    const auto& movedUnit = state.units.requireCore(0);
    CHECK(movedUnit.motion.position.x == 105.0f);
    CHECK(state.units.require(0).movement.physics.movementDashFrames == 0);
    CHECK(state.units.require(0).movement.physics.movementDashSpreadFrames == 6);

    const auto& stoppedUnit = state.units.requireCore(1);
    CHECK(state.units.require(1).frozenFrames() == 1);
    CHECK(stoppedUnit.motion.position.x == 200.0f);
    CHECK(stoppedUnit.motion.velocity.x == 0.0f);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_KeepsMovingCorpsesInMovementPhysics", "[battle][core][movement]")
{
    BattleRuntimeState state;
    state.gridTransform = { SceneTileWidth, 64 };
    auto dead = unit(1, 1, { 200, 100, 0 });
    dead.alive = false;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        dead,
    }));
    state.attacks = attackWorld();
    state.units.requireCore(0).motion.velocity = { 5, 0, 0 };
    state.units.requireCore(1).motion.velocity = { 6, 0, 8 };
    state.units.requireCore(1).motion.acceleration = { 0, 0, -4 };
    state.units.require(1).movement.physics.velocity = { 6, 0, 8 };
    state.units.require(1).movement.physics.acceleration = { 0, 0, -4 };
    state.movementPhysics.config.gravity = 0.0f;
    state.movementPhysics.config.friction = 0.0f;
    state.movementPhysics.config.postDashSpreadFrames = 6;
    state.movementPhysics.terrain.tileWidth = 100.0;
    state.movementPhysics.terrain.coordCount = 2;
    state.movementPhysics.terrain.defaultSeparationDistance = SceneTileWidth;
    state.movementPhysics.terrain.walkableByCell.assign(2 * 2, 1);

    runBattleFrame(state);

    const auto& corpse = state.units.requireCore(1);
    CHECK(corpse.motion.position.x == 206.0f);
    CHECK(corpse.motion.position.z == 8.0f);
    CHECK(state.units.require(1).id() == 1);
    CHECK(state.units.require(1).movement.active);
    CHECK(state.units.require(1).movement.physics.position.x == 206.0f);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_InactivatesInertDeadMovementAgents", "[battle][core][movement][ownership]")
{
    BattleRuntimeState state;
    state.gridTransform = { SceneTileWidth, 64 };
    auto dead = unit(1, 1, { 200, 100, 0 });
    dead.alive = false;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        dead,
    }));
    state.attacks = attackWorld();
    state.units.requireCore(0).motion.velocity = { 5, 0, 0 };
    state.movementPhysics.config.gravity = 0.0f;
    state.movementPhysics.config.friction = 0.0f;
    state.movementPhysics.config.postDashSpreadFrames = 6;
    state.movementPhysics.terrain.tileWidth = 100.0;
    state.movementPhysics.terrain.coordCount = 2;
    state.movementPhysics.terrain.defaultSeparationDistance = SceneTileWidth;
    state.movementPhysics.terrain.walkableByCell.assign(2 * 2, 1);

    runBattleFrame(state);

    REQUIRE(state.units.require(1).id() == 1);
    CHECK_FALSE(state.units.require(1).movement.active);
    CHECK(state.units.requireCore(1).motion.position.x == Catch::Approx(200.0f));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AppliesDeathKickVelocity", "[battle][core][movement]")
{
    BattleRuntimeState state;
    state.gridTransform = { SceneTileWidth, 64 };
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 200, 100, 0 }),
    }));
    state.attacks = attackWorld();
    state.movementPhysics.config.gravity = -4.0f;
    state.movementPhysics.config.friction = 0.0f;
    state.movementPhysics.config.postDashSpreadFrames = 6;
    state.movementPhysics.terrain.tileWidth = 100.0;
    state.movementPhysics.terrain.coordCount = 2;
    state.movementPhysics.terrain.defaultSeparationDistance = SceneTileWidth;
    state.movementPhysics.terrain.walkableByCell.assign(2 * 2, 1);
    state.units.requireCore(1).motion.velocity = { -50.0f, 20.0f, 40.0f };
    state.units.require(1).movement.physics.velocity = state.units.requireCore(1).motion.velocity;
    state.units.requireCore(0).stats.speed = 0;
    state.units.requireCore(1).stats.speed = 0;

    state.nextFrame.queueDamage({
        .request = {
            .attackerUnitId = 0,
            .defenderUnitId = 1,
            .baseDamage = 30,
            .preResolvedDamage = true,
        },
    });
    state.units.requireCore(1).vitals.hp = 20;
    writeBattleDamageRuntimeUnit(
        state.units.require(1).damage,
        makeBattleDamageUnitState(state.units.requireCore(1), static_cast<const BattleDamageRuntimeUnit*>(nullptr)));

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).size() == 1);
    CHECK_FALSE(state.units.requireCore(1).alive);
    CHECK_FALSE(gameplayEventsFor(result, BattleGameplayEventType::UnitDied, 1).empty());
    const auto& dead = state.units.requireCore(1);
    const double deathSpeed = std::sqrt(
        dead.motion.velocity.x * dead.motion.velocity.x
        + dead.motion.velocity.y * dead.motion.velocity.y
        + dead.motion.velocity.z * dead.motion.velocity.z);
    CHECK_FALSE(dead.alive);
    CHECK(dead.motion.velocity.x > dead.motion.velocity.z);
    CHECK(dead.motion.velocity.y != Catch::Approx(20.0));
    CHECK(dead.motion.velocity.x > 0.0f);
    CHECK(dead.motion.velocity.z > 0.0f);
    CHECK(dead.motion.velocity.z <= 6.0f);
    CHECK(deathSpeed == Catch::Approx(20.0 / 3.0 + 5.0));
    CHECK(dead.motion.position.x == Catch::Approx(200.0f));
    CHECK(dead.motion.position.z == Catch::Approx(36.0f));
    CHECK(state.units.require(1).id() == 1);
    CHECK(state.units.require(1).movement.physics.velocity.z == dead.motion.velocity.z);

}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AppliesFallbackDeathKickForAttackerlessDamage", "[battle][core][movement]")
{
    BattleRuntimeState state;
    state.gridTransform = { SceneTileWidth, 64 };
    configureRuntimeMovement(state, worldWith({
        unit(1, 1, { 200, 100, 0 }),
    }));
    state.attacks = attackWorld();
    state.movementPhysics.config.gravity = -4.0f;
    state.movementPhysics.config.friction = 0.0f;
    state.movementPhysics.config.postDashSpreadFrames = 6;
    state.movementPhysics.terrain.tileWidth = 100.0;
    state.movementPhysics.terrain.coordCount = 2;
    state.movementPhysics.terrain.defaultSeparationDistance = SceneTileWidth;
    state.movementPhysics.terrain.walkableByCell.assign(2 * 2, 1);

    state.nextFrame.queueDamage({
        .request = {
            .attackerUnitId = OptionalDamageAttackerUnitId,
            .defenderUnitId = 1,
            .baseDamage = 30,
            .preResolvedDamage = true,
        },
    });
    state.units.requireCore(1).vitals.hp = 20;
    writeBattleDamageRuntimeUnit(
        state.units.require(1).damage,
        makeBattleDamageUnitState(state.units.requireCore(1), static_cast<const BattleDamageRuntimeUnit*>(nullptr)));

    runBattleFrame(state);

    const auto& dead = state.units.requireCore(1);
    CHECK_FALSE(dead.alive);
    CHECK(dead.motion.velocity.x > 0.0f);
    CHECK(dead.motion.velocity.y == Catch::Approx(0.0f));
    CHECK(dead.motion.velocity.z > 0.0f);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ClampsDeathKickVelocity", "[battle][core][movement]")
{
    BattleRuntimeState state;
    state.gridTransform = { SceneTileWidth, 64 };
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 200, 100, 0 }),
    }));
    state.attacks = attackWorld();
    state.movementPhysics.config.gravity = -4.0f;
    state.movementPhysics.config.friction = 0.0f;
    state.movementPhysics.config.postDashSpreadFrames = 6;
    state.movementPhysics.terrain.tileWidth = 100.0;
    state.movementPhysics.terrain.coordCount = 16;
    state.movementPhysics.terrain.defaultSeparationDistance = SceneTileWidth;
    state.movementPhysics.terrain.walkableByCell.assign(16 * 16, 1);

    state.nextFrame.queueDamage({
        .request = {
            .attackerUnitId = 0,
            .defenderUnitId = 1,
            .baseDamage = 1000,
            .preResolvedDamage = true,
        },
    });
    state.units.requireCore(1).vitals.hp = 500;
    writeBattleDamageRuntimeUnit(
        state.units.require(1).damage,
        makeBattleDamageUnitState(state.units.requireCore(1), static_cast<const BattleDamageRuntimeUnit*>(nullptr)));

    auto result = runBattleFrame(state);

    const auto damageAmounts = damageLogAmountsFor(result, 1);
    REQUIRE(damageAmounts.size() == 1);
    CHECK_FALSE(state.units.requireCore(1).alive);
    CHECK_FALSE(gameplayEventsFor(result, BattleGameplayEventType::UnitDied, 1).empty());
    const auto& dead = state.units.requireCore(1);
    CHECK(damageAmounts[0] / 3.0 + 5.0 > 75.0);
    CHECK_FALSE(dead.alive);
    CHECK(dead.motion.velocity.norm() == Catch::Approx(75.0));
    CHECK(dead.motion.velocity.z == Catch::Approx(6.0));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_SeedsDeathKickForNextFramePhysics", "[battle][core][movement]")
{
    BattleRuntimeState state;
    state.gridTransform = { SceneTileWidth, 64 };
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 200, 100, 0 }),
        unit(2, 0, { 120, 100, 0 }),
    }));
    state.attacks = attackWorld();
    state.movementPhysics.config.gravity = -4.0f;
    state.movementPhysics.config.friction = 0.0f;
    state.movementPhysics.config.postDashSpreadFrames = 6;
    state.movementPhysics.terrain.tileWidth = 100.0;
    state.movementPhysics.terrain.coordCount = 16;
    state.movementPhysics.terrain.defaultSeparationDistance = SceneTileWidth;
    state.movementPhysics.terrain.walkableByCell.assign(16 * 16, 1);

    state.nextFrame.queueDamage({
        .request = {
            .attackerUnitId = 0,
            .defenderUnitId = 1,
            .baseDamage = 30,
            .preResolvedDamage = true,
        },
    });
    state.nextFrame.queueDamage({
        .request = {
            .attackerUnitId = 2,
            .defenderUnitId = 1,
            .baseDamage = 30,
            .preResolvedDamage = true,
        },
    });
    state.units.requireCore(1).vitals.hp = 20;
    writeBattleDamageRuntimeUnit(
        state.units.require(1).damage,
        makeBattleDamageUnitState(state.units.requireCore(1), static_cast<const BattleDamageRuntimeUnit*>(nullptr)));

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).size() == 1);
    CHECK_FALSE(state.units.requireCore(1).alive);
    CHECK_FALSE(gameplayEventsFor(result, BattleGameplayEventType::UnitDied, 1).empty());

    const auto& dead = state.units.requireCore(1);
    CHECK_FALSE(dead.alive);
    CHECK(dead.motion.velocity.norm() > 0.01f);
    CHECK(dead.motion.position.z >= 0.0f);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_MovingCorpsePhysicsPersistsIntoRuntime", "[battle][core][movement]")
{
    BattleRuntimeState state;
    state.gridTransform = { SceneTileWidth, 64 };
    auto dead = unit(1, 1, { 200, 100, 0 });
    dead.alive = false;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        dead,
    }));
    state.attacks = attackWorld();
    state.units.requireCore(1).motion.velocity = { 6, 0, 8 };
    state.units.requireCore(1).motion.acceleration = { 0, 0, -4 };
    state.units.require(1).movement.physics.velocity = { 6, 0, 8 };
    state.units.require(1).movement.physics.acceleration = { 0, 0, -4 };
    state.movementPhysics.config.gravity = -4.0f;
    state.movementPhysics.config.friction = 0.0f;
    state.movementPhysics.config.postDashSpreadFrames = 6;
    state.movementPhysics.terrain.tileWidth = 100.0;
    state.movementPhysics.terrain.coordCount = 2;
    state.movementPhysics.terrain.defaultSeparationDistance = SceneTileWidth;
    state.movementPhysics.terrain.walkableByCell.assign(2 * 2, 1);

    runBattleFrame(state);
    const auto& firstFrameCorpse = state.units.requireCore(1);
    CHECK(firstFrameCorpse.motion.position.z == 8.0f);

    runBattleFrame(state);

    const auto& deadUnit = state.units.requireCore(1);
    CHECK(deadUnit.motion.position.z == 12.0f);
    CHECK(deadUnit.motion.velocity.z == 0.0f);
    CHECK(state.units.require(1).movement.physics.position.z == deadUnit.motion.position.z);
}


TEST_CASE("BattleFrameRunner_AdvanceFrame_DropsDeferredAutoUltimateWhenBattleEndsFirst", "[battle][core][breakthrough]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 180, 100, 0 }),
        unit(2, 0, { 120, 120, 0 }, CombatStyle::Ranged),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 10, { 180, 100, 0 }),
        runtimeUnitSnapshot(2, 0, 100, { 120, 120, 0 }),
});
    state.units.require(0).status = statusRuntimeSnapshot(0, 100);
    state.units.require(1).status = statusRuntimeSnapshot(1, 10);
    state.units.require(2).status = statusRuntimeSnapshot(2, 100);

    queuePendingDamage(state, lethalDamageInput(0, 1));
    configureAutoUltimateActionRuntime(state, 2, 1);
    addTestPeriodicAutoUltimateRule(state, 2);

    auto result = runBattleFrame(state);

    CHECK(state.result.ended);
    CHECK(state.result.winningTeam == 0);
    CHECK(state.nextFrame.queuedAttacks().empty());
    CHECK(std::none_of(
        result.logEvents.begin(),
        result.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Status
                && event.sourceUnitId == 2
                && BattleLogTest::textOf(event) == "自動絕招·絕招";
        }));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_BattleEndEventEmitsOnce", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 210, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 10, { 210, 100, 0 }),
});
    queuePendingDamage(state, lethalDamageInput(0, 1));

    auto first = runBattleFrame(state);
    auto second = runBattleFrame(state);

    int firstEndEvents = 0;
    for (const auto& event : first.gameplayEvents)
    {
        if (event.type == BattleGameplayEventType::BattleEnded)
        {
            ++firstEndEvents;
        }
    }
    int secondEndEvents = 0;
    for (const auto& event : second.gameplayEvents)
    {
        if (event.type == BattleGameplayEventType::BattleEnded)
        {
            ++secondEndEvents;
        }
    }

    CHECK(firstEndEvents == 1);
    CHECK(secondEndEvents == 0);
}


TEST_CASE("BattleFrameRunner_RemovesExpiredAndDeadSourceAreasAtLifecycleBoundaries", "[battle][area][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 105, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);

    auto expiring = fixedCircleAreaRequest(0, 0, { 1 }, { 100, 100, 0 }, 0, 1);
    BattleAreaEffectSystem::create(state.areas, std::move(expiring));
    runBattleFrame(state);
    CHECK(state.areas.areas.empty());

    auto removeOnDeath = fixedCircleAreaRequest(1, 1, { 2 }, { 105, 100, 0 }, 1, 100);
    removeOnDeath.anchor = { BattleAreaAnchorKind::FollowSourceUnit, {}, 1 };
    removeOnDeath.sourceDeath = AreaSourceDeathPolicy::RemoveImmediately;
    BattleAreaEffectSystem::create(state.areas, std::move(removeOnDeath));
    const auto persistent = BattleAreaEffectSystem::create(
        state.areas,
        fixedCircleAreaRequest(1, 1, { 3 }, { 105, 100, 0 }, 1, 100));

    BattleAttackInstance projectile{ BattleAttackPayload(
        BattleAttackDelivery::projectile(),
        BattleProjectilePayloadClass::scriptedDamage(),
        BattleAttackReflectionLineageKind::Ordinary) };
    projectile.id = 10;
    projectile.state.attackSourceUnitId = 0;
    projectile.state.preferredTargetUnitId = 1;
    projectile.state.scriptedDamage = 200;
    projectile.state.totalFrame = 30;
    projectile.state.operationType = BattleOperationType::RangedProjectile;
    projectile.state.position = { 105, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };
    appendTrackedAttack(state, std::move(projectile));

    runBattleFrame(state);

    CHECK_FALSE(state.units.requireCore(1).alive);
    REQUIRE(state.areas.areas.size() == 1);
    CHECK(state.areas.areas[0].id == persistent.areaId);
}
