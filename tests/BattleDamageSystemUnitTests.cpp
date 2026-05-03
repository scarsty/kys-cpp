#include "battle/BattleDamageSystem.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess::Battle;

namespace
{

BattleDamageUnitState unit()
{
    BattleDamageUnitState state;
    state.id = 1;
    state.alive = true;
    state.hp = 100;
    state.maxHp = 200;
    state.attack = 50;
    return state;
}

BattleResourceUnitState resourceUnit(int id, int hp, int maxHp, int mp, int maxMp)
{
    BattleResourceUnitState state;
    state.id = id;
    state.alive = true;
    state.hp = hp;
    state.maxHp = maxHp;
    state.mp = mp;
    state.maxMp = maxMp;
    return state;
}

BattleStatusUnitState statusUnit(int id)
{
    BattleStatusUnitState state;
    state.id = id;
    state.alive = true;
    state.hp = 100;
    state.maxHp = 200;
    return state;
}

}  // namespace

TEST_CASE("BattleDamageSystem_Modifiers_ApplyPerUnitAttackerAndDefenderRules", "[battle][damage][unit]")
{
    BattleDamageModifierInput input;
    input.damage = 100;
    input.usingSkill = true;
    input.attacker.skillDamagePct = 25;
    input.attacker.flatDamageIncrease = 10;
    input.attacker.poisonDamageAmpPct = 50;
    input.attacker.outgoingDamageReduceDebuffs.push_back({ 3, 20 });
    input.defender.flatDamageReduction = 5;
    input.defender.damageReductionPct = 10;
    input.defender.poisonTimer = 4;
    input.defenderUnit = unit();

    auto result = BattleDamageSystem().applyModifiers(input);

    CHECK(static_cast<int>(result.damage) == 141);
    CHECK_FALSE(result.maxHitCapped);
}

TEST_CASE("BattleDamageSystem_Modifiers_RespectIgnoreDefenseAndMaxHitCap", "[battle][damage][unit]")
{
    BattleDamageModifierInput input;
    input.damage = 300;
    input.ignoreDefense = true;
    input.defender.flatDamageReduction = 999;
    input.defender.damageReductionPct = 90;
    input.defender.maxHitPctMaxHp = 25;
    input.defenderUnit = unit();

    auto result = BattleDamageSystem().applyModifiers(input);

    CHECK(result.damage == 50.0);
    CHECK(result.maxHitCapped);
    CHECK(result.maxHitPct == 25);
}

TEST_CASE("BattleDamageSystem_Defense_InvincibleFirstHitAndShieldAreSeparateLayers", "[battle][damage][unit]")
{
    BattleDamageSystem system;

    auto defender = unit();
    defender.invincible = 10;
    auto invincible = system.resolveDefense({ 80, false, false, true, defender });
    CHECK(invincible.damage == 0.0);
    CHECK(invincible.blockedByInvincible);

    defender = unit();
    defender.blockFirstHitsRemaining = 2;
    auto firstHit = system.resolveDefense({ 80, false, false, false, defender });
    CHECK(firstHit.damage == 0.0);
    CHECK(firstHit.defender.blockFirstHitsRemaining == 1);
    CHECK(firstHit.blockedByFirstHit);

    defender = unit();
    defender.shield = 50;
    auto shield = system.resolveDefense({ 80, false, false, false, defender });
    CHECK(shield.damage == 30.0);
    CHECK(shield.shieldAbsorbed == 50);
    CHECK(shield.defender.shield == 0);
    CHECK(shield.shieldBroken);
}

TEST_CASE("BattleDamageSystem_ExecutedHitsBypassInvincibleAndFirstHitBlock", "[battle][damage][unit]")
{
    auto defender = unit();
    defender.blockFirstHitsRemaining = 1;
    defender.invincible = 10;

    auto result = BattleDamageSystem().resolveDefense({ 80, true, false, true, defender });

    CHECK(result.damage == 80.0);
    CHECK_FALSE(result.blockedByInvincible);
    CHECK_FALSE(result.blockedByFirstHit);
    CHECK(result.defender.blockFirstHitsRemaining == 1);
}

