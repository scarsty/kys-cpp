#include "battle/BattleCore.h"
#include "battle/BattleCoreDetail.h"
#include "battle/BattleEffectCommandSystem.h"
#include "battle/BattleEffectEventBridge.h"
#include "BattleCoreTestHelpers.h"
#include "ChessBattleEffectParser.h"
#include "ChessBattleEffectTestHelpers.h"
#include "EffectCommandTestHelpers.h"

#include "BattleLogTestHelpers.h"
#include "BattleMovementTestHelpers.h"
#include "BattlePresentationTestHelpers.h"
#include "BattleRuntimeRecordTestHelpers.h"
#include "BattleRuntimeStateTestHelpers.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

using namespace KysChess::Battle;
using namespace KysChess::Battle::Test;
using namespace KysChess;
using namespace BattlePresentationTest;

namespace
{

BattleStatusEffectOrigin addTrueQiStatus(
    BattleRuntimeState& state,
    int sourceUnitId,
    int stacks,
    int pureDamagePerHit)
{
    const BattleStatusEffectOrigin origin{
        .binding = {
            .kind = EffectSourceKind::Magic,
            .sourceId = 106,
            .ownerUnitId = sourceUnitId,
            .sourceTeam = state.units.requireCore(sourceUnitId).team,
        },
        .ruleId = EffectRuleId{ static_cast<std::uint64_t>(106) << 32 },
        .ruleOrder = 4,
    };
    DealDamageAction damage;
    damage.amount.flat = pureDamagePerHit;
    damage.amount.statusScale = StatusNumberScale::PerContributionLayer;
    damage.kind = BattleDamageKind::Pure;
    EffectRule hitRule;
    hitRule.id = EffectRuleId{ 1 };
    hitRule.event = EffectEvent::HitBeforeDamage;
    hitRule.observation = EffectObservationScope::StatusHolderEventSource;
    hitRule.selector.kind = EffectSelectorKind::HitTarget;
    hitRule.actions = { EffectAction{ damage } };
    auto behavior = std::make_shared<StatusBehaviorDefinition>();
    behavior->rules = { std::move(hitRule) };
    auto& effects = state.units.require(sourceUnitId).status.effects;
    effects.statuses.push_back({
        .kind = BattleStatusKind::TrueQi,
        .producer = StatusProducerKey{
            .binding = origin.binding,
            .ruleId = origin.ruleId,
        },
        .behavior = behavior,
        .behaviorRuntime = initialStatusBehaviorRuntime(behavior),
        .sourceUnitId = sourceUnitId,
        .stacks = stacks,
        .origin = origin,
        .appliedSequence = effects.nextStatusSequence++,
    });
    return origin;
}

ApplyStatusAction swordGuardApplication()
{
    const auto definition = KysChess::Test::contractMagicDefinition(47);
    for (const auto& rule : definition.rules)
    {
        for (const auto& action : rule.actions)
        {
            const auto* application = std::get_if<ApplyStatusAction>(&action.value);
            if (application && application->status == BattleStatusKind::SwordGuard)
            {
                return *application;
            }
        }
    }
    FAIL("獨孤九劍契約缺少御劍護身");
    return {};
}

void applySwordGuard(BattleRuntimeState& state, int targetUnitId)
{
    EffectCommandMetadata metadata;
    metadata.binding = {
        .kind = EffectSourceKind::Magic,
        .sourceId = 47,
        .ownerUnitId = 0,
        .sourceTeam = 0,
    };
    metadata.ruleId = EffectRuleId{47};
    metadata.targetUnitId = targetUnitId;
    const EffectCommand command{
        metadata,
        KysChess::Battle::Test::statusApplication(swordGuardApplication()),
    };
    const auto reduced = BattleEffectCommandSystem().reduce(
        state,
        KysChess::Battle::Test::commandFixture(
            command,
            {.frame = state.movement.frame}));
    REQUIRE(reduced.entries.size() == 1);
}

}

TEST_CASE("Sword guard reduces and consumes only on the next direct attack",
          "[battle][core][sword-guard][integration]")
{
    auto baseline = hitDamageFrameState(20, 100);
    runBattleFrame(baseline.state);
    const int ordinaryDamage = 100 - baseline.state.units.requireCore(1).vitals.hp;
    REQUIRE(ordinaryDamage > 0);

    SECTION("直接攻擊減傷四成並消耗")
    {
        auto guarded = hitDamageFrameState(20, 100);
        applySwordGuard(guarded.state, 1);
        REQUIRE(guarded.state.units.require(1).statusEffects().has(BattleStatusKind::SwordGuard));

        runBattleFrame(guarded.state);

        const int guardedDamage = 100 - guarded.state.units.requireCore(1).vitals.hp;
        CHECK(guardedDamage == ordinaryDamage * 60 / 100);
        CHECK_FALSE(guarded.state.units.require(1).statusEffects().has(BattleStatusKind::SwordGuard));
    }

    SECTION("非攻擊傷害不減傷也不消耗")
    {
        auto guarded = hitDamageFrameState(20, 100);
        guarded.state.attacks = attackWorld();
        applySwordGuard(guarded.state, 1);
        guarded.state.nextFrame.queueDamage({.request = {
            .attackerUnitId = 0,
            .defenderUnitId = 1,
            .baseDamage = 10,
            .preResolvedDamage = true,
        }});

        runBattleFrame(guarded.state);

        CHECK(guarded.state.units.requireCore(1).vitals.hp == 90);
        CHECK(guarded.state.units.require(1).statusEffects().has(BattleStatusKind::SwordGuard));
    }

    SECTION("重複施加只刷新一層且一百幀後到期")
    {
        auto guarded = hitDamageFrameState(20, 100);
        guarded.state.attacks = attackWorld();
        applySwordGuard(guarded.state, 1);
        for (int frame = 0; frame < 25; ++frame) runBattleFrame(guarded.state);
        REQUIRE(guarded.state.units.require(1).statusEffects().remainingFrames(
            BattleStatusKind::SwordGuard) == 75);

        applySwordGuard(guarded.state, 1);
        const auto& statuses = guarded.state.units.require(1).statusEffects().statuses;
        CHECK(std::ranges::count(
            statuses,
            BattleStatusKind::SwordGuard,
            &BattleStatusContribution::kind) == 1);
        CHECK(guarded.state.units.require(1).statusEffects().remainingFrames(
            BattleStatusKind::SwordGuard) == 100);

        for (int frame = 0; frame < 100; ++frame) runBattleFrame(guarded.state);
        CHECK_FALSE(guarded.state.units.require(1).statusEffects().has(BattleStatusKind::SwordGuard));
    }
}

