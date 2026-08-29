#include "battle/BattleProjectileReflectionPolicy.h"
#include "BattleCoreTestHelpers.h"

using namespace KysChess::Battle;
using namespace KysChess::Battle::Test;
using namespace KysChess;
using namespace BattlePresentationTest;

TEST_CASE("BattleFrameRunner_ProjectileReflectionCreatesOwnedReturnAfterNormalDamage",
          "[battle][core][runtime][projectile_reflection]")
{
    auto frame = hitDamageFrameState(40, 100);
    auto& state = frame.state;
    addTypedAttributeModifier(
        state,
        1,
        BattleAttribute::ProjectileReflectChance,
        AttributeOperation::FlatAdd,
        100);
    addTypedAttributeModifier(
        state,
        0,
        BattleAttribute::ProjectileReflectChance,
        AttributeOperation::FlatAdd,
        100);
    const auto incomingAttackId = state.attacks.attacks.front().provenance.attackId;
    const auto incomingCast = state.attacks.attacks.front().provenance.cast;
    const int reflectorHpBefore = state.units.requireCore(1).vitals.hp;
    const int originalAttackerHpBefore = state.units.requireCore(0).vitals.hp;

    const auto incomingFrame = runBattleFrame(state);

    CHECK(state.units.requireCore(1).vitals.hp < reflectorHpBefore);
    CHECK(damageLogAmountsFor(incomingFrame, 1).size() == 1);
    CHECK_FALSE(state.result.ended);
    const auto& incoming = requireById(state.attacks.attacks, 10);
    REQUIRE(incoming.scheduledFinishReason);
    CHECK(*incoming.scheduledFinishReason == AttackFinishReason::ReflectedAtHit);
    REQUIRE(state.nextFrame.queuedAttacks().size() == 1);
    const auto& queuedReturn = state.nextFrame.queuedAttacks().front();
    CHECK(queuedReturn.initial.attackSourceUnitId == 1);
    CHECK(queuedReturn.initial.preferredTargetUnitId == 0);
    CHECK(queuedReturn.initial.position.x == Catch::Approx(105.0f));
    CHECK(queuedReturn.initial.position.y == Catch::Approx(100.0f));
    CHECK(queuedReturn.initial.velocity.x < 0.0f);
    CHECK(queuedReturn.initial.totalFrame == 30);
    CHECK(queuedReturn.initial.reflectionLineage
        == BattleAttackReflectionLineageKind::ReflectedReturn);
    REQUIRE(queuedReturn.initial.potencySnapshot);
    CHECK(*queuedReturn.initial.potencySnapshot == BattleAttackPotencySnapshot{ 30, 480 });
    CHECK(queuedReturn.initialFrame == 0);
    CHECK(queuedReturn.provenance.cast.rootCastId == incomingCast.rootCastId);
    REQUIRE(queuedReturn.provenance.cast.parentCastId);
    CHECK(*queuedReturn.provenance.cast.parentCastId == incomingCast.castId);
    CHECK(queuedReturn.provenance.cast.sourceUnitId == 1);
    CHECK(queuedReturn.provenance.cast.magicId == -1);
    CHECK(queuedReturn.provenance.cast.origin == CastOriginKind::Reflection);
    CHECK(queuedReturn.provenance.cast.propagation
        == CastPropagationPolicy::SourceHitRulesOnly);
    CHECK(queuedReturn.provenance.origin == BattleAttackOriginKind::Reflection);
    REQUIRE(queuedReturn.provenance.parentAttackId);
    CHECK(*queuedReturn.provenance.parentAttackId == incomingAttackId);
    CHECK_FALSE(queuedReturn.provenance.rootAttack);
    CHECK_FALSE(queuedReturn.provenance.mainProjectile);
    const auto reflectionCastId = queuedReturn.provenance.cast.castId;

    const auto returnFrame = runBattleFrame(state);

    CHECK(state.units.requireCore(0).vitals.hp < originalAttackerHpBefore);
    const auto reflectedDamageLogs = damageLogsFor(returnFrame, 0);
    REQUIRE(reflectedDamageLogs.size() == 1);
    CHECK(BattleLogTest::textOf(reflectedDamageLogs.front()) == "彈道反射");
    CHECK(state.nextFrame.queuedAttacks().empty());
    const auto reflected = std::ranges::find_if(
        state.attacks.attacks,
        [](const BattleAttackInstance& attack)
        {
            return attack.state.reflectionLineage
                == BattleAttackReflectionLineageKind::ReflectedReturn;
        });
    REQUIRE(reflected != state.attacks.attacks.end());
    CHECK(reflected->id != 10);
    CHECK(reflected->frame == 15);
    CHECK(reflected->state.attackSourceUnitId == 1);
    CHECK(reflected->state.preferredTargetUnitId == 0);
    CHECK(reflected->hitUnitIds == std::vector<int>{ 0 });
    REQUIRE(reflected->provenance.parentAttackId);
    CHECK(*reflected->provenance.parentAttackId == incomingAttackId);
    CHECK(reflected->provenance.origin == BattleAttackOriginKind::Reflection);
    CHECK_FALSE(reflected->provenance.mainProjectile);

    for (int frameIndex = 0; frameIndex < 15; ++frameIndex)
    {
        runBattleFrame(state);
    }
    CHECK_FALSE(state.castLifecycle.containsCast(reflectionCastId));
    CHECK_FALSE(state.castLifecycle.containsCast(incomingCast.castId));
    CHECK_FALSE(state.effectIntegration.casts.contains(incomingCast.castId));
}

