#include "battle/BattleDamageSystem.h"
#include "ChessBattleEffects.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <limits>

using namespace KysChess::Battle;

namespace
{

BattleDamageUnitState unit()
{
    BattleDamageUnitState state;
    state.id = 1;
    state.alive = true;
    state.vitals.hp = 100;
    state.vitals.maxHp = 200;
    state.attack = 50;
    return state;
}

BattleResourceUnitState resourceUnit(int id, int hp, int maxHp, int mp, int maxMp)
{
    BattleResourceUnitState state;
    state.id = id;
    state.alive = true;
    state.vitals = { hp, maxHp, mp, maxMp };
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

TEST_CASE("BattleDamageSystem_UsesGroupedVitalsForDamageState", "[battle][damage]")
{
    BattleDamageUnitState defender;
    defender.id = 1;
    defender.alive = true;
    defender.vitals = { 30, 100, 5, 20 };

    BattleDamageSystem system;
    auto result = system.applyDamageTaken(defender, 10);

    CHECK(result.defender.vitals.hp == 20);
    CHECK(result.defender.vitals.maxHp == 100);
    CHECK(result.defender.vitals.mp == 5);
    CHECK(result.defender.alive);
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
    input.defender.flatDamageReduction = 5;
    input.defender.damageReductionPct = 10;
    input.defender.poisoned = true;
    input.defenderUnit = unit();

    auto result = BattleDamageSystem().applyModifiers(input);

    CHECK(result.damage.toInt() == 175);
    CHECK_FALSE(result.maxHitCapped);
}

TEST_CASE("BattleDamageSystem_MagicBaseDamageUsesAttackDefenseCurve", "[battle][damage][unit]")
{
    BattleMagicBaseDamageInput input;
    input.attackerAttack = 90;
    input.magicPower = 60;
    input.defenderDefense = 30;
    input.randomVariance = -3;

    CHECK(BattleDamageSystem().resolveMagicBaseDamage(input) == 18);

    input.defenderDefense = -200;
    CHECK(BattleDamageSystem().resolveMagicBaseDamage(input) == 1);

    input.defenderDefense = 10000;
    input.randomVariance = -100;
    CHECK(BattleDamageSystem().resolveMagicBaseDamage(input) == 1);
}

TEST_CASE("BattleDamageSystem_HitShapeOwnsProjectileFalloffFacingAndOperationDamage", "[battle][damage][unit]")
{
    BattleHitShapeInput input;
    input.baseDamage = 100.0;
    input.projectileCancelDamage = 10;
    input.strengthPct = 200;
    input.frame = 5;
    input.totalFrame = 10;
    input.impactPosition = { -1.0f, 0.0f, 0.0f };
    input.defenderPosition = { 0.0f, 0.0f, 0.0f };
    input.defenderFacing = { 1.0f, 0.0f, 0.0f };
    input.operationType = BattleOperationType::TrackingProjectile;
    input.usingSkill = true;
    input.attackerActProperty = 150;
    input.defenderActProperty = 50;

    auto result = BattleDamageSystem().shapeHitDamage(input);

    CHECK(result.damage.toDouble() == Catch::Approx(395.8875));
    CHECK(result.knockbackStrength == Catch::Approx(2.0));
    CHECK(result.knockbackVelocityCap == Catch::Approx(3.0));
    CHECK(result.frozenFrames == 0);
}

TEST_CASE("BattleDamageSystem_DashHitShapeEmitsFreezeAndReducedDashDamage", "[battle][damage][unit]")
{
    BattleHitShapeInput input;
    input.baseDamage = 90.0;
    input.strengthPct = 100;
    input.frame = 0;
    input.totalFrame = 20;
    input.impactPosition = { 1.0f, 0.0f, 0.0f };
    input.defenderPosition = { 0.0f, 0.0f, 0.0f };
    input.defenderFacing = { 1.0f, 0.0f, 0.0f };
    input.operationType = BattleOperationType::Dash;

    auto result = BattleDamageSystem().shapeHitDamage(input);

    CHECK(result.damage.toDouble() == Catch::Approx(60.0));
    CHECK(result.frozenFrames == 5);
    CHECK(result.knockbackStrength == Catch::Approx(2.0));
    CHECK(result.knockbackVelocityCap == Catch::Approx(3.0));
}

TEST_CASE("BattleDamageSystem_PointBlankHitHasNoDirectionalDamageBonus", "[battle][damage][unit]")
{
    BattleHitShapeInput input;
    input.baseDamage = 100;
    input.impactPosition = { 5.0f, 7.0f, 0.0f };
    input.defenderPosition = input.impactPosition;
    input.defenderFacing = { 1.0f, 0.0f, 0.0f };

    const auto result = BattleDamageSystem().shapeHitDamage(input);

    CHECK(result.damage.toDouble() == Catch::Approx(100.0));
}

TEST_CASE("BattleDamageSystem_ScriptedHitRequestCarriesAcceptedStatusPayloads", "[battle][damage][unit]")
{
    BattleScriptedHitRequestInput input;
    input.attackerUnitId = 11;
    input.defenderUnitId = 22;
    input.stunFrames = 7;
    input.bleedStacks = 3;
    input.bleedMaxStacks = 9;

    auto request = BattleDamageSystem().makeScriptedHitRequest(input);

    CHECK(request.attackerUnitId == 11);
    CHECK(request.defenderUnitId == 22);
    CHECK(request.acceptedHit);
    CHECK(request.stunFrames == 7);
    CHECK(request.bleedStacks == 3);
    CHECK(request.bleedMaxStacks == 9);
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

    CHECK(result.damage == BattleFixed::fromInteger(50));
    CHECK(result.maxHitCapped);
    CHECK(result.maxHitPct == 25);
}

TEST_CASE("BattleDamageSystem_Modifiers_DefenseBypassStillAppliesDamageReduction", "[battle][damage][unit]")
{
    BattleDamageModifierInput input;
    input.damage = 200;
    input.defender.flatDamageReduction = 75;
    input.defender.damageReductionPct = 25;
    input.defenderUnit = unit();

    SECTION("explicit defense bypass")
    {
        input.ignoreDefense = true;

        const auto result = BattleDamageSystem().applyModifiers(input);

        CHECK(result.damage == BattleFixed::fromInteger(150));
        CHECK(result.combinedDamageReductionBasisPoints == 2500);
    }

    SECTION("pure damage")
    {
        input.damageKind = KysChess::BattleDamageKind::Pure;

        const auto result = BattleDamageSystem().applyModifiers(input);

        CHECK(result.damage == BattleFixed::fromInteger(150));
        CHECK(result.combinedDamageReductionBasisPoints == 2500);
    }
}

TEST_CASE("BattleDamageSystem_Defense_InvincibleAttackBlocksAndShieldAreSeparateLayers", "[battle][damage][unit]")
{
    BattleDamageSystem system;

    auto defender = unit();
    defender.invincible = 10;
    auto invincible = system.resolveDefense({ 80, false, false, true, defender });
    CHECK(invincible.damage == 0.0);
    CHECK(invincible.blockedByInvincible);

    defender = unit();
    defender.dualWieldBlocksRemaining = 1;
    auto dualWield = system.resolveDefense({ 80, false, false, false, defender });
    CHECK(dualWield.damage == 0.0);
    CHECK(dualWield.defender.dualWieldBlocksRemaining == 0);
    CHECK(dualWield.blockedByDualWield);

    defender = unit();
    defender.shield = 50;
    auto shield = system.resolveDefense({ 80, false, false, false, defender });
    CHECK(shield.damage == 30.0);
    CHECK(shield.shieldAbsorbed == 50);
    CHECK(shield.defender.shield == 0);
    CHECK(shield.shieldBroken);
}

TEST_CASE("BattleDamageSystem_ExecutedHitsBypassInvincibleAndAttackBlocks", "[battle][damage][unit]")
{
    auto defender = unit();
    defender.dualWieldBlocksRemaining = 1;
    defender.invincible = 10;

    auto result = BattleDamageSystem().resolveDefense({ 80, true, false, true, defender });

    CHECK(result.damage == 80);
    CHECK_FALSE(result.blockedByInvincible);
    CHECK_FALSE(result.blockedByDualWield);
    CHECK(result.defender.dualWieldBlocksRemaining == 1);
}

TEST_CASE("BattleDamageSystem_AbsorptionUsesReductionBudgetBeforeShield", "[battle][damage][absorption][unit]")
{
    BattleDamageDefenseInput input;
    input.damage = 100;
    input.defender = unit();
    input.defender.shield = 50;
    input.absorptionLayers = { { 1, 40 } };

    const auto result = BattleDamageSystem().resolveDefense(input);

    REQUIRE(result.absorptionReceipts.size() == 1);
    CHECK(result.absorptionReceipts[0].sequence == 1);
    CHECK(result.absorptionReceipts[0].absorbedDamage == 40);
    CHECK(result.shieldAbsorbed == 50);
    CHECK(result.damage == 10);
    CHECK(result.remainingDamageBasisPoints == 6000);
}

TEST_CASE("BattleDamageSystem_AbsorptionCannotExceedGlobalReductionCap", "[battle][damage][absorption][unit]")
{
    BattleDamageDefenseInput input;
    input.damage = 100;
    input.defender = unit();
    input.absorptionLayers = {
        { 1, 60 },
        { 2, 60 },
    };

    const auto result = BattleDamageSystem().resolveDefense(input);

    REQUIRE(result.absorptionReceipts.size() == 2);
    CHECK(result.absorptionReceipts[0].absorbedDamage == 60);
    CHECK(result.absorptionReceipts[1].absorbedDamage == 20);
    CHECK(result.damage == 20);
    CHECK(result.remainingDamageBasisPoints == 2000);

    input.damage = 20;
    input.remainingDamageBasisPoints = 2000;
    input.absorptionLayers = { { 3, 40 } };
    const auto alreadyCapped = BattleDamageSystem().resolveDefense(input);

    REQUIRE(alreadyCapped.absorptionReceipts.size() == 1);
    CHECK(alreadyCapped.absorptionReceipts[0].absorbedDamage == 0);
    CHECK(alreadyCapped.damage == 20);
    CHECK(alreadyCapped.remainingDamageBasisPoints == 2000);
}

TEST_CASE("BattleDamageSystem_DamageTaken_GrantsHurtInvincOrDeathPrevention", "[battle][damage][unit]")
{
    BattleDamageSystem system;

    auto hurt = unit();
    hurt.hurtInvincFrames = 5;
    auto hurtResult = system.applyDamageTaken(hurt, 20);
    CHECK(hurtResult.defender.vitals.hp == 80);
    CHECK(hurtResult.defender.invincible == 5);
    CHECK(hurtResult.hurtInvincGranted);
    CHECK_FALSE(hurtResult.died);

    auto protectedUnit = unit();
    protectedUnit.vitals.hp = 10;
    protectedUnit.deathPrevention = true;
    protectedUnit.deathPreventionFrames = 30;
    auto protectedResult = system.applyDamageTaken(protectedUnit, 20);
    CHECK(protectedResult.defender.vitals.hp == 1);
    CHECK(protectedResult.defender.invincible == 30);
    CHECK(protectedResult.defender.deathPreventionUsed);
    CHECK(protectedResult.deathPrevented);
    CHECK_FALSE(protectedResult.died);

    auto noHurtInvinc = unit();
    noHurtInvinc.hurtInvincFrames = 5;
    auto noHurtInvincResult = system.applyDamageTaken(noHurtInvinc, 20, false);
    CHECK(noHurtInvincResult.defender.vitals.hp == 80);
    CHECK(noHurtInvincResult.defender.invincible == 0);
    CHECK_FALSE(noHurtInvincResult.hurtInvincGranted);
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
    CHECK(result.unit.cooldownMax == 100);

    state.cooldown = 100;
    result = BattleDamageSystem().extendActiveCooldown(state, 25);
    CHECK(result.increased);
    CHECK(result.unit.cooldown == 125);
    CHECK(result.unit.cooldownMax == 125);

    state.cooldown = 125;
    state.cooldownMax = 100;
    result = BattleDamageSystem().extendActiveCooldown(state, 25);
    CHECK_FALSE(result.increased);
    CHECK(result.unit.cooldown == 125);
    CHECK(result.unit.cooldownMax == 100);

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

    CHECK(result.attacker.vitals.hp == 120);
    REQUIRE(result.heal);
    CHECK(result.heal->request.kind == BattleHealKind::OnHit);
    CHECK(result.heal->appliedAmount == 30);
    CHECK(result.hpHealed == 30);
    CHECK(result.target.vitals.mp == 0);
    CHECK(result.mpDrained == 12);
    CHECK(result.attacker.vitals.mp == 100);
    CHECK(result.mpRestored == 20);
}

TEST_CASE("BattleDamageSystem_OnHitHpUsesHealModifierBeforeMpDrainAndRestore", "[battle][damage][heal][unit]")
{
    BattleOnHitResourceInput input;
    input.attacker = resourceUnit(1, 50, 100, 20, 100);
    input.target = resourceUnit(2, 100, 100, 6, 100);
    input.mpOnHit = 5;
    input.hpOnHit = 7;
    input.mpDrain = 10;
    input.healModifiers.receivedHealPcts.push_back(25);

    const auto result = BattleDamageSystem().applyOnHitResources(input);

    CHECK(result.hpHealed == 1);
    CHECK(result.attacker.vitals.hp == 51);
    CHECK(result.mpDrained == 6);
    CHECK(result.target.vitals.mp == 0);
    CHECK(result.mpRestored == 11);
    CHECK(result.attacker.vitals.mp == 31);
}

TEST_CASE("BattleStatusSystem_PoisonHonorsStacksCapsAndMergePolicies", "[battle][status][poison][unit]")
{
    BattleStatusSystem system({});
    BattleStatusApplyRequest replace;
    replace.kind = KysChess::BattleStatusKind::Poison;
    replace.sourceUnitId = 1;
    replace.durationFrames = 120;
    replace.stacks = 5;
    replace.potency = 10;
    replace.stack = KysChess::EffectStackPolicy::Replace;
    replace.stackLimit = 4;

    auto replaced = system.apply(statusUnit(2), replace);

    REQUIRE(replaced.applied);
    CHECK(replaced.outcome == BattleStatusApplyOutcome::Applied);
    REQUIRE(replaced.target.effects.find(KysChess::BattleStatusKind::Poison));
    CHECK(replaced.target.effects.find(KysChess::BattleStatusKind::Poison)->stacks == 4);
    CHECK(system.snapshot(replaced.target).stacks(KysChess::BattleStatusKind::Poison) == 4);

    BattleStatusApplyRequest add = replace;
    add.sourceUnitId = 3;
    add.durationFrames = 150;
    add.stacks = 3;
    add.potency = 12;
    add.stack = KysChess::EffectStackPolicy::AddStack;
    add.stackLimit = 5;
    auto stacked = system.apply(replaced.target, add);

    REQUIRE(stacked.applied);
    CHECK(stacked.outcome == BattleStatusApplyOutcome::StackChanged);
    const auto* stackedPoison = stacked.target.effects.find(KysChess::BattleStatusKind::Poison);
    REQUIRE(stackedPoison);
    CHECK(stackedPoison->stacks == 5);
    CHECK(stackedPoison->remainingFrames == 150);
    CHECK(stackedPoison->potency == 12);
    CHECK(stackedPoison->sourceUnitId == 3);

    BattleStatusApplyRequest weaker = replace;
    weaker.sourceUnitId = 4;
    weaker.durationFrames = 180;
    weaker.stacks = 6;
    weaker.potency = 11;
    weaker.stack = KysChess::EffectStackPolicy::KeepStrongest;
    weaker.stackLimit = 6;
    auto kept = system.apply(stacked.target, weaker);

    CHECK_FALSE(kept.applied);
    CHECK(kept.outcome == BattleStatusApplyOutcome::KeptStronger);
    const auto* keptPoison = kept.target.effects.find(KysChess::BattleStatusKind::Poison);
    REQUIRE(keptPoison);
    CHECK(keptPoison->stacks == 5);
    CHECK(keptPoison->remainingFrames == 150);
    CHECK(keptPoison->potency == 12);
    CHECK(keptPoison->sourceUnitId == 3);

    const auto consumed = system.consume(kept.target, {
        .kind = KysChess::BattleStatusKind::Poison,
        .stacks = 2,
    });
    CHECK(consumed.consumed);
    CHECK(consumed.consumedStatus.stacks == 2);
    CHECK(consumed.remainingStacks == 3);
    REQUIRE(consumed.target.effects.find(KysChess::BattleStatusKind::Poison));
    CHECK(consumed.target.effects.find(KysChess::BattleStatusKind::Poison)->stacks == 3);
    CHECK(system.snapshot(consumed.target).stacks(KysChess::BattleStatusKind::Poison) == 3);
}

TEST_CASE("BattleStatusSystem_RemainingPoisonProjectionUsesFutureTickSchedule", "[battle][status][poison][unit]")
{
    const BattleRemainingPoisonDamageInput nonAligned{
        .firstFutureFrame = 41,
        .remainingFrames = 31,
        .remainingStacks = 1,
        .intervalFrames = 30,
        .currentHp = 101,
        .damagePct = 10,
    };
    CHECK(projectRemainingPoisonDamage(nonAligned) == 10);

    const BattleRemainingPoisonDamageInput decreasingHp{
        .firstFutureFrame = 1,
        .remainingFrames = 90,
        .remainingStacks = 3,
        .intervalFrames = 30,
        .currentHp = 101,
        .damagePct = 10,
    };
    CHECK(projectRemainingPoisonDamage(decreasingHp) == 27);

    const BattleRemainingPoisonDamageInput saturating{
        .firstFutureFrame = 30,
        .remainingFrames = 1,
        .remainingStacks = 1,
        .intervalFrames = 30,
        .currentHp = std::numeric_limits<int>::max(),
        .damagePct = std::numeric_limits<int>::max(),
    };
    CHECK(projectRemainingPoisonDamage(saturating)
          == std::numeric_limits<int>::max());

    const BattleRemainingPoisonDamageInput stackLimited{
        .firstFutureFrame = 1,
        .remainingFrames = 150,
        .remainingStacks = 2,
        .intervalFrames = 30,
        .currentHp = 100,
        .damagePct = 10,
    };
    CHECK(projectRemainingPoisonDamage(stackLimited) == 19);
}

TEST_CASE("BattleDamageSystem_Bleed_PreservesPendingTickAcrossApplications", "[battle][damage][unit]")
{
    auto target = statusUnit(2);
    target.effects.statuses.push_back({
        .kind = KysChess::BattleStatusKind::Bleed,
        .stacks = 2,
    });

    auto result = BattleDamageSystem().applyBleed(target, 1, 2, 3);

    CHECK(result.applied);
    auto* bleed = result.target.effects.find(KysChess::BattleStatusKind::Bleed);
    REQUIRE(bleed);
    CHECK(bleed->stacks == 3);
    CHECK(bleed->tickFramesRemaining == 10);
    CHECK(bleed->sourceUnitId == 1);
    CHECK(result.value == 3);

    bleed->tickFramesRemaining = 7;
    auto capped = BattleDamageSystem().applyBleed(result.target, 4, 1, 3);
    CHECK_FALSE(capped.applied);
    const auto* cappedBleed = capped.target.effects.find(KysChess::BattleStatusKind::Bleed);
    REQUIRE(cappedBleed);
    CHECK(cappedBleed->stacks == 3);
    CHECK(cappedBleed->tickFramesRemaining == 7);
    CHECK(cappedBleed->sourceUnitId == 4);

    const auto sequenceBeforeReplace = cappedBleed->appliedSequence;
    BattleStatusApplyRequest replace;
    replace.kind = KysChess::BattleStatusKind::Bleed;
    replace.sourceUnitId = 5;
    replace.stacks = 1;
    replace.stack = KysChess::EffectStackPolicy::Replace;
    auto replaced = BattleStatusSystem({ .bleedDamageIntervalFrames = 10 }).apply(
        capped.target,
        replace);

    const auto* replacedBleed = replaced.target.effects.find(KysChess::BattleStatusKind::Bleed);
    REQUIRE(replacedBleed);
    CHECK(replacedBleed->stacks == 1);
    CHECK(replacedBleed->tickFramesRemaining == 7);
    CHECK(replacedBleed->sourceUnitId == 5);
    CHECK(replacedBleed->appliedSequence > sequenceBeforeReplace);
}

TEST_CASE("BattleDamageSystem_TransactionPhysicalDamageAppliesAttackerDefenderModifiers", "[battle][damage][unit]")
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
    CHECK(result.defender.vitals.hp == 8);
    CHECK(result.defenderDelta.unitId == 2);
    CHECK(result.defenderDelta.hpDelta == -92);
    REQUIRE(result.events.size() == 1);
    CHECK(result.events[0].type == BattleDamageEventType::DamageApplied);
    CHECK(result.events[0].sourceUnitId == 1);
    CHECK(result.events[0].targetUnitId == 2);
    CHECK(result.events[0].value == 92);
}

TEST_CASE("BattleDamageSystem_TransactionShieldAbsorbsHpDamageFirst", "[battle][damage][unit]")
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
    CHECK(result.defender.vitals.hp == 70);
    CHECK(result.defender.shield == 0);
    CHECK(result.defenderDelta.shieldDelta == -50);
    REQUIRE(result.events.size() == 2);
    CHECK(result.events[0].type == BattleDamageEventType::ShieldAbsorbed);
    CHECK(result.events[0].value == 50);
    CHECK(result.events[1].type == BattleDamageEventType::DamageApplied);
    CHECK(result.events[1].value == 30);
}