TEST_CASE("Jiuyang producer queues exact status origin and preserves ultimate hit lineage",
          "[battle][core][true-qi][integration][origin]")
{
    const auto jiuyang = KysChess::Test::contractMagicDefinition(106);
    const auto producer = std::ranges::find_if(jiuyang.rules, [](const EffectRule& rule)
    {
        return rule.event == EffectEvent::AttackCommitted
            && std::ranges::any_of(rule.actions, [](const EffectAction& action)
            {
                const auto* application = std::get_if<ApplyStatusAction>(&action.value);
                return application && application->status == BattleStatusKind::TrueQi;
            });
    });
    REQUIRE(producer != jiuyang.rules.end());

    auto frame = hitDamageFrameState(20, 100, true);
    auto& state = frame.state;
    const EffectSourceBinding binding{
        .kind = EffectSourceKind::Magic,
        .sourceId = jiuyang.magicId,
        .ownerUnitId = 0,
        .sourceTeam = 0,
    };
    const auto boundIndex = state.effectRules.append(binding, *producer);
    REQUIRE(boundIndex < state.effectRules.rules().size());
    const auto producerOrder = state.effectRules.rules()[boundIndex].order;

    const auto producerCast = state.castLifecycle.beginRootCast({
        .sourceUnitId = 0,
        .magicId = jiuyang.magicId,
        .ultimate = true,
        .origin = CastOriginKind::Ultimate,
    });
    state.effectIntegration.casts.emplace(
        producerCast.provenance.castId,
        BattleEffectCastRuntimeContext{ .originalTargetUnitId = 1 });
    state.castLifecycle.completeWork(producerCast.commitBarrier);
    const auto dispatched = BattleEffectEventBridge().dispatch(
        state,
        {
            .frame = state.movement.frame,
            .eventOrdinal = 1,
            .ownerUnitId = 0,
        },
        EffectEvent::AttackCommitted,
        CastCommitEventData{
            .provenance = producerCast.provenance,
            .targetUnitId = 1,
        });
    REQUIRE(dispatched.commands.size() == 1);
    const auto reduced = BattleEffectCommandSystem().reduce(state, dispatched.commands);
    REQUIRE(reduced.entries.size() == 1);

    const auto& statuses = state.units.require(0).status.effects.statuses;
    REQUIRE(std::ranges::count(
        statuses, BattleStatusKind::TrueQi, &BattleStatusContribution::kind) == 1);
    const auto* trueQi = state.units.require(0).status.effects.find(
        BattleStatusKind::TrueQi);
    REQUIRE(trueQi);
    CHECK(trueQi->stacks == 1);
    REQUIRE(trueQi->behavior);
    REQUIRE(trueQi->behavior->rules.size() == 1);
    const auto& trueQiDamage = std::get<DealDamageAction>(
        trueQi->behavior->rules.front().actions.front().value);
    CHECK(trueQiDamage.amount.flat == 9);
    CHECK(trueQiDamage.amount.statusScale == StatusNumberScale::PerContributionLayer);
    REQUIRE(trueQi->origin);
    CHECK(trueQi->origin->binding == binding);
    CHECK(trueQi->origin->ruleId == producer->id);
    CHECK(trueQi->origin->ruleOrder == producerOrder);

    ChangeResourceAction lineageShield;
    lineageShield.resource = BattleResource::Shield;
    lineageShield.kind = ResourceChangeKind::Grant;
    lineageShield.amount.flat = 7;
    EffectRule statusLineage;
    statusLineage.id = EffectRuleId{ 9901 };
    statusLineage.event = EffectEvent::DamageResolved;
    statusLineage.castMatch = EffectCastMatch::OwnerAnyCast;
    statusLineage.selector.kind = EffectSelectorKind::Self;
    statusLineage.conditions = {
        IsUltimateCondition{},
        DamageKindInCondition{ { "純粹" } },
    };
    statusLineage.actions = { EffectAction{ lineageShield } };
    state.effectRules.append({
        .kind = EffectSourceKind::Magic,
        .sourceId = 9901,
        .ownerUnitId = 0,
        .sourceTeam = 0,
    }, statusLineage);

    auto attackOnly = statusLineage;
    attackOnly.id = EffectRuleId{ 9902 };
    attackOnly.conditions.push_back(DamageOriginIsAttackCondition{});
    std::get<ChangeResourceAction>(attackOnly.actions.front().value).amount.flat = 100;
    state.effectRules.append({
        .kind = EffectSourceKind::Magic,
        .sourceId = 9902,
        .ownerUnitId = 0,
        .sourceTeam = 0,
    }, attackOnly);

    const auto hitProvenance = state.attacks.attacks.front().provenance;
    REQUIRE(hitProvenance.cast.ultimate);
    state.castLifecycle.recordHit(hitProvenance, 1);
    const auto targetPosition = makeEffectUnitSnapshot(
        state,
        state.units.require(1)).position;
    const auto hitDispatch = BattleEffectEventBridge().dispatch(
        state,
        {
            .frame = state.movement.frame,
            .eventOrdinal = 2,
            .ownerUnitId = 0,
        },
        EffectEvent::HitBeforeDamage,
        HitEventData{
            .provenance = hitProvenance,
            .targetUnitId = 1,
            .originalTargetUnitId = 1,
            .contactPosition = targetPosition,
            .acceptedHit = true,
            .damageKind = BattleDamageKind::Skill,
        });
    REQUIRE(hitDispatch.commands.size() == 1);
    const auto& trueQiCommand = hitDispatch.commands.front();
    REQUIRE(trueQiCommand.metadata.statusContribution);
    const auto contribution = *trueQiCommand.metadata.statusContribution;
    CHECK(contribution.holderUnitId == 0);
    CHECK(contribution.sourceUnitId == 0);
    CHECK(contribution.kind == BattleStatusKind::TrueQi);
    CHECK(contribution.quantity == 1);
    CHECK(contribution.appliedSequence == trueQi->appliedSequence);
    CHECK(contribution.producerRuleId == producer->id);
    CHECK(contribution.producerRuleOrder == producerOrder);
    CHECK(contribution.producerActionOrder == 0);
    CHECK(contribution.behaviorRuleOrder == 0);

    std::vector<std::byte> frameMemory(256 * 1024);
    auto effectFrame = BattleFrameContext::begin(
        state,
        {},
        frameMemory.data(),
        frameMemory.size());
    CoreDetail::reduceEffectCommand(
        state,
        effectFrame,
        effectFrame.currentFrameDamage(),
        trueQiCommand);
    REQUIRE(effectFrame.currentFrameDamage().size() == 1);
    const auto& pending = effectFrame.currentFrameDamage().front();
    CHECK(pending.request.baseDamage == 9);
    REQUIRE(std::holds_alternative<EffectStatusDamageOrigin>(pending.effectOrigin));
    const auto& origin = std::get<EffectStatusDamageOrigin>(pending.effectOrigin);
    CHECK(origin.binding == binding);
    CHECK(origin.contribution == contribution);
    CHECK(origin.behaviorActionOrder == 0);
    REQUIRE(origin.triggeringCast);
    const auto& originCast = *origin.triggeringCast;
    CHECK(originCast.rootCastId == hitProvenance.cast.rootCastId);
    CHECK(originCast.castId == hitProvenance.cast.castId);
    CHECK(originCast.parentCastId == hitProvenance.cast.parentCastId);
    CHECK(originCast.sourceUnitId == hitProvenance.cast.sourceUnitId);
    CHECK(originCast.magicId == hitProvenance.cast.magicId);
    CHECK(originCast.ultimate == hitProvenance.cast.ultimate);
    CHECK(originCast.origin == hitProvenance.cast.origin);
    CHECK(originCast.propagation == hitProvenance.cast.propagation);
    REQUIRE(origin.triggeringAttack);
    const auto& originAttack = *origin.triggeringAttack;
    CHECK(originAttack.cast.castId == hitProvenance.cast.castId);
    CHECK(originAttack.propagation == hitProvenance.propagation);
    CHECK(originAttack.origin == hitProvenance.origin);
    CHECK(originAttack.attackId == hitProvenance.attackId);
    CHECK(originAttack.parentAttackId == hitProvenance.parentAttackId);
    CHECK(originAttack.attackOrdinal == hitProvenance.attackOrdinal);
    CHECK(originAttack.rootAttack == hitProvenance.rootAttack);
    CHECK(originAttack.mainProjectile == hitProvenance.mainProjectile);
    CHECK(originAttack.sharedHitGroupId == hitProvenance.sharedHitGroupId);

    CoreDetail::applyDamageAndLifecycle(state, effectFrame);

    CHECK(state.units.requireCore(1).vitals.hp == 91);
    CHECK(state.units.requireCore(0).shield == 7);
}

