#include "battle/BattlePresentationPlayback.h"
#include "battle/BattleLogSegments.h"
#include "BattlePresentationEffects.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>

using namespace KysChess::Battle;

namespace
{
BattleVisualEvent floatingTextEvent(int frame, int targetUnitId, int textSize, int textMotionType, std::string text)
{
    BattleVisualEvent event;
    event.type = BattleVisualEventType::FloatingText;
    event.frame = frame;
    event.targetUnitId = targetUnitId;
    event.textSize = textSize;
    event.textMotionType = textMotionType;
    event.text = std::move(text);
    return event;
}

BattleVisualEvent roleEffectEvent(int frame, int targetUnitId, int durationFrames, int effectId)
{
    BattleVisualEvent event;
    event.type = BattleVisualEventType::RoleEffect;
    event.frame = frame;
    event.targetUnitId = targetUnitId;
    event.durationFrames = durationFrames;
    event.effectId = effectId;
    return event;
}

BattleVisualEvent damageNumberEvent(int frame, int targetUnitId, int amount, int textSize)
{
    BattleVisualEvent event;
    event.type = BattleVisualEventType::DamageNumber;
    event.frame = frame;
    event.targetUnitId = targetUnitId;
    event.amount = amount;
    event.textSize = textSize;
    return event;
}

BattleVisualEvent cameraFocusEvent(int frame, int durationFrames, Pointf position)
{
    BattleVisualEvent event;
    event.type = BattleVisualEventType::CameraFocus;
    event.frame = frame;
    event.durationFrames = durationFrames;
    event.position = position;
    return event;
}

BattleVisualEvent projectileEvent(
    BattleVisualEventType type,
    int frame,
    int sourceUnitId,
    int targetUnitId,
    int amount,
    Pointf position)
{
    BattleVisualEvent event;
    event.type = type;
    event.frame = frame;
    event.sourceUnitId = sourceUnitId;
    event.targetUnitId = targetUnitId;
    event.amount = amount;
    event.durationFrames = 30;
    event.effectId = 10;
    event.position = position;
    event.visualEffectId = 33;
    event.velocity = { 2, 0, 0 };
    event.operationKind = 2;
    return event;
}
}