TEST_CASE("BattleDamageSystem_TransactionInvincibilityBlocksNormalDamageButNotExecute", "[battle][damage][unit]")
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
    CHECK(blocked.defender.vitals.hp == 100);
    REQUIRE(blocked.events.size() == 1);
    CHECK(blocked.events[0].type == BattleDamageEventType::BlockedByInvincible);

    auto executedInput = blockedInput;
    executedInput.defender.vitals.hp = 35;
    executedInput.request.canExecute = true;
    executedInput.request.executeThresholdPct = 20;

    auto executed = BattleDamageSystem().resolveTransaction(executedInput);

    CHECK(executed.executed);
    CHECK(executed.defender.alive == false);
    CHECK(executed.defender.vitals.hp == 0);
    CHECK(executed.finalHpDamage == 35);
    REQUIRE(executed.events.size() == 3);
    CHECK(executed.events[0].type == BattleDamageEventType::ExecuteTriggered);
    CHECK(executed.events[1].type == BattleDamageEventType::DamageApplied);
    CHECK(executed.events[2].type == BattleDamageEventType::UnitDied);
}

TEST_CASE("BattleDamageSystem_PreResolvedExecuteRequest_UsesCurrentHpThreshold", "[battle][damage][unit]")
{
    BattleDamageTransactionInput input;
    input.request.attackerUnitId = 1;
    input.request.defenderUnitId = 2;
    input.request.baseDamage = 12;
    input.request.preResolvedDamage = true;
    input.request.canExecute = true;
    input.request.executeThresholdPct = 50;
    input.attacker = unit();
    input.attacker.id = 1;
    input.defender = unit();
    input.defender.id = 2;
    input.defender.vitals.hp = 60;
    input.defender.vitals.maxHp = 200;

    auto result = BattleDamageSystem().resolveTransaction(input);

    CHECK(result.executed);
    CHECK_FALSE(result.defender.alive);
    CHECK(result.defender.vitals.hp == 0);
    CHECK(result.finalHpDamage == 60);
    REQUIRE(result.events.size() == 3);
    CHECK(result.events[0].type == BattleDamageEventType::ExecuteTriggered);
    CHECK(result.events[1].type == BattleDamageEventType::DamageApplied);
    CHECK(result.events[2].type == BattleDamageEventType::UnitDied);
}

