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

BattleFrameResult runBattleFrame(BattleRuntimeState& state, BattleFrameScratch& scratch)
{
    return BattleFrameRunner().runFrame(state, scratch);
}

BattleFrameResult runBattleFrame(BattleRuntimeState& state)
{
    BattleFrameScratch scratch;
    return runBattleFrame(state, scratch);
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
        runtimeUnit(1, 0, { 100, 100, 0 }),
        runtimeUnit(2, 1, { 500, 100, 0 }),
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
        runtimeUnit(1, 0, { 100, 100, 0 }),
        runtimeUnit(2, 1, { 500, 100, 0 }),
    };
    runtime.attacks.hitRadius = SceneTileWidth * 2.0;
    runtime.attacks.minimumVectorNorm = LegacyMinimumVectorNorm;
    runtime.attacks.bounceSpawnDistance = SceneTileWidth * 1.5;
    runtime.attacks.defaultProjectileSpeed = SceneTileWidth / 3.0;
    return runtime;
}

BattleFrameUnitRuntimeInput finishingSkillRuntime()
{
    BattleFrameUnitRuntimeInput input;
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

void applyRuntimeInput(BattleRuntimeUnit& unit, const BattleFrameUnitRuntimeInput& input)
{
    unit.cooldown = input.state.cooldown;
    unit.actFrame = input.state.actFrame;
    unit.actType = input.state.actType;
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
    attack.state.skillId = 100 + id;
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
    unit.hp = hp;
    unit.maxHp = 100;
    unit.mp = 20;
    unit.maxMp = 100;
    return unit;
}

BattleRuntimeUnit teamRuntimeUnitAt(int id, int team, int hp, Pointf position, int cooldown = 0)
{
    auto unit = teamRuntimeUnit(id, team, hp);
    unit.position = position;
    unit.cooldown = cooldown;
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
    request.initial.attackerUnitId = 1;
    request.initial.preferredTargetUnitId = 2;
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
    request.initial.attackerUnitId = 1;
    request.initial.preferredTargetUnitId = 2;
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

TEST_CASE("BattleRuntimeSession_QueuedAttackSpawnEntersOwnedRuntime", "[battle][runtime_session][ownership]")
{
    BattleRuntimeInit init;
    init.runtime = ownedRuntimeState();
    init.runtime.attacks.nextAttackId = 80;
    init.runtime.attacks.units = {
        { 1, 0, true, false, false, { 100, 120, 0 } },
        { 2, 1, true, false, false, { 106, 120, 0 } },
    };
    BattleRuntimeSession session(std::move(init));

    BattleAttackSpawnRequest request;
    request.initial.attackerUnitId = 1;
    request.initial.preferredTargetUnitId = 2;
    request.initial.skillId = 102;
    request.initial.totalFrame = 30;
    request.initial.visualEffectId = 45;
    request.initial.operationType = BattleOperationType::RangedProjectile;
    request.initial.position = { 100, 120, 0 };
    request.initial.velocity = { 6, 0, 0 };

    session.enqueueAttackSpawn(std::move(request));
    session.runFrame();

    CHECK(session.runtime().pendingAttackSpawns.empty());
    REQUIRE(session.runtime().attacks.attacks.size() == 1);
    CHECK(session.runtime().attacks.attacks[0].id == 80);
    CHECK(session.runtime().attacks.attacks[0].state.skillId == 102);
}

TEST_CASE("BattleFrameRunner_RunFrame_ConsumesExternalFrameScratch", "[battle][frame_runner][runtime][scratch]")
{
    auto state = runtimeFrameState();
    BattleFrameScratch scratch;

    scratch.runtime.percentRolls.push_back(12.0);
    scratch.projectileCancelBaseDamages.push_back({ 10, 20, 30 });
    scratch.hits.units.push_back({});
    scratch.hits.skills.push_back({});
    scratch.hits.items.push_back({});
    scratch.hits.scalars.push_back({});

    auto result = BattleFrameRunner().runFrame(state, scratch);

    CHECK(result.runtimeResults.empty());
    CHECK(scratch.runtime.percentRolls.empty());
    CHECK(scratch.projectileCancelBaseDamages.empty());
    CHECK(scratch.hits.units.empty());
    CHECK(scratch.hits.skills.empty());
    CHECK(scratch.hits.items.empty());
    CHECK(scratch.hits.scalars.empty());
}

TEST_CASE("BattleFrameRunner_RunFrame_AdvancesRuntimeUnitsFromUnitStore", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    state.units.units = {
        teamRuntimeUnit(1, 0, 80),
        teamRuntimeUnit(2, 1, 100),
    };
    auto& unit = state.units.requireUnit(1);
    unit.cooldown = 1;
    unit.haveAction = true;
    unit.operationType = BattleOperationType::RangedProjectile;
    unit.actType = 2;
    unit.physicalPower = 4;

    auto result = runBattleFrame(state);

    REQUIRE(result.runtimeResults.size() == 2);
    CHECK(result.runtimeResults[0].unitId == 1);
    CHECK(result.runtimeResults[0].result.skillFinished);
    CHECK(state.units.requireUnit(1).cooldown == 0);
    CHECK(state.units.requireUnit(1).mp == 21);
    CHECK(state.units.requireUnit(1).physicalPower == 5);
    CHECK_FALSE(state.units.requireUnit(1).haveAction);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_QueuesSkillFinishedTeamHealInsideFrameState", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    BattleFrameScratch scratch;

    KysChess::RoleComboState combo;
    combo.postSkillInvincFrames = 12;
    combo.onSkillTeamHealPending = true;
    combo.onSkillTeamHeal = 7;
    combo.onSkillTeamHealPct = 3;
    state.combo.units.emplace(1, combo);
    state.units.units = {
        teamRuntimeUnit(1, 0, 80),
    };
    applyRuntimeInput(state.units.requireUnit(1), finishingSkillRuntime());

    auto result = runBattleFrame(state, scratch);

    REQUIRE(result.runtimeResults.size() == 1);
    CHECK(result.runtimeResults[0].unitId == 1);
    CHECK(result.runtimeResults[0].result.skillFinished);
    REQUIRE(result.runtimeResults[0].comboEvents.size() == 1);
    CHECK(result.runtimeResults[0].comboEvents[0].type == BattleComboFrameRuntimeEventType::PostSkillInvincibility);

    REQUIRE(result.commands.empty());
    CHECK(state.teamEffects.pendingCommands.empty());
    REQUIRE(state.teamEffects.committedEvents.size() == 1);
    CHECK(state.teamEffects.committedEvents[0].sourceUnitId == 1);
    CHECK(state.teamEffects.committedEvents[0].targetUnitId == 1);
    CHECK(state.teamEffects.committedEvents[0].value == 10);
    CHECK_FALSE(state.combo.units.at(1).onSkillTeamHealPending);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AppliesSkillFinishedTeamHealToUnitStore", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    BattleFrameScratch scratch;
    state.units.units = {
        teamRuntimeUnit(1, 0, 50),
        teamRuntimeUnit(2, 0, 90),
        teamRuntimeUnit(3, 1, 10),
    };

    KysChess::RoleComboState combo;
    combo.onSkillTeamHealPending = true;
    combo.onSkillTeamHeal = 5;
    combo.onSkillTeamHealPct = 10;
    state.combo.units.emplace(1, combo);
    applyRuntimeInput(state.units.requireUnit(1), finishingSkillRuntime());

    auto result = runBattleFrame(state, scratch);

    REQUIRE(state.teamEffects.committedEvents.size() == 2);
    CHECK(state.teamEffects.committedEvents[0].type == BattleTeamEffectEventType::Heal);
    CHECK(state.teamEffects.committedEvents[0].sourceUnitId == 1);
    CHECK(state.teamEffects.committedEvents[0].targetUnitId == 1);
    CHECK(state.teamEffects.committedEvents[0].value == 15);
    CHECK(state.teamEffects.committedEvents[1].targetUnitId == 2);
    CHECK(state.teamEffects.committedEvents[1].value == 10);
    CHECK(state.units.requireUnit(1).hp == 65);
    CHECK(state.units.requireUnit(2).hp == 100);
    CHECK(state.units.requireUnit(3).hp == 10);

    const auto healLog = std::find_if(
        result.frame.logEvents.begin(),
        result.frame.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Heal
                && event.sourceUnitId == 1
                && event.targetUnitId == 1;
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
    poisoned.id = 2;
    poisoned.alive = true;
    poisoned.hp = 80;
    poisoned.maxHp = 100;
    poisoned.poisonTimer = 3;
    poisoned.poisonTickPct = 10;
    poisoned.poisonSourceId = 1;
    state.status.units.push_back(runtimeStatusUnit(poisoned));

    state.units.units = {
        teamRuntimeUnit(1, 0, 100),
        teamRuntimeUnit(2, 1, 80),
    };
    state.deathEffects.store.units = { { 1 }, { 2 } };

    runBattleFrame(state);

    REQUIRE(state.damage.committedTransactions.size() == 1);
    const auto& transaction = state.damage.committedTransactions[0];
    CHECK(transaction.finalHpDamage == 8);
    CHECK(transaction.defender.id == 2);
    CHECK(transaction.defender.hp == 72);
    CHECK(state.units.requireUnit(2).hp == 72);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ConvertsBleedTickToDamageTransaction", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    state.status.config.bleedDamageIntervalFrames = 10;

    BattleStatusUnitState bleeding;
    bleeding.id = 2;
    bleeding.alive = true;
    bleeding.hp = 80;
    bleeding.maxHp = 100;
    bleeding.bleedStacks = 6;
    bleeding.bleedTimer = 1;
    bleeding.bleedSourceId = 1;
    state.status.units.push_back(runtimeStatusUnit(bleeding));

    state.units.units = {
        teamRuntimeUnit(1, 0, 100),
        teamRuntimeUnit(2, 1, 80),
    };
    state.deathEffects.store.units = { { 1 }, { 2 } };

    runBattleFrame(state);

    REQUIRE(state.damage.committedTransactions.size() == 1);
    const auto& transaction = state.damage.committedTransactions[0];
    CHECK(transaction.finalHpDamage == 6);
    CHECK(transaction.defender.id == 2);
    CHECK(transaction.defender.hp == 74);
    CHECK(state.units.requireUnit(2).hp == 74);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AppliesFrameRuntimeTeamEffects", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    BattleFrameScratch scratch;
    state.world.frame = 6;
    state.teamEffects.healAuraRadius = SceneTileWidth * 6.0;
    state.units.units = {
        teamRuntimeUnitAt(1, 0, 50, { 0, 0, 0 }),
        teamRuntimeUnitAt(2, 0, 80, { 100, 0, 0 }, 50),
        teamRuntimeUnitAt(3, 0, 60, { 300, 0, 0 }, 50),
        teamRuntimeUnitAt(4, 1, 20, { 50, 0, 0 }),
    };

    KysChess::RoleComboState combo;
    combo.hpRegenPct = 20;
    combo.hpRegenInterval = 6;
    combo.healAuraFlat = 5;
    combo.healAuraPct = 10;
    combo.healAuraInterval = 6;
    combo.healedATKSPDBoostPct = 20;
    state.combo.units.emplace(1, combo);

    auto runtime = finishingSkillRuntime();
    runtime.state.cooldown = 0;
    applyRuntimeInput(state.units.requireUnit(1), runtime);

    auto result = runBattleFrame(state, scratch);

    CHECK(state.units.requireUnit(1).hp == 70);
    CHECK(state.units.requireUnit(2).hp == 95);
    CHECK(state.units.requireUnit(2).cooldown == 39);
    CHECK(state.units.requireUnit(3).hp == 60);
    CHECK(state.units.requireUnit(3).cooldown == 49);
    CHECK(state.units.requireUnit(4).hp == 20);

    REQUIRE(state.teamEffects.committedEvents.size() == 3);
    CHECK(state.teamEffects.committedEvents[0].targetUnitId == 1);
    CHECK(state.teamEffects.committedEvents[0].value == 20);
    CHECK(state.teamEffects.committedEvents[1].targetUnitId == 2);
    CHECK(state.teamEffects.committedEvents[1].value == 15);
    CHECK(state.teamEffects.committedEvents[2].type == BattleTeamEffectEventType::CooldownReduced);
    CHECK(state.teamEffects.committedEvents[2].value == 10);

    const auto selfRegenLog = std::find_if(
        result.frame.logEvents.begin(),
        result.frame.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Heal
                && event.targetUnitId == 1
                && event.text == "生命回復";
        });
    CHECK(selfRegenLog != result.frame.logEvents.end());

    const auto auraLog = std::find_if(
        result.frame.logEvents.begin(),
        result.frame.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Heal
                && event.targetUnitId == 2
                && event.text == "治療光環";
        });
    CHECK(auraLog != result.frame.logEvents.end());
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AppliesBurstHealFrameTrigger", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    BattleFrameScratch scratch;
    state.units.units = {
        teamRuntimeUnit(1, 0, 40),
    };

    KysChess::AppliedEffectInstance healBurst;
    healBurst.type = KysChess::EffectType::HealBurst;
    healBurst.trigger = KysChess::Trigger::WhileLowHP;
    healBurst.triggerValue = 50;
    healBurst.value = 25;
    healBurst.maxCount = 1;

    KysChess::RoleComboState combo;
    combo.triggeredEffects.push_back(healBurst);
    state.combo.units.emplace(1, combo);

    auto runtime = finishingSkillRuntime();
    runtime.state.cooldown = 0;
    applyRuntimeInput(state.units.requireUnit(1), runtime);

    auto result = runBattleFrame(state, scratch);

    CHECK(state.units.requireUnit(1).hp == 65);
    CHECK(state.combo.units.at(1).effectActivationCounts.at(0) == 1);

    const auto healLog = std::find_if(
        result.frame.logEvents.begin(),
        result.frame.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Heal
                && event.sourceUnitId == 1
                && event.targetUnitId == 1
                && event.amount == 25
                && event.text == "爆發治療";
        });
    CHECK(healLog != result.frame.logEvents.end());
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AppliesPostSkillInvincibilityThroughEffectExecutor", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    BattleFrameScratch scratch;
    state.units.units = {
        teamRuntimeUnit(1, 0, 80),
    };
    state.units.units[0].invincible = 3;

    KysChess::RoleComboState combo;
    combo.postSkillInvincFrames = 12;
    state.combo.units.emplace(1, combo);

    applyRuntimeInput(state.units.requireUnit(1), finishingSkillRuntime());

    auto result = runBattleFrame(state, scratch);

    REQUIRE(state.effects.committedCommands.size() == 1);
    CHECK(state.effects.committedCommands[0].type == BattleEffectCommandType::AddInvincibility);
    CHECK(state.effects.committedCommands[0].label == "技能後無敵");
    CHECK(state.effects.committedCommands[0].value == 9);
    CHECK(state.units.requireUnit(1).invincible == 12);

    const auto statusLog = std::find_if(
        result.frame.logEvents.begin(),
        result.frame.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Status
                && event.sourceUnitId == 1
                && event.targetUnitId == 1
                && event.amount == 9
                && event.text == "技能後無敵";
        });
    CHECK(statusLog != result.frame.logEvents.end());
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AppliesProjectileCancelDamageCommand", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    BattleFrameScratch scratch;
    state.attacks.units = {
        { 1, 0, true, false, false, { 100, 100, 0 } },
        { 2, 1, true, false, false, { 900, 900, 0 } },
    };
    state.attacks.attacks.push_back(cancelProjectile(10, 1));
    state.attacks.attacks.push_back(cancelProjectile(20, 2));
    scratch.projectileCancelBaseDamages.push_back({ 10, -1, 25 });
    scratch.projectileCancelBaseDamages.push_back({ 20, -1, 12 });

    auto result = runBattleFrame(state, scratch);

    REQUIRE(result.attackEvents.size() == 3);
    const auto& cancel = result.attackEvents[2];
    CHECK(cancel.type == BattleAttackEventType::ProjectileCancel);
    CHECK(cancel.projectileCancelDamage == 25);
    CHECK(cancel.otherProjectileCancelDamage == 12);

    REQUIRE(result.commands.empty());
    REQUIRE(result.projectileCancelDamageCommands.size() == 1);
    const auto* command = &result.projectileCancelDamageCommands[0];
    REQUIRE(command);
    CHECK(command->attackId == 10);
    CHECK(command->otherAttackId == 20);
    CHECK(command->sourceUnitId == 1);
    CHECK(command->otherSourceUnitId == 2);
    CHECK(command->damage == 25);
    CHECK(command->otherDamage == 12);

    REQUIRE(state.attacks.attacks.size() == 2);
    CHECK(state.attacks.attacks[0].state.projectileCancelWeaken == 12);
    CHECK(state.attacks.attacks[1].state.projectileCancelWeaken == 25);
}