TEST_CASE("True-Qi damage is queued before the accepted base hit",
          "[battle][core][true-qi][ordering]")
{
    auto frame = hitDamageFrameState(20, 100);
    addTrueQiStatus(frame.state, 0, 3, 9);
    const auto castId = frame.state.attacks.attacks.front().provenance.cast.castId;

    const auto result = runBattleFrame(frame.state);

    CHECK(damageLogAmountsFor(result, 1) == std::vector<int>{ 27, 35 });
    CHECK(frame.state.units.requireCore(1).vitals.hp == 38);
    const auto& aggregate = frame.state.castLifecycle.runtime(castId).aggregate;
    CHECK(aggregate.totalActualHpDamage == 62);
    CHECK(aggregate.highestActualHpDamage == 35);
    CHECK(aggregate.distinctHitUnitIds
        == std::set<int>{ 1 });
}

TEST_CASE("Defensive cooldown extension preserves the original cast hit targets",
          "[battle][core][cast-lifecycle][cooldown][regression]")
{
    auto frame = hitDamageFrameState(20, 100);
    auto& state = frame.state;
    auto& attacker = state.units.requireCore(0);
    attacker.haveAction = true;
    attacker.operationType = BattleOperationType::Melee;
    attacker.animation.actType = 1;
    attacker.animation.cooldown = 20;
    attacker.animation.cooldownMax = 20;
    addTypedAttributeModifier(state, 1,
        BattleAttribute::IncomingCooldownExtensionChance,
        AttributeOperation::FlatAdd, 100);
    addTypedAttributeModifier(state, 1,
        BattleAttribute::IncomingCooldownExtensionPercent,
        AttributeOperation::FlatAdd, 50);
    const auto castId = state.attacks.attacks.front().provenance.cast.castId;

    const auto result = runBattleFrame(state);

    CHECK(state.units.requireCore(0).animation.cooldown == 29);
    CHECK(state.units.requireCore(0).vitals.hp == 80);
    CHECK(hasLogText(result, "冷卻延長（+10幀）"));
    CHECK(damageLogAmountsFor(result, 1) == std::vector<int>{ 35 });
    const auto& aggregate = state.castLifecycle.runtime(castId).aggregate;
    CHECK(aggregate.distinctHitUnitIds == std::set<int>{ 1 });
    CHECK(aggregate.totalActualHpDamage == 35);
    CHECK(aggregate.highestActualHpDamage == 35);
    REQUIRE(aggregate.attacksByOrdinal.size() == 1);
    CHECK(aggregate.attacksByOrdinal.begin()->second.hitUnitIds == std::set<int>{ 1 });
}

TEST_CASE("Routed damage keeps causal provenance without inventing attack hits", "[battle][core][routing][regression]")
{
    bool deathReaction{};
    SECTION("retargeted hit effect") {}
    SECTION("defender death explosion") { deathReaction = true; }
    CAPTURE(deathReaction);
    auto frame = hitDamageFrameState(20, deathReaction ? 25 : 100);
    auto& state = frame.state;
    DealDamageAction damage;
    damage.amount.flat = 12;
    damage.kind = BattleDamageKind::Pure;
    damage.appliesDamageModifiers = false;
    EffectRule rule;
    rule.id = EffectRuleId{ 9001 };
    rule.castMatch = EffectCastMatch::OwnerAnyCast;
    rule.event = deathReaction ? EffectEvent::UnitDied : EffectEvent::HitBeforeDamage;
    rule.selector.kind = deathReaction ? EffectSelectorKind::Enemies : EffectSelectorKind::Self;
    rule.actions = { EffectAction{ damage } };
    state.effectRules.append({
        .kind = EffectSourceKind::Magic,
        .sourceId = 9001,
        .ownerUnitId = deathReaction ? 1 : 0,
        .sourceTeam = deathReaction ? 1 : 0,
    }, rule);
    const auto castId = state.attacks.attacks.front().provenance.cast.castId;

    const auto result = runBattleFrame(state);
    CHECK(damageLogAmountsFor(result, 0) == std::vector<int>{ 12 });
    CHECK(state.units.requireCore(0).vitals.hp == 68);
    const auto snapshot = state.castLifecycle.snapshot();
    if (deathReaction) REQUIRE(snapshot.retiredCasts.size() == 1);
    const auto& aggregate = deathReaction
        ? snapshot.retiredCasts.front().aggregate
        : state.castLifecycle.runtime(castId).aggregate;
    CHECK(aggregate.distinctHitUnitIds == std::set<int>{ 1 });
    CHECK(aggregate.totalActualHpDamage == (deathReaction ? 25 : 35));
    if (deathReaction)
    {
        CHECK(state.castLifecycle.activeCastCount() == 0);
        CHECK(state.castLifecycle.trackedWorkCount() == 0);
    }
}

TEST_CASE("Area effect projectiles borrow only a retained cast owned by their source", "[battle][core][routing][regression]")
{
    for (const int sourceUnitId : { 0, 1 })
    {
        for (const bool retainCast : { false, true })
        {
            CAPTURE(sourceUnitId, retainCast);
            auto fixture = hitDamageFrameState(20, 100);
            auto& state = fixture.state;
            const auto attack = state.attacks.attacks.front().provenance;
            std::vector<std::byte> memory(256 * 1024);
            auto frame = BattleFrameContext::begin(state, {}, memory.data(), memory.size());
            EffectCommandMetadata metadata;
            metadata.binding = { .kind = EffectSourceKind::Magic, .sourceId = 9002,
                .ownerUnitId = sourceUnitId, .sourceTeam = sourceUnitId };
            metadata.targetUnitId = 1 - sourceUnitId;
            DealDamageEffectCommand damage;
            damage.amount = 12;
            damage.kind = BattleDamageKind::Pure;
            damage.delivery.areaProjectiles = AreaProjectileDamageDelivery{ .rangeTiles = 2, .maximumTargets = 1 };
            EffectExecutionInputs inputs{ .cast = attack.cast, .attack = attack,
                .retainCastUntilDamageDescendants = retainCast };
            const auto output = BattleEffectCommandSystem::prepareDamageOutput(metadata, damage, inputs);
            CoreDetail::appendEffectDamageOutput(state, frame, frame.currentFrameDamage(), output, inputs);
            REQUIRE(frame.mutableAreaProjectileFollowUps().size() == 1);
            const auto& followUp = frame.mutableAreaProjectileFollowUps().front();
            CHECK(followUp.cast.sourceUnitId == sourceUnitId);
            const bool borrowed = sourceUnitId == 0 && retainCast;
            CHECK((followUp.cast.castId == attack.cast.castId) == borrowed);
            CHECK(followUp.sourceAttack.has_value() == borrowed);
            CHECK(followUp.ownsRootCast == !borrowed);
            CHECK(followUp.expansionWork.valid());
        }
    }
}

