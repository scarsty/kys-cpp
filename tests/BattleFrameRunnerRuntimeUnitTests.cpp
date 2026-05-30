#include "battle/BattleCore.h"
#include "BattleLogTestHelpers.h"
#include "BattlePresentationTestHelpers.h"
#include "battle/BattleHitResolver.h"
#include "battle/BattleRuntimeSession.h"
#include "battle/BattleRuntimeUnitSpawn.h"
#include "Find.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <variant>
#include <vector>

using namespace KysChess::Battle;
using namespace BattlePresentationTest;

namespace
{

constexpr double SceneTileWidth = 36.0;
constexpr double MaxEffectiveBattleReach = 480.0;
constexpr double TestMinimumVectorNorm = 0.0001;
constexpr int HealEffectId = 0;

BattlePresentationFrame runBattleFrame(BattleRuntimeState& state)
{
    return BattleFrameRunner().runFrame(state);
}

void seedDamageExtrasFromUnits(BattleRuntimeState& state);

BattleMovementConfig runtimeMovementConfig()
{
    BattleMovementGeometry geometry;
    geometry.tileWidth = SceneTileWidth;
    geometry.meleeAttackEffectOffset = SceneTileWidth * 2.0;
    geometry.meleeAttackHitRadius = SceneTileWidth * 2.0;
    geometry.dashFrames = 5;
    geometry.dashCooldownFrames = 18;
    geometry.maxRangedReach = MaxEffectiveBattleReach;
    return BattleGeometry(geometry).movementConfig();
}

BattleUnitState runtimeUnit(int id, int team, Pointf position)
{
    BattleUnitState state;
    state.id = id;
    state.team = team;
    state.alive = true;
    state.position = position;
    state.speed = 5.0;
    state.reach = 137.5;
    return state;
}

BattleRuntimeUnit runtimeUnitFromWorld(const BattleUnitState& worldUnit)
{
    BattleRuntimeUnit unit;
    unit.id = worldUnit.id;
    unit.realRoleId = worldUnit.id;
    unit.team = worldUnit.team;
    unit.alive = worldUnit.alive;
    unit.vitals = { 100, 100, 20, 100 };
    unit.stats.speed = static_cast<int>(worldUnit.speed);
    unit.motion.position = worldUnit.position;
    unit.motion.velocity = worldUnit.velocity;
    unit.style = worldUnit.style;
    return unit;
}

void seedCanonicalUnitsFromMovementUnits(BattleRuntimeState& state, const std::vector<BattleUnitState>& units)
{
    state.units = {};
    for (const auto& worldUnit : units)
    {
        appendRuntimeUnit(
            state,
            makeRuntimeUnitSpawn(runtimeUnitFromWorld(worldUnit), KysChess::RoleComboState{}));
    }
}

void seedEmptyComboStatesFromUnits(BattleRuntimeState& state)
{
    for (const auto& unit : state.units.cores())
    {
        state.units.require(unit.id).combo = KysChess::RoleComboState{};
    }
}

BattleRuntimeState runtimeFrameState()
{
    BattleRuntimeState state;
    state.movement.frame = 6;
    state.movement.config = runtimeMovementConfig();
    seedCanonicalUnitsFromMovementUnits(state, {
        runtimeUnit(0, 0, { 100, 100, 0 }),
        runtimeUnit(1, 1, { 500, 100, 0 }),
    });
    state.attacks.hitRadius = SceneTileWidth * 2.0;
    state.attacks.minimumVectorNorm = TestMinimumVectorNorm;
    state.attacks.bounceSpawnDistance = SceneTileWidth * 1.5;
    state.attacks.defaultProjectileSpeed = SceneTileWidth / 3.0;
    seedDamageExtrasFromUnits(state);
    return state;
}

BattleRuntimeState ownedRuntimeState()
{
    BattleRuntimeState runtime;
    runtime.movement.frame = 6;
    runtime.movement.config = runtimeMovementConfig();
    seedCanonicalUnitsFromMovementUnits(runtime, {
        runtimeUnit(0, 0, { 100, 100, 0 }),
        runtimeUnit(1, 1, { 500, 100, 0 }),
    });
    runtime.attacks.hitRadius = SceneTileWidth * 2.0;
    runtime.attacks.minimumVectorNorm = TestMinimumVectorNorm;
    runtime.attacks.bounceSpawnDistance = SceneTileWidth * 1.5;
    runtime.attacks.defaultProjectileSpeed = SceneTileWidth / 3.0;
    seedDamageExtrasFromUnits(runtime);
    return runtime;
}

BattleUnitFrameTickInput finishingSkillRuntime()
{
    BattleUnitFrameTickInput input;
    input.state.cooldown = 1;
    input.state.actType = 2;
    input.state.operationType = BattleOperationType::RangedProjectile;
    input.state.haveAction = true;
    input.state.physicalPower = 4;
    input.frame = 6;
    input.mpRegenIntervalFrames = 3;
    input.physicalPowerRegenIntervalFrames = 3;
    return input;
}

void applyRuntimeInput(BattleRuntimeUnit& unit, const BattleUnitFrameTickInput& input)
{
    unit.animation.cooldown = input.state.cooldown;
    unit.animation.actFrame = input.state.actFrame;
    unit.animation.actType = input.state.actType;
    unit.operationType = input.state.operationType;
    unit.haveAction = input.state.haveAction;
    unit.physicalPower = input.state.physicalPower;
}

BattleAttackInstance cancelProjectile(int id, int attackerUnitId)
{
    BattleAttackInstance attack;
    attack.id = id;
    attack.frame = 5;
    attack.state.attackerUnitId = attackerUnitId;
    attack.state.totalFrame = 30;
    attack.state.operationType = BattleOperationType::RangedProjectile;
    attack.state.position = { 500, 500, 0 };
    return attack;
}

BattleRuntimeUnit teamRuntimeUnit(int id, int team, int hp)
{
    BattleRuntimeUnit unit;
    unit.id = id;
    unit.team = team;
    unit.alive = true;
    unit.vitals.hp = hp;
    unit.vitals.maxHp = 100;
    unit.vitals.mp = 20;
    unit.vitals.maxMp = 100;
    return unit;
}

BattleRuntimeUnit teamRuntimeUnitAt(int id, int team, int hp, Pointf position, int cooldown = 0)
{
    auto unit = teamRuntimeUnit(id, team, hp);
    unit.motion.position = position;
    unit.animation.cooldown = cooldown;
    return unit;
}

void seedRuntimeUnits(BattleRuntimeState& state, const std::vector<BattleRuntimeUnit>& units)
{
    std::map<int, KysChess::RoleComboState> savedCombos;
    for (const auto& record : state.units.all())
    {
        savedCombos.emplace(record.id(), record.combo);
    }
    std::map<int, BattleStatusRuntimeUnit> previousStatuses;
    for (const auto& record : state.units.all())
    {
        previousStatuses.emplace(record.id(), record.status);
    }
    std::map<int, BattleDamageRuntimeUnit> previousDamageExtras;
    for (const auto& record : state.units.all())
    {
        previousDamageExtras.emplace(record.id(), record.damage);
    }

    state.units = {};
    state.movement.movementReservations.clear();
        state.damage.presentationStylesByDefender.clear();
            state.units = {};
    for (auto unit : units)
    {
        KysChess::RoleComboState combo;
        if (const auto comboIt = savedCombos.find(unit.id);
            comboIt != savedCombos.end())
        {
            combo = comboIt->second;
        }

        auto spawn = makeRuntimeUnitSpawn(std::move(unit), std::move(combo));
        if (const auto statusIt = previousStatuses.find(spawn.unit.id);
            statusIt != previousStatuses.end())
        {
            spawn.status = statusIt->second;
        }
        if (const auto damageIt = previousDamageExtras.find(spawn.unit.id);
            damageIt != previousDamageExtras.end())
        {
            spawn.damage = damageIt->second;
        }
        appendRuntimeUnit(state, std::move(spawn));
    }
}

BattleStatusRuntimeUnit runtimeStatusUnit(const BattleStatusUnitState& unit)
{
    return makeBattleStatusRuntimeUnit(unit);
}

void seedDamageExtrasFromUnits(BattleRuntimeState& state)
{
    for (const auto& unit : state.units.cores())
    {
        state.units.require(unit.id).combo = KysChess::RoleComboState{};
        state.units.require(unit.id).damage = makeBattleDamageRuntimeUnit(
            makeBattleDamageUnitState(unit, static_cast<const BattleDamageRuntimeUnit*>(nullptr)));
    }
}

}  // namespace