TEST_CASE("BattleFrameRunner_ProjectileReflectionCommitsBeforeDefenseBlocksHpDamage",
          "[battle][core][runtime][projectile_reflection]")
{
    auto frame = hitDamageFrameState(70, 100);
    auto& state = frame.state;
    state.units.require(1).damage.dualWieldBlocksRemaining = 1;
    addTypedAttributeModifier(
        state,
        1,
        BattleAttribute::ProjectileReflectChance,
        AttributeOperation::FlatAdd,
        100);

    runBattleFrame(state);

    CHECK(state.units.requireCore(1).vitals.hp == 100);
    CHECK(state.units.require(1).damage.dualWieldBlocksRemaining == 0);
    REQUIRE(state.nextFrame.queuedAttacks().size() == 1);
    CHECK(state.nextFrame.queuedAttacks().front().initial.reflectionLineage
        == BattleAttackReflectionLineageKind::ReflectedReturn);
}

TEST_CASE("BattleFrameRunner_ReflectedReturnUsesOriginalAttackerDefense",
          "[battle][core][runtime][projectile_reflection]")
{
    auto frame = hitDamageFrameState(70, 100);
    auto& state = frame.state;
    addTypedAttributeModifier(
        state,
        1,
        BattleAttribute::ProjectileReflectChance,
        AttributeOperation::FlatAdd,
        100);
    runBattleFrame(state);
    REQUIRE(state.nextFrame.queuedAttacks().size() == 1);

    const int attackerHpBeforeReturn = state.units.requireCore(0).vitals.hp;
    state.units.require(0).damage.dualWieldBlocksRemaining = 1;
    const auto returnFrame = runBattleFrame(state);

    CHECK(state.units.requireCore(0).vitals.hp == attackerHpBeforeReturn);
    CHECK(state.units.require(0).damage.dualWieldBlocksRemaining == 0);
    CHECK(damageLogAmountsFor(returnFrame, 0).empty());
}