TEST_CASE("Healing preserves cast provenance after settlement without reviving cast work", "[battle][core][routing][healing][regression]")
{
    for (const bool settled : { false, true })
    {
        CAPTURE(settled);
        auto fixture = hitDamageFrameState(20, 100);
        auto& state = fixture.state;
        state.units.requireCore(0).vitals.hp = 40;
        const auto cast = state.castLifecycle.beginRootCast({ 0, 9003, false });
        if (settled)
        {
            state.castLifecycle.completeWork(cast.commitBarrier);
            state.castLifecycle.drainReadyEvents(0);
            state.castLifecycle.drainReadyEvents(0);
            REQUIRE_FALSE(state.castLifecycle.containsCast(cast.provenance.castId));
        }

        const EffectSourceBinding binding{ .kind = EffectSourceKind::Magic,
            .sourceId = 9003, .ownerUnitId = 0, .sourceTeam = 0 };
        ChangeResourceAction heal;
        heal.resource = BattleResource::Hp;
        heal.kind = ResourceChangeKind::Restore;
        heal.amount.flat = 10;
        EffectRule healRule;
        healRule.id = EffectRuleId{ 9003 };
        healRule.event = settled ? EffectEvent::CastSettled : EffectEvent::CastContinuation;
        healRule.castMatch = EffectCastMatch::OwnerAnyCast;
        healRule.selector.kind = EffectSelectorKind::Self;
        healRule.actions = { EffectAction{ heal } };
        state.effectRules.append(binding, healRule);

        EffectRule reaction;
        reaction.id = EffectRuleId{ 9004 };
        reaction.event = EffectEvent::HealApplied;
        reaction.selector.kind = EffectSelectorKind::Enemies;
        reaction.actions = { EffectAction{ DealDamageAction{
            .amount = EffectNumber{ .flat = 7 }, .kind = BattleDamageKind::Pure,
        } } };
        state.effectRules.append(binding, reaction);

        auto dispatched = BattleEffectEventBridge().dispatch(state,
            { .ownerUnitId = 0 }, healRule.event,
            CastAggregateEventData{ .provenance = cast.provenance, .originalTargetUnitId = 1 });
        REQUIRE(dispatched.commands.size() == 1);
        std::vector<std::byte> memory(256 * 1024);
        auto frame = BattleFrameContext::begin(state, {}, memory.data(), memory.size());
        frame.queueEffectCommands(std::move(dispatched.commands));
        CoreDetail::reduceEffectCommandBatches(state, frame, frame.currentFrameDamage());
        CHECK(state.units.requireCore(0).vitals.hp == 50);
        REQUIRE_FALSE(state.heals.events.empty());
        REQUIRE(state.heals.events.back().request.cast.has_value());
        CHECK(state.heals.events.back().request.cast->castId == cast.provenance.castId);
        CoreDetail::applyDamageAndLifecycle(state, frame);
        CHECK(state.units.requireCore(1).vitals.hp == 93);
        CHECK(state.castLifecycle.containsCast(cast.provenance.castId) == !settled);
        if (!settled) CHECK(state.castLifecycle.outstandingWork(cast.provenance.castId) == 1);
    }
}

TEST_CASE("True-Qi damage behaviorally grants hurt invincibility before the base hit",
          "[battle][core][true-qi][ordering][hurt-invincibility]")
{
    auto frame = hitDamageFrameState(20, 100);
    addTrueQiStatus(frame.state, 0, 3, 9);
    frame.state.units.require(1).damage.hurtInvincFrames = 5;

    const auto result = runBattleFrame(frame.state);

    CHECK(damageLogAmountsFor(result, 1) == std::vector<int>{ 27 });
    CHECK(frame.state.units.requireCore(1).vitals.hp == 73);
    CHECK(frame.state.units.requireCore(1).invincible == 5);
    CHECK(std::ranges::any_of(result.logEvents, [](const BattleLogEvent& event)
    {
        return event.type == BattleLogEventType::Status
            && event.targetUnitId == 1
            && event.amount == 5
            && BattleLogTest::textOf(event).contains("受傷無敵");
    }));
}

TEST_CASE("Main-projectile effects resolve before True-Qi and the accepted base hit",
          "[battle][core][true-qi][ordering][main-projectile]")
{
    auto frame = hitDamageFrameState(20, 100);
    addTrueQiStatus(frame.state, 0, 3, 9);

    EffectRule mainProjectile;
    mainProjectile.id = EffectRuleId{ 7100 };
    mainProjectile.event = EffectEvent::MainProjectileBeforeDamage;
    mainProjectile.castMatch = EffectCastMatch::OwnerAnyCast;
    mainProjectile.selector.kind = EffectSelectorKind::HitTarget;
    mainProjectile.actions = { EffectAction{ DealDamageAction{
        .amount = EffectNumber{ .flat = 5 },
        .kind = BattleDamageKind::Pure,
    } } };
    frame.state.effectRules.append({
        .kind = EffectSourceKind::Magic,
        .sourceId = 7100,
        .ownerUnitId = 0,
        .sourceTeam = 0,
    }, mainProjectile);

    const auto result = runBattleFrame(frame.state);

    CHECK(damageLogAmountsFor(result, 1) == std::vector<int>{ 5, 27, 35 });
    CHECK(frame.state.units.requireCore(1).vitals.hp == 33);
}

TEST_CASE("Lethal True-Qi damage resolves before and suppresses the base hit",
          "[battle][core][true-qi][ordering][death]")
{
    auto frame = hitDamageFrameState(20, 25);
    addTrueQiStatus(frame.state, 0, 3, 9);

    const auto result = runBattleFrame(frame.state);

    CHECK(damageLogAmountsFor(result, 1) == std::vector<int>{ 25 });
    CHECK_FALSE(frame.state.units.requireCore(1).alive);
    const auto lifecycle = frame.state.castLifecycle.snapshot();
    REQUIRE(lifecycle.retiredCasts.size() == 1);
    CHECK(lifecycle.retiredCasts.front().aggregate.totalActualHpDamage == 25);
    CHECK(lifecycle.retiredCasts.front().aggregate.distinctHitUnitIds
        == std::set<int>{ 1 });
    CHECK(frame.state.castLifecycle.activeCastCount() == 0);
    CHECK(frame.state.castLifecycle.trackedWorkCount() == 0);
}

