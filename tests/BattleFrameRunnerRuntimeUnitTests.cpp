#include "battle/BattleCore.h"
#include "BattleLogTestHelpers.h"
#include "battle/BattleHitResolver.h"
#include "battle/BattleRuntimeSession.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <variant>
#include <vector>

using namespace KysChess::Battle;

namespace
{

constexpr double SceneTileWidth = 36.0;
constexpr double MaxEffectiveBattleReach = 480.0;
constexpr double LegacyMinimumVectorNorm = 0.0001;
constexpr int HealEffectId = 0;

BattleFrameResult runBattleFrame(BattleRuntimeState& state)
{
    return BattleFrameRunner().runFrame(state);
}
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
    state.realRoleId = id;
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
    unit.realRoleId = worldUnit.realRoleId;
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
    state.unitStore.units.clear();
    state.movement.agents.clear();
    for (const auto& worldUnit : units)
    {
        state.unitStore.units.push_back(runtimeUnitFromWorld(worldUnit));
        BattleMovementAgentState agent;
        agent.physics.position = worldUnit.position;
        agent.physics.velocity = worldUnit.velocity;
        state.movement.agents.emplace(worldUnit.id, agent);
    }
}

void seedEmptyComboStatesFromUnits(BattleRuntimeState& state)
{
    state.combo.units.clear();
    for (const auto& unit : state.unitStore.units)
    {
        state.combo.units.emplace(unit.id, KysChess::RoleComboState{});
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
    state.attacks.minimumVectorNorm = LegacyMinimumVectorNorm;
    state.attacks.bounceSpawnDistance = SceneTileWidth * 1.5;
    state.attacks.defaultProjectileSpeed = SceneTileWidth / 3.0;
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
    runtime.attacks.minimumVectorNorm = LegacyMinimumVectorNorm;
    runtime.attacks.bounceSpawnDistance = SceneTileWidth * 1.5;
    runtime.attacks.defaultProjectileSpeed = SceneTileWidth / 3.0;
    seedEmptyComboStatesFromUnits(runtime);
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
    state.unitStore.units = units;
    state.movement.agents.clear();
    state.movement.movementReservations.clear();
    for (const auto& unit : state.unitStore.units)
    {
        BattleMovementAgentState agent;
        agent.physics.position = unit.motion.position;
        agent.physics.velocity = unit.motion.velocity;
        agent.physics.acceleration = unit.motion.acceleration;
        state.movement.agents.emplace(unit.id, agent);
    }
}

BattleStatusRuntimeUnit runtimeStatusUnit(const BattleStatusUnitState& unit)
{
    return makeBattleStatusRuntimeUnit(unit);
}

void seedDamageExtrasFromUnits(BattleRuntimeState& state)
{
    state.combo.units.clear();
    state.damage.unitExtras.clear();
    for (const auto& unit : state.unitStore.units)
    {
        state.combo.units.emplace(unit.id, KysChess::RoleComboState{});
        state.damage.unitExtras.push_back(makeBattleDamageRuntimeUnit(
            makeBattleDamageUnitState(unit, static_cast<const BattleDamageRuntimeUnit*>(nullptr))));
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
    runtime.pendingAttackSpawns.push_back(request);

    auto first = runBattleFrame(runtime);
    auto second = runBattleFrame(runtime);

    CHECK(runtime.pendingAttackSpawns.empty());
    REQUIRE(runtime.attacks.attacks.size() == 1);
    CHECK(runtime.attacks.attacks[0].id == 70);
    CHECK(runtime.attacks.nextAttackId == 71);
    CHECK(first.frame.frame == 7);
    CHECK(second.frame.frame == 8);
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
    runtime.pendingAttackSpawns.push_back(request);

    BattleRuntimeSession session(std::move(runtime));

    const auto first = session.runFrame();
    const auto second = session.runFrame();

    CHECK(session.runtime().pendingAttackSpawns.empty());
    REQUIRE(session.runtime().attacks.attacks.size() == 1);
    CHECK(session.runtime().attacks.attacks[0].id == 70);
    CHECK(session.runtime().attacks.nextAttackId == 71);
    CHECK(first.frame.frame == 7);
    CHECK(second.frame.frame == 8);
}

TEST_CASE("BattleRuntimeSession_CreateInitializedBuildsOwnedRuntimeStores", "[battle][runtime_session][ownership]")
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
    input.comboStates.emplace(0, KysChess::RoleComboState{});

    auto session = BattleRuntimeSession::createInitialized(std::move(input)).session;

    CHECK(session.runtime().movement.frame == 42);
    REQUIRE(session.runtime().unitStore.units.size() == 1);
    CHECK(session.runtime().unitStore.units[0].id == 0);
    REQUIRE(session.runtime().movement.agents.size() == 1);
    CHECK(session.runtime().movement.agents.contains(0));
    CHECK(session.runtime().action.castFrames == session.runtime().movementPhysics.actionCastFrames);
    CHECK(session.runtime().damage.sortPendingDamageByDefenderMagnitude);
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
    input.comboStates.emplace(0, KysChess::RoleComboState{});

    auto session = BattleRuntimeSession::createInitialized(std::move(input)).session;

    CHECK(session.runtime().attacks.spendNonThroughOnHit);
}

TEST_CASE("BattleRuntimeSession_CreateInitializedKeepsDerivedMotionStoresAlignedAfterFrame", "[battle][runtime_session][ownership]")
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
    input.comboStates.emplace(0, KysChess::RoleComboState{});

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
    input.comboStates.emplace(1, KysChess::RoleComboState{});

    auto session = BattleRuntimeSession::createInitialized(std::move(input)).session;
    session.runFrame();

    const auto& runtime = session.runtime();
    const auto& unit = runtime.unitStore.requireUnit(0);
    REQUIRE(runtime.unitStore.units.size() == 2);
    REQUIRE(runtime.movement.agents.contains(0));
    CHECK(runtime.movement.agents.at(0).physics.position.x == unit.motion.position.x);
    CHECK(runtime.movement.agents.at(0).physics.position.y == unit.motion.position.y);
}