TEST_CASE("BattleDamageSystem_TransactionReportsDeathProtection", "[battle][damage][unit]")
{
    BattleDamageTransactionInput protectedInput;
    protectedInput.request.attackerUnitId = 1;
    protectedInput.request.defenderUnitId = 2;
    protectedInput.request.baseDamage = 50;
    protectedInput.attacker = unit();
    protectedInput.attacker.id = 1;
    protectedInput.defender = unit();
    protectedInput.defender.id = 2;
    protectedInput.defender.vitals.hp = 20;
    protectedInput.defender.deathPrevention = true;
    protectedInput.defender.deathPreventionFrames = 30;

    auto protectedResult = BattleDamageSystem().resolveTransaction(protectedInput);

    CHECK(protectedResult.defender.vitals.hp == 1);
    CHECK(protectedResult.defender.alive);
    CHECK(protectedResult.defender.deathPreventionUsed);
    CHECK(protectedResult.defender.invincible == 30);
    CHECK(protectedResult.defenderDelta.hpDelta == -19);
    REQUIRE(protectedResult.events.size() == 2);
    CHECK(protectedResult.events[0].type == BattleDamageEventType::DamageApplied);
    CHECK(protectedResult.events[1].type == BattleDamageEventType::DeathPrevented);
}

TEST_CASE("BattleDamageSystem_TransactionMpDamageSkipsHpDefenseLayers", "[battle][damage][unit]")
{
    BattleDamageTransactionInput input;
    input.request.attackerUnitId = 1;
    input.request.defenderUnitId = 2;
    input.request.mpDamage = 35;
    input.attacker = unit();
    input.attacker.id = 1;
    input.defender = unit();
    input.defender.id = 2;
    input.defender.vitals.mp = 20;
    input.defender.vitals.maxMp = 100;

    auto result = BattleDamageSystem().resolveTransaction(input);

    CHECK(result.finalHpDamage == 0);
    CHECK(result.finalMpDamage == 20);
    CHECK(result.defender.vitals.hp == 100);
    CHECK(result.defender.vitals.mp == 0);
    CHECK(result.defenderDelta.mpDelta == -20);
    REQUIRE(result.events.size() == 1);
    CHECK(result.events[0].type == BattleDamageEventType::MpDamageApplied);
    CHECK(result.events[0].value == 20);
}