TEST_CASE("BattleDamageSystem_DamageTaken_GrantsHurtInvincOrDeathPrevention", "[battle][damage][unit]")
{
    BattleDamageSystem system;

    auto hurt = unit();
    hurt.hurtInvincFrames = 5;
    auto hurtResult = system.applyDamageTaken(hurt, 20);
    CHECK(hurtResult.defender.hp == 80);
    CHECK(hurtResult.defender.invincible == 5);
    CHECK(hurtResult.hurtInvincGranted);
    CHECK_FALSE(hurtResult.died);

    auto protectedUnit = unit();
    protectedUnit.hp = 10;
    protectedUnit.deathPrevention = true;
    protectedUnit.deathPreventionFrames = 30;
    auto protectedResult = system.applyDamageTaken(protectedUnit, 20);
    CHECK(protectedResult.defender.hp == 1);
    CHECK(protectedResult.defender.invincible == 30);
    CHECK(protectedResult.defender.deathPreventionUsed);
    CHECK(protectedResult.deathPrevented);
    CHECK_FALSE(protectedResult.died);
}

TEST_CASE("BattleDamageSystem_KillReward_HealsInvincibleAndBloodlustPerUnit", "[battle][damage][unit]")
{
    auto killer = unit();
    killer.hp = 120;
    killer.killHealPct = 50;
    killer.killInvincFrames = 12;
    killer.bloodlustAttackPerKill = 7;

    auto result = BattleDamageSystem().applyKillReward({ killer });

    CHECK(result.killer.hp == 200);
    CHECK(result.healed == 80);
    CHECK(result.killer.invincible == 12);
    CHECK(result.invincibilityGranted == 12);
    CHECK(result.killer.attack == 57);
    CHECK(result.attackGranted == 7);
}

TEST_CASE("BattleDamageSystem_CooldownExtension_RequiresActiveActionAndCaps", "[battle][damage][unit]")
{
    BattleCooldownState state;
    state.alive = true;
    state.cooldown = 50;
    state.cooldownMax = 100;
    state.haveAction = true;
    state.operationType = BattleOperationType::Melee;
    state.actType = 1;

    auto result = BattleDamageSystem().extendActiveCooldown(state, 25);
    CHECK(result.increased);
    CHECK(result.before == 50);
    CHECK(result.after == 75);
    CHECK(result.unit.cooldown == 75);

    state.cooldown = 125;
    result = BattleDamageSystem().extendActiveCooldown(state, 25);
    CHECK_FALSE(result.increased);
    CHECK(result.unit.cooldown == 125);

    state.cooldown = 50;
    state.haveAction = false;
    result = BattleDamageSystem().extendActiveCooldown(state, 25);
    CHECK_FALSE(result.increased);
    CHECK(result.unit.cooldown == 50);
}

TEST_CASE("BattleDamageSystem_ExecuteThreshold_UsesProjectedHpAfterPendingDamage", "[battle][damage][unit]")
{
    BattleExecuteInput input;
    input.projectedHpBeforeDamage = 60;
    input.maxHp = 200;
    input.pendingDamage = 25;
    input.appliesHpDamage = true;
    input.thresholdPct = 20;

    CHECK(BattleDamageSystem().shouldExecute(input));

    input.pendingDamage = 19;
    CHECK_FALSE(BattleDamageSystem().shouldExecute(input));

    input.appliesHpDamage = false;
    CHECK_FALSE(BattleDamageSystem().shouldExecute(input));
}

TEST_CASE("BattleDamageSystem_OnHitResources_ApplyPerUnitMpHpAndDrain", "[battle][damage][unit]")
{
    BattleOnHitResourceInput input;
    input.attacker = resourceUnit(1, 90, 120, 80, 100);
    input.target = resourceUnit(2, 100, 100, 12, 100);
    input.mpOnHit = 15;
    input.hpOnHit = 40;
    input.mpDrain = 20;

    auto result = BattleDamageSystem().applyOnHitResources(input);

    CHECK(result.attacker.hp == 120);
    CHECK(result.hpHealed == 30);
    CHECK(result.target.mp == 0);
    CHECK(result.mpDrained == 12);
    CHECK(result.attacker.mp == 100);
    CHECK(result.mpRestored == 20);
}