TEST_CASE("BattleRuntimeState_RunFrame_OwnsPendingAttackSpawnsAcrossFrames", "[battle][frame_runner][runtime][ownership]")
{
    auto runtime = ownedRuntimeState();
    runtime.attacks.nextAttackId = 70;

    BattleAttackSpawnRequest request;
    request.initial.attackerUnitId = 0;
    request.initial.preferredTargetUnitId = 0;
    request.initial.skillId = 101;
    request.initial.totalFrame = 30;
    request.initial.visualEffectId = 44;
    request.initial.operationType = BattleOperationType::RangedProjectile;
    request.initial.position = { 100, 120, 0 };
    request.initial.velocity = { 6, 0, 0 };
    runtime.nextFrame.queueAttack(request);

    auto first = runBattleFrame(runtime);
    auto second = runBattleFrame(runtime);

    CHECK(runtime.nextFrame.queuedAttacksForTest().empty());
    REQUIRE(runtime.attacks.attacks.size() == 1);
    CHECK(runtime.attacks.attacks[0].id == 70);
    CHECK(runtime.attacks.nextAttackId == 71);
    CHECK(first.frame == 7);
    CHECK(second.frame == 8);
}

TEST_CASE("BattleRuntimeSession_RunFrame_OwnsRuntimeAcrossFrames", "[battle][runtime_session][ownership]")
{
    auto runtime = ownedRuntimeState();
    runtime.attacks.nextAttackId = 70;

    BattleAttackSpawnRequest request;
    request.initial.attackerUnitId = 0;
    request.initial.preferredTargetUnitId = 0;
    request.initial.skillId = 101;
    request.initial.totalFrame = 30;
    request.initial.visualEffectId = 44;
    request.initial.operationType = BattleOperationType::RangedProjectile;
    request.initial.position = { 100, 120, 0 };
    request.initial.velocity = { 6, 0, 0 };
    runtime.nextFrame.queueAttack(request);

    BattleRuntimeSession session(std::move(runtime));

    const auto first = session.runFrame();
    const auto second = session.runFrame();

    CHECK(session.runtime().nextFrame.queuedAttacksForTest().empty());
    REQUIRE(session.runtime().attacks.attacks.size() == 1);
    CHECK(session.runtime().attacks.attacks[0].id == 70);
    CHECK(session.runtime().attacks.nextAttackId == 71);
    CHECK(first.frame == 7);
    CHECK(second.frame == 8);
}