TEST_CASE("BattleFrameRunner_ReflectedReturnUsesReflectorCriticalStats",
          "[battle][core][runtime][projectile_reflection][ownership]")
{
    const auto reflectedDamage = [](int originalAttackerCritical, int reflectorCritical)
    {
        auto frame = hitDamageFrameState(20, 100);
        auto& state = frame.state;
        addTypedAttributeModifier(
            state,
            1,
            BattleAttribute::ProjectileReflectChance,
            AttributeOperation::FlatAdd,
            100);
        addTypedAttributeModifier(
            state,
            0,
            BattleAttribute::CriticalChance,
            AttributeOperation::FlatAdd,
            originalAttackerCritical);
        addTypedAttributeModifier(
            state,
            0,
            BattleAttribute::CriticalDamage,
            AttributeOperation::FlatAdd,
            150);
        addTypedAttributeModifier(
            state,
            1,
            BattleAttribute::CriticalChance,
            AttributeOperation::FlatAdd,
            reflectorCritical);
        addTypedAttributeModifier(
            state,
            1,
            BattleAttribute::CriticalDamage,
            AttributeOperation::FlatAdd,
            150);

        runBattleFrame(state);
        const auto result = runBattleFrame(state);
        const auto amounts = damageLogAmountsFor(result, 0);
        REQUIRE(amounts.size() == 1);
        return amounts.front();
    };

    const int incomingAttackerCriticalOnly = reflectedDamage(100, 0);
    const int reflectorCriticalOnly = reflectedDamage(0, 100);

    CHECK(reflectorCriticalOnly > incomingAttackerCriticalOnly);
}

TEST_CASE("BattleFrameRunner_ReflectedReturnReevaluatesIgnoreDefenseFromItsReflector",
          "[battle][core][runtime][projectile_reflection][ownership]")
{
    const auto damageByRuleOwner = [](int ignoreDefenseOwnerUnitId)
    {
        auto frame = hitDamageFrameState(40, 100);
        auto& state = frame.state;
        state.units.requireCore(0).stats.defence = 1000;
        state.units.requireCore(1).stats.defence = 1000;
        addTypedAttributeModifier(
            state,
            1,
            BattleAttribute::ProjectileReflectChance,
            AttributeOperation::FlatAdd,
            100);
        addTestIgnoreDefenseRule(state, ignoreDefenseOwnerUnitId, 100);

        const auto incomingFrame = runBattleFrame(state);
        const auto incomingDamage = damageLogAmountsFor(incomingFrame, 1);
        REQUIRE(incomingDamage.size() == 1);
        const auto returnFrame = runBattleFrame(state);
        const auto returnDamage = damageLogsFor(returnFrame, 0);
        REQUIRE(returnDamage.size() == 1);
        CHECK(returnDamage.front().sourceUnitId == 1);
        CHECK(returnDamage.front().targetUnitId == 0);
        return std::array{ incomingDamage.front(), returnDamage.front().amount };
    };

    const auto originalAttackerOwnsPenetration = damageByRuleOwner(0);
    const auto reflectorOwnsPenetration = damageByRuleOwner(1);

    CHECK(originalAttackerOwnsPenetration[0] > reflectorOwnsPenetration[0]);
    CHECK(reflectorOwnsPenetration[1] > originalAttackerOwnsPenetration[1]);
}