TEST_CASE("BattleDamageSystem_TransactionOnHitResourcesAndCooldownRequireAcceptedHit", "[battle][damage][unit]")
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
    input.attacker.vitals.hp = 90;
    input.attacker.vitals.maxHp = 120;
    input.attacker.vitals.mp = 80;
    input.attacker.vitals.maxMp = 100;
    input.defender = unit();
    input.defender.id = 2;
    input.defender.vitals.mp = 12;
    input.defender.vitals.maxMp = 100;
    input.defenderCooldown.alive = true;
    input.defenderCooldown.cooldown = 50;
    input.defenderCooldown.cooldownMax = 100;
    input.defenderCooldown.haveAction = true;
    input.defenderCooldown.operationType = BattleOperationType::Melee;
    input.defenderCooldown.actType = 1;

    auto result = BattleDamageSystem().resolveTransaction(input);

    CHECK(result.attacker.vitals.hp == 120);
    CHECK(result.attacker.vitals.mp == 100);
    CHECK(result.defender.vitals.mp == 0);
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
    CHECK(blocked.attacker.vitals.hp == 90);
    CHECK(blocked.attacker.vitals.mp == 80);
    CHECK(blocked.defender.vitals.mp == 12);
    CHECK(blocked.defenderCooldown.cooldown == 50);
    CHECK(blocked.cooldownDelta == 0);
}