TEST_CASE("BattleRuntimeSession_RunFrame_DoesNotReplayKnockback", "[battle][runtime_session][ownership]")
{
    auto runtime = ownedRuntimeState();
    runtime.units.requireCore(0).stats.speed = 0;
    runtime.units.requireCore(1).stats.speed = 0;
    runtime.units.requireCore(1).vitals.hp = 100;
    runtime.units.requireCore(1).motion.facing = { -1, 0, 0 };
    seedDamageExtrasFromUnits(runtime);

    BattleAttackInstance attack;
    attack.id = 10;
    attack.state.attackerUnitId = 0;
    attack.state.preferredTargetUnitId = 1;
    attack.state.skillId = 101;
    attack.state.skillMagicPower = 120;
    attack.state.totalFrame = 30;
    attack.frame = 29;
    attack.state.operationType = BattleOperationType::Melee;
    attack.state.position = runtime.units.requireCore(1).motion.position;
    attack.state.velocity = { 1, 0, 0 };
    runtime.attacks.attacks.push_back(attack);

    BattleRuntimeSession session(std::move(runtime));

    session.runFrame();

    const auto& unit = session.runtime().units.requireCore(1);
    CHECK(unit.motion.velocity.norm() > 0.01f);
    CHECK(session.runtime().units.require(1).movement.physics.velocity.x == unit.motion.velocity.x);
    CHECK(session.runtime().units.require(1).movement.physics.velocity.y == unit.motion.velocity.y);
}