TEST_CASE("BattleFrameRunner_ReflectedReturnUsesTheLiveActPropertyMatchup",
          "[battle][core][runtime][projectile_reflection][ownership]")
{
    const auto reflectedDamage = [](
        int copiedIncomingProperty,
        int reflectorLiveProperty,
        int originalAttackerLiveProperty)
    {
        auto frame = hitDamageFrameState(40, 100);
        auto& state = frame.state;
        auto& incoming = state.attacks.attacks.front();
        incoming.state.skillMagicType = 2;
        incoming.state.skillAttackerActProperty = copiedIncomingProperty;
        state.units.requireCore(1).actPropertiesByMagicType[2] = reflectorLiveProperty;
        state.units.requireCore(0).actPropertiesByMagicType[2] =
            originalAttackerLiveProperty;
        addTypedAttributeModifier(
            state,
            1,
            BattleAttribute::ProjectileReflectChance,
            AttributeOperation::FlatAdd,
            100);

        runBattleFrame(state);
        const auto returnFrame = runBattleFrame(state);
        const auto damage = damageLogsFor(returnFrame, 0);
        REQUIRE(damage.size() == 1);
        CHECK(damage.front().sourceUnitId == 1);
        CHECK(damage.front().targetUnitId == 0);
        return damage.front().amount;
    };

    const int copiedStrongButReflectorWeak = reflectedDamage(60, -60, 0);
    const int copiedWeakButReflectorStrong = reflectedDamage(-60, 60, 0);
    CHECK(copiedWeakButReflectorStrong > copiedStrongButReflectorWeak);

    const int copiedIncomingWeak = reflectedDamage(-60, 15, -10);
    const int copiedIncomingStrong = reflectedDamage(60, 15, -10);
    CHECK(copiedIncomingWeak == copiedIncomingStrong);

    const int defenderLiveStrong = reflectedDamage(0, 0, 60);
    const int defenderLiveWeak = reflectedDamage(0, 0, -60);
    CHECK(defenderLiveWeak > defenderLiveStrong);
}

TEST_CASE("BattleFrameRunner_ReflectedReturnUsesReflectorCooldownExtensionStats",
          "[battle][core][runtime][projectile_reflection][ownership]")
{
    const auto cooldownsAfterReturn = [](int extensionOwnerUnitId)
    {
        auto frame = hitDamageFrameState(20, 100);
        auto& state = frame.state;
        for (const int unitId : { 0, 1 })
        {
            auto& unit = state.units.requireCore(unitId);
            unit.haveAction = true;
            unit.operationType = BattleOperationType::Melee;
            unit.animation.actType = 1;
            unit.animation.cooldown = 20;
            unit.animation.cooldownMax = 20;
        }
        addTypedAttributeModifier(
            state,
            1,
            BattleAttribute::ProjectileReflectChance,
            AttributeOperation::FlatAdd,
            100);
        addTypedAttributeModifier(
            state,
            extensionOwnerUnitId,
            BattleAttribute::OutgoingCooldownExtensionChance,
            AttributeOperation::FlatAdd,
            100);
        addTypedAttributeModifier(
            state,
            extensionOwnerUnitId,
            BattleAttribute::OutgoingCooldownExtensionPercent,
            AttributeOperation::FlatAdd,
            50);

        runBattleFrame(state);
        const auto returnFrame = runBattleFrame(state);
        const auto damage = damageLogsFor(returnFrame, 0);
        REQUIRE(damage.size() == 1);
        CHECK(damage.front().sourceUnitId == 1);
        return std::array{
            state.units.requireCore(0).animation.cooldown,
            state.units.requireCore(1).animation.cooldown,
        };
    };

    const auto originalAttackerOwnsExtension = cooldownsAfterReturn(0);
    const auto reflectorOwnsExtension = cooldownsAfterReturn(1);

    CHECK(reflectorOwnsExtension[0] > originalAttackerOwnsExtension[0]);
    CHECK(originalAttackerOwnsExtension[1] > reflectorOwnsExtension[1]);
}

TEST_CASE("BattleFrameRunner_ReflectedReturnUsesReflectorExecuteRule",
          "[battle][core][runtime][projectile_reflection][ownership]")
{
    const auto originalAttackerSurvives = [](int executeOwnerUnitId)
    {
        auto frame = hitDamageFrameState(10, 100);
        auto& state = frame.state;
        state.units.requireCore(0).vitals.hp = 40;
        addTypedAttributeModifier(
            state,
            1,
            BattleAttribute::ProjectileReflectChance,
            AttributeOperation::FlatAdd,
            100);
        addTestExecuteRule(state, executeOwnerUnitId, 50);

        runBattleFrame(state);
        const auto returnFrame = runBattleFrame(state);
        const auto damage = damageLogsFor(returnFrame, 0);
        REQUIRE_FALSE(damage.empty());
        CHECK(damage.front().sourceUnitId == 1);
        return state.units.requireCore(0).alive;
    };

    CHECK(originalAttackerSurvives(0));
    CHECK_FALSE(originalAttackerSurvives(1));
}

