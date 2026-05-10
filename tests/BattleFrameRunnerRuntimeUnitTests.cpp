#include "battle/BattleCore.h"
#include "battle/BattleHitResolver.h"
#include "battle/BattleRuntimeSession.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <variant>

using namespace KysChess::Battle;

namespace
{

constexpr double SceneTileWidth = 36.0;
constexpr double MaxEffectiveBattleReach = 480.0;
constexpr double LegacyMinimumVectorNorm = 0.0001;

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

BattleRuntimeState runtimeFrameState()
{
    BattleRuntimeState state;
    state.world.frame = 6;
    state.world.config = runtimeMovementConfig();
    state.world.units = {
        runtimeUnit(0, 0, { 100, 100, 0 }),
        runtimeUnit(1, 1, { 500, 100, 0 }),
    };
    state.attacks.hitRadius = SceneTileWidth * 2.0;
    state.attacks.minimumVectorNorm = LegacyMinimumVectorNorm;
    state.attacks.bounceSpawnDistance = SceneTileWidth * 1.5;
    state.attacks.defaultProjectileSpeed = SceneTileWidth / 3.0;
    return state;
}

BattleRuntimeState ownedRuntimeState()
{
    BattleRuntimeState runtime;
    runtime.world.frame = 6;
    runtime.world.config = runtimeMovementConfig();
    runtime.world.units = {
        runtimeUnit(0, 0, { 100, 100, 0 }),
        runtimeUnit(1, 1, { 500, 100, 0 }),
    };
    runtime.attacks.hitRadius = SceneTileWidth * 2.0;
    runtime.attacks.minimumVectorNorm = LegacyMinimumVectorNorm;
    runtime.attacks.bounceSpawnDistance = SceneTileWidth * 1.5;
    runtime.attacks.defaultProjectileSpeed = SceneTileWidth / 3.0;
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

BattleStatusRuntimeUnit runtimeStatusUnit(const BattleStatusUnitState& unit)
{
    return makeBattleStatusRuntimeUnit(unit);
}

}  // namespace

TEST_CASE("BattleRuntimeState_RunFrame_OwnsPendingAttackSpawnsAcrossFrames", "[battle][frame_runner][runtime][ownership]")
{
    auto runtime = ownedRuntimeState();
    runtime.attacks.nextAttackId = 70;
    runtime.attacks.units = {
        { 1, 0, true, false, false, { 100, 120, 0 } },
        { 2, 1, true, false, false, { 106, 120, 0 } },
    };

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
    CHECK(first.frame.snapshot.frame == 7);
    CHECK(second.frame.snapshot.frame == 8);
}

TEST_CASE("BattleRuntimeSession_RunFrame_OwnsRuntimeAcrossFrames", "[battle][runtime_session][ownership]")
{
    auto runtime = ownedRuntimeState();
    runtime.attacks.nextAttackId = 70;
    runtime.attacks.units = {
        { 1, 0, true, false, false, { 100, 120, 0 } },
        { 2, 1, true, false, false, { 106, 120, 0 } },
    };

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

    BattleRuntimeInit init;
    init.runtime = std::move(runtime);
    BattleRuntimeSession session(std::move(init));

    const auto first = session.runFrame();
    const auto second = session.runFrame();

    CHECK(session.runtime().pendingAttackSpawns.empty());
    REQUIRE(session.runtime().attacks.attacks.size() == 1);
    CHECK(session.runtime().attacks.attacks[0].id == 70);
    CHECK(session.runtime().attacks.nextAttackId == 71);
    CHECK(first.frame.snapshot.frame == 7);
    CHECK(second.frame.snapshot.frame == 8);
}

TEST_CASE("BattleRuntimeSession_CommitSetupConfigurationAppliesOwnedRuntimeSetup", "[battle][runtime_session][ownership]")
{
    BattleRuntimeInit init;
    init.runtime = ownedRuntimeState();
    BattleRuntimeSession session(std::move(init));

    BattleRuntimeSetupConfiguration config;
    config.world.frame = 42;
    config.world.config = runtimeMovementConfig();
    config.world.units = {
        runtimeUnit(0, 0, { 128, 256, 0 }),
    };
    config.attacks.nextAttackId = 90;
    config.action.castFrames = { 0, 2, 3, 4 };
    config.damage.aggregatePendingTransactionsByDefender = true;

    session.commitSetupConfiguration(std::move(config));

    CHECK(session.runtime().world.frame == 42);
    REQUIRE(session.runtime().world.units.size() == 1);
    CHECK(session.runtime().world.units[0].id == 0);
    CHECK(session.runtime().attacks.nextAttackId == 90);
    CHECK(session.runtime().action.castFrames.size() == 4);
    CHECK(session.runtime().damage.aggregatePendingTransactionsByDefender);
}

TEST_CASE("BattleRuntimeSession_CommitSetupPlacementUpdatesOwnedRuntime", "[battle][runtime_session][ownership]")
{
    BattleRuntimeInit init;
    init.runtime = ownedRuntimeState();
    init.runtime.units.gridTransform = { SceneTileWidth, 18 };
    init.runtime.units.units = {
        teamRuntimeUnitAt(0, 0, 100, { 100, 100, 0 }),
    };
    init.runtime.world.units = {
        runtimeUnit(0, 0, { 100, 100, 0 }),
    };
    BattleRuntimeSession session(std::move(init));

    BattleSetupPlacementInput placement;
    placement.units.push_back({ 0, 3, 4, 2 });

    session.commitSetupPlacement(placement);

    const auto& unit = session.runtime().units.requireUnit(0);
    CHECK(unit.grid.x == 3);
    CHECK(unit.grid.y == 4);
    CHECK(unit.motion.facing.x == -1.0f);
    CHECK(unit.motion.facing.y == 0.0f);
    REQUIRE(session.runtime().world.units.size() == 1);
    CHECK(session.runtime().world.units[0].position.x == unit.motion.position.x);
    CHECK(session.runtime().world.units[0].position.y == unit.motion.position.y);
}

TEST_CASE("BattleFrameRunner_RunFrame_UsesRuntimeOwnedFrameState", "[battle][frame_runner][runtime]")
{
    auto state = runtimeFrameState();

    auto result = BattleFrameRunner().runFrame(state);

    CHECK(result.unitApplications.empty());
}

TEST_CASE("BattleFrameRunner_RunFrame_PublishesStateApplications", "[battle][frame_runner][runtime]")
{
    auto state = runtimeFrameState();
    state.units.units = {
        teamRuntimeUnit(0, 0, 80),
    };
    KysChess::RoleComboState combo;
    combo.shield = 12;
    combo.blockFirstHitsRemaining = 2;
    state.combo.units.emplace(0, combo);
    state.units.requireUnit(0).invincible = 4;
    BattleStatusRuntimeUnit status;
    status.id = 0;
    status.effects.frozenTimer = 3;
    status.effects.frozenMaxTimer = 9;
    state.status.units.push_back(status);

    auto result = runBattleFrame(state);

    REQUIRE(result.stateApplications.statusRenderUnits.size() == 1);
    CHECK(result.stateApplications.statusRenderUnits[0].unitId == 0);
    CHECK(result.stateApplications.statusRenderUnits[0].invincible == 4);
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
    state.units.units = {
        teamRuntimeUnit(0, 0, 80),
        teamRuntimeUnit(1, 1, 100),
    };
    auto& unit = state.units.requireUnit(0);
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
    CHECK(state.units.requireUnit(0).animation.cooldown == 0);
    CHECK(state.units.requireUnit(0).vitals.mp == 21);
    CHECK(state.units.requireUnit(0).physicalPower == 5);
    CHECK_FALSE(state.units.requireUnit(0).haveAction);
}

TEST_CASE("BattleFrameRunner_RunFrame_AppliesRuntimeMpRegenBlockAndRecovery", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    state.units.units = {
        teamRuntimeUnit(0, 0, 80),
        teamRuntimeUnit(1, 1, 100),
    };
    state.units.requireUnit(0).mpRecoveryBonusPct = 100;
    state.units.requireUnit(0).mpBlocked = true;

    auto result = runBattleFrame(state);

    REQUIRE(result.unitApplications.size() == 2);
    CHECK(state.units.requireUnit(0).vitals.mp == 20);
    CHECK(result.unitApplications[0].finalMp == 20);
    CHECK(state.units.requireUnit(1).vitals.mp == 21);
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
    state.units.units = {
        teamRuntimeUnit(0, 0, 80),
    };
    applyRuntimeInput(state.units.requireUnit(0), finishingSkillRuntime());

    auto result = runBattleFrame(state);

    REQUIRE(result.unitApplications.size() == 1);
    CHECK(result.unitApplications[0].unitId == 0);
    CHECK(result.unitApplications[0].cooldown == 0);

    CHECK(state.teamEffects.pendingCommands.empty());
    REQUIRE(result.effectCommands.size() == 1);
    CHECK(result.effectCommands[0].type == BattleEffectCommandType::AddInvincibility);
    CHECK(result.effectCommands[0].targetUnitId == 0);
    CHECK(result.effectCommands[0].value == 12);
    REQUIRE(result.teamEffectEvents.size() == 1);
    CHECK(result.teamEffectEvents[0].sourceUnitId == 0);
    CHECK(result.teamEffectEvents[0].targetUnitId == 0);
    CHECK(result.teamEffectEvents[0].value == 10);
    CHECK_FALSE(state.combo.units.at(0).onSkillTeamHealPending);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AppliesSkillFinishedTeamHealToUnitStore", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    state.units.units = {
        teamRuntimeUnit(0, 0, 50),
        teamRuntimeUnit(1, 0, 90),
        teamRuntimeUnit(2, 1, 10),
    };