TEST_CASE("BattleDamageSystem_TransactionOnHitMpGainHonorsMpBlockAndRecoveryBonus", "[battle][damage][unit]")
{
    BattleDamageTransactionInput input;
    input.request.attackerUnitId = 1;
    input.request.defenderUnitId = 2;
    input.request.acceptedHit = true;
    input.request.mpOnHit = 10;
    input.request.mpDrain = 10;
    input.attacker = unit();
    input.attacker.id = 1;
    input.attacker.vitals.mp = 20;
    input.attacker.vitals.maxMp = 100;
    input.attacker.mpRecoveryBonusPct = 50;
    input.defender = unit();
    input.defender.id = 2;
    input.defender.vitals.mp = 10;

    auto result = BattleDamageSystem().resolveTransaction(input);

    CHECK(result.attacker.vitals.mp == 50);
    CHECK(result.defender.vitals.mp == 0);
    CHECK(result.attackerDelta.mpDelta == 30);
    CHECK(result.defenderDelta.mpDelta == -10);

    input.attacker.mpBlocked = true;
    auto blocked = BattleDamageSystem().resolveTransaction(input);

    CHECK(blocked.attacker.vitals.mp == 20);
    CHECK(blocked.defender.vitals.mp == 0);
    CHECK(blocked.attackerDelta.mpDelta == 0);
    CHECK(blocked.defenderDelta.mpDelta == -10);
}