TEST_CASE("BattleFrameRunner_ReflectionChildRejectsAttackSpawnedObserversButKeepsHitObservers",
          "[battle][core][runtime][projectile_reflection][propagation]")
{
    auto frame = hitDamageFrameState(20, 100);
    auto& state = frame.state;
    addTypedAttributeModifier(
        state,
        1,
        BattleAttribute::ProjectileReflectChance,
        AttributeOperation::FlatAdd,
        100);
    const EffectSourceBinding binding{
        .kind = EffectSourceKind::Combo,
        .sourceId = 9'170,
        .ownerUnitId = 1,
        .sourceTeam = 1,
    };

    ChangeResourceAction forbiddenShield;
    forbiddenShield.resource = BattleResource::Shield;
    forbiddenShield.kind = ResourceChangeKind::Grant;
    forbiddenShield.amount.flat = 11;
    ModifyAttackAction forbiddenExpansion;
    forbiddenExpansion.pattern.kind = AttackPatternKind::Fan;
    forbiddenExpansion.pattern.projectileCount = 3;
    forbiddenExpansion.pattern.spreadDegrees = 30;
    EffectRule attackSpawnedObserver;
    attackSpawnedObserver.id = { 1 };
    attackSpawnedObserver.event = EffectEvent::AttackSpawned;
    attackSpawnedObserver.observation = EffectObservationScope::OwnerTeamEventSource;
    attackSpawnedObserver.selector.kind = EffectSelectorKind::Self;
    attackSpawnedObserver.actions = {
        { EffectActionValue{ forbiddenShield } },
        { EffectActionValue{ forbiddenExpansion } },
    };
    state.effectRules.append(binding, attackSpawnedObserver);

    ChangeResourceAction allowedShield;
    allowedShield.resource = BattleResource::Shield;
    allowedShield.kind = ResourceChangeKind::Grant;
    allowedShield.amount.flat = 7;
    EffectRule hitObserver;
    hitObserver.id = { 2 };
    hitObserver.event = EffectEvent::HitBeforeDamage;
    hitObserver.observation = EffectObservationScope::OwnerTeamEventSource;
    hitObserver.selector.kind = EffectSelectorKind::Self;
    hitObserver.actions = {
        { EffectActionValue{ allowedShield } },
    };
    state.effectRules.append(binding, hitObserver);

    runBattleFrame(state);
    runBattleFrame(state);

    CHECK(state.effectRules.activationCount(binding, attackSpawnedObserver.id) == 0);
    CHECK(state.effectRules.activationCount(binding, hitObserver.id) == 1);
    CHECK(state.units.requireCore(1).shield == 7);
    CHECK(std::ranges::count_if(
        state.attacks.attacks,
        [](const BattleAttackInstance& attack)
        {
            return attack.state.reflectionLineage
                == BattleAttackReflectionLineageKind::ReflectedReturn;
        }) == 1);
}

TEST_CASE("BattleFrameRunner_ReflectedReturnAppliesInheritedProjectileCancelWeaken",
          "[battle][core][runtime][projectile_reflection][projectile_cancel]")
{
    const auto reflectedDamage = [](int projectileCancelWeaken)
    {
        auto frame = hitDamageFrameState(40, 100);
        auto& state = frame.state;
        state.attacks.attacks.front().state.projectileCancelWeaken =
            projectileCancelWeaken;
        addTypedAttributeModifier(
            state,
            1,
            BattleAttribute::ProjectileReflectChance,
            AttributeOperation::FlatAdd,
            100);

        runBattleFrame(state);
        REQUIRE(state.nextFrame.queuedAttacks().size() == 1);
        const auto& queuedReturn = state.nextFrame.queuedAttacks().front();
        CHECK(queuedReturn.initial.projectileCancelWeaken == projectileCancelWeaken);
        CHECK(queuedReturn.initialFrame == 0);

        const auto returnFrame = runBattleFrame(state);
        const auto damage = damageLogsFor(returnFrame, 0);
        REQUIRE(damage.size() == 1);
        CHECK(damage.front().sourceUnitId == 1);
        return damage.front().amount;
    };

    CHECK(reflectedDamage(15) < reflectedDamage(0));
}

