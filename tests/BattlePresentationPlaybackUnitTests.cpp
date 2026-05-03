#include "battle/BattlePresentationPlayback.h"
#include "BattleSceneAct.h"

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <iterator>
#include <string>

using namespace KysChess::Battle;

namespace
{
std::string readTextFile(const char* path)
{
    std::ifstream file(path);
    return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
}
}  // namespace

TEST_CASE("BattlePresentationPlaybackPlanner_MapsEveryCommittedPresentationEvent", "[battle][presentation][unit]")
{
    BattlePresentationFrame frame;
    frame.snapshot.frame = 18;
    frame.snapshot.units.push_back({ 1, 11, "source", 0, true, 100, 100, 20, 100, 0, 0, { 1, 2, 0 }, {} });
    frame.gameplayEvents = {
        { BattleGameplayEventType::DamageApplied, 18, 1, 2, 30, -1 },
        { BattleGameplayEventType::ProjectileHit, 18, 1, 2, 0, 10 },
    };
    frame.presentationEvents = {
        { BattlePresentationEventType::DamageLog, 18, 1, 2, 30, 0, -1, 0, 0, "", "skill", "detail" },
        { BattlePresentationEventType::HealLog, 18, 1, 1, 12, 0, -1, 0, 0, "heal" },
        { BattlePresentationEventType::StatusLog, 18, 1, 2, 0, 0, -1, 0, 0, "status" },
        { BattlePresentationEventType::FloatingText, 18, -1, 2, 0, 0, -1, 24, 1, "float" },
        { BattlePresentationEventType::RoleEffect, 18, -1, 2, 0, 48, 7 },
        { BattlePresentationEventType::DamageNumber, 18, -1, 2, 15, 0, -1, 30 },
        { BattlePresentationEventType::CameraFocus, 18, -1, -1, 0, 10, -1, 0, 0, "", "", "", {}, { 9, 8, 0 } },
        { BattlePresentationEventType::ProjectileSpawned, 18, 1, 2, 0, 30, 10, 0, 0, "", "", "", {}, { 10, 12, 0 }, 33, { 2, 0, 0 }, 2 },
        { BattlePresentationEventType::ProjectileMoved, 18, 1, 2, 0, 30, 10, 0, 0, "", "", "", {}, { 11, 12, 0 }, 33, { 2, 0, 0 }, 2 },
        { BattlePresentationEventType::ProjectileHit, 18, 1, 2, 0, 30, 10, 0, 0, "", "", "", {}, { 11, 12, 0 }, 33, { 2, 0, 0 }, 2 },
        { BattlePresentationEventType::ProjectileExpired, 18, 1, -1, 0, 30, 10, 0, 0, "", "", "", {}, { 11, 12, 0 }, 33, { 2, 0, 0 }, 2 },
        { BattlePresentationEventType::ProjectileTargetLost, 18, 1, -1, -1, 30, 10, 0, 0, "", "", "", {}, { 11, 12, 0 }, 33, { 2, 0, 0 }, 2 },
        { BattlePresentationEventType::ProjectileCancelled, 18, 1, 2, 20, 30, 10, 0, 0, "", "", "", {}, { 11, 12, 0 }, 33, { 2, 0, 0 }, 2 },
        { BattlePresentationEventType::ProjectileBounced, 18, 1, 3, 20, 30, 10, 0, 0, "", "", "", {}, { 11, 12, 0 }, 33, { 2, 0, 0 }, 2 },
    };

    BattlePresentationPlaybackPlanner planner;
    auto plan = planner.build(frame);

    REQUIRE(plan.snapshot.frame == 18);
    REQUIRE(plan.snapshot.units.size() == 1);
    REQUIRE(plan.commands.size() == frame.presentationEvents.size());
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
    CHECK(plan.commands[7].type == BattlePresentationCommandType::SpawnProjectile);
    CHECK(plan.commands[7].projectileAttackId == 10);
    CHECK(plan.commands[7].visualEffectId == 33);
    CHECK(plan.commands[7].projectileSourceUnitId == 1);
    CHECK(plan.commands[7].projectileTargetUnitId == 2);
    CHECK(plan.commands[7].projectilePosition.x == 10);
    CHECK(plan.commands[7].projectileVelocity.x == 2);
    CHECK(plan.commands[7].projectileDurationFrames == 30);
    CHECK(plan.commands[7].projectileOperationKind == 2);
    CHECK(plan.commands[8].type == BattlePresentationCommandType::MoveProjectile);
    CHECK(plan.commands[8].projectileAttackId == 10);
    CHECK(plan.commands[8].visualEffectId == 33);
    CHECK(plan.commands[8].projectilePosition.x == 11);
    CHECK(plan.commands[8].projectileVelocity.x == 2);
    CHECK(plan.commands[8].projectileDurationFrames == 30);
    CHECK(plan.commands[8].projectileOperationKind == 2);
    CHECK(plan.commands[9].type == BattlePresentationCommandType::ImpactProjectile);
    CHECK(plan.commands[9].projectileAttackId == 10);
    CHECK(plan.commands[10].type == BattlePresentationCommandType::ExpireProjectile);
    CHECK(plan.commands[11].type == BattlePresentationCommandType::CancelProjectile);
    CHECK(plan.commands[11].projectileRelatedAttackId == -1);
    CHECK(plan.commands[12].type == BattlePresentationCommandType::CancelProjectile);
    CHECK(plan.commands[12].projectileRelatedAttackId == 20);
    CHECK(plan.commands[13].type == BattlePresentationCommandType::BounceProjectile);
    CHECK(plan.commands[13].projectileAttackId == 10);
    CHECK(plan.commands[13].projectileTargetUnitId == 3);
    CHECK(plan.commands[13].projectileRelatedAttackId == 20);
}