TEST_CASE("BattlePresentationPlaybackPlanner_MapsEveryVisualEvent", "[battle][presentation][unit]")
{
    BattlePresentationFrame frame;
    frame.frame = 18;
    frame.gameplayEvents = {
        { BattleGameplayEventType::DamageApplied, 18, 1, 2, 30, -1 },
        { BattleGameplayEventType::ProjectileHit, 18, 1, 2, 0, 10 },
    };
    frame.logEvents = {
        { KysChess::Battle::BattleLogEventType::Damage, 18, 1, 2, 30, BattleLogCategory::Status, BattleLogPerspective::Targeted, battleLogText("detail"), "skill" },
        { KysChess::Battle::BattleLogEventType::Heal, 18, 1, 1, 12, BattleLogCategory::Status, BattleLogPerspective::Targeted, battleLogText("heal") },
        { KysChess::Battle::BattleLogEventType::Status, 18, 1, 2, 0, BattleLogCategory::Status, BattleLogPerspective::Targeted, battleLogText("status") },
    };
    frame.visualEvents.push_back(floatingTextEvent(18, 2, 24, 1, "float"));
    frame.visualEvents.push_back(roleEffectEvent(18, 2, 48, 7));
    frame.visualEvents.push_back(damageNumberEvent(18, 2, 15, 30));
    frame.visualEvents.push_back(cameraFocusEvent(18, 10, { 9, 8, 0 }));
    frame.visualEvents.push_back(projectileEvent(BattleVisualEventType::ProjectileSpawned, 18, 1, 2, 0, { 10, 12, 0 }));
    frame.visualEvents.push_back(projectileEvent(BattleVisualEventType::ProjectileMoved, 18, 1, 2, 0, { 11, 12, 0 }));
    frame.visualEvents.push_back(projectileEvent(BattleVisualEventType::ProjectileHit, 18, 1, 2, 0, { 11, 12, 0 }));
    frame.visualEvents.push_back(projectileEvent(BattleVisualEventType::ProjectileExpired, 18, 1, -1, 0, { 11, 12, 0 }));
    frame.visualEvents.push_back(projectileEvent(BattleVisualEventType::ProjectileTargetLost, 18, 1, -1, -1, { 11, 12, 0 }));
    frame.visualEvents.push_back(projectileEvent(BattleVisualEventType::ProjectileCancelled, 18, 1, 2, 20, { 11, 12, 0 }));
    frame.visualEvents.push_back(projectileEvent(BattleVisualEventType::ProjectileBounced, 18, 1, 3, 20, { 11, 12, 0 }));

    BattlePresentationPlaybackPlanner planner;
    frame.visualEvents[4].through = true;
    auto plan = planner.build(frame);

    REQUIRE(plan.commands.size() == frame.visualEvents.size());
    CHECK(plan.commands[0].type == BattlePresentationCommandType::SpawnFloatingText);
    CHECK(plan.commands[0].textSize == 24);
    CHECK(plan.commands[0].textMotionType == 1);
    CHECK(plan.commands[1].type == BattlePresentationCommandType::SpawnRoleEffect);
    CHECK(plan.commands[1].effectId == 7);
    CHECK(plan.commands[1].durationFrames == 48);
    CHECK(plan.commands[2].type == BattlePresentationCommandType::SpawnDamageNumber);
    CHECK(plan.commands[2].amount == 15);
    CHECK(plan.commands[3].type == BattlePresentationCommandType::FocusCamera);
    CHECK(plan.commands[3].position.x == 9);
    CHECK(plan.commands[4].type == BattlePresentationCommandType::SpawnProjectile);
    CHECK(plan.commands[4].projectileAttackId == 10);
    CHECK(plan.commands[4].visualEffectId == 33);
    CHECK(plan.commands[4].projectileSourceUnitId == 1);
    CHECK(plan.commands[4].projectileTargetUnitId == 2);
    CHECK(plan.commands[4].projectilePosition.x == 10);
    CHECK(plan.commands[4].projectileVelocity.x == 2);
    CHECK(plan.commands[4].projectileDurationFrames == 30);
    CHECK(plan.commands[4].projectileOperationKind == 2);
    CHECK(plan.commands[4].projectileThrough);
    CHECK(plan.commands[5].type == BattlePresentationCommandType::MoveProjectile);
    CHECK(plan.commands[5].projectileAttackId == 10);
    CHECK(plan.commands[5].visualEffectId == 33);
    CHECK(plan.commands[5].projectilePosition.x == 11);
    CHECK(plan.commands[5].projectileVelocity.x == 2);
    CHECK(plan.commands[5].projectileDurationFrames == 30);
    CHECK(plan.commands[5].projectileOperationKind == 2);
    CHECK(plan.commands[6].type == BattlePresentationCommandType::ImpactProjectile);
    CHECK(plan.commands[6].projectileAttackId == 10);
    CHECK(plan.commands[7].type == BattlePresentationCommandType::ExpireProjectile);
    CHECK(plan.commands[8].type == BattlePresentationCommandType::CancelProjectile);
    CHECK(plan.commands[8].projectileRelatedAttackId == -1);
    CHECK(plan.commands[9].type == BattlePresentationCommandType::CancelProjectile);
    CHECK(plan.commands[9].projectileRelatedAttackId == 20);
    CHECK(plan.commands[10].type == BattlePresentationCommandType::BounceProjectile);
    CHECK(plan.commands[10].projectileAttackId == 10);
    CHECK(plan.commands[10].projectileTargetUnitId == 3);
    CHECK(plan.commands[10].projectileRelatedAttackId == 20);
}

TEST_CASE("BattlePresentationPlaybackPlanner_PreservesCommandOrder", "[battle][presentation][unit]")
{
    BattlePresentationFrame frame;
    frame.frame = 3;
    frame.gameplayEvents = {
        { BattleGameplayEventType::CastStarted, 3, 1, 2 },
        { BattleGameplayEventType::DamageApplied, 3, 1, 2, 9 },
    };
    frame.logEvents = {
        { KysChess::Battle::BattleLogEventType::Status, 3, -1, 1, 0, BattleLogCategory::Status, BattleLogPerspective::Targeted, battleLogText("first") },
        { KysChess::Battle::BattleLogEventType::Damage, 3, 1, 2, 9 },
    };
    frame.visualEvents.push_back(floatingTextEvent(3, 1, 24, 0, "second"));
    frame.visualEvents.push_back(damageNumberEvent(3, 2, 9, 24));

    BattlePresentationPlaybackPlanner planner;
    auto plan = planner.build(frame);

    REQUIRE(plan.commands.size() == 2);
    CHECK(plan.commands[0].type == BattlePresentationCommandType::SpawnFloatingText);
    CHECK(plan.commands[0].text == "second");
    CHECK(plan.commands[1].type == BattlePresentationCommandType::SpawnDamageNumber);
    CHECK(plan.commands[1].amount == 9);
}

TEST_CASE("BattlePresentationPlaybackPlanner_IgnoresNonVisualOutputs", "[battle][presentation][unit]")
{
    BattlePresentationFrame frame;
    frame.frame = 5;
    frame.gameplayEvents = {
        { BattleGameplayEventType::CastStarted, 5, 1, 2 },
        { BattleGameplayEventType::ProjectileMoved, 5, 1, 2, 0, 10, { 4, 5, 0 } },
        { BattleGameplayEventType::DamageApplied, 5, 1, 2, 9 },
    };
    frame.logEvents = {
        { KysChess::Battle::BattleLogEventType::Status, 5, -1, -1, 0, BattleLogCategory::Status, BattleLogPerspective::Targeted, battleLogText("log-only") },
    };
    frame.visualEvents.push_back(floatingTextEvent(5, 1, 24, 0, "visual-only"));

    BattlePresentationPlaybackPlanner planner;
    auto plan = planner.build(frame);

    REQUIRE(plan.commands.size() == 1);
    CHECK(plan.commands[0].type == BattlePresentationCommandType::SpawnFloatingText);
    CHECK(plan.commands[0].text == "visual-only");
}