TEST_CASE("BattleRuntimeUnitSpawn_AppendsUnitRecordWithPerUnitFacts", "[battle][runtime_session][ownership]")
{
    BattleRuntimeState runtime;
    BattleRuntimeUnit unit;
    unit.id = 4;
    unit.team = 1;
    unit.alive = true;
    unit.vitals.maxHp = 100;
    unit.motion.position = { 32.0f, 48.0f, 0.0f };

    KysChess::RoleComboState combo;
    combo.onSkillTeamHealPending = true;

    BattleActionPlanSeed plan;
    plan.unitId = 99;

    appendRuntimeUnit(runtime, makeRuntimeUnitSpawn(std::move(unit), combo, plan));

    REQUIRE(runtime.units.size() == 1);
    const auto& record = runtime.units.require(4);
    CHECK(record.core.id == 4);
    CHECK(record.combo.onSkillTeamHealPending);
    CHECK(record.movement.physics.position.x == 32.0f);
    REQUIRE(record.actionPlan() != nullptr);
    CHECK(record.actionPlan()->unitId == 4);
}

TEST_CASE("BattleRuntimeSession_CreateInitializedBuildsOwnedRuntimeRecords", "[battle][runtime_session][ownership]")
{
    BattleRuntimeSessionCreationInput input;
    input.rules = makeHadesBattleRuntimeRules(SceneTileWidth, 18);
    input.battleFrame = 42;

    BattleSetupUnitInput unit;
    unit.unitId = 0;
    unit.realRoleId = 1000;
    unit.name = "測試";
    unit.team = 0;
    unit.alive = true;
    unit.vitals = { 100, 100, 0, 100 };
    unit.stats = { 10, 10, 10 };
    unit.motion.position = { 128, 256, 0 };
    input.units.push_back(unit);

    auto session = BattleRuntimeSession::createInitialized(std::move(input)).session;

    CHECK(session.runtime().movement.frame == 42);
    REQUIRE(session.runtime().units.size() == 1);
    CHECK(session.runtime().units.requireCore(0).id == 0);
    REQUIRE(session.runtime().units.size() == 1);
    CHECK(session.runtime().units.require(0).id() == 0);
    CHECK(session.runtime().action.castFrames == session.runtime().movementPhysics.actionCastFrames);
    CHECK(session.runtime().damage.sortPendingDamageByDefenderMagnitude);
}

TEST_CASE("BattleRuntimeSession_CreateInitializedBuildsMovementAgentRowsForDeadUnits", "[battle][runtime_session][ownership]")
{
    BattleRuntimeSessionCreationInput input;
    input.rules = makeHadesBattleRuntimeRules(SceneTileWidth, 18);

    BattleSetupUnitInput live;
    live.unitId = 0;
    live.realRoleId = 1000;
    live.name = "測試";
    live.team = 0;
    live.alive = true;
    live.vitals = { 100, 100, 0, 100 };
    live.stats = { 10, 10, 10 };
    live.motion.position = { 128, 256, 0 };
    input.units.push_back(live);

    BattleSetupUnitInput dead = live;
    dead.unitId = 1;
    dead.realRoleId = 1001;
    dead.name = "死亡測試";
    dead.team = 1;
    dead.alive = false;
    dead.vitals.hp = 0;
    dead.motion.position = { 256, 256, 0 };
    input.units.push_back(dead);

    auto session = BattleRuntimeSession::createInitialized(std::move(input)).session;

    REQUIRE(session.runtime().units.size() == 2);
    CHECK(session.runtime().units.require(0).id() == 0);
    CHECK(session.runtime().units.require(1).id() == 1);
    CHECK(session.runtime().units.require(0).movement.active);
    CHECK_FALSE(session.runtime().units.require(1).movement.active);
}

TEST_CASE("BattleRuntimeSession_CreateInitializedSpendsNonThroughProjectilesOnHit", "[battle][runtime_session][ownership]")
{
    BattleRuntimeSessionCreationInput input;
    input.rules = makeHadesBattleRuntimeRules(SceneTileWidth, 18);

    BattleSetupUnitInput unit;
    unit.unitId = 0;
    unit.realRoleId = 1000;
    unit.name = "測試";
    unit.team = 0;
    unit.alive = true;
    unit.vitals = { 100, 100, 0, 100 };
    unit.stats = { 10, 10, 10 };
    unit.motion.position = { 128, 256, 0 };
    input.units.push_back(unit);

    auto session = BattleRuntimeSession::createInitialized(std::move(input)).session;

    CHECK(session.runtime().attacks.spendNonThroughOnHit);
}