TEST_CASE("BattlePresentationPlaybackPlanner_PreservesCommandOrder", "[battle][presentation][unit]")
{
    BattlePresentationFrame frame;
    frame.snapshot.frame = 3;
    frame.gameplayEvents = {
        { BattleGameplayEventType::CastStarted, 3, 1, 2 },
        { BattleGameplayEventType::DamageApplied, 3, 1, 2, 9 },
    };
    frame.presentationEvents = {
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

TEST_CASE("BattlePresentationPlaybackPlanner_IgnoresGameplayEvents", "[battle][presentation][unit]")
{
    BattlePresentationFrame frame;
    frame.snapshot.frame = 5;
    frame.gameplayEvents = {
        { BattleGameplayEventType::CastStarted, 5, 1, 2 },
        { BattleGameplayEventType::ProjectileMoved, 5, 1, 2, 0, 10, { 4, 5, 0 } },
        { BattleGameplayEventType::DamageApplied, 5, 1, 2, 9 },
    };
    frame.presentationEvents = {
        { BattlePresentationEventType::StatusLog, 5, -1, -1, 0, 0, -1, 0, 0, "presentation-only" },
    };

    BattlePresentationPlaybackPlanner planner;
    auto plan = planner.build(frame);

    REQUIRE(plan.commands.size() == 1);
    CHECK(plan.commands[0].type == BattlePresentationCommandType::RecordStatus);
    CHECK(plan.commands[0].text == "presentation-only");
}

TEST_CASE("BattlePresentationPlaybackPlanner_TargetLostCancelHasNoRelatedProjectile", "[battle][presentation][unit]")
{
    BattlePresentationFrame frame;
    frame.snapshot.frame = 9;
    frame.presentationEvents = {
        { BattlePresentationEventType::ProjectileTargetLost, 9, 1, -1, 0, 20, 10, 0, 0, "", "", "", {}, { 30, 40, 0 }, 12, { 1, 0, 0 }, 2 },
    };

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

TEST_CASE("BattleSceneAct_AttackEffectRenderTeamFallsBackToLegacyAttacker", "[battle][presentation][unit]")
{
    Role attacker;
    attacker.Team = 0;

    BattleSceneAct::AttackEffect effect;
    effect.Attacker = &attacker;

    CHECK(effect.renderTeam() == 0);

    effect.VisualTeam = 1;

    CHECK(effect.renderTeam() == 1);
}

TEST_CASE("BattleScenePresentationPlayer_ProjectilePlaybackDoesNotWriteGameplayFields", "[battle][presentation][unit]")
{
    const auto source = readTextFile("src/BattleScenePresentationPlayer.cpp");
    REQUIRE_FALSE(source.empty());

    CHECK(source.find("PreferredTarget") == std::string::npos);
    CHECK(source.find(".Attacker") == std::string::npos);
    CHECK(source.find(".Defender") == std::string::npos);
    CHECK(source.find(".UsingMagic") == std::string::npos);
    CHECK(source.find(".UsingHiddenWeapon") == std::string::npos);
    CHECK(source.find(".OperationType") == std::string::npos);
    CHECK(source.find(".NoHurt") == std::string::npos);
    CHECK(source.find(".Track") == std::string::npos);
    CHECK(source.find(".Through") == std::string::npos);
    CHECK(source.find("ScriptedDamage") == std::string::npos);
    CHECK(source.find("ScriptedStunFrames") == std::string::npos);
    CHECK(source.find("ScriptedBleedStacks") == std::string::npos);
    CHECK(source.find("BounceRemaining") == std::string::npos);
}