    KysChess::RoleComboState combo;
    combo.onSkillTeamHealPending = true;
    combo.onSkillTeamHeal = 5;
    combo.onSkillTeamHealPct = 10;
    state.combo.units.emplace(0, combo);
    applyRuntimeInput(state.units.requireUnit(0), finishingSkillRuntime());

    auto result = runBattleFrame(state);

    REQUIRE(result.teamEffectEvents.size() == 2);
    CHECK(result.teamEffectEvents[0].type == BattleTeamEffectEventType::Heal);
    CHECK(result.teamEffectEvents[0].sourceUnitId == 0);
    CHECK(result.teamEffectEvents[0].targetUnitId == 0);
    CHECK(result.teamEffectEvents[0].value == 15);
    CHECK(result.teamEffectEvents[1].targetUnitId == 1);
    CHECK(result.teamEffectEvents[1].value == 10);
    CHECK(state.units.requireUnit(0).vitals.hp == 65);
    CHECK(state.units.requireUnit(1).vitals.hp == 100);
    CHECK(state.units.requireUnit(2).vitals.hp == 10);

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
    CHECK(healLog->text == "技能群療");
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ConvertsPoisonTickToDamageTransaction", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    state.world.frame = 30;
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

    state.units.units = {
        teamRuntimeUnit(0, 0, 100),
        teamRuntimeUnit(1, 1, 80),
    };
    state.deathEffects.store.units = { { 0 }, { 1 } };