TEST_CASE("True-Qi contributes once for every accepted contact of a multi-hit attack",
          "[battle][core][true-qi][multi-hit]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 105, 100, 0 }),
        unit(2, 1, { 205, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    addTrueQiStatus(state, 0, 3, 9);

    BattleAttackInstance projectile{ ordinaryProjectilePayload() };
    projectile.id = 10;
    projectile.state.attackSourceUnitId = 0;
    projectile.state.skillId = 101;
    projectile.state.skillMagicPower = 240;
    projectile.state.preferredTargetUnitId = 1;
    projectile.state.requirePreferredTarget = true;
    projectile.state.totalFrame = 30;
    projectile.state.operationType = BattleOperationType::RangedProjectile;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };
    appendTrackedAttack(state, std::move(projectile));

    const auto rootProvenance = state.attacks.attacks.front().provenance;
    BattleAttackInstance followUp{ ordinaryProjectilePayload() };
    followUp.id = 11;
    followUp.state.attackSourceUnitId = 0;
    followUp.state.skillId = 101;
    followUp.state.skillMagicPower = 240;
    followUp.state.preferredTargetUnitId = 2;
    followUp.state.requirePreferredTarget = true;
    followUp.state.totalFrame = 30;
    followUp.state.operationType = BattleOperationType::RangedProjectile;
    followUp.state.position = { 200, 100, 0 };
    followUp.state.velocity = { 5, 0, 0 };
    const auto reservation = state.castLifecycle.reserveAttack(
        rootProvenance.cast.castId,
        {
            .parentAttackId = rootProvenance.attackId,
            .origin = BattleAttackOriginKind::FollowUp,
            .mainProjectile = false,
        });
    followUp.provenance = completeAttackProvenance(
        reservation.provenance,
        battleAttackIdFromRuntimeId(followUp.id));
    followUp.castWork = reservation.work;
    state.castLifecycle.transferToLiveAttack(
        followUp.castWork,
        followUp.provenance.attackId);
    state.attacks.attacks.push_back(std::move(followUp));

    const auto castId = state.attacks.attacks.front().provenance.cast.castId;
    const auto result = runBattleFrame(state);

    const auto firstTargetDamage = damageLogAmountsFor(result, 1);
    const auto secondTargetDamage = damageLogAmountsFor(result, 2);
    REQUIRE(firstTargetDamage.size() >= 2);
    REQUIRE(secondTargetDamage.size() >= 2);
    CHECK(firstTargetDamage.front() == 27);
    CHECK(secondTargetDamage.front() == 27);
    const auto& aggregate = state.castLifecycle.runtime(castId).aggregate;
    CHECK(aggregate.distinctHitUnitIds == std::set<int>{ 1, 2 });
}








TEST_CASE("BattleFrameRunner_PrunesPendingCastWhenCasterDiesDuringDamageLifecycle", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 220, 100, 0 }, CombatStyle::Ranged),
    }));
    state.movement.frame = 1;
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    preparePendingCastCommitFrame(state, 1, BattleOperationType::RangedProjectile, 0);
    configureRuntimeActionPlan(state, frameCastInput(1, 0));
    auto pending = framePendingCastAction();
    pending.targetUnitId = 0;
    setTrackedPendingCast(state, 1, std::move(pending));

    queuePendingDamage(state, lethalDamageInput(0, 1));

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).size() == 1);
    CHECK_FALSE(state.units.requireCore(1).alive);
    CHECK(state.units.pendingCastCount() == 0);
    CHECK_FALSE(state.units.requireCore(1).haveAction);
    CHECK(state.units.requireCore(1).operationType == BattleOperationType::None);
    CHECK(state.units.requireCore(1).animation.actType == -1);
}