TEST_CASE("BattleDamageSystem_Poison_OnlyStrongerPoisonRefreshesTarget", "[battle][damage][unit]")
{
    auto target = statusUnit(2);
    target.poisonTickPct = 8;
    target.poisonTimer = 30;
    target.poisonSourceId = 7;

    auto weak = BattleDamageSystem().applyPoisonIfStronger({ target, 1, 5, 60 });
    CHECK_FALSE(weak.applied);
    CHECK(weak.target.poisonTickPct == 8);
    CHECK(weak.target.poisonTimer == 30);
    CHECK(weak.target.poisonSourceId == 7);

    auto strong = BattleDamageSystem().applyPoisonIfStronger({ target, 1, 12, 60 });
    CHECK(strong.applied);
    CHECK(strong.target.poisonTickPct == 12);
    CHECK(strong.target.poisonTimer == 60);
    CHECK(strong.target.poisonSourceId == 1);
    CHECK(strong.value == 12);
}

TEST_CASE("BattleDamageSystem_Bleed_StacksToCapAndInitializesTimer", "[battle][damage][unit]")
{
    auto target = statusUnit(2);
    target.bleedStacks = 2;
    target.bleedTimer = 0;

    auto result = BattleDamageSystem().applyBleed(target, 1, 2, 3);

    CHECK(result.applied);
    CHECK(result.target.bleedStacks == 3);
    CHECK(result.target.bleedTimer == 10);
    CHECK(result.target.bleedSourceId == 1);
    CHECK(result.value == 3);

    result.target.bleedTimer = 7;
    auto capped = BattleDamageSystem().applyBleed(result.target, 4, 1, 3);
    CHECK_FALSE(capped.applied);
    CHECK(capped.target.bleedStacks == 3);
    CHECK(capped.target.bleedTimer == 7);
    CHECK(capped.target.bleedSourceId == 4);
}

TEST_CASE("BattleDamageSystem_DamageReduceDebuff_AppendsValidTimedDebuff", "[battle][damage][unit]")
{
    auto target = statusUnit(2);

    auto result = BattleDamageSystem().applyDamageReduceDebuff(target, 45, 30);

    REQUIRE(result.applied);
    REQUIRE(result.target.damageReduceDebuffs.size() == 1);
    CHECK(result.target.damageReduceDebuffs[0].remainingFrames == 45);
    CHECK(result.target.damageReduceDebuffs[0].pct == 30);
    CHECK(result.value == 30);
}

TEST_CASE("傷害交易_物理傷害套用攻守方修正並輸出差量", "[battle][damage][unit]")
{
    BattleDamageTransactionInput input;
    input.request.attackerUnitId = 1;
    input.request.defenderUnitId = 2;
    input.request.baseDamage = 100;
    input.request.usingSkill = true;
    input.attacker = unit();
    input.attacker.id = 1;
    input.defender = unit();
    input.defender.id = 2;
    input.attackerModifiers.skillDamagePct = 25;
    input.defenderModifiers.flatDamageReduction = 10;
    input.defenderModifiers.damageReductionPct = 20;

    auto result = BattleDamageSystem().resolveTransaction(input);

    CHECK(result.finalHpDamage == 92);
    CHECK(result.defender.hp == 8);
    CHECK(result.defenderDelta.unitId == 2);
    CHECK(result.defenderDelta.hpDelta == -92);
    REQUIRE(result.events.size() == 1);
    CHECK(result.events[0].type == BattleDamageEventType::DamageApplied);
    CHECK(result.events[0].sourceUnitId == 1);
    CHECK(result.events[0].targetUnitId == 2);
    CHECK(result.events[0].value == 92);
}