TEST_CASE("BattleDamageSystem_TransactionAcceptedZeroDamageEffectsCanApplyStatus", "[battle][damage][unit]")
{
    BattleDamageTransactionInput input;
    input.request.attackerUnitId = 1;
    input.request.defenderUnitId = 2;
    input.request.acceptedHit = true;
    input.request.stunFrames = 6;
    input.request.bleedStacks = 1;
    input.request.bleedMaxStacks = 3;
    input.attacker = unit();
    input.attacker.id = 1;
    input.defender = unit();
    input.defender.id = 2;
    input.defenderStatus = statusUnit(2);

    auto result = BattleDamageSystem().resolveTransaction(input);

    CHECK(result.finalHpDamage == 0);
    CHECK(result.defenderStatus.effects.remainingFrames(KysChess::BattleStatusKind::Stun) == 6);
    REQUIRE(result.defenderStatus.effects.find(KysChess::BattleStatusKind::Bleed));
    CHECK(result.defenderStatus.effects.find(KysChess::BattleStatusKind::Bleed)->stacks == 1);
    REQUIRE(result.events.size() == 2);
    CHECK(result.events[0].statusType == BattleDamageStatusType::Bleed);
    CHECK(result.events[1].statusType == BattleDamageStatusType::Stun);
}

TEST_CASE("BattleDamageSystem_TransactionFrozenAppliesResistanceAndControlImmunity", "[battle][damage][unit]")
{
    BattleDamageTransactionInput input;
    input.request.attackerUnitId = 1;
    input.request.defenderUnitId = 2;
    input.request.acceptedHit = true;
    input.request.stunFrames = 10;
    input.attacker = unit();
    input.attacker.id = 1;
    input.defender = unit();
    input.defender.id = 2;
    input.defender.shield = 1;
    input.defenderStatus = statusUnit(2);
    input.defenderStatus.effects.freezeReductionPct = 20;
    input.defenderStatus.effects.shieldFreezeResPct = 30;
    input.defenderStatus.effects.controlImmunityFrames = 3;

    auto result = BattleDamageSystem().resolveTransaction(input);

    CHECK(result.defenderStatus.effects.controlImmunityFrames == 0);
    CHECK(result.defenderStatus.effects.remainingFrames(KysChess::BattleStatusKind::Stun) == 2);
    CHECK(result.defenderStatus.effects.maximumFrames(KysChess::BattleStatusKind::Stun) == 2);
    REQUIRE(result.events.size() == 1);
    CHECK(result.events[0].statusType == BattleDamageStatusType::Stun);
    CHECK(result.events[0].value == 2);

    input.defenderStatus.hp = 40;
    input.defenderStatus.maxHp = 200;
    auto lowHp = BattleDamageSystem().resolveTransaction(input);

    CHECK_FALSE(lowHp.defenderStatus.effects.has(KysChess::BattleStatusKind::Stun));
    CHECK(lowHp.defenderStatus.effects.controlImmunityFrames == 3);
    CHECK(lowHp.events.empty());
}