TEST_CASE("BattleFrameRunner_AdvanceFrame_RecordsProjectileCancelPairWithOtherAttackId", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 900, 900, 0 }, CombatStyle::Ranged),
    }));
    state.attacks = attackWorld();

    BattleAttackInstance first{ ordinaryProjectilePayload() };
    first.id = 10;
    first.state.attackSourceUnitId = 0;
    first.frame = 5;
    first.state.totalFrame = 30;
    first.state.position = { 500, 500, 0 };
    first.state.operationType = BattleOperationType::TrackingProjectile;
    first.state.projectileCancelDamage = 11;

    BattleAttackInstance second{ ordinaryProjectilePayload() };
    second.id = 20;
    second.state.attackSourceUnitId = 1;
    second.frame = 5;
    second.state.totalFrame = 30;
    second.state.position = { 500, 500, 0 };
    second.state.operationType = BattleOperationType::RangedProjectile;
    second.state.projectileCancelDamage = 10;

    seedRuntimeUnitsFromWorld(state);
    appendTrackedAttack(state, std::move(first));
    appendTrackedAttack(state, std::move(second));

    auto result = runBattleFrame(state);

    REQUIRE(result.gameplayEvents.size() >= 3);
    CHECK(result.gameplayEvents[2].type == BattleGameplayEventType::ProjectileCancelled);
    CHECK(result.gameplayEvents[2].effectId == 10);
    CHECK(result.gameplayEvents[2].otherAttackId == 20);
    CHECK(result.gameplayEvents[2].amount == 0);
    CHECK(result.visualEvents[2].type == BattleVisualEventType::ProjectileCancelled);
    CHECK(result.visualEvents[2].amount == 20);
    REQUIRE(result.logEvents.size() == 1);
    CHECK(result.logEvents[0].type == BattleLogEventType::Status);
    CHECK(result.logEvents[0].sourceUnitId == 0);
    CHECK(result.logEvents[0].targetUnitId == 1);
    CHECK(result.logEvents[0].amount == 17);
    CHECK(BattleLogTest::textOf(result.logEvents[0]) == "抵消彈道 #10 vs #20（17 - 10 = 7）");
    CHECK(BattleLogTest::hasSegment(result.logEvents[0], "#10", BattleLogTextTone::ProjectileId));
    CHECK(BattleLogTest::hasSegment(result.logEvents[0], "#20", BattleLogTextTone::ProjectileId));
    CHECK(BattleLogTest::hasSegment(result.logEvents[0], " - ", BattleLogTextTone::FormulaValue));
    CHECK(BattleLogTest::hasSegment(result.logEvents[0], " = ", BattleLogTextTone::FormulaValue));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_RecordsTargetLostCancellationWithoutPairedAttack", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 700, 100, 0 }, CombatStyle::Ranged),
        unit(2, 1, { 700, 120, 0 }, CombatStyle::Ranged),
    }));
    state.attacks = attackWorld();

    BattleAttackInstance projectile{ ordinaryProjectilePayload() };
    projectile.id = 10;
    projectile.state.attackSourceUnitId = 0;
    projectile.state.preferredTargetUnitId = 2;
    projectile.state.requirePreferredTarget = true;
    projectile.state.totalFrame = 30;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };

    seedRuntimeUnitsFromWorld(state);
    state.units.requireCore(2).alive = false;
    appendTrackedAttack(state, std::move(projectile));

    auto result = runBattleFrame(state);

    REQUIRE(result.gameplayEvents.size() >= 2);
    REQUIRE(result.logEvents.size() == 1);
    REQUIRE(result.visualEvents.size() == 2);

    CHECK(std::any_of(
        result.gameplayEvents.begin(),
        result.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::ProjectileCancelled
                && event.effectId == 10;
        }));
    CHECK(result.gameplayEvents[1].effectId == 10);
    CHECK(result.gameplayEvents[1].targetUnitId == -1);
    CHECK(result.gameplayEvents[1].otherAttackId == -1);
    CHECK(result.visualEvents[1].type == BattleVisualEventType::ProjectileTargetLost);
    CHECK(result.visualEvents[1].amount == -1);
    CHECK(result.logEvents[0].type == BattleLogEventType::Status);
    CHECK(result.logEvents[0].sourceUnitId == 0);
    CHECK(result.logEvents[0].targetUnitId == -1);
    CHECK(BattleLogTest::textOf(result.logEvents[0]) == "彈道停止：1枚目標遺失");
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AggregatesProjectileContactIgnoredByInvincible", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 105, 100, 0 }, CombatStyle::Ranged),
    }));
    state.attacks = attackWorld();

    BattleAttackInstance projectile{ ordinaryProjectilePayload() };
    projectile.id = 10;
    projectile.state.attackSourceUnitId = 0;
    projectile.state.totalFrame = 30;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };
    projectile.state.operationType = BattleOperationType::RangedProjectile;

    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 100, { 105, 100, 0 }),
});
    state.units.requireCore(1).invincible = 3;
    appendTrackedAttack(state, projectile);
    projectile.id = 11;
    appendTrackedAttack(state, std::move(projectile));

    auto result = runBattleFrame(state);

    REQUIRE(result.gameplayEvents.size() == 4);
    CHECK(result.gameplayEvents[1].type == BattleGameplayEventType::StatusApplied);
    CHECK(result.gameplayEvents[1].sourceUnitId == 0);
    CHECK(result.gameplayEvents[1].targetUnitId == 1);
    CHECK(result.gameplayEvents[3].type == BattleGameplayEventType::StatusApplied);
    CHECK(result.gameplayEvents[3].sourceUnitId == 0);
    CHECK(result.gameplayEvents[3].targetUnitId == 1);
    CHECK(damageLogAmountsFor(result).empty());
    CHECK(gameplayEventsFor(result, BattleGameplayEventType::DamageApplied).empty());
    CHECK(state.units.requireCore(1).invincible > 0);
    REQUIRE(result.logEvents.size() == 1);
    CHECK(result.logEvents[0].type == BattleLogEventType::Status);
    CHECK(result.logEvents[0].sourceUnitId == 1);
    CHECK(result.logEvents[0].targetUnitId == -1);
    CHECK(BattleLogTest::textOf(result.logEvents[0]) == "彈道命中無敵：2枚傷害忽略");
    CHECK(result.gameplayEvents[1].type == BattleGameplayEventType::StatusApplied);
    CHECK(result.gameplayEvents[1].text == "彈道命中無敵：傷害忽略");
    CHECK(result.gameplayEvents[3].type == BattleGameplayEventType::StatusApplied);
    CHECK(result.gameplayEvents[3].text == "彈道命中無敵：傷害忽略");
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ExecutePiercesInvincibility", "[battle][core][execute]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 105, 100, 0 }, CombatStyle::Ranged),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 20, { 105, 100, 0 }),
    });
    state.units.requireCore(1).vitals.maxHp = 100;
    state.units.requireCore(1).invincible = 3;
    state.units.require(0).status = statusRuntimeSnapshot(0, 100);
    state.units.require(1).status = statusRuntimeSnapshot(1, 20);
    DealDamageAction execute;
    execute.amount.base = EffectNumberBase::TargetMaxHp;
    execute.amount.percent = 100;
    execute.kind = BattleDamageKind::Execute;
    EffectRule executeRule;
    executeRule.id = { 1 };
    executeRule.event = EffectEvent::MainProjectileBeforeDamage;
    executeRule.selector.kind = EffectSelectorKind::HitTarget;
    executeRule.conditions = { TargetHpRatioAtMostCondition{ 50 } };
    executeRule.actions = { EffectAction{ .value = execute } };
    state.effectRules.append(testOwnerRuleBinding(state, 0, 9150), executeRule);

    auto request = attackSpawnRequest();
    request.initial.preferredTargetUnitId = 1;
    request.initial.requirePreferredTarget = true;
    request.initial.through = false;
    request.initial.skillMagicPower = 240;
    request.initial.position = { 100, 100, 0 };
    request.initial.velocity = { 5, 0, 0 };
    request.provenance.mainProjectile = true;
    queueTrackedAttack(state, std::move(request));

    auto result = runBattleFrame(state);

    CHECK_FALSE(state.units.requireCore(1).alive);
    CHECK(state.units.requireCore(1).vitals.hp == 0);
    CHECK_FALSE(gameplayEventsFor(result, BattleGameplayEventType::UnitDied, 1).empty());
    CHECK(std::none_of(
        result.logEvents.begin(),
        result.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return BattleLogTest::textOf(event).contains("彈道命中無敵");
        }));
}

TEST_CASE("BattleFrameRunner_PrunesFinishedRuntimeAttacksAfterFrame", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    }));
    state.attacks = attackWorld();
    BattleAttackInstance attack{ ordinaryProjectilePayload() };
    attack.id = 77;
    attack.frame = 0;
    attack.state.attackSourceUnitId = 0;
    attack.state.preferredTargetUnitId = 0;
    attack.state.operationType = BattleOperationType::RangedProjectile;
    attack.state.position = { 100, 100, 0 };
    attack.state.velocity = { 0, 0, 0 };
    attack.state.totalFrame = 1;
    seedRuntimeUnitsFromWorld(state);
    appendTrackedAttack(state, std::move(attack));

    auto result = runBattleFrame(state);

    REQUIRE(hasProjectilePresentationEvent(result));
    CHECK(std::any_of(
        result.visualEvents.begin(),
        result.visualEvents.end(),
        [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::ProjectileExpired && event.effectId == 77;
        }));
    CHECK(state.attacks.attacks.empty());
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_RecordsProjectileGameplayEventsSeparatelyFromPresentation", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 105, 100, 0 }),
    }));
    state.attacks = attackWorld();

    BattleAttackInstance projectile{ ordinaryProjectilePayload() };
    projectile.id = 10;
    projectile.state.attackSourceUnitId = 0;
    projectile.state.totalFrame = 30;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };

    BattleAttackInstance expiringProjectile{ ordinaryProjectilePayload() };
    expiringProjectile.id = 20;
    expiringProjectile.state.attackSourceUnitId = 0;
    expiringProjectile.state.totalFrame = 1;
    expiringProjectile.noHurt = true;
    expiringProjectile.state.position = { 300, 100, 0 };
    expiringProjectile.state.velocity = { 5, 0, 0 };

    seedRuntimeUnitsFromWorld(state);
    appendTrackedAttack(state, std::move(projectile));
    appendTrackedAttack(state, std::move(expiringProjectile));

    auto result = runBattleFrame(state);

    REQUIRE(result.gameplayEvents.size() == 4);
    REQUIRE(result.logEvents.empty());
    REQUIRE(result.visualEvents.size() == 4);

    CHECK(result.gameplayEvents[0].type == BattleGameplayEventType::ProjectileMoved);
    CHECK(result.gameplayEvents[0].effectId == 10);
    CHECK(result.gameplayEvents[0].sourceUnitId == 0);
    CHECK(result.gameplayEvents[0].position.x == 105.0f);
    CHECK(result.visualEvents[0].type == BattleVisualEventType::ProjectileMoved);

    CHECK(result.gameplayEvents[1].type == BattleGameplayEventType::ProjectileHit);
    CHECK(result.gameplayEvents[1].effectId == 10);
    CHECK(result.gameplayEvents[1].sourceUnitId == 0);
    CHECK(result.gameplayEvents[1].targetUnitId == 1);
    CHECK(result.visualEvents[1].type == BattleVisualEventType::ProjectileHit);

    CHECK(result.gameplayEvents[2].type == BattleGameplayEventType::ProjectileMoved);
    CHECK(result.gameplayEvents[2].effectId == 20);
    CHECK(result.visualEvents[2].type == BattleVisualEventType::ProjectileMoved);

    CHECK(result.gameplayEvents[3].type == BattleGameplayEventType::ProjectileExpired);
    CHECK(result.gameplayEvents[3].effectId == 20);
    CHECK(result.visualEvents[3].type == BattleVisualEventType::ProjectileExpired);
}