TEST_CASE("傷害交易_護盾先吸收生命傷害", "[battle][damage][unit]")
{
    BattleDamageTransactionInput input;
    input.request.attackerUnitId = 1;
    input.request.defenderUnitId = 2;
    input.request.baseDamage = 80;
    input.attacker = unit();
    input.attacker.id = 1;
    input.defender = unit();
    input.defender.id = 2;
    input.defender.shield = 50;

    auto result = BattleDamageSystem().resolveTransaction(input);

    CHECK(result.shieldAbsorbed == 50);
    CHECK(result.finalHpDamage == 30);
    CHECK(result.defender.hp == 70);
    CHECK(result.defender.shield == 0);
    CHECK(result.defenderDelta.shieldDelta == -50);
    REQUIRE(result.events.size() == 2);
    CHECK(result.events[0].type == BattleDamageEventType::ShieldAbsorbed);
    CHECK(result.events[0].value == 50);
    CHECK(result.events[1].type == BattleDamageEventType::DamageApplied);
    CHECK(result.events[1].value == 30);
}

TEST_CASE("傷害交易_無敵阻擋一般傷害但不阻擋處決", "[battle][damage][unit]")
{
    BattleDamageTransactionInput blockedInput;
    blockedInput.request.attackerUnitId = 1;
    blockedInput.request.defenderUnitId = 2;
    blockedInput.request.baseDamage = 80;
    blockedInput.attacker = unit();
    blockedInput.attacker.id = 1;
    blockedInput.defender = unit();
    blockedInput.defender.id = 2;
    blockedInput.defender.invincible = 10;

    auto blocked = BattleDamageSystem().resolveTransaction(blockedInput);

    CHECK(blocked.finalHpDamage == 0);
    CHECK(blocked.defender.hp == 100);
    REQUIRE(blocked.events.size() == 1);
    CHECK(blocked.events[0].type == BattleDamageEventType::BlockedByInvincible);

    auto executedInput = blockedInput;
    executedInput.defender.hp = 35;
    executedInput.request.canExecute = true;
    executedInput.request.executeThresholdPct = 20;

    auto executed = BattleDamageSystem().resolveTransaction(executedInput);

    CHECK(executed.executed);
    CHECK(executed.defender.alive == false);
    CHECK(executed.defender.hp == 0);
    CHECK(executed.finalHpDamage == 35);
    REQUIRE(executed.events.size() == 3);
    CHECK(executed.events[0].type == BattleDamageEventType::ExecuteTriggered);
    CHECK(executed.events[1].type == BattleDamageEventType::DamageApplied);
    CHECK(executed.events[2].type == BattleDamageEventType::UnitDied);
}

TEST_CASE("傷害交易_死亡保護與擊殺獎勵由同一交易輸出", "[battle][damage][unit]")
{
    BattleDamageTransactionInput protectedInput;
    protectedInput.request.attackerUnitId = 1;
    protectedInput.request.defenderUnitId = 2;
    protectedInput.request.baseDamage = 50;
    protectedInput.attacker = unit();
    protectedInput.attacker.id = 1;
    protectedInput.defender = unit();
    protectedInput.defender.id = 2;
    protectedInput.defender.hp = 20;
    protectedInput.defender.deathPrevention = true;
    protectedInput.defender.deathPreventionFrames = 30;

    auto protectedResult = BattleDamageSystem().resolveTransaction(protectedInput);

    CHECK(protectedResult.defender.hp == 1);
    CHECK(protectedResult.defender.alive);
    CHECK(protectedResult.defender.deathPreventionUsed);
    CHECK(protectedResult.defender.invincible == 30);
    CHECK(protectedResult.defenderDelta.hpDelta == -19);
    REQUIRE(protectedResult.events.size() == 2);
    CHECK(protectedResult.events[0].type == BattleDamageEventType::DamageApplied);
    CHECK(protectedResult.events[1].type == BattleDamageEventType::DeathPrevented);

    BattleDamageTransactionInput killInput;
    killInput.request.attackerUnitId = 1;
    killInput.request.defenderUnitId = 2;
    killInput.request.baseDamage = 50;
    killInput.attacker = unit();
    killInput.attacker.id = 1;
    killInput.attacker.hp = 120;
    killInput.attacker.killHealPct = 50;
    killInput.attacker.killInvincFrames = 12;
    killInput.attacker.bloodlustAttackPerKill = 7;
    killInput.defender = unit();
    killInput.defender.id = 2;
    killInput.defender.hp = 40;

    auto kill = BattleDamageSystem().resolveTransaction(killInput);

    CHECK_FALSE(kill.defender.alive);
    CHECK(kill.attacker.hp == 200);
    CHECK(kill.attacker.invincible == 12);
    CHECK(kill.attacker.attack == 57);
    CHECK(kill.attackerDelta.hpDelta == 80);
    CHECK(kill.attackerDelta.invincibleDelta == 12);
    CHECK(kill.attackerDelta.attackDelta == 7);
    REQUIRE(kill.events.size() == 3);
    CHECK(kill.events[0].type == BattleDamageEventType::DamageApplied);
    CHECK(kill.events[1].type == BattleDamageEventType::UnitDied);
    CHECK(kill.events[2].type == BattleDamageEventType::KillRewardApplied);
}

