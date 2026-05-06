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

BattleTeamEffectUnit teamEffectUnit(int id, int team, int hp)
{
    BattleTeamEffectUnit unit;
    unit.id = id;
    unit.team = team;
    unit.alive = true;
    unit.hp = hp;
    unit.maxHp = 100;
    return unit;
}

BattleTeamEffectUnit teamEffectUnitAt(int id, int team, int hp, Pointf position, int cooldown = 0)
{
    auto unit = teamEffectUnit(id, team, hp);
    unit.x = position.x;
    unit.y = position.y;
    unit.cooldown = cooldown;
    return unit;
}

const BattleTeamEffectUnit& teamEffectUnitById(const BattleTeamEffectWorld& world, int id)
{
    for (const auto& unit : world.units)
    {
        if (unit.id == id)
        {
            return unit;
        }
    }
    FAIL("team effect unit not found");
}

BattleDamageUnitState damageUnit(int id, int hp)
{
    BattleDamageUnitState unit;
    unit.id = id;
    unit.alive = true;
    unit.hp = hp;
    unit.maxHp = 100;
    return unit;
}

BattleCooldownState cooldownUnit()
{
    BattleCooldownState state;
    state.alive = true;
    return state;
}

BattleEffectUnit effectUnit(int id, int team, int hp, int invincible)
{
    BattleEffectUnit unit;
    unit.id = id;
    unit.team = team;
    unit.alive = true;
    unit.hp = hp;
    unit.maxHp = 100;
    unit.invincible = invincible;
    return unit;
}