TEST_CASE("BattleFrameRunner_ReflectedReturnUsesItsOwnFlightFramesForDecay",
          "[battle][core][runtime][projectile_reflection][flight]")
{
    const auto returnedHit = [](Pointf originalAttackerPosition)
    {
        auto frame = hitDamageFrameState(40, 100);
        auto& state = frame.state;
        state.attacks.hitRadius = 72.0;
        state.units.setPosition(0, originalAttackerPosition, state.gridTransform);
        auto& originalAttacker = state.units.require(0);
        originalAttacker.movement.physics.position = originalAttackerPosition;
        originalAttacker.status.effects.setFrames(BattleStatusKind::Stun, 60);
        addTypedAttributeModifier(
            state,
            1,
            BattleAttribute::ProjectileReflectChance,
            AttributeOperation::FlatAdd,
            100);

        runBattleFrame(state);
        REQUIRE(state.nextFrame.queuedAttacks().size() == 1);
        CHECK(state.nextFrame.queuedAttacks().front().initialFrame == 0);
        CHECK(state.nextFrame.queuedAttacks().front().initial.totalFrame == 30);

        for (int elapsedFrames = 1; elapsedFrames <= 30; ++elapsedFrames)
        {
            const auto returnFrame = runBattleFrame(state);
            const auto damage = damageLogsFor(returnFrame, 0);
            if (!damage.empty())
            {
                REQUIRE(damage.size() == 1);
                CHECK(damage.front().sourceUnitId == 1);
                return std::array{ damage.front().amount, elapsedFrames };
            }
        }
        FAIL("回程彈道未在生命週期內命中原始攻擊者");
        return std::array<int, 2>{};
    };

    const auto nearReturn = returnedHit({ 180, 100, 0 });
    const auto farReturn = returnedHit({ 300, 100, 0 });

    CHECK(farReturn[1] > nearReturn[1]);
    CHECK(farReturn[0] < nearReturn[0]);
}

TEST_CASE("BattleFrameRunner_ReflectedReturnRequiresPositiveHpDamageForGusuMurongDebuff",
          "[battle][core][runtime][projectile_reflection][effect]")
{
    auto frame = hitDamageFrameState(40, 100);
    auto& state = frame.state;
    addTypedAttributeModifier(
        state,
        1,
        BattleAttribute::ProjectileReflectChance,
        AttributeOperation::FlatAdd,
        100);
    const auto murong = addShippedGusuMurongDamageDebuffRule(state, 1);
    runBattleFrame(state);
    REQUIRE(state.nextFrame.queuedAttacks().size() == 1);

    SECTION("閃避")
    {
        addTypedAttributeModifier(
            state,
            0,
            BattleAttribute::DodgeChance,
            AttributeOperation::FlatAdd,
            100);
    }

    SECTION("格擋")
    {
        addTypedAttributeModifier(
            state,
            0,
            BattleAttribute::BlockChance,
            AttributeOperation::FlatAdd,
            100);
    }

    SECTION("護盾完全吸收")
    {
        state.units.requireCore(0).shield = 1000;
    }

    const int hpBeforeReturn = state.units.requireCore(0).vitals.hp;
    const auto returnFrame = runBattleFrame(state);

    CHECK(state.units.requireCore(0).vitals.hp == hpBeforeReturn);
    CHECK(damageLogAmountsFor(returnFrame, 0).empty());
    CHECK(state.effectRules.activationCount(murong.binding, murong.ruleId) == 0);
    CHECK_FALSE(hasGusuMurongDamageDebuff(state, 1, 0));
}