    auto result = runBattleFrame(state);

    REQUIRE(result.damageTransactions.size() == 1);
    const auto& transaction = result.damageTransactions[0];
    CHECK(transaction.finalHpDamage == 8);
    CHECK(transaction.defender.id == 1);
    CHECK(transaction.defender.vitals.hp == 72);
    CHECK(state.units.requireUnit(1).vitals.hp == 72);
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

    state.units.units = {
        teamRuntimeUnit(0, 0, 100),
        teamRuntimeUnit(1, 1, 80),
    };
    state.deathEffects.store.units = { { 0 }, { 1 } };

    auto result = runBattleFrame(state);

    REQUIRE(result.damageTransactions.size() == 1);
    const auto& transaction = result.damageTransactions[0];
    CHECK(transaction.finalHpDamage == 6);
    CHECK(transaction.defender.id == 1);
    CHECK(transaction.defender.vitals.hp == 74);
    CHECK(state.units.requireUnit(1).vitals.hp == 74);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AppliesFrameRuntimeTeamEffects", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    state.world.frame = 6;
    state.teamEffects.healAuraRadius = SceneTileWidth * 6.0;
    state.units.units = {
        teamRuntimeUnitAt(0, 0, 50, { 0, 0, 0 }),
        teamRuntimeUnitAt(1, 0, 80, { 100, 0, 0 }, 50),
        teamRuntimeUnitAt(2, 0, 60, { 300, 0, 0 }, 50),
        teamRuntimeUnitAt(3, 1, 20, { 50, 0, 0 }),
    };

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
    applyRuntimeInput(state.units.requireUnit(0), runtime);