TEST_CASE("BattlePresentationPlaybackPlanner_TargetLostCancelHasNoRelatedProjectile", "[battle][presentation][unit]")
{
    BattlePresentationFrame frame;
    frame.frame = 9;
    BattleVisualEvent targetLost;
    targetLost.type = BattleVisualEventType::ProjectileTargetLost;
    targetLost.frame = 9;
    targetLost.sourceUnitId = 1;
    targetLost.targetUnitId = -1;
    targetLost.durationFrames = 20;
    targetLost.effectId = 10;
    targetLost.position = { 30, 40, 0 };
    targetLost.visualEffectId = 12;
    targetLost.velocity = { 1, 0, 0 };
    targetLost.operationKind = 2;
    frame.visualEvents.push_back(targetLost);

    BattlePresentationPlaybackPlanner planner;
    auto plan = planner.build(frame);

    REQUIRE(plan.commands.size() == 1);
    CHECK(plan.commands[0].type == BattlePresentationCommandType::CancelProjectile);
    CHECK(plan.commands[0].projectileAttackId == 10);
    CHECK(plan.commands[0].projectileRelatedAttackId == -1);
    CHECK(plan.commands[0].visualEffectId == 12);
    CHECK(plan.commands[0].projectileVelocity.x == 1);
    CHECK(plan.commands[0].projectileDurationFrames == 20);
    CHECK(plan.commands[0].projectileOperationKind == 2);
}

TEST_CASE("BattleAttackEffect_RenderTeamUsesVisualTeamOnly", "[battle][presentation][unit]")
{
    BattleAttackEffect effect;
    CHECK(effect.renderTeam() == -1);

    effect.VisualTeam = 1;

    CHECK(effect.renderTeam() == 1);
}

TEST_CASE("BattleAttackEffect_AdvanceVisualOnlyEffectsTicksOnlyVisualLifetime", "[battle][presentation][unit]")
{
    std::deque<BattleAttackEffect> effects;

    BattleAttackEffect visualRoleEffect;
    visualRoleEffect.VisualOnly = 1;
    visualRoleEffect.TotalFrame = 3;
    effects.push_back(visualRoleEffect);

    BattleAttackEffect legacyGameplayProjectile;
    legacyGameplayProjectile.TotalFrame = 3;
    effects.push_back(legacyGameplayProjectile);

    advanceBattleVisualOnlyEffects(effects);

    CHECK(effects[0].Frame == 1);
    CHECK(effects[1].Frame == 0);
}

TEST_CASE("BattleAttackEffect_VisualOnlyProjectileFadeAdvancesToExpiry", "[battle][presentation][unit]")
{
    std::deque<BattleAttackEffect> effects;

    BattleAttackEffect impactedVisualProjectile;
    impactedVisualProjectile.VisualOnly = 1;
    impactedVisualProjectile.TotalFrame = 30;
    impactedVisualProjectile.Frame = 28;
    effects.push_back(impactedVisualProjectile);

    advanceBattleVisualOnlyEffects(effects);
    CHECK(effects.front().Frame == 29);

    advanceBattleVisualOnlyEffects(effects);
    CHECK(effects.front().Frame == 30);
    CHECK(effects.front().Frame >= effects.front().TotalFrame);
}

TEST_CASE("BattleAttackEffect_PresentationLifetimeAdvancesOnlyOnBattleFrameTicks", "[battle][presentation][unit]")
{
    std::deque<BattleAttackEffect> effects;

    BattleAttackEffect roleEffect;
    roleEffect.VisualOnly = 1;
    roleEffect.TotalFrame = 2;
    roleEffect.Frame = 1;
    effects.push_back(roleEffect);

    BattleAttackEffect runtimeOwnedEffect;
    runtimeOwnedEffect.VisualOnly = 0;
    runtimeOwnedEffect.TotalFrame = 2;
    runtimeOwnedEffect.Frame = 1;
    effects.push_back(runtimeOwnedEffect);

    advanceBattlePresentationEffects(effects, false);

    REQUIRE(effects.size() == 2);
    CHECK(effects[0].Frame == 1);
    CHECK(effects[1].Frame == 1);

    advanceBattlePresentationEffects(effects, true);

    REQUIRE(effects.size() == 1);
    CHECK(effects.front().VisualOnly == 0);
    CHECK(effects.front().Frame == 1);
}