TEST_CASE("BattleFrameRunner_ReflectedReturnPreservesBounceBudgetWithoutRereflection",
          "[battle][core][runtime][projectile_reflection][bounce]")
{
    auto frame = hitDamageFrameState(25, 100);
    auto& state = frame.state;
    auto teammate = runtimeUnitSnapshot(2, 0, 100, { 150, 100, 0 });
    appendRuntimeUnit(state, makeRuntimeUnitSpawn(std::move(teammate)));
    auto& incoming = state.attacks.attacks.front();
    incoming.state.bounceRemaining = 1;
    incoming.state.bounceRange = 120;
    incoming.state.bounceChancePct = 100;
    incoming.state.bounceRollPct = 0;
    addTypedAttributeModifier(
        state,
        1,
        BattleAttribute::ProjectileReflectChance,
        AttributeOperation::FlatAdd,
        100);
    addTypedAttributeModifier(
        state,
        0,
        BattleAttribute::ProjectileReflectChance,
        AttributeOperation::FlatAdd,
        100);
    addTypedAttributeModifier(
        state,
        2,
        BattleAttribute::ProjectileReflectChance,
        AttributeOperation::FlatAdd,
        100);
    const auto murong = addShippedGusuMurongDamageDebuffRule(state, 1);

    runBattleFrame(state);

    CHECK(state.attacks.attacks.size() == 1);
    CHECK(state.attacks.attacks.front().state.bounceRemaining == 1);
    REQUIRE(state.nextFrame.queuedAttacks().size() == 1);
    CHECK(state.nextFrame.queuedAttacks().front().initial.bounceRemaining == 1);
    CHECK(state.nextFrame.queuedAttacks().front().initial.reflectionLineage
        == BattleAttackReflectionLineageKind::ReflectedReturn);

    runBattleFrame(state);

    const auto reflectedBounce = std::ranges::find_if(
        state.attacks.attacks,
        [](const BattleAttackInstance& attack)
        {
            return attack.provenance.origin == BattleAttackOriginKind::Bounce
                && attack.state.reflectionLineage
                    == BattleAttackReflectionLineageKind::ReflectedReturn;
        });
    REQUIRE(reflectedBounce != state.attacks.attacks.end());
    CHECK(reflectedBounce->state.attackSourceUnitId == 1);
    CHECK(reflectedBounce->state.preferredTargetUnitId == 2);
    CHECK(reflectedBounce->state.bounceRemaining == 0);
    CHECK(reflectedBounce->hitUnitIds == std::vector<int>{ 0 });
    CHECK(state.nextFrame.queuedAttacks().empty());

    const auto bounceFrame = runBattleFrame(state);

    const auto bounceDamageLogs = damageLogsFor(bounceFrame, 2);
    REQUIRE(bounceDamageLogs.size() == 1);
    CHECK(BattleLogTest::textOf(bounceDamageLogs.front())
        == "彈道反射、連鎖彈");
    CHECK(state.nextFrame.queuedAttacks().empty());
    CHECK(reflectedBounce->state.reflectionLineage
        == BattleAttackReflectionLineageKind::ReflectedReturn);
    CHECK(state.effectRules.activationCount(murong.binding, murong.ruleId) == 2);
    CHECK(hasGusuMurongDamageDebuff(state, 1, 0));
    CHECK(hasGusuMurongDamageDebuff(state, 1, 2));
}