TEST_CASE("BattleRuntimeSession_CreateInitializedKeepsDerivedMotionStateAlignedAfterFrame", "[battle][runtime_session][ownership]")
{
    BattleRuntimeSessionCreationInput input;
    input.rules = makeHadesBattleRuntimeRules(SceneTileWidth, 18);
    input.rules.movementCollisionWorld.walkableByCell.assign(18 * 18, 1);

    BattleSetupUnitInput ally;
    ally.unitId = 0;
    ally.realRoleId = 1000;
    ally.name = "我方";
    ally.team = 0;
    ally.alive = true;
    ally.vitals = { 100, 100, 0, 100 };
    ally.stats = { 10, 10, 10 };
    ally.motion.position = { 128, 128, 0 };
    input.units.push_back(ally);

    BattleSetupUnitInput enemy;
    enemy.unitId = 1;
    enemy.realRoleId = 1001;
    enemy.name = "敵方";
    enemy.team = 1;
    enemy.alive = true;
    enemy.vitals = { 100, 100, 0, 100 };
    enemy.stats = { 10, 10, 10 };
    enemy.motion.position = { 360, 128, 0 };
    input.units.push_back(enemy);

    auto session = BattleRuntimeSession::createInitialized(std::move(input)).session;
    session.runFrame();

    const auto& runtime = session.runtime();
    const auto& unit = runtime.units.requireCore(0);
    REQUIRE(runtime.units.size() == 2);
    REQUIRE(runtime.units.require(0).id() == 0);
    CHECK(runtime.units.require(0).movement.physics.position.x == unit.motion.position.x);
    CHECK(runtime.units.require(0).movement.physics.position.y == unit.motion.position.y);
}

TEST_CASE("BattleFrameRunner_RunFrame_UsesRuntimeOwnedFrameState", "[battle][frame_runner][runtime]")
{
    auto state = runtimeFrameState();

    BattleFrameRunner().runFrame(state);

    REQUIRE(state.units.size() == 2);
    CHECK(state.units.requireCore(0).alive);
    CHECK(state.units.requireCore(1).alive);
}

TEST_CASE("BattleFrameRunner_RunFrame_PublishesStateApplications", "[battle][frame_runner][runtime]")
{
    auto state = runtimeFrameState();
    seedRuntimeUnits(state, {
        teamRuntimeUnit(0, 0, 80),
    });
    state.units.require(0).combo = KysChess::RoleComboState{};
    state.units.requireCore(0).shield = 12;
    BattleDamageRuntimeUnit damage;
    damage.blockFirstHitsRemaining = 2;
    state.units.require(0).damage = damage;
    state.units.requireCore(0).invincible = 4;
    BattleStatusRuntimeUnit status;
    status.effects.frozenTimer = 3;
    status.effects.frozenMaxTimer = 9;
    state.units.require(0).status = status;

    runBattleFrame(state);

    const auto& runtimeUnit = state.units.requireCore(0);
    const auto& statusUnit = state.units.require(0).status;
    CHECK(runtimeUnit.invincible == 3);
    CHECK(statusUnit.effects.frozenTimer == 3);
    CHECK(statusUnit.effects.frozenMaxTimer == 9);
    CHECK(runtimeUnit.shield == 12);
    CHECK(state.units.require(0).damage.blockFirstHitsRemaining == 2);
}

TEST_CASE("BattleFrameRunner_RunFrame_AdvancesRuntimeUnits", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    seedRuntimeUnits(state, {
        teamRuntimeUnit(0, 0, 80),
        teamRuntimeUnit(1, 1, 100),
    });
    auto& unit = state.units.requireCore(0);
    unit.animation.cooldown = 1;
    unit.haveAction = true;
    unit.operationType = BattleOperationType::RangedProjectile;
    unit.animation.actType = 2;
    unit.physicalPower = 4;

    runBattleFrame(state);

    const auto& updated = state.units.requireCore(0);
    CHECK(updated.animation.cooldown == 0);
    CHECK(updated.animation.actType == -1);
    CHECK(updated.motion.velocity.x == 0.0f);
    CHECK(updated.motion.velocity.y == 0.0f);
    CHECK(updated.vitals.mp == 21);
    CHECK(updated.physicalPower == 5);
    CHECK_FALSE(updated.haveAction);
}