    auto result = runBattleFrame(state);

    CHECK(state.units.requireUnit(0).vitals.hp == 70);
    CHECK(state.units.requireUnit(1).vitals.hp == 95);
    CHECK(state.units.requireUnit(1).animation.cooldown == 39);
    CHECK(state.units.requireUnit(2).vitals.hp == 60);
    CHECK(state.units.requireUnit(2).animation.cooldown == 49);
    CHECK(state.units.requireUnit(3).vitals.hp == 20);

    REQUIRE(result.teamEffectEvents.size() == 3);
    CHECK(result.teamEffectEvents[0].targetUnitId == 0);
    CHECK(result.teamEffectEvents[0].value == 20);
    CHECK(result.teamEffectEvents[1].targetUnitId == 1);
    CHECK(result.teamEffectEvents[1].value == 15);
    CHECK(result.teamEffectEvents[2].type == BattleTeamEffectEventType::CooldownReduced);
    CHECK(result.teamEffectEvents[2].value == 10);

    const auto selfRegenLog = std::find_if(
        result.frame.logEvents.begin(),
        result.frame.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Heal
                && event.targetUnitId == 0
                && event.text == "生命回復";
        });
    CHECK(selfRegenLog != result.frame.logEvents.end());

    const auto auraLog = std::find_if(
        result.frame.logEvents.begin(),
        result.frame.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Heal
                && event.targetUnitId == 1
                && event.text == "治療光環";
        });
    CHECK(auraLog != result.frame.logEvents.end());
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AppliesBurstHealFrameTrigger", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    state.units.units = {
        teamRuntimeUnit(0, 0, 40),
    };

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
    applyRuntimeInput(state.units.requireUnit(0), runtime);

    auto result = runBattleFrame(state);

    CHECK(state.units.requireUnit(0).vitals.hp == 65);
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
                && event.text == "爆發治療";
        });
    CHECK(healLog != result.frame.logEvents.end());
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AppliesPostSkillInvincibilityThroughEffectExecutor", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    state.units.units = {
        teamRuntimeUnit(0, 0, 80),
    };
    state.units.units[0].invincible = 3;

    KysChess::RoleComboState combo;
    combo.postSkillInvincFrames = 12;
    state.combo.units.emplace(0, combo);

    applyRuntimeInput(state.units.requireUnit(0), finishingSkillRuntime());

    auto result = runBattleFrame(state);

    REQUIRE(result.effectCommands.size() == 1);
    CHECK(result.effectCommands[0].type == BattleEffectCommandType::AddInvincibility);
    CHECK(result.effectCommands[0].label == "技能後無敵");
    CHECK(result.effectCommands[0].value == 9);
    CHECK(state.units.requireUnit(0).invincible == 12);

    const auto statusLog = std::find_if(
        result.frame.logEvents.begin(),
        result.frame.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Status
                && event.sourceUnitId == 0
                && event.targetUnitId == 0
                && event.amount == 9
                && event.text == "技能後無敵";
        });
    CHECK(statusLog != result.frame.logEvents.end());
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AppliesProjectileCancelDamageCommand", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    state.attacks.units = {
        { 0, 0, true, false, false, { 100, 100, 0 } },
        { 1, 1, true, false, false, { 900, 900, 0 } },
    };
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