TEST_CASE("BattleFrameRunner_RunFrame_UsesRuntimeOwnedFrameState", "[battle][frame_runner][runtime]")
{
    auto state = runtimeFrameState();

    auto result = BattleFrameRunner().runFrame(state);

    REQUIRE(result.unitApplications.size() == 2);
    CHECK(result.unitApplications[0].unitId == 0);
    CHECK(result.unitApplications[1].unitId == 1);
}

TEST_CASE("BattleFrameRunner_RunFrame_PublishesStateApplications", "[battle][frame_runner][runtime]")
{
    auto state = runtimeFrameState();
    seedRuntimeUnits(state, {
        teamRuntimeUnit(0, 0, 80),
    });
    state.combo.units.emplace(0, KysChess::RoleComboState{});
    state.unitStore.requireUnit(0).shield = 12;
    BattleDamageRuntimeUnit damage;
    damage.id = 0;
    damage.blockFirstHitsRemaining = 2;
    state.damage.unitExtras.push_back(damage);
    state.unitStore.requireUnit(0).invincible = 4;
    BattleStatusRuntimeUnit status;
    status.id = 0;
    status.effects.frozenTimer = 3;
    status.effects.frozenMaxTimer = 9;
    state.status.units.push_back(status);

    auto result = runBattleFrame(state);

    REQUIRE(result.stateApplications.statusRenderUnits.size() == 1);
    CHECK(result.stateApplications.statusRenderUnits[0].unitId == 0);
    CHECK(result.stateApplications.statusRenderUnits[0].invincible == 3);
    CHECK(result.stateApplications.statusRenderUnits[0].frozenFrames == 3);
    CHECK(result.stateApplications.statusRenderUnits[0].frozenMaxFrames == 9);
    REQUIRE(result.stateApplications.comboRenderUnits.size() == 1);
    CHECK(result.stateApplications.comboRenderUnits[0].unitId == 0);
    CHECK(result.stateApplications.comboRenderUnits[0].shield == 12);
    CHECK(result.stateApplications.comboRenderUnits[0].blockFirstHitsRemaining == 2);
}