const BattleEffectUnit& effectUnitById(const BattleEffectWorld& world, int id)
{
    for (const auto& unit : world.units)
    {
        if (unit.id == id)
        {
            return unit;
        }
    }
    FAIL("effect unit not found");
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

    auto first = BattleFrameRunner().runFrame(runtime);
    auto second = BattleFrameRunner().runFrame(runtime);

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

TEST_CASE("BattleFrameRunner_AdvanceFrame_QueuesSkillFinishedTeamHealInsideFrameState", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();

    KysChess::RoleComboState combo;
    combo.postSkillInvincFrames = 12;
    combo.onSkillTeamHealPending = true;
    combo.onSkillTeamHeal = 7;
    combo.onSkillTeamHealPct = 3;
    state.combo.units.emplace(1, combo);

    BattleFrameRuntimeUnitInput runtime;
    runtime.unitId = 1;
    runtime.input = finishingSkillRuntime();
    runtime.hp = 80;
    runtime.maxHp = 100;
    runtime.alive = true;
    state.runtime.units.push_back(runtime);

    auto result = BattleFrameRunner().runFrame(state);

    REQUIRE(state.runtime.committedResults.size() == 1);
    CHECK(state.runtime.committedResults[0].unitId == 1);
    CHECK(state.runtime.committedResults[0].result.skillFinished);
    REQUIRE(state.runtime.committedResults[0].comboEvents.size() == 1);
    CHECK(state.runtime.committedResults[0].comboEvents[0].type == BattleComboFrameRuntimeEventType::PostSkillInvincibility);

    REQUIRE(result.commands.empty());
    REQUIRE(state.teamEffects.pendingCommands.size() == 1);
    const auto* command = std::get_if<BattleTeamHealCommand>(&state.teamEffects.pendingCommands[0]);
    REQUIRE(command);
    CHECK(command->sourceUnitId == 1);
    CHECK(command->flatHeal == 7);
    CHECK(command->pctHeal == 3);
    CHECK(command->reason == "技能群療");
    CHECK_FALSE(state.combo.units.at(1).onSkillTeamHealPending);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AppliesSkillFinishedTeamHealToTeamWorld", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    state.teamEffects.world.units = {
        teamEffectUnit(1, 0, 50),
        teamEffectUnit(2, 0, 90),
        teamEffectUnit(3, 1, 10),
    };

    KysChess::RoleComboState combo;
    combo.onSkillTeamHealPending = true;
    combo.onSkillTeamHeal = 5;
    combo.onSkillTeamHealPct = 10;
    state.combo.units.emplace(1, combo);

    BattleFrameRuntimeUnitInput runtime;
    runtime.unitId = 1;
    runtime.input = finishingSkillRuntime();
    runtime.hp = 50;
    runtime.maxHp = 100;
    runtime.alive = true;
    state.runtime.units.push_back(runtime);

    auto result = BattleFrameRunner().runFrame(state);

    REQUIRE(state.teamEffects.committedEvents.size() == 2);
    CHECK(state.teamEffects.committedEvents[0].type == BattleTeamEffectEventType::Heal);
    CHECK(state.teamEffects.committedEvents[0].sourceUnitId == 1);
    CHECK(state.teamEffects.committedEvents[0].targetUnitId == 1);
    CHECK(state.teamEffects.committedEvents[0].value == 15);
    CHECK(state.teamEffects.committedEvents[1].targetUnitId == 2);
    CHECK(state.teamEffects.committedEvents[1].value == 10);
    CHECK(teamEffectUnitById(state.teamEffects.world, 1).hp == 65);
    CHECK(teamEffectUnitById(state.teamEffects.world, 2).hp == 100);
    CHECK(teamEffectUnitById(state.teamEffects.world, 3).hp == 10);

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
    state.status.units.push_back(poisoned);

    state.damage.units = {
        damageUnit(1, 100),
        damageUnit(2, 80),
    };
    state.damage.cooldowns.emplace(1, cooldownUnit());
    state.damage.cooldowns.emplace(2, cooldownUnit());

    BattleFrameRunner().runFrame(state);

    REQUIRE(state.damage.committedTransactions.size() == 1);
    const auto& transaction = state.damage.committedTransactions[0];
    CHECK(transaction.finalHpDamage == 8);
    CHECK(transaction.defender.id == 2);
    CHECK(transaction.defender.hp == 72);
    CHECK(state.status.units[0].hp == 72);
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
    state.status.units.push_back(bleeding);

    state.damage.units = {
        damageUnit(1, 100),
        damageUnit(2, 80),
    };
    state.damage.cooldowns.emplace(1, cooldownUnit());
    state.damage.cooldowns.emplace(2, cooldownUnit());

    BattleFrameRunner().runFrame(state);

    REQUIRE(state.damage.committedTransactions.size() == 1);
    const auto& transaction = state.damage.committedTransactions[0];
    CHECK(transaction.finalHpDamage == 6);
    CHECK(transaction.defender.id == 2);
    CHECK(transaction.defender.hp == 74);
    CHECK(state.status.units[0].hp == 74);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AppliesFrameRuntimeTeamEffects", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    state.world.frame = 6;
    state.teamEffects.healAuraRadius = SceneTileWidth * 6.0;
    state.teamEffects.world.units = {
        teamEffectUnitAt(1, 0, 50, { 0, 0, 0 }),
        teamEffectUnitAt(2, 0, 80, { 100, 0, 0 }, 50),
        teamEffectUnitAt(3, 0, 60, { 300, 0, 0 }, 50),
        teamEffectUnitAt(4, 1, 20, { 50, 0, 0 }),
    };

    KysChess::RoleComboState combo;
    combo.hpRegenPct = 20;
    combo.hpRegenInterval = 6;
    combo.healAuraFlat = 5;
    combo.healAuraPct = 10;
    combo.healAuraInterval = 6;
    combo.healedATKSPDBoostPct = 20;
    state.combo.units.emplace(1, combo);

    BattleFrameRuntimeUnitInput runtime;
    runtime.unitId = 1;
    runtime.input = finishingSkillRuntime();
    runtime.input.state.cooldown = 0;
    runtime.hp = 50;
    runtime.maxHp = 100;
    runtime.alive = true;
    state.runtime.units.push_back(runtime);

    auto result = BattleFrameRunner().runFrame(state);

    CHECK(teamEffectUnitById(state.teamEffects.world, 1).hp == 70);
    CHECK(teamEffectUnitById(state.teamEffects.world, 2).hp == 95);
    CHECK(teamEffectUnitById(state.teamEffects.world, 2).cooldown == 40);
    CHECK(teamEffectUnitById(state.teamEffects.world, 3).hp == 60);
    CHECK(teamEffectUnitById(state.teamEffects.world, 3).cooldown == 50);
    CHECK(teamEffectUnitById(state.teamEffects.world, 4).hp == 20);

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
    state.teamEffects.world.units = {
        teamEffectUnit(1, 0, 40),
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

    BattleFrameRuntimeUnitInput runtime;
    runtime.unitId = 1;
    runtime.input = finishingSkillRuntime();
    runtime.input.state.cooldown = 0;
    runtime.hp = 40;
    runtime.maxHp = 100;
    runtime.alive = true;
    state.runtime.units.push_back(runtime);

    auto result = BattleFrameRunner().runFrame(state);

    CHECK(teamEffectUnitById(state.teamEffects.world, 1).hp == 65);
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
    state.effects.world.units = {
        effectUnit(1, 0, 80, 3),
    };

    KysChess::RoleComboState combo;
    combo.postSkillInvincFrames = 12;
    state.combo.units.emplace(1, combo);

    BattleFrameRuntimeUnitInput runtime;
    runtime.unitId = 1;
    runtime.input = finishingSkillRuntime();
    runtime.hp = 80;
    runtime.maxHp = 100;
    runtime.alive = true;
    state.runtime.units.push_back(runtime);

    auto result = BattleFrameRunner().runFrame(state);

    REQUIRE(state.effects.committedCommands.size() == 1);
    CHECK(state.effects.committedCommands[0].type == BattleEffectCommandType::AddInvincibility);
    CHECK(state.effects.committedCommands[0].label == "技能後無敵");
    CHECK(state.effects.committedCommands[0].value == 9);
    CHECK(effectUnitById(state.effects.world, 1).invincible == 12);

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
    state.attacks.units = {
        { 1, 0, true, false, false, { 100, 100, 0 } },
        { 2, 1, true, false, false, { 900, 900, 0 } },
    };
    state.attacks.attacks.push_back(cancelProjectile(10, 1));
    state.attacks.attacks.push_back(cancelProjectile(20, 2));
    state.projectileCancel.baseDamages.push_back({ 10, -1, 25 });
    state.projectileCancel.baseDamages.push_back({ 20, -1, 12 });

    auto result = BattleFrameRunner().runFrame(state);

    REQUIRE(result.attackEvents.size() == 3);
    const auto& cancel = result.attackEvents[2];
    CHECK(cancel.type == BattleAttackEventType::ProjectileCancel);
    CHECK(cancel.projectileCancelDamage == 25);
    CHECK(cancel.otherProjectileCancelDamage == 12);

    REQUIRE(result.commands.empty());
    REQUIRE(state.projectileCancel.committedCommands.size() == 1);
    const auto* command = &state.projectileCancel.committedCommands[0];
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
