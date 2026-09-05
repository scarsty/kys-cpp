#include "battle/BattleDamageSystem.h"
#include "ChessBattleEffectTypes.h"
#include "BattleCoreTestHelpers.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <limits>

using namespace KysChess::Battle;
using namespace KysChess;
using namespace KysChess::Battle::Test;

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

BattleStatusProducerProvenance bleedProducer(int sourceUnitId, int sourceId)
{
    return makeBattleStatusProducerProvenance(
        {
            .kind = EffectSourceKind::Combo,
            .sourceId = sourceId,
            .ownerUnitId = sourceUnitId,
        },
        EffectRuleId{ static_cast<std::uint64_t>(sourceId) },
        4,
        2);
}

void bindTestStatusBehavior(
    BattleStatusApplyRequest& request,
    std::shared_ptr<const StatusBehaviorDefinition> behavior,
    int sourceId)
{
    const EffectSourceBinding binding{
        .kind = EffectSourceKind::Magic,
        .sourceId = sourceId,
        .ownerUnitId = request.sourceUnitId,
    };
    const EffectRuleId ruleId{ static_cast<std::uint64_t>(sourceId) };
    request.producer = StatusProducerKey{ .binding = binding, .ruleId = ruleId };
    request.producerFamily = StatusProducerFamilyKey{
        .sourceKind = binding.kind,
        .sourceId = sourceId,
        .logicalOwnerUnitId = request.sourceUnitId,
        .ruleId = ruleId,
    };
    request.behavior = std::move(behavior);
    request.origin = BattleStatusEffectOrigin{ binding, ruleId, 0 };
}