TEST_CASE("傷害交易_魔力傷害不觸發生命傷害防禦層", "[battle][damage][unit]")
{
    BattleDamageTransactionInput input;
    input.request.attackerUnitId = 1;
    input.request.defenderUnitId = 2;
    input.request.mpDamage = 35;
    input.attacker = unit();
    input.attacker.id = 1;
    input.defender = unit();
    input.defender.id = 2;
    input.defender.mp = 20;
    input.defender.maxMp = 100;
    input.defender.invincible = 10;
    input.defender.shield = 50;

    auto result = BattleDamageSystem().resolveTransaction(input);

    CHECK(result.finalHpDamage == 0);
    CHECK(result.finalMpDamage == 20);
    CHECK(result.defender.hp == 100);
    CHECK(result.defender.mp == 0);
    CHECK(result.defender.shield == 50);
    CHECK(result.defenderDelta.mpDelta == -20);
    REQUIRE(result.events.size() == 1);
    CHECK(result.events[0].type == BattleDamageEventType::MpDamageApplied);
    CHECK(result.events[0].value == 20);
}

TEST_CASE("傷害交易_命中後資源與冷卻延長只在有效命中後生效", "[battle][damage][unit]")
{
    BattleDamageTransactionInput input;
    input.request.attackerUnitId = 1;
    input.request.defenderUnitId = 2;
    input.request.baseDamage = 20;
    input.request.mpOnHit = 15;
    input.request.hpOnHit = 40;
    input.request.mpDrain = 20;
    input.request.cooldownExtendPct = 25;
    input.attacker = unit();
    input.attacker.id = 1;
    input.attacker.hp = 90;
    input.attacker.maxHp = 120;
    input.attacker.mp = 80;
    input.attacker.maxMp = 100;
    input.defender = unit();
    input.defender.id = 2;
    input.defender.mp = 12;
    input.defender.maxMp = 100;
    input.defenderCooldown.alive = true;
    input.defenderCooldown.cooldown = 50;
    input.defenderCooldown.cooldownMax = 100;
    input.defenderCooldown.haveAction = true;
    input.defenderCooldown.operationType = BattleOperationType::Melee;
    input.defenderCooldown.actType = 1;

    auto result = BattleDamageSystem().resolveTransaction(input);

    CHECK(result.attacker.hp == 120);
    CHECK(result.attacker.mp == 100);
    CHECK(result.defender.mp == 0);
    CHECK(result.attackerDelta.hpDelta == 30);
    CHECK(result.attackerDelta.mpDelta == 20);
    CHECK(result.defenderDelta.mpDelta == -12);
    CHECK(result.defenderCooldown.cooldown == 75);
    CHECK(result.cooldownDelta == 25);

    bool sawHpRestore = false;
    bool sawMpRestore = false;
    bool sawMpDrain = false;
    bool sawCooldown = false;
    for (const auto& event : result.events)
    {
        sawHpRestore = sawHpRestore || event.type == BattleDamageEventType::HpRestored;
        sawMpRestore = sawMpRestore || event.type == BattleDamageEventType::MpRestored;
        sawMpDrain = sawMpDrain || event.type == BattleDamageEventType::MpDrained;
        sawCooldown = sawCooldown || event.type == BattleDamageEventType::CooldownExtended;
    }
    CHECK(sawHpRestore);
    CHECK(sawMpRestore);
    CHECK(sawMpDrain);
    CHECK(sawCooldown);

    input.defender.invincible = 10;
    auto blocked = BattleDamageSystem().resolveTransaction(input);

    CHECK(blocked.finalHpDamage == 0);
    CHECK(blocked.attacker.hp == 90);
    CHECK(blocked.attacker.mp == 80);
    CHECK(blocked.defender.mp == 12);
    CHECK(blocked.defenderCooldown.cooldown == 50);
    CHECK(blocked.cooldownDelta == 0);
}