TEST_CASE("BattleFrameRunner_AdvanceFrame_QueuesHitGeneratedProjectilesForNextFrame", "[battle][core][ownership]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 105, 100, 0 }),
        unit(2, 1, { 140, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    ModifyAttackAction nearbyTracking;
    nearbyTracking.runtimeBehavior = NearbyTrackingAttackBehavior{
        .rangePixels = 80,
        .damagePct = 45,
    };
    addTestOwnerRule(
        state,
        0,
        9500,
        1,
        EffectEvent::MainProjectileBeforeDamage,
        EffectActionValue{ nearbyTracking });

    BattleAttackInstance projectile{ ordinaryProjectilePayload() };
    projectile.id = 10;
    projectile.state.attackSourceUnitId = 0;
    projectile.state.skillId = 101;
    projectile.state.skillMagicPower = 480;
    projectile.state.totalFrame = 30;
    projectile.state.operationType = BattleOperationType::RangedProjectile;
    projectile.state.visualEffectId = 44;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };
    appendTrackedAttack(state, std::move(projectile));

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).size() == 1);
    REQUIRE(state.nextFrame.queuedAttacks().size() == 2);
    CHECK(state.nextFrame.queuedAttacks()[0].initial.attackSourceUnitId == 0);
    CHECK(state.nextFrame.queuedAttacks()[0].initial.preferredTargetUnitId == 1);
    CHECK(state.nextFrame.queuedAttacks()[0].initial.suppressNearbyTrackingProjectileProc);
    CHECK(state.nextFrame.queuedAttacks()[1].initial.attackSourceUnitId == 0);
    CHECK(state.nextFrame.queuedAttacks()[1].initial.preferredTargetUnitId == 2);
    CHECK(state.nextFrame.queuedAttacks()[1].initial.suppressNearbyTrackingProjectileProc);
}


TEST_CASE("BattleFrameRunner_AdvanceFrame_AppliesMainProjectileImpactFreezeInCore", "[battle][core][breakthrough]")
{
    auto frame = hitDamageFrameState(70, 100);
    auto& state = frame.state;

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).size() == 1);
    CHECK(state.units.require(1).frozenFrames() == 5);
    CHECK(state.units.require(1).status.effects.maximumFrames(BattleStatusKind::Stun) == 5);
}


TEST_CASE("BattleFrameRunner_AdvanceFrame_DoesNotApplyImpactFreezeForNonMainProjectile", "[battle][core][breakthrough]")
{
    auto frame = hitDamageFrameState(70, 100);
    auto& state = frame.state;
    state.attacks.attacks.front().provenance.mainProjectile = false;

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).size() == 1);
    CHECK_FALSE(state.units.require(1).frozen());
}


TEST_CASE("BattleFrameRunner_AdvanceFrame_LabelsChainedProjectileTargetLost", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 700, 100, 0 }, CombatStyle::Ranged),
    }));
    state.attacks = attackWorld();

    BattleAttackInstance projectile{ ordinaryProjectilePayload() };
    projectile.id = 10;
    projectile.state.attackSourceUnitId = 0;
    projectile.state.preferredTargetUnitId = 1;
    projectile.state.requirePreferredTarget = true;
    projectile.state.totalFrame = 30;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };

    seedRuntimeUnitsFromWorld(state);
    state.units.requireCore(1).alive = false;
    appendTrackedAttack(state, std::move(projectile), 9);

    auto result = runBattleFrame(state);

    REQUIRE(result.gameplayEvents.size() >= 2);
    CHECK(std::any_of(
        result.gameplayEvents.begin(),
        result.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::ProjectileCancelled
                && event.effectId == 10;
        }));
    CHECK(std::any_of(
        result.logEvents.begin(),
        result.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return BattleLogTest::textOf(event) == "連鎖彈道停止：1枚原目標失效";
        }));
}


TEST_CASE("BattleFrameRunner_AdvanceFrame_CoalescesSameFrameChainedProjectileStopLogs", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 700, 100, 0 }, CombatStyle::Ranged),
    }));
    state.attacks = attackWorld();

    BattleAttackInstance first{ ordinaryProjectilePayload() };
    first.id = 10;
    first.state.attackSourceUnitId = 0;
    first.state.preferredTargetUnitId = 1;
    first.state.requirePreferredTarget = true;
    first.state.totalFrame = 30;
    first.state.position = { 100, 100, 0 };
    first.state.velocity = { 5, 0, 0 };

    BattleAttackInstance second = first;
    second.id = 11;

    seedRuntimeUnitsFromWorld(state);
    state.units.requireCore(1).alive = false;
    appendTrackedAttack(state, std::move(first), 8);
    appendTrackedAttack(state, std::move(second), 9);

    auto result = runBattleFrame(state);

    const int targetLostCount = static_cast<int>(std::count_if(
        result.visualEvents.begin(),
        result.visualEvents.end(),
        [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::ProjectileTargetLost;
        }));
    CHECK(targetLostCount == 2);
    const int stopLogCount = static_cast<int>(std::count_if(
        result.logEvents.begin(),
        result.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return BattleLogTest::textOf(event) == "連鎖彈道停止：2枚原目標失效";
        }));
    CHECK(stopLogCount == 1);
}