TEST_CASE("BattleFrameRunner_RunFrame_AdvancesRuntimeUnitsFromUnitStore", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    seedRuntimeUnits(state, {
        teamRuntimeUnit(0, 0, 80),
        teamRuntimeUnit(1, 1, 100),
    });
    auto& unit = state.unitStore.requireUnit(0);
    unit.animation.cooldown = 1;
    unit.haveAction = true;
    unit.operationType = BattleOperationType::RangedProjectile;
    unit.animation.actType = 2;
    unit.physicalPower = 4;

    auto result = runBattleFrame(state);

    REQUIRE(result.unitApplications.size() == 2);
    CHECK(result.unitApplications[0].unitId == 0);
    CHECK(result.unitApplications[0].cooldown == 0);
    CHECK(result.unitApplications[0].actType == -1);
    CHECK_FALSE(result.unitApplications[0].resetDashVelocity);
    CHECK(state.unitStore.requireUnit(0).animation.cooldown == 0);
    CHECK(state.unitStore.requireUnit(0).vitals.mp == 21);
    CHECK(state.unitStore.requireUnit(0).physicalPower == 5);
    CHECK_FALSE(state.unitStore.requireUnit(0).haveAction);
}

TEST_CASE("BattleFrameRunner_RunFrame_AppliesRuntimeMpRegenBlockAndRecovery", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    seedRuntimeUnits(state, {
        teamRuntimeUnit(0, 0, 80),
        teamRuntimeUnit(1, 1, 100),
    });
    state.unitStore.requireUnit(0).mpRecoveryBonusPct = 100;
    state.unitStore.requireUnit(0).mpBlocked = true;

    auto result = runBattleFrame(state);

    REQUIRE(result.unitApplications.size() == 2);
    CHECK(state.unitStore.requireUnit(0).vitals.mp == 20);
    CHECK(result.unitApplications[0].finalMp == 20);
    CHECK(state.unitStore.requireUnit(1).vitals.mp == 21);
    CHECK(result.unitApplications[1].finalMp == 21);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_QueuesSkillFinishedTeamHealInsideFrameState", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();

    KysChess::RoleComboState combo;
    combo.postSkillInvincFrames = 12;
    combo.onSkillTeamHealPending = true;
    combo.onSkillTeamHeal = 7;
    combo.onSkillTeamHealPct = 3;
    state.combo.units.emplace(0, combo);
    seedRuntimeUnits(state, {
        teamRuntimeUnit(0, 0, 80),
    });
    applyRuntimeInput(state.unitStore.requireUnit(0), finishingSkillRuntime());

    auto result = runBattleFrame(state);

    REQUIRE(result.unitApplications.size() == 1);
    CHECK(result.unitApplications[0].unitId == 0);
    CHECK(result.unitApplications[0].cooldown == 0);

    CHECK(state.teamEffects.pendingCommands.empty());
    CHECK(result.effectCommands.empty());
    REQUIRE(result.teamEffectEvents.size() == 1);
    CHECK(result.teamEffectEvents[0].sourceUnitId == 0);
    CHECK(result.teamEffectEvents[0].targetUnitId == 0);
    CHECK(result.teamEffectEvents[0].value == 10);
    CHECK_FALSE(state.combo.units.at(0).onSkillTeamHealPending);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AppliesSkillFinishedTeamHealToUnitStore", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    seedRuntimeUnits(state, {
        teamRuntimeUnit(0, 0, 50),
        teamRuntimeUnit(1, 0, 90),
        teamRuntimeUnit(2, 1, 10),
    });

    KysChess::RoleComboState combo;
    combo.onSkillTeamHealPending = true;
    combo.onSkillTeamHeal = 5;
    combo.onSkillTeamHealPct = 10;
    state.combo.units.emplace(0, combo);
    applyRuntimeInput(state.unitStore.requireUnit(0), finishingSkillRuntime());

    auto result = runBattleFrame(state);

    REQUIRE(result.teamEffectEvents.size() == 2);
    CHECK(result.teamEffectEvents[0].type == BattleTeamEffectEventType::Heal);
    CHECK(result.teamEffectEvents[0].sourceUnitId == 0);
    CHECK(result.teamEffectEvents[0].targetUnitId == 0);
    CHECK(result.teamEffectEvents[0].value == 15);
    CHECK(result.teamEffectEvents[1].targetUnitId == 1);
    CHECK(result.teamEffectEvents[1].value == 10);
    CHECK(state.unitStore.requireUnit(0).vitals.hp == 65);
    CHECK(state.unitStore.requireUnit(1).vitals.hp == 100);
    CHECK(state.unitStore.requireUnit(2).vitals.hp == 10);
    CHECK(std::count_if(
        result.frame.visualEvents.begin(),
        result.frame.visualEvents.end(),
        [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::RoleEffect
                && event.effectId == HealEffectId;
        }) == 2);

    const auto healLog = std::find_if(
        result.frame.logEvents.begin(),
        result.frame.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Heal
                && event.sourceUnitId == 0
                && event.targetUnitId == 0;
        });
    REQUIRE(healLog != result.frame.logEvents.end());
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
    state.status.units.push_back(runtimeStatusUnit(poisoned));

    seedRuntimeUnits(state, {
        teamRuntimeUnit(0, 0, 100),
        teamRuntimeUnit(1, 1, 80),
    });
    seedDamageExtrasFromUnits(state);
    state.deathEffects.store.units = { { 0 }, { 1 } };

    auto result = runBattleFrame(state);

    REQUIRE(result.damageTransactions.size() == 1);
    const auto& transaction = result.damageTransactions[0];
    CHECK(transaction.finalHpDamage == 8);
    CHECK(transaction.defender.id == 1);
    CHECK(transaction.defender.vitals.hp == 72);
    CHECK(state.unitStore.requireUnit(1).vitals.hp == 72);
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
    state.status.units.push_back(runtimeStatusUnit(bleeding));

    seedRuntimeUnits(state, {
        teamRuntimeUnit(0, 0, 100),
        teamRuntimeUnit(1, 1, 80),
    });
    seedDamageExtrasFromUnits(state);
    state.deathEffects.store.units = { { 0 }, { 1 } };

    auto result = runBattleFrame(state);

    REQUIRE(result.damageTransactions.size() == 1);
    const auto& transaction = result.damageTransactions[0];
    CHECK(transaction.finalHpDamage == 6);
    CHECK(transaction.defender.id == 1);
    CHECK(transaction.defender.vitals.hp == 74);
    CHECK(state.unitStore.requireUnit(1).vitals.hp == 74);
    auto bleedLog = std::find_if(result.frame.logEvents.begin(), result.frame.logEvents.end(), [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Damage
                && event.targetUnitId == 1
                && event.amount == 6
                && BattleLogTest::textOf(event) == "流血";
        });
    CHECK(bleedLog != result.frame.logEvents.end());
    auto bleedNumber = std::find_if(result.frame.visualEvents.begin(), result.frame.visualEvents.end(), [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::DamageNumber
                && event.targetUnitId == 1
                && event.amount == 6
                && event.color.r == 190
                && event.color.g == 120
                && event.color.b == 60;
        });
    CHECK(bleedNumber != result.frame.visualEvents.end());
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
    state.status.units = {
        runtimeStatusUnit(status0),
        runtimeStatusUnit(status1),
    };
    state.unitStore.requireUnit(0).invincible = 3;

    auto result = runBattleFrame(state);

    CHECK(state.unitStore.requireUnit(0).invincible == 2);
    auto applied = std::find_if(result.stateApplications.statusRenderUnits.begin(), result.stateApplications.statusRenderUnits.end(), [](const BattleFrameRenderStatusUnit& unit)
        {
            return unit.unitId == 0;
        });
    REQUIRE(applied != result.stateApplications.statusRenderUnits.end());
    CHECK(applied->invincible == 2);
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
    combo.hpRegenPct = 20;
    combo.hpRegenInterval = 6;
    combo.healAuraFlat = 5;
    combo.healAuraPct = 10;
    combo.healAuraInterval = 6;
    combo.healedATKSPDBoostPct = 20;
    state.combo.units.emplace(0, combo);

    auto runtime = finishingSkillRuntime();
    runtime.state.cooldown = 0;
    applyRuntimeInput(state.unitStore.requireUnit(0), runtime);

    auto result = runBattleFrame(state);

    CHECK(state.unitStore.requireUnit(0).vitals.hp == 70);
    CHECK(state.unitStore.requireUnit(1).vitals.hp == 95);
    CHECK(state.unitStore.requireUnit(1).animation.cooldown == 39);
    CHECK(state.unitStore.requireUnit(2).vitals.hp == 60);
    CHECK(state.unitStore.requireUnit(2).animation.cooldown == 49);
    CHECK(state.unitStore.requireUnit(3).vitals.hp == 20);

    REQUIRE(result.teamEffectEvents.size() == 3);
    CHECK(result.teamEffectEvents[0].targetUnitId == 0);
    CHECK(result.teamEffectEvents[0].value == 20);
    CHECK(result.teamEffectEvents[1].targetUnitId == 1);
    CHECK(result.teamEffectEvents[1].value == 15);
    CHECK(result.teamEffectEvents[2].type == BattleTeamEffectEventType::CooldownReduced);
    CHECK(result.teamEffectEvents[2].value == 10);
    CHECK(std::count_if(
        result.frame.visualEvents.begin(),
        result.frame.visualEvents.end(),
        [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::RoleEffect
                && event.effectId == HealEffectId;
        }) == 2);

    const auto selfRegenLog = std::find_if(
        result.frame.logEvents.begin(),
        result.frame.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Heal
                && event.targetUnitId == 0
                && BattleLogTest::textOf(event) == "生命回復";
        });
    CHECK(selfRegenLog != result.frame.logEvents.end());

    const auto auraLog = std::find_if(
        result.frame.logEvents.begin(),
        result.frame.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Heal
                && event.targetUnitId == 1
                && BattleLogTest::textOf(event) == "治療光環";
        });
    CHECK(auraLog != result.frame.logEvents.end());
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
    state.combo.units.emplace(0, combo);

    auto runtime = finishingSkillRuntime();
    runtime.state.cooldown = 0;
    applyRuntimeInput(state.unitStore.requireUnit(0), runtime);

    auto result = runBattleFrame(state);

    CHECK(state.unitStore.requireUnit(0).vitals.hp == 65);
    CHECK(state.combo.units.at(0).effectActivationCounts.at(0) == 1);

    const auto healLog = std::find_if(
        result.frame.logEvents.begin(),
        result.frame.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Heal
                && event.sourceUnitId == 0
                && event.targetUnitId == 0
                && event.amount == 25
                && BattleLogTest::textOf(event) == "爆發治療";
        });
    CHECK(healLog != result.frame.logEvents.end());
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DoesNotApplyPostSkillInvincibilityOnSkillFinish", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    seedRuntimeUnits(state, {
        teamRuntimeUnit(0, 0, 80),
    });
    state.unitStore.units[0].invincible = 3;

    KysChess::RoleComboState combo;
    combo.postSkillInvincFrames = 12;
    state.combo.units.emplace(0, combo);

    applyRuntimeInput(state.unitStore.requireUnit(0), finishingSkillRuntime());

    auto result = runBattleFrame(state);

    CHECK(result.effectCommands.empty());
    CHECK(state.unitStore.requireUnit(0).invincible == 3);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AppliesProjectileCancelDamageCommand", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    state.attacks.attacks.push_back(cancelProjectile(10, 0));
    state.attacks.attacks.push_back(cancelProjectile(20, 1));
    state.attacks.attacks[0].state.projectileCancelDamage = 25;
    state.attacks.attacks[1].state.projectileCancelDamage = 12;
    auto result = runBattleFrame(state);

    REQUIRE(result.attackEvents.size() == 3);
    const auto& cancel = result.attackEvents[2];
    CHECK(cancel.type == BattleAttackEventType::ProjectileCancel);
    CHECK(cancel.projectileCancelDamage == 25);
    CHECK(cancel.otherProjectileCancelDamage == 12);

    REQUIRE(result.projectileCancelDamageCommands.size() == 1);
    const auto* command = &result.projectileCancelDamageCommands[0];
    REQUIRE(command);
    CHECK(command->attackId == 10);
    CHECK(command->otherAttackId == 20);
    CHECK(command->sourceUnitId == 0);
    CHECK(command->otherSourceUnitId == 1);
    CHECK(command->damage == 25);
    CHECK(command->otherDamage == 12);

    REQUIRE(state.attacks.attacks.size() == 2);
    CHECK(state.attacks.attacks[0].state.projectileCancelWeaken == 12);
    CHECK(state.attacks.attacks[1].state.projectileCancelWeaken == 25);
}