TEST_CASE("傷害交易_命中後狀態由交易輸出", "[battle][damage][unit]")
{
    BattleDamageTransactionInput input;
    input.request.attackerUnitId = 1;
    input.request.defenderUnitId = 2;
    input.request.baseDamage = 10;
    input.request.poisonPct = 12;
    input.request.poisonDurationFrames = 60;
    input.request.bleedStacks = 2;
    input.request.bleedMaxStacks = 3;
    input.request.damageReduceDebuffDurationFrames = 45;
    input.request.damageReduceDebuffPct = 30;
    input.request.frozenFrames = 5;
    input.request.mpBlockFrames = 9;
    input.attacker = unit();
    input.attacker.id = 1;
    input.defender = unit();
    input.defender.id = 2;
    input.defenderStatus = statusUnit(2);
    input.defenderStatus.poisonTickPct = 8;
    input.defenderStatus.poisonTimer = 30;
    input.defenderStatus.bleedStacks = 2;

    auto result = BattleDamageSystem().resolveTransaction(input);

    CHECK(result.defenderStatus.poisonTickPct == 12);
    CHECK(result.defenderStatus.poisonTimer == 60);
    CHECK(result.defenderStatus.poisonSourceId == 1);
    CHECK(result.defenderStatus.bleedStacks == 3);
    CHECK(result.defenderStatus.bleedTimer == 10);
    CHECK(result.defenderStatus.bleedSourceId == 1);
    REQUIRE(result.defenderStatus.damageReduceDebuffs.size() == 1);
    CHECK(result.defenderStatus.damageReduceDebuffs[0].remainingFrames == 45);
    CHECK(result.defenderStatus.damageReduceDebuffs[0].pct == 30);
    CHECK(result.defenderStatus.frozenTimer == 5);
    CHECK(result.defenderStatus.frozenMaxTimer == 5);
    CHECK(result.defenderStatus.mpBlockTimer == 9);

    std::vector<BattleDamageStatusType> statuses;
    for (const auto& event : result.events)
    {
        if (event.type == BattleDamageEventType::StatusApplied)
        {
            statuses.push_back(event.statusType);
        }
    }
    REQUIRE(statuses.size() == 5);
    CHECK(statuses[0] == BattleDamageStatusType::Poison);
    CHECK(statuses[1] == BattleDamageStatusType::Bleed);
    CHECK(statuses[2] == BattleDamageStatusType::DamageReduceDebuff);
    CHECK(statuses[3] == BattleDamageStatusType::Frozen);
    CHECK(statuses[4] == BattleDamageStatusType::MpBlocked);
}

TEST_CASE("傷害交易_較弱中毒不會刷新交易目標", "[battle][damage][unit]")
{
    BattleDamageTransactionInput input;
    input.request.attackerUnitId = 1;
    input.request.defenderUnitId = 2;
    input.request.baseDamage = 10;
    input.request.poisonPct = 5;
    input.request.poisonDurationFrames = 60;
    input.attacker = unit();
    input.attacker.id = 1;
    input.defender = unit();
    input.defender.id = 2;
    input.defenderStatus = statusUnit(2);
    input.defenderStatus.poisonTickPct = 8;
    input.defenderStatus.poisonTimer = 30;
    input.defenderStatus.poisonSourceId = 7;

    auto result = BattleDamageSystem().resolveTransaction(input);

    CHECK(result.defenderStatus.poisonTickPct == 8);
    CHECK(result.defenderStatus.poisonTimer == 30);
    CHECK(result.defenderStatus.poisonSourceId == 7);
    for (const auto& event : result.events)
    {
        CHECK_FALSE(event.statusType == BattleDamageStatusType::Poison);
    }
}