TEST_CASE("BattleFrameRunner_LethalContactPreservesDeadReflectorReturnAndHitRules",
          "[battle][core][runtime][projectile_reflection][ownership]")
{
    auto frame = hitDamageFrameState(40, 20);
    auto& state = frame.state;
    state.units.requireCore(0).stats.defence = 1000;
    addTypedAttributeModifier(
        state,
        1,
        BattleAttribute::ProjectileReflectChance,
        AttributeOperation::FlatAdd,
        100);

    EffectRule debuffRule;
    debuffRule.id = EffectRuleId{ 901 };
    debuffRule.event = EffectEvent::DamageResolved;
    debuffRule.selector.kind = EffectSelectorKind::TransactionTarget;
    debuffRule.conditions = {
        DamagePerspectiveCondition{ DamagePerspective::Dealt },
        AcceptedHitCondition{ .requirePositiveDamage = true },
    };
    ModifyDamageAction debuff;
    debuff.perspective = DamageModifierPerspective::Outgoing;
    debuff.stage = DamageModifierStage::BeforeDefense;
    debuff.channel = DamageChannel::All;
    debuff.amount.flat = -45;
    debuff.operation = DamageModifierOperation::PercentAdd;
    debuff.durationFrames = 70;
    debuff.stack = EffectStackPolicy::Independent;
    debuffRule.actions.push_back({ debuff });
    const EffectSourceBinding reflectorBinding{
        .kind = EffectSourceKind::Combo,
        .sourceId = 902,
        .ownerUnitId = 1,
        .sourceTeam = 1,
    };
    state.effectRules.append(reflectorBinding, debuffRule);

    EffectRule magicObserver;
    magicObserver.id = EffectRuleId{ 903 };
    magicObserver.event = EffectEvent::HitBeforeDamage;
    magicObserver.observation = EffectObservationScope::EventTarget;
    magicObserver.selector.kind = EffectSelectorKind::Self;
    ChangeResourceAction grantShield;
    grantShield.resource = BattleResource::Shield;
    grantShield.kind = ResourceChangeKind::Grant;
    grantShield.amount.flat = 9;
    magicObserver.actions.push_back({ grantShield });
    const EffectSourceBinding incomingMagicBinding{
        .kind = EffectSourceKind::Magic,
        .sourceId = 101,
        .ownerUnitId = 0,
        .sourceTeam = 0,
    };
    state.effectRules.append(incomingMagicBinding, magicObserver);

    const auto incomingFrame = runBattleFrame(state);

    CHECK_FALSE(state.units.requireCore(1).alive);
    CHECK(damageLogAmountsFor(incomingFrame, 1).size() == 1);
    CHECK_FALSE(state.result.ended);
    REQUIRE(state.nextFrame.queuedAttacks().size() == 1);
    CHECK(state.nextFrame.queuedAttacks().front().initial.attackSourceUnitId == 1);

    const int attackerHpBeforeReturn = state.units.requireCore(0).vitals.hp;
    const auto returnFrame = runBattleFrame(state);

    CHECK(state.units.requireCore(0).vitals.hp < attackerHpBeforeReturn);
    CHECK(damageLogAmountsFor(returnFrame, 0).size() == 1);
    CHECK(state.units.requireCore(0).shield == 0);
    CHECK(state.effectRules.activationCount(incomingMagicBinding, magicObserver.id) == 0);
    const auto appliedDebuff = std::ranges::find_if(
        state.effectCommands.damageModifiers,
        [](const BattleDamageModifierInstance& modifier)
        {
            return modifier.binding.ownerUnitId == 1
                && modifier.targetUnitId == 0
                && modifier.perspective == DamageModifierPerspective::Outgoing
                && modifier.operation == DamageModifierOperation::PercentAdd
                && modifier.amount == -45;
        });
    REQUIRE(appliedDebuff != state.effectCommands.damageModifiers.end());
    CHECK(appliedDebuff->binding.kind == reflectorBinding.kind);
    CHECK(appliedDebuff->binding.sourceId == reflectorBinding.sourceId);
    CHECK(appliedDebuff->binding.ownerUnitId == reflectorBinding.ownerUnitId);
    CHECK(appliedDebuff->binding.sourceTeam == reflectorBinding.sourceTeam);
    CHECK(appliedDebuff->expiresFrameExclusive
        == static_cast<std::int64_t>(appliedDebuff->appliedFrame) + 70);
}