TEST_CASE("BattleFrameRunner_AdvanceFrame_ProjectileCancelLogPutsWinnerOnLeft", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 900, 900, 0 }, CombatStyle::Ranged),
    }));
    state.attacks = attackWorld();

    BattleAttackInstance first{ ordinaryProjectilePayload() };
    first.id = 10;
    first.state.attackSourceUnitId = 0;
    first.frame = 5;
    first.state.totalFrame = 30;
    first.state.position = { 500, 500, 0 };
    first.state.operationType = BattleOperationType::RangedProjectile;
    first.state.projectileCancelDamage = 10;

    BattleAttackInstance second{ ordinaryProjectilePayload() };
    second.id = 20;
    second.state.attackSourceUnitId = 1;
    second.frame = 5;
    second.state.totalFrame = 30;
    second.state.position = { 500, 500, 0 };
    second.state.operationType = BattleOperationType::TrackingProjectile;
    second.state.projectileCancelDamage = 11;

    seedRuntimeUnitsFromWorld(state);
    appendTrackedAttack(state, std::move(first));
    appendTrackedAttack(state, std::move(second));

    auto result = runBattleFrame(state);

    REQUIRE(result.logEvents.size() == 1);
    CHECK(result.logEvents[0].sourceUnitId == 1);
    CHECK(result.logEvents[0].targetUnitId == 0);
    CHECK(result.logEvents[0].amount == 17);
    CHECK(BattleLogTest::textOf(result.logEvents[0]) == "抵消彈道 #20 vs #10（17 - 10 = 7）");
    CHECK(BattleLogTest::hasSegment(result.logEvents[0], "#20", BattleLogTextTone::ProjectileId));
    CHECK(BattleLogTest::hasSegment(result.logEvents[0], "#10", BattleLogTextTone::ProjectileId));
    CHECK(BattleLogTest::hasSegment(result.logEvents[0], " - ", BattleLogTextTone::FormulaValue));
    CHECK(BattleLogTest::hasSegment(result.logEvents[0], " = ", BattleLogTextTone::FormulaValue));
}


TEST_CASE("BattleFrameRunner_AdvanceFrame_RecordsBounceAsAttackSpawnedGameplay", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 105, 100, 0 }),
        unit(2, 1, { 180, 100, 0 }),
    }));
    state.attacks = attackWorld();

    BattleAttackInstance projectile{ ordinaryProjectilePayload() };
    projectile.id = 10;
    projectile.state.attackSourceUnitId = 0;
    projectile.state.skillId = 101;
    projectile.state.skillMagicPower = 840;
    projectile.state.preferredTargetUnitId = 1;
    projectile.state.totalFrame = 30;
    projectile.state.visualEffectId = 44;
    projectile.state.operationType = BattleOperationType::RangedProjectile;
    projectile.state.bounceRemaining = 1;
    projectile.state.bounceRange = 500;
    projectile.state.bounceChancePct = 100;
    projectile.state.bounceRollPct = 0;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };

    state.attacks.nextAttackId = 30;
    seedRuntimeUnitsFromWorld(state);
    appendTrackedAttack(state, std::move(projectile));

    auto result = runBattleFrame(state);

    REQUIRE(result.gameplayEvents.size() >= 3);
    const auto* bounce = findVisualEvent(result, BattleVisualEventType::ProjectileBounced, 10);
    REQUIRE(bounce);
    const auto gameplaySpawn = std::find_if(
        result.gameplayEvents.begin(),
        result.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::AttackSpawned
                && event.effectId == 30;
        });
    REQUIRE(gameplaySpawn != result.gameplayEvents.end());
    CHECK(gameplaySpawn->sourceUnitId == 0);
    CHECK(gameplaySpawn->targetUnitId == 2);
    CHECK(bounce->effectId == 10);
    CHECK(bounce->amount == 30);
    const auto visualSpawn = std::find_if(
        result.visualEvents.begin(),
        result.visualEvents.end(),
        [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::ProjectileSpawned
                && event.effectId == 30;
        });
    REQUIRE(visualSpawn != result.visualEvents.end());
    const auto& spawnEvent = *visualSpawn;
    CHECK(spawnEvent.type == BattleVisualEventType::ProjectileSpawned);
    CHECK(spawnEvent.effectId == 30);
    CHECK(spawnEvent.sourceUnitId == 0);
    CHECK(spawnEvent.targetUnitId == 2);
    CHECK(spawnEvent.durationFrames >= 20);
    CHECK(spawnEvent.visualEffectId == 44);
    CHECK(spawnEvent.position.x != 0.0f);
    CHECK(spawnEvent.velocity.x > 0.0f);
    CHECK(spawnEvent.operationKind == 2);
}


TEST_CASE("BattleFrameRunner_AdvanceFrame_LogsBounceChainTerminalReasons", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 105, 100, 0 }),
        unit(2, 1, { 400, 100, 0 }),
    }));
    state.attacks = attackWorld();

    BattleAttackInstance projectile{ ordinaryProjectilePayload() };
    projectile.id = 10;
    projectile.state.attackSourceUnitId = 0;
    projectile.state.skillId = 101;
    projectile.state.skillMagicPower = 840;
    projectile.state.preferredTargetUnitId = 1;
    projectile.state.totalFrame = 30;
    projectile.state.operationType = BattleOperationType::RangedProjectile;
    projectile.state.bounceRemaining = 2;
    projectile.state.bounceRange = 90;
    projectile.state.bounceChancePct = 100;
    projectile.state.bounceRollPct = 0;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };

    seedRuntimeUnitsFromWorld(state);
    appendTrackedAttack(state, std::move(projectile));

    auto result = runBattleFrame(state);

    REQUIRE(result.gameplayEvents.size() >= 3);
    CHECK(std::any_of(
        result.gameplayEvents.begin(),
        result.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::ProjectileExpired
                && event.effectId == 10;
        }));
    const auto damageLog = std::find_if(
        result.logEvents.begin(),
        result.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Damage
                && event.sourceUnitId == 0
                && event.targetUnitId == 1;
        });
    const auto terminalLog = std::find_if(
        result.logEvents.begin(),
        result.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.sourceUnitId == 0
                && event.targetUnitId == -1
                && BattleLogTest::textOf(event) == "連鎖彈道停止：1枚搜尋範圍內無可連鎖目標";
        });
    REQUIRE(damageLog != result.logEvents.end());
    REQUIRE(terminalLog != result.logEvents.end());
    CHECK(damageLog < terminalLog);
}

TEST_CASE("Seven star combat projectiles respect defence dodge and block", "[battle][core][seven-star]")
{
    const auto damageAgainst = [](int defence, std::optional<BattleAttribute> immunity, int magicPower = 300)
    {
        auto frame = hitDamageFrameState(0, 10000);
        auto& state = frame.state;
        state.random = BattleRuntimeRandom(123);
        auto& attack = state.attacks.attacks.front();
        attack.state.skillId = -1;
        attack.state.skillMagicPower = 0;
        attack.state.potencySnapshot = BattleAttackPotencySnapshot{400, magicPower};
        attack.state.damageKind = BattleDamageKind::Physical;
        attack.state.suppressNearbyTrackingProjectileProc = true;
        attack.provenance.propagation = CastPropagationPolicy::NoEffectRules;
        attack.provenance.mainProjectile = false;
        state.units.requireCore(1).stats.defence = defence;
        if (immunity)
            addTypedAttributeModifier(state, 1, *immunity, AttributeOperation::FlatAdd, 100);
        runBattleFrame(state);
        return 10000 - state.units.requireCore(1).vitals.hp;
    };
    const int unarmoured = damageAgainst(0, {});
    const int armoured = damageAgainst(800, {});
    CHECK(unarmoured > 0);
    CHECK(unarmoured < 400);
    CHECK(armoured > 0);
    CHECK(armoured < unarmoured);
    CHECK(damageAgainst(0, {}, 0) < unarmoured);
    CHECK(damageAgainst(0, BattleAttribute::DodgeChance) == 0);
    CHECK(damageAgainst(0, BattleAttribute::BlockChance) == 0);
}