TEST_CASE("傷害交易_已確認命中的零傷害特效仍可套用狀態", "[battle][damage][unit]")
{
    BattleDamageTransactionInput input;
    input.request.attackerUnitId = 1;
    input.request.defenderUnitId = 2;
    input.request.acceptedHit = true;
    input.request.frozenFrames = 6;
    input.request.bleedStacks = 1;
    input.request.bleedMaxStacks = 3;
    input.attacker = unit();
    input.attacker.id = 1;
    input.defender = unit();
    input.defender.id = 2;
    input.defenderStatus = statusUnit(2);

    auto result = BattleDamageSystem().resolveTransaction(input);

    CHECK(result.finalHpDamage == 0);
    CHECK(result.defenderStatus.frozenTimer == 6);
    CHECK(result.defenderStatus.bleedStacks == 1);
    REQUIRE(result.events.size() == 2);
    CHECK(result.events[0].statusType == BattleDamageStatusType::Bleed);
    CHECK(result.events[1].statusType == BattleDamageStatusType::Frozen);
}

TEST_CASE("傷害交易_冰凍套用抗性與控制免疫", "[battle][damage][unit]")
{
    BattleDamageTransactionInput input;
    input.request.attackerUnitId = 1;
    input.request.defenderUnitId = 2;
    input.request.acceptedHit = true;
    input.request.frozenFrames = 10;
    input.attacker = unit();
    input.attacker.id = 1;
    input.defender = unit();
    input.defender.id = 2;
    input.defender.shield = 1;
    input.defenderStatus = statusUnit(2);
    input.defenderStatus.freezeReductionPct = 20;
    input.defenderStatus.shieldFreezeResPct = 30;
    input.defenderStatus.controlImmunityFrames = 3;

    auto result = BattleDamageSystem().resolveTransaction(input);

    CHECK(result.defenderStatus.controlImmunityFrames == 0);
    CHECK(result.defenderStatus.frozenTimer == 2);
    CHECK(result.defenderStatus.frozenMaxTimer == 2);
    REQUIRE(result.events.size() == 1);
    CHECK(result.events[0].statusType == BattleDamageStatusType::Frozen);
    CHECK(result.events[0].value == 2);

    input.defenderStatus.hp = 40;
    input.defenderStatus.maxHp = 200;
    auto lowHp = BattleDamageSystem().resolveTransaction(input);

    CHECK(lowHp.defenderStatus.frozenTimer == 0);
    CHECK(lowHp.defenderStatus.controlImmunityFrames == 3);
    CHECK(lowHp.events.empty());
}

TEST_CASE("傷害交易_已結算傷害不重跑防禦層但仍處理死亡", "[battle][damage][unit]")
{
    BattleDamageTransactionInput input;
    input.request.attackerUnitId = 1;
    input.request.defenderUnitId = 2;
    input.request.baseDamage = 40;
    input.request.preResolvedDamage = true;
    input.attacker = unit();
    input.attacker.id = 1;
    input.attacker.hp = 120;
    input.attacker.killHealPct = 50;
    input.defender = unit();
    input.defender.id = 2;
    input.defender.hp = 30;
    input.defender.invincible = 10;
    input.defender.shield = 99;

    auto result = BattleDamageSystem().resolveTransaction(input);

    CHECK_FALSE(result.defender.alive);
    CHECK(result.defender.hp == 0);
    CHECK(result.defender.shield == 99);
    CHECK(result.attacker.hp == 200);
    CHECK(result.attackerDelta.hpDelta == 80);
    REQUIRE(result.events.size() == 3);
    CHECK(result.events[0].type == BattleDamageEventType::DamageApplied);
    CHECK(result.events[1].type == BattleDamageEventType::UnitDied);
    CHECK(result.events[2].type == BattleDamageEventType::KillRewardApplied);
}