TEST_CASE("BattleDamageSystem_TransactionPreResolvedDamageStillAppliesInstantDefense", "[battle][damage][unit]")
{
    BattleDamageTransactionInput input;
    input.request.attackerUnitId = 1;
    input.request.defenderUnitId = 2;
    input.request.baseDamage = 40;
    input.request.preResolvedDamage = true;
    input.attacker = unit();
    input.attacker.id = 1;
    input.attacker.vitals.hp = 120;
    input.defender = unit();
    input.defender.id = 2;
    input.defender.vitals.hp = 30;
    input.defender.invincible = 10;
    input.defender.shield = 99;

    auto result = BattleDamageSystem().resolveTransaction(input);

    CHECK(result.blockedByInvincible);
    CHECK(result.finalHpDamage == 0);
    CHECK(result.defender.alive);
    CHECK(result.defender.vitals.hp == 30);
    CHECK(result.defender.shield == 99);
    CHECK(result.attacker.vitals.hp == 120);
    CHECK(result.attackerDelta.hpDelta == 0);
    REQUIRE(result.events.size() == 1);
    CHECK(result.events[0].type == BattleDamageEventType::BlockedByInvincible);
}

TEST_CASE("BattleDamageSystem_TransactionTypedDamageTakenRespectsPreResolvedBoundary", "[battle][damage][status][unit]")
{
    BattleDamageTransactionInput input;
    input.request.attackerUnitId = 1;
    input.request.defenderUnitId = 2;
    input.attacker = unit();
    input.attacker.id = 1;
    input.defender = unit();
    input.defender.id = 2;
    input.defenderStatus = statusUnit(2);
    input.defenderStatus.effects.statuses.push_back({
        .kind = KysChess::BattleStatusKind::WitheredBone,
        .sourceUnitId = 1,
        .remainingFrames = 120,
        .potency = 25,
        .secondaryPotency = 75,
    });

    input.request.baseDamage = 66;
    const auto resolvedHere = BattleDamageSystem().resolveTransaction(input);
    CHECK(resolvedHere.finalHpDamage == 82);

    input.request.baseDamage = 82;
    input.request.preResolvedDamage = true;
    const auto resolvedEarlier = BattleDamageSystem().resolveTransaction(input);
    CHECK(resolvedEarlier.finalHpDamage == 82);

    input.request.baseDamage = 66;
    input.request.preResolvedModifierPolicy =
        BattlePreResolvedModifierPolicy::DefenderTypedStatuses;
    input.request.usingSkill = true;
    input.attackerModifiers.skillDamagePct = 100;
    input.attackerModifiers.flatDamageIncrease = 100;
    input.defenderModifiers.flatDamageReduction = 50;
    input.defenderModifiers.damageReductionPct = 50;
    const auto typedDefenderOnly = BattleDamageSystem().resolveTransaction(input);
    CHECK(typedDefenderOnly.finalHpDamage == 82);

    input.defenderStatus.effects.statuses.push_back({
        .kind = KysChess::BattleStatusKind::BattleSpirit,
        .sourceUnitId = 2,
        .remainingFrames = 120,
        .secondaryPotency = 10,
    });
    input.request.baseDamage = 80;
    const auto typedReduction = BattleDamageSystem().resolveTransaction(input);
    CHECK(typedReduction.resolvedDamageBeforeDefense == 90);
    CHECK(typedReduction.finalHpDamage == 90);
    CHECK(typedReduction.combinedDamageReductionBasisPoints == 1000);
}