TEST_CASE("BattleFrameRunner_RunFrame_AppliesRuntimeMpRegenBlockAndRecovery", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    seedRuntimeUnits(state, {
        teamRuntimeUnit(0, 0, 80),
        teamRuntimeUnit(1, 1, 100),
    });
    state.units.require(0).status.effects.mpBlockTimer = 2;
    KysChess::ChessBattleEffects::applyEffect(
        state.units.require(0).combo,
        { KysChess::EffectType::MPRecoveryBonus, 100 });

    runBattleFrame(state);

    CHECK(state.units.requireCore(0).vitals.mp == 20);
    CHECK(state.units.requireCore(1).vitals.mp == 21);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_QueuesSkillFinishedTeamHealInsideFrameState", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();

    KysChess::RoleComboState combo;
    KysChess::ChessBattleEffects::applyEffect(combo, { KysChess::EffectType::PostSkillInvincFrames, 12 });
    combo.onSkillTeamHealPending = true;
    KysChess::ChessBattleEffects::applyEffect(combo, { KysChess::EffectType::OnSkillTeamHeal, 7 });
    KysChess::ChessBattleEffects::applyEffect(combo, { KysChess::EffectType::OnSkillTeamHealPct, 3 });
    state.units.require(0).combo = combo;
    seedRuntimeUnits(state, {
        teamRuntimeUnit(0, 0, 80),
    });
    applyRuntimeInput(state.units.requireCore(0), finishingSkillRuntime());

    runBattleFrame(state);

    CHECK(state.units.requireCore(0).animation.cooldown == 0);
    CHECK(state.units.requireCore(0).vitals.hp == 90);
    CHECK_FALSE(state.units.require(0).combo.onSkillTeamHealPending);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AppliesSkillFinishedTeamHealToRuntimeUnits", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    seedRuntimeUnits(state, {
        teamRuntimeUnit(0, 0, 50),
        teamRuntimeUnit(1, 0, 90),
        teamRuntimeUnit(2, 1, 10),
    });

    KysChess::RoleComboState combo;
    combo.onSkillTeamHealPending = true;
    KysChess::ChessBattleEffects::applyEffect(combo, { KysChess::EffectType::OnSkillTeamHeal, 5 });
    KysChess::ChessBattleEffects::applyEffect(combo, { KysChess::EffectType::OnSkillTeamHealPct, 10 });
    state.units.require(0).combo = combo;
    applyRuntimeInput(state.units.requireCore(0), finishingSkillRuntime());

    auto result = runBattleFrame(state);

    CHECK(state.units.requireCore(0).vitals.hp == 65);
    CHECK(state.units.requireCore(1).vitals.hp == 100);
    CHECK(state.units.requireCore(2).vitals.hp == 10);
    CHECK(std::count_if(
        result.visualEvents.begin(),
        result.visualEvents.end(),
        [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::RoleEffect
                && event.effectId == HealEffectId;
        }) == 2);

    const auto healLog = std::find_if(
        result.logEvents.begin(),
        result.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Heal
                && event.sourceUnitId == 0
                && event.targetUnitId == 0;
        });
    REQUIRE(healLog != result.logEvents.end());
    CHECK(healLog->amount == 15);
    CHECK(BattleLogTest::textOf(*healLog) == "技能群療");
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ConvertsPoisonTickToDamageTransaction", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    state.movement.frame = 30;
    state.status.config.poisonDamageIntervalFrames = 30;

    BattleStatusUnitState poisoned;
    poisoned.id = 1;
    poisoned.alive = true;
    poisoned.hp = 80;
    poisoned.maxHp = 100;
    poisoned.effects.poisonTimer = 3;
    poisoned.effects.poisonTickPct = 10;
    poisoned.effects.poisonSourceId = 0;
    seedRuntimeUnits(state, {
        teamRuntimeUnit(0, 0, 100),
        teamRuntimeUnit(1, 1, 80),
    });
    state.units.require(1).status = runtimeStatusUnit(poisoned);
    seedDamageExtrasFromUnits(state);

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1) == std::vector<int>{ 8 });
    CHECK(damageLogSourceIdsFor(result, 1) == std::vector<int>{ 0 });
    CHECK(state.units.requireCore(1).vitals.hp == 72);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ConvertsBleedTickToDamageTransaction", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    state.status.config.bleedDamageIntervalFrames = 10;

    BattleStatusUnitState bleeding;
    bleeding.id = 1;
    bleeding.alive = true;
    bleeding.hp = 80;
    bleeding.maxHp = 100;
    bleeding.effects.bleedStacks = 6;
    bleeding.effects.bleedTimer = 1;
    bleeding.effects.bleedSourceId = 0;
    seedRuntimeUnits(state, {
        teamRuntimeUnit(0, 0, 100),
        teamRuntimeUnit(1, 1, 80),
    });
    state.units.require(1).status = runtimeStatusUnit(bleeding);
    seedDamageExtrasFromUnits(state);

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1) == std::vector<int>{ 6 });
    CHECK(damageLogSourceIdsFor(result, 1) == std::vector<int>{ 0 });
    CHECK(state.units.requireCore(1).vitals.hp == 74);
    auto bleedLog = std::find_if(result.logEvents.begin(), result.logEvents.end(), [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Damage
                && event.targetUnitId == 1
                && event.amount == 6
                && BattleLogTest::textOf(event) == "流血";
        });
    CHECK(bleedLog != result.logEvents.end());
    auto bleedNumber = std::find_if(result.visualEvents.begin(), result.visualEvents.end(), [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::DamageNumber
                && event.targetUnitId == 1
                && event.amount == 6
                && event.color.r == 190
                && event.color.g == 120
                && event.color.b == 60;
        });
    CHECK(bleedNumber != result.visualEvents.end());
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DecrementsInvincibility", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    seedRuntimeUnits(state, {
        teamRuntimeUnit(0, 0, 100),
        teamRuntimeUnit(1, 1, 100),
    });
    BattleStatusUnitState status0;
    status0.id = 0;
    BattleStatusUnitState status1;
    status1.id = 1;
    state.units.require(0).status = runtimeStatusUnit(status0);
    state.units.require(1).status = runtimeStatusUnit(status1);
    state.units.requireCore(0).invincible = 3;

    runBattleFrame(state);

    CHECK(state.units.requireCore(0).invincible == 2);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AppliesFrameRuntimeTeamEffects", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    state.movement.frame = 6;
    state.teamEffects.healAuraRadius = SceneTileWidth * 6.0;
    seedRuntimeUnits(state, {
        teamRuntimeUnitAt(0, 0, 50, { 0, 0, 0 }),
        teamRuntimeUnitAt(1, 0, 80, { 100, 0, 0 }, 50),
        teamRuntimeUnitAt(2, 0, 60, { 300, 0, 0 }, 50),
        teamRuntimeUnitAt(3, 1, 20, { 50, 0, 0 }),
    });

    KysChess::RoleComboState combo;
    KysChess::ChessBattleEffects::applyEffect(combo, { KysChess::EffectType::HPRegenPct, 20, 6 });
    KysChess::ChessBattleEffects::applyEffect(combo, { KysChess::EffectType::HealAuraFlat, 5, 6 });
    KysChess::ChessBattleEffects::applyEffect(combo, { KysChess::EffectType::HealAuraPct, 10, 6 });
    KysChess::ChessBattleEffects::applyEffect(combo, { KysChess::EffectType::HealedATKSPDBoost, 20 });
    state.units.require(0).combo = combo;

    auto runtime = finishingSkillRuntime();
    runtime.state.cooldown = 0;
    applyRuntimeInput(state.units.requireCore(0), runtime);

    auto result = runBattleFrame(state);

    CHECK(state.units.requireCore(0).vitals.hp == 70);
    CHECK(state.units.requireCore(1).vitals.hp == 95);
    CHECK(state.units.requireCore(1).animation.cooldown == 39);
    CHECK(state.units.requireCore(2).vitals.hp == 60);
    CHECK(state.units.requireCore(2).animation.cooldown == 49);
    CHECK(state.units.requireCore(3).vitals.hp == 20);

    CHECK(std::count_if(
        result.visualEvents.begin(),
        result.visualEvents.end(),
        [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::RoleEffect
                && event.effectId == HealEffectId;
        }) == 2);

    const auto selfRegenLog = std::find_if(
        result.logEvents.begin(),
        result.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Heal
                && event.targetUnitId == 0
                && BattleLogTest::textOf(event) == "生命回復";
        });
    CHECK(selfRegenLog != result.logEvents.end());

    const auto auraLog = std::find_if(
        result.logEvents.begin(),
        result.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Heal
                && event.targetUnitId == 1
                && BattleLogTest::textOf(event) == "治療光環";
        });
    CHECK(auraLog != result.logEvents.end());
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AppliesBurstHealFrameTrigger", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    seedRuntimeUnits(state, {
        teamRuntimeUnit(0, 0, 40),
    });

    KysChess::AppliedEffectInstance healBurst;
    healBurst.type = KysChess::EffectType::HealBurst;
    healBurst.trigger = KysChess::Trigger::WhileLowHP;
    healBurst.triggerValue = 50;
    healBurst.value = 25;
    healBurst.maxCount = 1;

    KysChess::RoleComboState combo;
    combo.triggeredEffects.push_back(healBurst);
    state.units.require(0).combo = combo;

    auto runtime = finishingSkillRuntime();
    runtime.state.cooldown = 0;
    applyRuntimeInput(state.units.requireCore(0), runtime);

    auto result = runBattleFrame(state);

    CHECK(state.units.requireCore(0).vitals.hp == 65);
    CHECK(state.units.require(0).combo.effectActivationCounts.at(0) == 1);

    const auto healLog = std::find_if(
        result.logEvents.begin(),
        result.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Heal
                && event.sourceUnitId == 0
                && event.targetUnitId == 0
                && event.amount == 25
                && BattleLogTest::textOf(event) == "爆發治療";
        });
    CHECK(healLog != result.logEvents.end());
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DoesNotApplyPostSkillInvincibilityOnSkillFinish", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    seedRuntimeUnits(state, {
        teamRuntimeUnit(0, 0, 80),
    });
    state.units.requireCore(0).invincible = 3;

    KysChess::RoleComboState combo;
    KysChess::ChessBattleEffects::applyEffect(combo, { KysChess::EffectType::PostSkillInvincFrames, 12 });
    state.units.require(0).combo = combo;

    applyRuntimeInput(state.units.requireCore(0), finishingSkillRuntime());

    auto result = runBattleFrame(state);

    CHECK(state.units.requireCore(0).invincible == 2);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AppliesProjectileCancelDamageCommand", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    state.attacks.attacks.push_back(cancelProjectile(10, 0));
    state.attacks.attacks.push_back(cancelProjectile(20, 1));
    state.attacks.attacks[0].state.projectileCancelDamage = 25;
    state.attacks.attacks[1].state.projectileCancelDamage = 12;
    auto result = runBattleFrame(state);

    REQUIRE(result.gameplayEvents.size() == 3);
    CHECK(result.gameplayEvents[2].type == BattleGameplayEventType::ProjectileCancelled);
    CHECK(result.gameplayEvents[2].effectId == 10);
    CHECK(result.gameplayEvents[2].otherAttackId == 20);

    REQUIRE(state.attacks.attacks.size() == 2);
    CHECK(state.attacks.attacks[0].state.projectileCancelWeaken == 12);
    CHECK(state.attacks.attacks[1].state.projectileCancelWeaken == 25);

    REQUIRE(result.logEvents.size() == 1);
    const auto& log = result.logEvents[0];
    CHECK(log.type == BattleLogEventType::Status);
    CHECK(log.category == BattleLogCategory::ProjectileCancel);
    CHECK(log.sourceUnitId == 0);
    CHECK(log.targetUnitId == 1);
    CHECK(log.amount == 25);
    CHECK(log.secondaryAmount == 12);
}