BattleStatusContribution damageInterceptorContribution(
    BattleStatusKind kind,
    std::shared_ptr<const StatusBehaviorDefinition> behavior,
    int stacks,
    int sourceId = 0,
    std::uint64_t appliedSequence = 1)
{
    if (sourceId == 0) sourceId = 6000 + static_cast<int>(kind);
    const EffectSourceBinding binding{
        .kind = EffectSourceKind::Magic,
        .sourceId = sourceId,
        .ownerUnitId = 2,
    };
    const EffectRuleId ruleId{ static_cast<std::uint64_t>(sourceId) };
    return {
        .kind = kind,
        .producer = StatusProducerKey{ .binding = binding, .ruleId = ruleId },
        .producerFamily = StatusProducerFamilyKey{
            .sourceKind = binding.kind,
            .sourceId = binding.sourceId,
            .logicalOwnerUnitId = binding.ownerUnitId,
            .ruleId = ruleId,
        },
        .behavior = std::move(behavior),
        .sourceUnitId = 2,
        .stacks = stacks,
        .origin = BattleStatusEffectOrigin{ binding, ruleId, 0 },
        .appliedSequence = appliedSequence,
    };
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

TEST_CASE("BattleDamageSystem_Modifiers_ApplySignedPersistentPercentagesAtTheSameSlots",
          "[battle][damage][unit][status][signed]")
{
    BattleDamageModifierInput input;
    input.damage = 100;
    input.usingSkill = true;
    input.attacker.skillDamagePct = -20;
    input.defender.damageTakenIncreasePct = -25;
    input.defenderUnit = unit();

    const auto result = BattleDamageSystem().applyModifiers(input);

    CHECK(result.damage.toInt() == 60);
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
    input.bleedProducer = bleedProducer(11, 91);

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
    auto invincible = system.resolveDefense({ 80, false, true, defender });
    CHECK(invincible.damage == 0.0);
    CHECK(invincible.blockedByInvincible);

    defender = unit();
    defender.dualWieldBlocksRemaining = 1;
    auto dualWield = system.resolveDefense({ 80, false, false, defender });
    CHECK(dualWield.damage == 0.0);
    CHECK(dualWield.defender.dualWieldBlocksRemaining == 0);
    CHECK(dualWield.blockedByDualWield);

    defender = unit();
    defender.shield = 50;
    auto shield = system.resolveDefense({ 80, false, false, defender });
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

    auto result = BattleDamageSystem().resolveDefense({ 80, true, true, defender });

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

TEST_CASE("BattleStatusSystem_PoisonHonorsReplacementAndStrongestPolicies", "[battle][status][poison][unit]")
{
    BattleStatusSystem system({});
    BattleStatusApplyRequest replace;
    replace.kind = KysChess::BattleStatusKind::Poison;
    replace.sourceUnitId = 1;
    replace.durationFrames = 120;
    replace.stacks = 2;
    replace.stack = KysChess::EffectStackPolicy::Replace;
    replace.stackLimit = 5;
    bindTestStatusBehavior(replace, poisonStatusBehavior(10), 101);

    auto replaced = system.apply(statusUnit(2), replace);

    REQUIRE(replaced.applied);
    CHECK(replaced.outcome == BattleStatusApplyOutcome::Applied);
    REQUIRE(replaced.target.effects.find(KysChess::BattleStatusKind::Poison));
    CHECK(replaced.target.effects.find(KysChess::BattleStatusKind::Poison)->stacks == 2);
    CHECK(system.snapshot(replaced.target).stacks(KysChess::BattleStatusKind::Poison) == 2);

    BattleStatusApplyRequest weaker = replace;
    weaker.sourceUnitId = 4;
    weaker.durationFrames = 180;
    weaker.stacks = 6;
    weaker.stack = KysChess::EffectStackPolicy::KeepStrongest;
    weaker.stackLimit = 6;
    bindTestStatusBehavior(weaker, poisonStatusBehavior(9), 103);
    auto kept = system.apply(replaced.target, weaker);

    CHECK_FALSE(kept.applied);
    CHECK(kept.outcome == BattleStatusApplyOutcome::KeptStronger);
    const auto* keptPoison = kept.target.effects.find(KysChess::BattleStatusKind::Poison);
    REQUIRE(keptPoison);
    CHECK(keptPoison->stacks == 2);
    CHECK(keptPoison->remainingFrames == 120);
    CHECK(poisonDamagePercent(keptPoison->behavior) == 10);
    CHECK(keptPoison->sourceUnitId == 1);

    BattleStatusApplyRequest equal = weaker;
    bindTestStatusBehavior(equal, poisonStatusBehavior(10), 104);
    equal.durationFrames = 180;
    const auto equalKept = system.apply(replaced.target, equal);

    CHECK_FALSE(equalKept.applied);
    CHECK(equalKept.outcome == BattleStatusApplyOutcome::KeptStronger);
    const auto* equalPoison = equalKept.target.effects.find(
        KysChess::BattleStatusKind::Poison);
    REQUIRE(equalPoison);
    CHECK(equalPoison->remainingFrames == 120);
    CHECK(equalPoison->stacks == 2);
    CHECK(poisonDamagePercent(equalPoison->behavior) == 10);
    CHECK(equalPoison->sourceUnitId == 1);

    BattleStatusApplyRequest stronger = weaker;
    bindTestStatusBehavior(stronger, poisonStatusBehavior(13), 105);
    stronger.durationFrames = 180;
    stronger.stacks = 6;
    stronger.origin = BattleStatusEffectOrigin{
        .binding = {
            .kind = KysChess::EffectSourceKind::Magic,
            .sourceId = 21,
            .ownerUnitId = 4,
            .sourceTeam = 0,
        },
        .ruleId = KysChess::EffectRuleId{ 21 },
        .ruleOrder = 7,
    };
    const auto strongerApplied = system.apply(equalKept.target, stronger);

    REQUIRE(strongerApplied.applied);
    CHECK(strongerApplied.outcome == BattleStatusApplyOutcome::Replaced);
    const auto* strongerPoison = strongerApplied.target.effects.find(
        KysChess::BattleStatusKind::Poison);
    REQUIRE(strongerPoison);
    CHECK(strongerPoison->remainingFrames == 180);
    CHECK(strongerPoison->stacks == 6);
    CHECK(poisonDamagePercent(strongerPoison->behavior) == 13);
    CHECK(strongerPoison->sourceUnitId == 4);
    CHECK(strongerPoison->origin == stronger.origin);

    BattleStatusApplyRequest reset = stronger;
    reset.sourceUnitId = 5;
    reset.durationFrames = 90;
    reset.stacks = 2;
    reset.stack = KysChess::EffectStackPolicy::Replace;
    bindTestStatusBehavior(reset, poisonStatusBehavior(10), 106);
    const auto resetApplied = system.apply(strongerApplied.target, reset);

    REQUIRE(resetApplied.applied);
    CHECK(resetApplied.outcome == BattleStatusApplyOutcome::Replaced);
    const auto* resetPoison = resetApplied.target.effects.find(
        KysChess::BattleStatusKind::Poison);
    REQUIRE(resetPoison);
    CHECK(resetPoison->remainingFrames == 90);
    CHECK(resetPoison->stacks == 2);
    CHECK(poisonDamagePercent(resetPoison->behavior) == 10);
    CHECK(resetPoison->sourceUnitId == 5);
    CHECK(resetPoison->origin == reset.origin);

    const auto consumed = system.consume(kept.target, {
        .kind = KysChess::BattleStatusKind::Poison,
        .stacks = 2,
    });
    CHECK(consumed.consumed);
    CHECK(consumed.consumedStatus.stacks == 2);
    CHECK(consumed.remainingStacks == 0);
    CHECK_FALSE(consumed.target.effects.find(KysChess::BattleStatusKind::Poison));
    CHECK(system.snapshot(consumed.target).stacks(KysChess::BattleStatusKind::Poison) == 0);
}

TEST_CASE("BattleStatusSystem keeps stacked contribution origin immutable at the layer cap",
          "[battle][status][origin][stack]")
{
    BattleStatusSystem system({});
    BattleStatusApplyRequest first;
    first.kind = KysChess::BattleStatusKind::TrueQi;
    first.sourceUnitId = 1;
    first.stacks = 10;
    first.stack = KysChess::EffectStackPolicy::AddStack;
    first.stackLimit = 10;
    first.origin = BattleStatusEffectOrigin{
        .binding = {
            .kind = KysChess::EffectSourceKind::Magic,
            .sourceId = 106,
            .ownerUnitId = 1,
            .sourceTeam = 0,
            .runtimeInstanceId = 7,
        },
        .ruleId = KysChess::EffectRuleId{ 100 },
        .ruleOrder = 4,
    };
    first.producer = StatusProducerKey{
        .binding = first.origin->binding,
        .ruleId = first.origin->ruleId,
    };
    first.producerFamily = StatusProducerFamilyKey{
        .sourceKind = first.origin->binding.kind,
        .sourceId = first.origin->binding.sourceId,
        .logicalOwnerUnitId = first.origin->binding.ownerUnitId,
        .ruleId = first.origin->ruleId,
    };
    first.behavior = trueQiStatusBehavior(9);
    const auto applied = system.apply(statusUnit(1), first);
    REQUIRE(applied.applied);

    auto cappedRefresh = first;
    cappedRefresh.stacks = 1;
    cappedRefresh.origin->binding.runtimeInstanceId = 8;
    cappedRefresh.origin->ruleId = KysChess::EffectRuleId{ 200 };
    cappedRefresh.origin->ruleOrder = 9;
    const auto refreshed = system.apply(applied.target, cappedRefresh);

    CHECK_FALSE(refreshed.applied);
    CHECK(refreshed.outcome == BattleStatusApplyOutcome::StackChanged);
    const auto* trueQi = refreshed.target.effects.find(
        KysChess::BattleStatusKind::TrueQi);
    REQUIRE(trueQi);
    CHECK(trueQi->stacks == 10);
    CHECK(std::get<DealDamageAction>(
        trueQi->behavior->rules.front().actions.front().value).amount.flat == 9);
    CHECK(trueQi->origin == first.origin);
}

TEST_CASE("BattleStatusSystem saturates additive status arithmetic",
          "[battle][status][stack][boundary]")
{
    const auto verifyLayers = [](KysChess::BattleStatusKind kind, int durationFrames)
    {
        BattleStatusSystem system({});
        BattleStatusApplyRequest request;
        request.kind = kind;
        request.sourceUnitId = 1;
        request.durationFrames = durationFrames;
        request.stacks = std::numeric_limits<int>::max() - 1;
        request.stack = KysChess::EffectStackPolicy::AddStack;
        if (kind == KysChess::BattleStatusKind::Bleed)
            request.targetTotalLimit = std::numeric_limits<int>::max();
        else
            request.stackLimit = std::numeric_limits<int>::max();
        const auto behavior = kind == KysChess::BattleStatusKind::Bleed
            ? makeRuntimeBleedStatusBehavior()
            : trueQiStatusBehavior(1);
        bindTestStatusBehavior(request, behavior, 1000 + static_cast<int>(kind));

        const auto first = system.apply(statusUnit(2), request);
        REQUIRE(first.target.effects.find(kind));
        CHECK(first.target.effects.find(kind)->stacks
            == std::numeric_limits<int>::max() - 1);

        request.stacks = 10;
        const auto second = system.apply(first.target, request);
        REQUIRE(second.target.effects.find(kind));
        CHECK(second.target.effects.find(kind)->stacks
            == std::numeric_limits<int>::max());
    };

    verifyLayers(KysChess::BattleStatusKind::Bleed, 0);
    verifyLayers(KysChess::BattleStatusKind::TrueQi, 0);

    BattleStatusSystem system({});
    BattleStatusApplyRequest stun;
    stun.kind = KysChess::BattleStatusKind::Stun;
    stun.sourceUnitId = 1;
    stun.durationFrames = std::numeric_limits<int>::max() - 1;
    stun.stack = KysChess::EffectStackPolicy::Independent;
    const auto firstStun = system.apply(statusUnit(2), stun);
    stun.durationFrames = 10;
    const auto secondStun = system.apply(firstStun.target, stun);
    REQUIRE(secondStun.target.effects.find(KysChess::BattleStatusKind::Stun));
    CHECK(secondStun.target.effects.find(KysChess::BattleStatusKind::Stun)
              ->remainingFrames
        == std::numeric_limits<int>::max());
}

TEST_CASE("BattleStatusSystem_RemainingPoisonProjectionUsesFutureTickSchedule", "[battle][status][poison][unit]")
{
    const BattleRemainingPoisonDamageInput nonAligned{
        .framesUntilNextTick = 11,
        .remainingFrames = 31,
        .remainingStacks = 1,
        .intervalFrames = 30,
        .currentHp = 101,
        .damagePct = 10,
    };
    CHECK(projectRemainingPoisonDamage(nonAligned) == 10);

    const BattleRemainingPoisonDamageInput decreasingHp{
        .framesUntilNextTick = 1,
        .remainingFrames = 90,
        .remainingStacks = 3,
        .intervalFrames = 30,
        .currentHp = 101,
        .damagePct = 10,
    };
    CHECK(projectRemainingPoisonDamage(decreasingHp) == 27);

    const BattleRemainingPoisonDamageInput saturating{
        .framesUntilNextTick = 1,
        .remainingFrames = 1,
        .remainingStacks = 1,
        .intervalFrames = 30,
        .currentHp = std::numeric_limits<int>::max(),
        .damagePct = std::numeric_limits<int>::max(),
    };
    CHECK(projectRemainingPoisonDamage(saturating)
          == std::numeric_limits<int>::max());

    const BattleRemainingPoisonDamageInput stackLimited{
        .framesUntilNextTick = 1,
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
    auto result = BattleDamageSystem().applyBleed(
        target, bleedProducer(1, 101), 2, 3);

    CHECK(result.applied);
    auto* bleed = result.target.effects.find(KysChess::BattleStatusKind::Bleed);
    REQUIRE(bleed);
    CHECK(bleed->stacks == 2);
    REQUIRE(bleed->behaviorRuntime.size() == 1);
    CHECK(bleed->behaviorRuntime.front().intervalFramesRemaining == 10);
    CHECK(bleed->sourceUnitId == 1);
    CHECK(result.value == 2);

    bleed->behaviorRuntime.front().intervalFramesRemaining = 7;
    auto capped = BattleDamageSystem().applyBleed(
        result.target, bleedProducer(1, 101), 2, 3);
    CHECK(capped.applied);
    const auto* cappedBleed = capped.target.effects.find(KysChess::BattleStatusKind::Bleed);
    REQUIRE(cappedBleed);
    CHECK(cappedBleed->stacks == 3);
    CHECK(cappedBleed->behaviorRuntime.front().intervalFramesRemaining == 7);
    CHECK(cappedBleed->sourceUnitId == 1);

    const auto rejectedAtSharedCap = BattleDamageSystem().applyBleed(
        capped.target, bleedProducer(4, 404), 1, 3);
    CHECK_FALSE(rejectedAtSharedCap.applied);
    CHECK(rejectedAtSharedCap.target.effects.statuses.size() == 1);
    CHECK(BattleStatusSystem({}).snapshot(rejectedAtSharedCap.target).stacks(
        KysChess::BattleStatusKind::Bleed) == 3);
    const auto* unchanged = rejectedAtSharedCap.target.effects.find(
        KysChess::BattleStatusKind::Bleed);
    REQUIRE(unchanged);
    CHECK(unchanged->sourceUnitId == 1);
    CHECK(unchanged->behaviorRuntime.front().intervalFramesRemaining == 7);
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

TEST_CASE("BattleDamageSystem consumes one damage-block charge per eligible transaction",
          "[battle][damage][status][charge]")
{
    BattleDamageTransactionInput input;
    input.request.attackerUnitId = 1;
    input.request.defenderUnitId = 2;
    input.request.baseDamage = 40;
    input.attacker = unit();
    input.attacker.id = 1;
    input.defender = unit();
    input.defender.id = 2;
    input.defenderStatus = statusUnit(2);
    input.defenderStatus.effects.statuses.push_back(damageInterceptorContribution(
        KysChess::BattleStatusKind::DamageBlockLayer,
        damageBlockStatusBehavior(),
        2));

    const auto first = BattleDamageSystem().resolveTransaction(input);

    CHECK(first.blockedByDamageLayer);
    CHECK(first.finalHpDamage == 0);
    const auto* remaining = first.defenderStatus.effects.find(
        KysChess::BattleStatusKind::DamageBlockLayer);
    REQUIRE(remaining);
    CHECK(remaining->stacks == 1);

    input.defender = first.defender;
    input.defenderStatus = first.defenderStatus;
    const auto second = BattleDamageSystem().resolveTransaction(input);

    CHECK(second.blockedByDamageLayer);
    CHECK(second.finalHpDamage == 0);
    CHECK_FALSE(second.defenderStatus.effects.has(
        KysChess::BattleStatusKind::DamageBlockLayer));

    input.defender = second.defender;
    input.defenderStatus = second.defenderStatus;
    const auto unblocked = BattleDamageSystem().resolveTransaction(input);

    CHECK_FALSE(unblocked.blockedByDamageLayer);
    CHECK(unblocked.finalHpDamage == 40);
}

TEST_CASE("BattleDamageSystem consumes the next-hit cap even when it does not reduce damage",
          "[battle][damage][status][charge][cap]")
{
    const auto resolve = [](int damage)
    {
        BattleDamageTransactionInput input;
        input.request.attackerUnitId = 1;
        input.request.defenderUnitId = 2;
        input.request.baseDamage = damage;
        input.attacker = unit();
        input.attacker.id = 1;
        input.defender = unit();
        input.defender.id = 2;
        input.defenderStatus = statusUnit(2);
        input.defenderStatus.effects.statuses.push_back(damageInterceptorContribution(
            KysChess::BattleStatusKind::SingleHitCapLayer,
            singleHitCapStatusBehavior(30),
            1));
        return BattleDamageSystem().resolveTransaction(input);
    };

    const auto belowCap = resolve(20);
    CHECK(belowCap.singleHitCapConsumed);
    CHECK_FALSE(belowCap.singleHitCapped);
    CHECK(belowCap.finalHpDamage == 20);
    CHECK_FALSE(belowCap.defenderStatus.effects.has(
        KysChess::BattleStatusKind::SingleHitCapLayer));

    const auto aboveCap = resolve(50);
    CHECK(aboveCap.singleHitCapConsumed);
    CHECK(aboveCap.singleHitCapped);
    CHECK(aboveCap.finalHpDamage == 30);
    CHECK_FALSE(aboveCap.defenderStatus.effects.has(
        KysChess::BattleStatusKind::SingleHitCapLayer));
}

TEST_CASE("BattleDamageSystem resolves blocks before the strongest next-hit cap and consumes exactly one contribution",
          "[battle][damage][status][charge][cap][priority]")
{
    const auto makeInput = []
    {
        BattleDamageTransactionInput input;
        input.request.attackerUnitId = 1;
        input.request.defenderUnitId = 2;
        input.request.baseDamage = 80;
        input.attacker = unit();
        input.attacker.id = 1;
        input.defender = unit();
        input.defender.id = 2;
        input.defenderStatus = statusUnit(2);
        input.defenderStatus.effects.statuses.push_back(
            damageInterceptorContribution(
                BattleStatusKind::SingleHitCapLayer,
                singleHitCapStatusBehavior(30),
                1,
                6030,
                1));
        input.defenderStatus.effects.statuses.push_back(
            damageInterceptorContribution(
                BattleStatusKind::SingleHitCapLayer,
                singleHitCapStatusBehavior(15),
                1,
                6015,
                2));
        input.defenderStatus.effects.statuses.push_back(
            damageInterceptorContribution(
                BattleStatusKind::DamageBlockLayer,
                damageBlockStatusBehavior(),
                1,
                6099,
                3));
        return input;
    };

    auto input = makeInput();
    const auto blocked = BattleDamageSystem().resolveTransaction(input);
    CHECK(blocked.blockedByDamageLayer);
    CHECK_FALSE(blocked.singleHitCapConsumed);
    CHECK(blocked.finalHpDamage == 0);
    CHECK_FALSE(blocked.defenderStatus.effects.has(
        BattleStatusKind::DamageBlockLayer));
    CHECK(BattleStatusSystem({}).snapshot(blocked.defenderStatus).stacks(
        BattleStatusKind::SingleHitCapLayer) == 2);

    input.defender = blocked.defender;
    input.defenderStatus = blocked.defenderStatus;
    const auto capped = BattleDamageSystem().resolveTransaction(input);
    CHECK_FALSE(capped.blockedByDamageLayer);
    CHECK(capped.singleHitCapConsumed);
    CHECK(capped.singleHitCapped);
    CHECK(capped.finalHpDamage == 15);
    const auto remainingCaps = BattleStatusSystem({}).snapshot(
        capped.defenderStatus);
    CHECK(remainingCaps.stacks(BattleStatusKind::SingleHitCapLayer) == 1);
    REQUIRE(capped.defenderStatus.effects.statuses.size() == 1);
    CHECK(capped.defenderStatus.effects.statuses.front().producer->binding.sourceId
        == 6030);
}

TEST_CASE("BattleDamageSystem breaks equal next-hit cap values by oldest application",
          "[battle][damage][status][charge][cap][priority]")
{
    BattleDamageTransactionInput input;
    input.request.attackerUnitId = 1;
    input.request.defenderUnitId = 2;
    input.request.baseDamage = 80;
    input.attacker = unit();
    input.attacker.id = 1;
    input.defender = unit();
    input.defender.id = 2;
    input.defenderStatus = statusUnit(2);
    input.defenderStatus.effects.statuses.push_back(
        damageInterceptorContribution(
            BattleStatusKind::SingleHitCapLayer,
            singleHitCapStatusBehavior(20),
            1,
            6021,
            9));
    input.defenderStatus.effects.statuses.push_back(
        damageInterceptorContribution(
            BattleStatusKind::SingleHitCapLayer,
            singleHitCapStatusBehavior(20),
            1,
            6022,
            4));

    const auto result = BattleDamageSystem().resolveTransaction(input);

    CHECK(result.singleHitCapConsumed);
    CHECK(result.finalHpDamage == 20);
    REQUIRE(result.defenderStatus.effects.statuses.size() == 1);
    CHECK(result.defenderStatus.effects.statuses.front().producer->binding.sourceId
        == 6021);
}

TEST_CASE("BattleDamageSystem invincibility dual-wield and execute do not consume block or cap out of order",
          "[battle][damage][status][charge][cap][priority]")
{
    const auto resolve = [](int invincible, int dualWield, bool execute)
    {
        BattleDamageTransactionInput input;
        input.request.attackerUnitId = 1;
        input.request.defenderUnitId = 2;
        input.request.baseDamage = 80;
        input.request.canExecute = execute;
        input.request.executeThresholdPct = execute ? 100 : 0;
        input.attacker = unit();
        input.attacker.id = 1;
        input.defender = unit();
        input.defender.id = 2;
        input.defender.invincible = invincible;
        input.defender.dualWieldBlocksRemaining = dualWield;
        input.defenderStatus = statusUnit(2);
        input.defenderStatus.effects.statuses.push_back(
            damageInterceptorContribution(
                BattleStatusKind::DamageBlockLayer,
                damageBlockStatusBehavior(),
                1,
                6101,
                1));
        input.defenderStatus.effects.statuses.push_back(
            damageInterceptorContribution(
                BattleStatusKind::SingleHitCapLayer,
                singleHitCapStatusBehavior(15),
                1,
                6102,
                2));
        return BattleDamageSystem().resolveTransaction(input);
    };

    const auto invincible = resolve(10, 1, false);
    CHECK(invincible.blockedByInvincible);
    CHECK(invincible.defender.dualWieldBlocksRemaining == 1);
    CHECK(invincible.defenderStatus.effects.statuses.size() == 2);

    const auto dualWield = resolve(0, 1, false);
    CHECK(dualWield.blockedByDualWield);
    CHECK(dualWield.defender.dualWieldBlocksRemaining == 0);
    CHECK(dualWield.defenderStatus.effects.statuses.size() == 2);

    const auto executed = resolve(10, 1, true);
    CHECK(executed.executed);
    CHECK(executed.killed);
    CHECK(executed.defenderStatus.effects.statuses.size() == 2);
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
    input.request.bleedProducer = bleedProducer(1, 101);
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
    input.defenderStatus.effects.statuses.push_back(boundStatusBehaviorContribution(
        KysChess::BattleStatusKind::WitheredBone,
        witheredBoneStatusBehavior(25, 75),
        1,
        9001));

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

    input.defenderStatus.effects.statuses.push_back(boundStatusBehaviorContribution(
        KysChess::BattleStatusKind::BattleSpirit,
        battleSpiritStatusBehavior(0, 10),
        2,
        9002));
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
    input.defenderStatus.effects.statuses.push_back(boundStatusBehaviorContribution(
        KysChess::BattleStatusKind::BattleSpirit,
        battleSpiritStatusBehavior(0, 70),
        2,
        9003));
    input.absorptionLayers = { { 1, 50 } };

    const auto reduced = BattleDamageSystem().resolveTransaction(input);

    CHECK(reduced.combinedDamageReductionBasisPoints == 8000);
    REQUIRE(reduced.absorptionReceipts.size() == 1);
    CHECK(reduced.absorptionReceipts.front().absorbedDamage == 10);
    CHECK(reduced.shieldAbsorbed == 15);
    CHECK(reduced.finalHpDamage == 5);
}

TEST_CASE("Persistent status damage modifiers preserve the established accumulator and defense pipeline",
          "[battle][damage][status][persistent][parity]")
{
    struct Scenario
    {
        int baseDamage{};
        bool usingSkill{};
        bool ignoreDefense{};
        int shield{};
        int maxHitPct{};
    };
    const std::array scenarios{
        Scenario{ 67, true, false, 0, 0 },
        Scenario{ 203, true, true, 17, 20 },
        Scenario{ 67, false, false, 9, 0 },
    };

    for (const auto& scenario : scenarios)
    {
        CAPTURE(
            scenario.baseDamage,
            scenario.usingSkill,
            scenario.ignoreDefense,
            scenario.shield,
            scenario.maxHitPct);
        BattleDamageTransactionInput statusInput;
        statusInput.request.attackerUnitId = 1;
        statusInput.request.defenderUnitId = 2;
        statusInput.request.baseDamage = scenario.baseDamage;
        statusInput.request.usingSkill = scenario.usingSkill;
        statusInput.request.ignoreDefense = scenario.ignoreDefense;
        statusInput.attacker = unit();
        statusInput.attacker.id = 1;
        statusInput.attacker.vitals = { 1000, 1000, 0, 0 };
        statusInput.defender = unit();
        statusInput.defender.id = 2;
        statusInput.defender.vitals = { 1000, 1000, 0, 0 };
        statusInput.defender.shield = scenario.shield;
        statusInput.attackerModifiers.flatDamageIncrease = 7;
        statusInput.defenderModifiers.flatDamageReduction = 3;
        statusInput.defenderModifiers.damageReductionPct = 11;
        statusInput.defenderModifiers.maxHitPctMaxHp = scenario.maxHitPct;
        statusInput.attackerStatus = statusUnit(1);
        statusInput.defenderStatus = statusUnit(2);
        statusInput.attackerStatus.effects.statuses.push_back(
            boundStatusBehaviorContribution(
                BattleStatusKind::BattleSpirit,
                battleSpiritStatusBehavior(5, 0),
                1,
                9101,
                3,
                1));
        statusInput.defenderStatus.effects.statuses.push_back(
            boundStatusBehaviorContribution(
                BattleStatusKind::BattleSpirit,
                battleSpiritStatusBehavior(0, 7),
                2,
                9102,
                3,
                1));
        statusInput.defenderStatus.effects.statuses.push_back(
            boundStatusBehaviorContribution(
                BattleStatusKind::WitheredBone,
                witheredBoneStatusBehavior(25, 75),
                1,
                9103,
                1,
                2));

        auto explicitInput = statusInput;
        explicitInput.attackerStatus.effects.statuses.clear();
        explicitInput.defenderStatus.effects.statuses.clear();
        explicitInput.attackerModifiers.skillDamagePct += 15;
        explicitInput.defenderModifiers.damageReductionPct += 21;
        explicitInput.defenderModifiers.damageTakenIncreasePct += 25;

        const auto fromStatuses = BattleDamageSystem().resolveTransaction(statusInput);
        const auto fromEstablishedAccumulators = BattleDamageSystem().resolveTransaction(
            explicitInput);
        CHECK(fromStatuses.resolvedDamageBeforeDefense
            == fromEstablishedAccumulators.resolvedDamageBeforeDefense);
        CHECK(fromStatuses.finalHpDamage == fromEstablishedAccumulators.finalHpDamage);
        CHECK(fromStatuses.shieldAbsorbed == fromEstablishedAccumulators.shieldAbsorbed);
        CHECK(fromStatuses.combinedDamageReductionBasisPoints
            == fromEstablishedAccumulators.combinedDamageReductionBasisPoints);
        CHECK(fromStatuses.singleHitCapped
            == fromEstablishedAccumulators.singleHitCapped);
        CHECK(fromStatuses.defender.vitals.hp
            == fromEstablishedAccumulators.defender.vitals.hp);
        CHECK(fromStatuses.defender.shield
            == fromEstablishedAccumulators.defender.shield);
    }
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