TEST_CASE("BattleDamageSystem_PreResolvedTypedReductionSharesCapWithAbsorptionAndKeepsDefenseLayers", "[battle][damage][status][absorption][unit]")
{
    BattleDamageTransactionInput input;
    input.request.attackerUnitId = 1;
    input.request.defenderUnitId = 2;
    input.request.baseDamage = 100;
    input.request.preResolvedDamage = true;
    input.request.preResolvedModifierPolicy =
        BattlePreResolvedModifierPolicy::DefenderTypedStatuses;
    input.attacker = unit();
    input.attacker.id = 1;
    input.defender = unit();
    input.defender.id = 2;
    input.defender.shield = 15;
    input.defenderStatus = statusUnit(2);
    input.defenderStatus.effects.statuses.push_back({
        .kind = KysChess::BattleStatusKind::BattleSpirit,
        .sourceUnitId = 2,
        .remainingFrames = 120,
        .secondaryPotency = 70,
    });
    input.absorptionLayers = { { 1, 50 } };

    const auto reduced = BattleDamageSystem().resolveTransaction(input);

    CHECK(reduced.combinedDamageReductionBasisPoints == 8000);
    REQUIRE(reduced.absorptionReceipts.size() == 1);
    CHECK(reduced.absorptionReceipts.front().absorbedDamage == 10);
    CHECK(reduced.shieldAbsorbed == 15);
    CHECK(reduced.finalHpDamage == 5);
}

TEST_CASE("BattleDamageSystem_PreservesTypedPoisonAndBleedKinds", "[battle][damage][status][unit]")
{
    for (const auto kind : {
             KysChess::BattleDamageKind::Poison,
             KysChess::BattleDamageKind::Bleed,
         })
    {
        BattleDamageTransactionInput input;
        input.request.attackerUnitId = 1;
        input.request.defenderUnitId = 2;
        input.request.baseDamage = 10;
        input.request.damageKind = kind;
        input.request.preResolvedDamage = true;
        input.request.preResolvedModifierPolicy =
            BattlePreResolvedModifierPolicy::DefenderTypedStatuses;
        input.attacker = unit();
        input.attacker.id = 1;
        input.defender = unit();
        input.defender.id = 2;
        input.defenderStatus = statusUnit(2);

        const auto result = BattleDamageSystem().resolveTransaction(input);

        CHECK(result.damageKind == kind);
        REQUIRE(result.events.size() == 1);
        CHECK(result.events.front().damageKind == kind);
    }
}
