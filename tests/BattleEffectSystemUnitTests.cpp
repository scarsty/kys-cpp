#include "EffectDispatchTestHelpers.h"
#include "battle/BattleEffectSystem.h"
#include "battle/BattleRuntimeRandom.h"
#include "BattleCoreTestHelpers.h"
#include "ChessBattleEffectValidation.h"
#include "ChessGameplayEffect.h"

#include <yaml-cpp/yaml.h>

#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

using namespace KysChess;
using namespace KysChess::Battle;
using namespace KysChess::Battle::Test;

namespace
{

EffectUnitSnapshot makeUnit(int id,
                            int team,
                            int hp,
                            int maxHp,
                            int mp = 0,
                            int maxMp = 100,
                            Pointf position = {})
{
    EffectUnitSnapshot unit;
    unit.id = id;
    unit.team = team;
    unit.hp = hp;
    unit.maxHp = maxHp;
    unit.mp = mp;
    unit.maxMp = maxMp;
    unit.position = position;
    return unit;
}

EffectSourceBinding magicBinding(int magicId = 59)
{
    return {
        .kind = EffectSourceKind::Magic,
        .sourceId = magicId,
        .ownerUnitId = 1,
        .sourceTeam = 0,
    };
}

BattleCastProvenance castProvenance(int magicId,
                                    CastPropagationPolicy propagation = CastPropagationPolicy::SourceRules)
{
    return {
        .rootCastId = BattleCastId(1),
        .castId = BattleCastId(1),
        .sourceUnitId = 1,
        .magicId = magicId,
        .ultimate = true,
        .origin = CastOriginKind::Ultimate,
        .propagation = propagation,
    };
}

BattleAttackProvenance attackProvenance(int magicId,
                                        CastPropagationPolicy propagation = CastPropagationPolicy::SourceRules,
                                        int ordinal = 0)
{
    return {
        .cast = castProvenance(magicId, propagation),
        .propagation = propagation,
        .origin = BattleAttackOriginKind::Initial,
        .attackId = BattleAttackId(1),
        .attackOrdinal = ordinal,
        .rootAttack = true,
        .mainProjectile = true,
    };
}

template<class Payload>
EffectEventData makeContext(EffectEvent event,
                               EffectSourceBinding binding,
                               const EffectUnitSnapshot& owner,
                               const std::vector<EffectUnitSnapshot>& units,
                               Payload payload)
{
    return {
        .event = event,
        .header = {
            .frame = 10,
            .eventOrdinal = 20,
            .binding = binding,
            .owner = &owner,
            .battle = BattleEffectReadView(units),
        },
        .payload = std::move(payload),
    };
}

template<class Action>
EffectAction effectAction(Action action)
{
    return { EffectActionValue{ std::move(action) } };
}

template<class Action>
EffectAction stateAction(Action action)
{
    return effectAction(StateMachineAction{ std::move(action) });
}

EffectRule makeRule(std::uint64_t id,
                    EffectEvent event,
                    EffectSelector selector,
                    std::vector<EffectAction> actions,
                    std::vector<EffectCondition> conditions = {})
{
    EffectRule rule;
    rule.id = EffectRuleId{ id };
    rule.event = event;
    rule.selector = selector;
    rule.conditions = std::move(conditions);
    rule.actions = std::move(actions);
    return rule;
}

EffectSelector selfSelector()
{
    return { .kind = EffectSelectorKind::Self };
}

EffectSelector hitTargetSelector()
{
    return { .kind = EffectSelectorKind::HitTarget };
}

ApplyStatusAction aggregatingPoisonWithDeathShield(
    int damagePercent,
    std::uint64_t extraRuleId,
    int shieldAmount)
{
    ApplyStatusAction poison;
    poison.status = BattleStatusKind::Poison;
    poison.durationFrames = 90;
    poison.quantity = SetStatusTriggerCharges{ 3 };
    poison.reapplication = StatusReapplicationPolicy::KeepHigherDamage;
    poison.poisonSameEventMerge = PoisonSameEventMerge::SumDamagePercent;
    auto behavior = std::make_shared<StatusBehaviorDefinition>(
        *poisonStatusBehavior(damagePercent));
    ChangeResourceAction shield;
    shield.resource = BattleResource::Shield;
    shield.kind = ResourceChangeKind::Grant;
    shield.amount.flat = shieldAmount;
    auto extra = makeRule(
        extraRuleId,
        EffectEvent::UnitDied,
        EffectSelector{ .kind = EffectSelectorKind::StatusHolder },
        { effectAction(shield) });
    extra.observation = EffectObservationScope::StatusHolderEventTarget;
    behavior->rules.push_back(std::move(extra));
    poison.behavior = std::move(behavior);
    return poison;
}

}  // namespace

TEST_CASE("EffectResourcesBeforeCastSnapshot copies share immutable storage",
          "[battle][effect][resources]")
{
    const EffectResourcesBeforeCastSnapshot original{
        { 1, 80, 100 },
        { 2, 40, 100 },
    };
    const auto copy = original;

    REQUIRE(original.size() == 2);
    CHECK(copy.values().data() == original.values().data());
}

TEST_CASE("BattleEffectSystem interval rules count eligible frame events and reset", "[battle][effect][interval]")
{
    auto owner = makeUnit(1, 0, 1000, 1000, 0, 100);
    owner.alive = true;
    const std::vector units{ owner };
    auto context = makeContext(
        EffectEvent::FrameAdvanced,
        magicBinding(59),
        owner,
        units,
        FrameTickEventData{ .deltaFrames = 1 });

    ChangeResourceAction restoreMp;
    restoreMp.resource = BattleResource::Mp;
    restoreMp.kind = ResourceChangeKind::Grant;
    restoreMp.amount.flat = 1;
    auto rule = makeRule(
        1,
        EffectEvent::FrameAdvanced,
        selfSelector(),
        { effectAction(restoreMp) });
    rule.intervalFrames = 3;

    const auto binding = magicBinding(59);
    BattleEffectRuleStore store;
    store.append(binding, rule);
    CHECK(store.runtime(binding, rule.id).intervalFramesRemaining == 3);

    BattleRuntimeRandom random(1);
    context.header.frame = 1;
    CHECK(BattleEffectSystem{}.dispatch(store, context, random).commands.empty());
    CHECK(store.runtime(binding, rule.id).intervalFramesRemaining == 2);

    // No FrameAdvanced dispatch means the owner timer is paused.
    context.header.frame = 3;
    CHECK(BattleEffectSystem{}.dispatch(store, context, random).commands.empty());
    CHECK(store.runtime(binding, rule.id).intervalFramesRemaining == 1);

    context.header.frame = 4;
    const auto first = BattleEffectSystem{}.dispatch(store, context, random);
    REQUIRE(first.commands.size() == 1);
    CHECK(store.activationCount(binding, rule.id) == 1);
    CHECK(store.runtime(binding, rule.id).intervalFramesRemaining == 3);

    for (int frame : { 5, 6 })
    {
        context.header.frame = frame;
        CHECK(BattleEffectSystem{}.dispatch(store, context, random).commands.empty());
    }
    context.header.frame = 7;
    CHECK(BattleEffectSystem{}.dispatch(store, context, random).commands.size() == 1);
    CHECK(store.activationCount(binding, rule.id) == 2);
    CHECK(store.runtime(binding, rule.id).intervalFramesRemaining == 3);
}

TEST_CASE("BattleEffectSystem scales poison explosion once by contribution layers",
          "[battle][effect][status][poison_explosion]")
{
    auto owner = makeUnit(1, 0, 0, 1000);
    owner.alive = false;
    owner.statusDetails.push_back({
        .state = BattleStatusKind::PoisonExplosion,
        .sourceUnitId = owner.id,
        .stacks = 3,
    });
    const auto enemy = makeUnit(2, 1, 1000, 1000);
    const std::vector units{ owner, enemy };

    DealDamageAction damage;
    damage.amount.flat = 240;
    damage.amount.statusScale = StatusNumberScale::PerContributionLayer;
    damage.kind = BattleDamageKind::Pure;
    ApplyStatusAction poison;
    poison.status = BattleStatusKind::Poison;
    poison.durationFrames = 120;
    poison.quantity = SetStatusTriggerCharges{ 4 };
    poison.reapplication = StatusReapplicationPolicy::ReplaceExistingPoison;
    poison.behavior = poisonStatusBehavior(10);
    auto rule = makeRule(
        1,
        EffectEvent::UnitDied,
        EffectSelector{ .kind = EffectSelectorKind::Enemies, .count = 1 },
        { effectAction(damage), effectAction(poison) });
    rule.observation = EffectObservationScope::StatusHolderEventTarget;
    auto behavior = std::make_shared<StatusBehaviorDefinition>();
    behavior->rules.push_back(rule);
    std::vector runtime{ EffectRuleRuntimeState{} };
    const auto active = statusBehaviorFixture({
        .binding = magicBinding(95),
        .producerRuleId = EffectRuleId{ 95 },
        .producerRuleOrder = 4,
        .holderUnitId = owner.id,
        .sourceUnitId = owner.id,
        .kind = BattleStatusKind::PoisonExplosion,
        .quantity = 3,
        .appliedSequence = 1,
        .behavior = behavior,
        .runtime = &runtime,
    }, nullptr);
    BattleRuntimeRandom random(1);
    const auto context = makeContext(
        EffectEvent::UnitDied,
        magicBinding(95),
        owner,
        units,
        DeathEventData{
            .deadBefore = owner,
            .deadAfter = owner,
            .cause = EffectEnvironmentDamageOrigin{},
        });

    const auto dispatched = dispatchStatusFixture(
        context, random, std::span{ &active, std::size_t{ 1 } });
    REQUIRE(dispatched.commands.size() == 2);
    const auto& damageCommand = std::get<DealDamageEffectCommand>(
        dispatched.commands[0].value);
    CHECK(damageCommand.amount == 720);
    CHECK(damageCommand.transactionCount == 1);
    CHECK(std::holds_alternative<ApplyStatusEffectCommand>(
        dispatched.commands[1].value));
}

TEST_CASE("BattleEffectSystem keeps poison preflight ordering metadata and incompatible candidates",
          "[battle][effect][status][poison][aggregation][ordering]")
{
    const auto owner = makeUnit(1, 0, 1000, 1000);
    const auto enemy = makeUnit(2, 1, 1000, 1000);
    const std::vector units{ owner, enemy };

    ChangeResourceAction unrelatedShield;
    unrelatedShield.resource = BattleResource::Shield;
    unrelatedShield.kind = ResourceChangeKind::Grant;
    unrelatedShield.amount.flat = 1;
    const auto unrelatedRule = makeRule(
        1,
        EffectEvent::HitBeforeDamage,
        hitTargetSelector(),
        { effectAction(unrelatedShield) });

    const auto firstRule = makeRule(
        2,
        EffectEvent::HitBeforeDamage,
        hitTargetSelector(),
        { effectAction(aggregatingPoisonWithDeathShield(7, 101, 5)) });
    const auto secondRule = makeRule(
        3,
        EffectEvent::HitBeforeDamage,
        hitTargetSelector(),
        { effectAction(aggregatingPoisonWithDeathShield(11, 202, 5)) });
    const auto incompatibleRule = makeRule(
        4,
        EffectEvent::HitBeforeDamage,
        hitTargetSelector(),
        { effectAction(aggregatingPoisonWithDeathShield(13, 303, 6)) });

    BattleEffectRuleStore store;
    const auto binding = magicBinding(21);
    store.append(binding, unrelatedRule);
    store.append(binding, firstRule);
    store.append(binding, secondRule);
    store.append(binding, incompatibleRule);
    const auto earliestPoisonOrder = store.rules()[1].order;
    BattleRuntimeRandom random(1);
    const auto provenance = attackProvenance(21);
    const auto context = makeContext(
        EffectEvent::HitBeforeDamage,
        magicBinding(21),
        owner,
        units,
        HitEventData{
            .provenance = provenance,
            .targetUnitId = enemy.id,
            .originalTargetUnitId = enemy.id,
            .contactPosition = enemy.position,
            .acceptedHit = true,
        });

    const auto dispatched = BattleEffectSystem{}.dispatch(store, context, random);

    REQUIRE(dispatched.commands.size() == 3);
    CHECK(std::holds_alternative<ChangeResourceEffectCommand>(
        dispatched.commands[0].value));
    CHECK(dispatched.commands[0].metadata.ruleId == unrelatedRule.id);

    const auto& aggregateCommand = dispatched.commands[1];
    CHECK(aggregateCommand.metadata.binding == binding);
    CHECK(aggregateCommand.metadata.ruleId == firstRule.id);
    CHECK(aggregateCommand.metadata.ruleOrder == earliestPoisonOrder);
    CHECK(aggregateCommand.metadata.authoredActionOrder == 0);
    CHECK(aggregateCommand.metadata.actionOrder == 0);
    CHECK(aggregateCommand.metadata.targetOrder == 0);
    CHECK(aggregateCommand.metadata.commandOrdinal == 1);
    CHECK(aggregateCommand.metadata.targetUnitId == enemy.id);
    CHECK(aggregateCommand.metadata.eventSourceUnitId == owner.id);
    CHECK(aggregateCommand.metadata.executionLane == EffectExecutionLane::Configured);
    CHECK_FALSE(aggregateCommand.metadata.statusContribution);
    const auto& aggregated = std::get<ApplyStatusEffectCommand>(
        aggregateCommand.value);
    CHECK(poisonDamagePercent(aggregated.behavior) == 18);
    CHECK(aggregated.durationFrames == 90);
    CHECK(aggregated.stacks == 3);
    CHECK(aggregated.poisonSameEventMerge == PoisonSameEventMerge::None);

    const auto& incompatibleCommand = dispatched.commands[2];
    CHECK(incompatibleCommand.metadata.ruleId == incompatibleRule.id);
    const auto& incompatible = std::get<ApplyStatusEffectCommand>(
        incompatibleCommand.value);
    CHECK(poisonDamagePercent(incompatible.behavior) == 13);
    CHECK(incompatible.poisonSameEventMerge == PoisonSameEventMerge::None);
}

TEST_CASE("BattleEffectSystem aggregates same-event poison across configured source kinds",
          "[battle][effect][status][poison][aggregation][merged]")
{
    const auto owner = makeUnit(1, 0, 1000, 1000);
    const auto enemy = makeUnit(2, 1, 1000, 1000);
    const std::vector units{ owner, enemy };

    const auto poisonApplication = [](int damagePercent, int triggerCount)
    {
        ApplyStatusAction poison;
        poison.status = BattleStatusKind::Poison;
        poison.durationFrames = 90;
        poison.quantity = SetStatusTriggerCharges{ triggerCount };
        poison.reapplication = StatusReapplicationPolicy::KeepHigherDamage;
        poison.poisonSameEventMerge = PoisonSameEventMerge::SumDamagePercent;
        poison.behavior = poisonStatusBehavior(damagePercent);
        return poison;
    };

    BattleEffectRuleStore store;
    store.append(
        EffectSourceBinding{
            .kind = EffectSourceKind::Combo,
            .sourceId = 21,
            .ownerUnitId = owner.id,
            .sourceTeam = owner.team,
        },
        makeRule(
            1,
            EffectEvent::HitBeforeDamage,
            hitTargetSelector(),
            { effectAction(poisonApplication(7, 3)) }));
    store.append(
        EffectSourceBinding{
            .kind = EffectSourceKind::Equipment,
            .sourceId = 31,
            .ownerUnitId = owner.id,
            .sourceTeam = owner.team,
        },
        makeRule(
            2,
            EffectEvent::HitBeforeDamage,
            hitTargetSelector(),
            { effectAction(poisonApplication(11, 5)) }));
    const auto context = makeContext(
        EffectEvent::HitBeforeDamage,
        magicBinding(21),
        owner,
        units,
        HitEventData{
            .provenance = attackProvenance(21),
            .targetUnitId = enemy.id,
            .originalTargetUnitId = enemy.id,
            .contactPosition = enemy.position,
            .acceptedHit = true,
        });
    BattleRuntimeRandom random(1);

    const auto dispatched = dispatchMergedFixture(
        store,
        context,
        random,
        {});

    REQUIRE(dispatched.commands.size() == 1);
    const auto& aggregated = std::get<ApplyStatusEffectCommand>(
        dispatched.commands.front().value);
    CHECK(poisonDamagePercent(aggregated.behavior) == 18);
    CHECK(aggregated.stacks == 5);
    CHECK(aggregated.poisonSameEventMerge == PoisonSameEventMerge::None);
}

TEST_CASE("BattleEffectSystem aggregates poison only when the complete extra behavior agrees",
          "[battle][effect][status][poison][aggregation][behavior]")
{
    const auto owner = makeUnit(1, 0, 1000, 1000);
    const auto enemy = makeUnit(2, 1, 1000, 1000);
    const std::vector units{ owner, enemy };
    const auto dispatch = [&](ApplyStatusAction first, ApplyStatusAction second)
    {
        BattleEffectRuleStore store;
        store.append(
            magicBinding(21),
            makeRule(
                1,
                EffectEvent::HitBeforeDamage,
                hitTargetSelector(),
                { effectAction(std::move(first)) }));
        store.append(
            magicBinding(21),
            makeRule(
                2,
                EffectEvent::HitBeforeDamage,
                hitTargetSelector(),
                { effectAction(std::move(second)) }));
        BattleRuntimeRandom random(1);
        const auto context = makeContext(
                EffectEvent::HitBeforeDamage,
                magicBinding(21),
                owner,
                units,
                HitEventData{
                    .provenance = attackProvenance(21),
                    .targetUnitId = enemy.id,
                    .originalTargetUnitId = enemy.id,
                    .contactPosition = enemy.position,
                    .acceptedHit = true,
                });
        return BattleEffectSystem{}.dispatch(store, context, random);
    };

    const auto compatible = dispatch(
        aggregatingPoisonWithDeathShield(7, 101, 5),
        aggregatingPoisonWithDeathShield(11, 202, 5));
    REQUIRE(compatible.commands.size() == 1);
    const auto& aggregated = std::get<ApplyStatusEffectCommand>(
        compatible.commands.front().value);
    CHECK(poisonDamagePercent(aggregated.behavior) == 18);
    REQUIRE(aggregated.behavior);
    CHECK(aggregated.behavior->rules.size() == 2);

    const auto incompatible = dispatch(
        aggregatingPoisonWithDeathShield(7, 101, 5),
        aggregatingPoisonWithDeathShield(11, 202, 6));
    REQUIRE(incompatible.commands.size() == 2);
    CHECK(poisonDamagePercent(std::get<ApplyStatusEffectCommand>(
        incompatible.commands[0].value).behavior) == 7);
    CHECK(poisonDamagePercent(std::get<ApplyStatusEffectCommand>(
        incompatible.commands[1].value).behavior) == 11);
}

TEST_CASE("BattleEffectSystem repetition shares one authored producer-family capacity",
          "[battle][effect][status][family][repetition]")
{
    const auto owner = makeUnit(1, 0, 1000, 1000);
    const auto target = makeUnit(2, 1, 1000, 1000);
    const std::vector units{ owner, target };
    const auto binding = magicBinding(106);

    ApplyStatusAction trueQi;
    trueQi.status = BattleStatusKind::TrueQi;
    trueQi.quantity = AddStatusLayers{ .count = 6, .limit = 10 };
    trueQi.behavior = trueQiStatusBehavior(9);
    auto repeatedRule = makeRule(
        1,
        EffectEvent::HitBeforeDamage,
        hitTargetSelector(),
        { effectAction(trueQi) });
    repeatedRule.repetitionCount = EffectNumber{ .flat = 2 };

    const auto dispatch = [&](EffectRule rule)
    {
        BattleEffectRuleStore store;
        store.append(binding, std::move(rule));
        BattleRuntimeRandom random(1);
        return BattleEffectSystem{}.dispatch(
            store,
            makeContext(
                EffectEvent::HitBeforeDamage,
                binding,
                owner,
                units,
                HitEventData{
                    .provenance = attackProvenance(106),
                    .targetUnitId = target.id,
                }),
            random);
    };
    const auto applyCommands = [&](const BattleEffectDispatchResult& dispatched)
    {
        BattleStatusUnitState state{
            .id = target.id,
            .alive = true,
            .hp = target.hp,
            .maxHp = target.maxHp,
        };
        for (const auto& command : dispatched.commands)
        {
            state = BattleEffectCommandSystem::applyStatusCommand(
                std::move(state),
                command.metadata,
                std::get<ApplyStatusEffectCommand>(command.value),
                { .frame = 10 },
                {},
                false).target;
        }
        return state;
    };

    const auto repeated = dispatch(repeatedRule);
    REQUIRE(repeated.commands.size() == 2);
    CHECK(repeated.commands[0].metadata.authoredActionOrder == 0);
    CHECK(repeated.commands[1].metadata.authoredActionOrder == 0);
    CHECK(repeated.commands[0].metadata.actionOrder == 0);
    CHECK(repeated.commands[1].metadata.actionOrder == 1);
    const auto repeatedState = applyCommands(repeated);
    REQUIRE(repeatedState.effects.statuses.size() == 1);
    CHECK(repeatedState.effects.statuses.front().stacks == 10);
    CHECK(repeatedState.effects.statuses.front().familyLocalLimit == 10);

    auto separateRule = repeatedRule;
    separateRule.repetitionCount.reset();
    separateRule.actions.push_back(effectAction(trueQi));
    const auto separate = dispatch(std::move(separateRule));
    REQUIRE(separate.commands.size() == 2);
    CHECK(separate.commands[0].metadata.authoredActionOrder == 0);
    CHECK(separate.commands[1].metadata.authoredActionOrder == 1);
    const auto separateState = applyCommands(separate);
    REQUIRE(separateState.effects.statuses.size() == 2);
    CHECK(separateState.effects.statuses[0].stacks == 6);
    CHECK(separateState.effects.statuses[1].stacks == 6);
}

TEST_CASE("BattleEffectSystem gives conditional leaves distinct producer identities",
          "[battle][effect][status][family][conditional]")
{
    const auto owner = makeUnit(1, 0, 1000, 1000);
    const auto target = makeUnit(2, 1, 1000, 1000);
    const std::vector units{ owner, target };
    const auto sourceBinding = magicBinding(106);

    ApplyStatusAction first;
    first.status = BattleStatusKind::TrueQi;
    first.quantity = AddStatusLayers{ .count = 6, .limit = 10 };
    first.behavior = trueQiStatusBehavior(9);
    auto second = first;
    second.behavior = trueQiStatusBehavior(12);
    auto conditional = std::make_shared<ConditionalEffectAction>();
    conditional->conditions = { TargetNotInvincibleCondition{} };
    conditional->whenTrue = {
        effectAction(std::move(first)),
        effectAction(std::move(second)),
    };
    auto rule = makeRule(
        1,
        EffectEvent::HitBeforeDamage,
        hitTargetSelector(),
        { effectAction(std::move(conditional)) });

    BattleEffectRuleStore store;
    store.append(sourceBinding, rule);
    BattleRuntimeRandom random(1);
    const auto dispatched = BattleEffectSystem{}.dispatch(
        store,
        makeContext(
            EffectEvent::HitBeforeDamage,
            sourceBinding,
            owner,
            units,
            HitEventData{
                .provenance = attackProvenance(106),
                .targetUnitId = target.id,
            }),
        random);

    REQUIRE(dispatched.commands.size() == 2);
    CHECK(dispatched.commands[0].metadata.authoredActionOrder == 0);
    CHECK(dispatched.commands[1].metadata.authoredActionOrder == 1);
    CHECK(dispatched.commands[0].metadata.actionOrder == 0);
    CHECK(dispatched.commands[1].metadata.actionOrder == 1);

    BattleStatusUnitState state{
        .id = target.id,
        .alive = true,
        .hp = target.hp,
        .maxHp = target.maxHp,
    };
    for (const auto& command : dispatched.commands)
    {
        state = BattleEffectCommandSystem::applyStatusCommand(
            std::move(state),
            command.metadata,
            std::get<ApplyStatusEffectCommand>(command.value),
            { .frame = 1 },
            {},
            false).target;
    }
    REQUIRE(state.effects.statuses.size() == 2);
    REQUIRE(state.effects.statuses[0].producerFamily);
    REQUIRE(state.effects.statuses[1].producerFamily);
    CHECK(state.effects.statuses[0].producerFamily
        != state.effects.statuses[1].producerFamily);
    CHECK(state.effects.statuses[0].stacks == 6);
    CHECK(state.effects.statuses[1].stacks == 6);
}

TEST_CASE("BattleEffectSystem refreshes contribution quantity after partial self-consumption",
          "[battle][effect][status][snapshot][liveness]")
{
    const auto owner = makeUnit(1, 0, 1000, 1000);
    const auto target = makeUnit(2, 1, 1000, 1000);
    const std::vector units{ owner, target };

    ConsumeThisStatusAction consume;
    consume.quantity = 1;
    ApplyStatusAction depleted;
    depleted.status = BattleStatusKind::Stun;
    depleted.duration = EffectNumber{ .base = EffectNumberBase::TargetMaxHp, .percent = 10 };
    depleted.reapplication = StatusReapplicationPolicy::KeepLongerDuration;
    consume.whenDepleted = depleted;
    auto consumingRule = makeRule(
        1,
        EffectEvent::HitBeforeDamage,
        EffectSelector{ .kind = EffectSelectorKind::StatusHolder },
        { effectAction(consume) });
    consumingRule.observation = EffectObservationScope::StatusHolderEventSource;

    ChangeResourceAction shield;
    shield.resource = BattleResource::Shield;
    shield.kind = ResourceChangeKind::Grant;
    shield.amount.base = EffectNumberBase::CurrentContributionQuantity;
    shield.amount.percent = 100;
    auto staleRule = makeRule(
        2,
        EffectEvent::HitBeforeDamage,
        EffectSelector{ .kind = EffectSelectorKind::StatusHolder },
        { effectAction(shield) });
    staleRule.observation = EffectObservationScope::StatusHolderEventSource;
    staleRule.maxActivations = 1;

    auto behavior = std::make_shared<StatusBehaviorDefinition>();
    behavior->rules = { consumingRule, staleRule };
    std::vector runtime(2, EffectRuleRuntimeState{});
    const auto active = statusBehaviorFixture({
        .binding = magicBinding(106),
        .producerRuleId = EffectRuleId{ 106 },
        .producerRuleOrder = 8,
        .holderUnitId = owner.id,
        .sourceUnitId = owner.id,
        .kind = BattleStatusKind::TrueQi,
        .quantity = 2,
        .appliedSequence = 9,
        .behavior = behavior,
        .runtime = &runtime,
    }, nullptr);
    const auto context = makeContext(
        EffectEvent::HitBeforeDamage,
        magicBinding(106),
        owner,
        units,
        HitEventData{
            .provenance = attackProvenance(106),
            .targetUnitId = target.id,
        });
    BattleRuntimeRandom random(77);

    const auto dispatched = dispatchStatusFixture(
        context,
        random,
        std::span{ &active, std::size_t{ 1 } });

    REQUIRE(dispatched.commands.size() == 2);
    CHECK(std::holds_alternative<ConsumeThisStatusEffectCommand>(
        dispatched.commands.front().value));
    const auto& consumption = std::get<ConsumeThisStatusEffectCommand>(
        dispatched.commands.front().value);
    CHECK(consumption.request.filter.holderUnitId == owner.id);
    CHECK(consumption.request.filter.appliedSequence == 9);
    CHECK(consumption.request.stacks == 1);
    REQUIRE(consumption.whenDepleted);
    CHECK(consumption.whenDepleted->durationFrames == 100);
    const auto& shieldCommand = std::get<ChangeResourceEffectCommand>(
        dispatched.commands.back().value);
    CHECK(shieldCommand.resolvedAmount() == 1);
    CHECK(random.rawDrawCount() == 0);
    CHECK(runtime[0].activationCount == 1);
    CHECK(runtime[1].activationCount == 1);
}

TEST_CASE("BattleEffectSystem refreshes contribution quantity after a compatible application",
          "[battle][effect][status][snapshot][liveness][apply]")
{
    const auto owner = makeUnit(1, 0, 1000, 1000);
    const auto enemy = makeUnit(2, 1, 1000, 1000);
    const std::vector units{ owner, enemy };
    const auto binding = magicBinding(106);

    DealDamageAction scaledDamage;
    scaledDamage.amount.flat = 10;
    scaledDamage.amount.statusScale = StatusNumberScale::PerContributionLayer;
    scaledDamage.kind = BattleDamageKind::Pure;
    auto behaviorRule = makeRule(
        2,
        EffectEvent::HitBeforeDamage,
        hitTargetSelector(),
        { effectAction(scaledDamage) });
    behaviorRule.observation = EffectObservationScope::StatusHolderEventSource;
    auto behavior = std::make_shared<StatusBehaviorDefinition>();
    behavior->rules = { behaviorRule };

    ApplyStatusAction addLayer;
    addLayer.status = BattleStatusKind::TrueQi;
    addLayer.quantity = AddStatusLayers{ .count = 1, .limit = 10 };
    addLayer.behavior = behavior;
    auto producer = makeRule(
        1,
        EffectEvent::HitBeforeDamage,
        selfSelector(),
        { effectAction(addLayer) });

    BattleEffectRuleStore store;
    store.append(binding, producer);
    const auto producerOrder = store.rules().front().order;
    const StatusProducerKey producerKey{ binding, producer.id, 0 };
    const StatusProducerFamilyKey familyKey{
        binding.kind,
        binding.sourceId,
        binding.ownerUnitId,
        producer.id,
        0,
    };
    std::vector runtime(1, EffectRuleRuntimeState{});
    BattleStatusEffectState holderEffects;
    holderEffects.statuses = {
        {
            .kind = BattleStatusKind::TrueQi,
            .producer = producerKey,
            .producerFamily = familyKey,
            .familyLocalLimit = 10,
            .behavior = behavior,
            .behaviorRuntime = runtime,
            .sourceUnitId = owner.id,
            .stacks = 2,
            .origin = BattleStatusEffectOrigin{ binding, producer.id, producerOrder },
            .appliedSequence = 1,
        },
    };
    const auto active = statusBehaviorFixture({
        .binding = binding,
        .producerRuleId = producer.id,
        .producerRuleOrder = producerOrder,
        .producerActionOrder = 0,
        .holderUnitId = owner.id,
        .sourceUnitId = owner.id,
        .kind = BattleStatusKind::TrueQi,
        .quantity = 2,
        .appliedSequence = 1,

        .behavior = behavior,
        .runtime = &runtime,
    }, &holderEffects);
    const auto context = makeContext(
        EffectEvent::HitBeforeDamage,
        binding,
        owner,
        units,
        HitEventData{
            .provenance = attackProvenance(106),
            .targetUnitId = enemy.id,
        });
    BattleRuntimeRandom random(1);

    const auto dispatched = dispatchMergedFixture(
        store,
        context,
        random,
        std::span{ &active, std::size_t{ 1 } });

    REQUIRE(dispatched.commands.size() == 2);
    CHECK(std::holds_alternative<ApplyStatusEffectCommand>(
        dispatched.commands.front().value));
    const auto& damage = std::get<DealDamageEffectCommand>(
        dispatched.commands.back().value);
    CHECK(damage.amount == 30);
    CHECK(runtime.front().activationCount == 1);
}

TEST_CASE("BattleEffectSystem replacement invalidates the old generation until the next event",
          "[battle][effect][status][snapshot][liveness][replace]")
{
    const auto owner = makeUnit(1, 0, 1000, 1000);
    const auto holder = makeUnit(2, 1, 1000, 1000);
    const std::vector units{ owner, holder };

    ChangeResourceAction staleShield;
    staleShield.resource = BattleResource::Shield;
    staleShield.kind = ResourceChangeKind::Grant;
    staleShield.amount.flat = 99;
    auto staleRule = makeRule(
        2,
        EffectEvent::HitBeforeDamage,
        EffectSelector{ .kind = EffectSelectorKind::StatusHolder },
        { effectAction(staleShield) });
    staleRule.observation = EffectObservationScope::StatusHolderEventTarget;
    staleRule.chancePct = 50;
    auto oldBehavior = std::make_shared<StatusBehaviorDefinition>(
        *poisonStatusBehavior(5));
    oldBehavior->rules.push_back(staleRule);

    ChangeResourceAction replacementShield = staleShield;
    replacementShield.amount.flat = 123;
    auto replacementRule = staleRule;
    replacementRule.id = EffectRuleId{ 3 };
    replacementRule.chancePct = 100;
    replacementRule.actions = { effectAction(replacementShield) };
    auto newBehavior = std::make_shared<StatusBehaviorDefinition>(
        *poisonStatusBehavior(10));
    newBehavior->rules.push_back(replacementRule);

    ApplyStatusAction replace;
    replace.status = BattleStatusKind::Poison;
    replace.durationFrames = 90;
    replace.quantity = SetStatusTriggerCharges{ 3 };
    replace.reapplication = StatusReapplicationPolicy::KeepHigherDamage;
    replace.poisonSameEventMerge = PoisonSameEventMerge::SumDamagePercent;
    replace.behavior = newBehavior;
    auto configuredRule = makeRule(
        1,
        EffectEvent::HitBeforeDamage,
        hitTargetSelector(),
        { effectAction(replace) });

    const EffectSourceBinding configuredBinding{
        .kind = EffectSourceKind::Combo,
        .sourceId = 1,
        .ownerUnitId = owner.id,
        .sourceTeam = owner.team,
    };
    const auto oldBinding = magicBinding(90);
    BattleEffectRuleStore store;
    store.append(configuredBinding, configuredRule);
    std::vector oldRuntime(oldBehavior->rules.size(), EffectRuleRuntimeState{});
    BattleStatusEffectState holderEffects;
    holderEffects.nextStatusSequence = 2;
    holderEffects.statuses = {
        {
            .kind = BattleStatusKind::Poison,
            .producer = StatusProducerKey{ oldBinding, EffectRuleId{ 90 }, 0 },
            .producerFamily = StatusProducerFamilyKey{
                oldBinding.kind,
                oldBinding.sourceId,
                oldBinding.ownerUnitId,
                EffectRuleId{ 90 },
                0,
            },
            .behavior = oldBehavior,
            .behaviorRuntime = oldRuntime,
            .sourceUnitId = owner.id,
            .remainingFrames = 90,
            .maximumFrames = 90,
            .stacks = 3,
            .origin = BattleStatusEffectOrigin{ oldBinding, EffectRuleId{ 90 }, 10 },
            .appliedSequence = 1,
        },
    };
    const auto oldActive = statusBehaviorFixture({
        .binding = oldBinding,
        .producerRuleId = EffectRuleId{ 90 },
        .producerRuleOrder = 10,
        .holderUnitId = holder.id,
        .sourceUnitId = owner.id,
        .kind = BattleStatusKind::Poison,
        .quantity = 3,
        .appliedSequence = 1,

        .behavior = oldBehavior,
        .runtime = &oldRuntime,
    }, &holderEffects);
    const auto context = makeContext(
        EffectEvent::HitBeforeDamage,
        configuredBinding,
        owner,
        units,
        HitEventData{
            .provenance = attackProvenance(90),
            .targetUnitId = holder.id,
        });
    BattleRuntimeRandom random(9);

    const auto first = dispatchMergedFixture(
        store,
        context,
        random,
        std::span{ &oldActive, std::size_t{ 1 } });
    REQUIRE(first.commands.size() == 1);
    CHECK(std::holds_alternative<ApplyStatusEffectCommand>(
        first.commands.front().value));
    CHECK(random.rawDrawCount() == 0);
    CHECK(oldRuntime.back() == EffectRuleRuntimeState{});

    std::vector newRuntime(newBehavior->rules.size(), EffectRuleRuntimeState{});
    const auto newActive = statusBehaviorFixture({
        .binding = configuredBinding,
        .producerRuleId = configuredRule.id,
        .producerRuleOrder = store.rules().front().order,
        .holderUnitId = holder.id,
        .sourceUnitId = owner.id,
        .kind = BattleStatusKind::Poison,
        .quantity = 3,
        .appliedSequence = 2,
        .behavior = newBehavior,
        .runtime = &newRuntime,
    }, nullptr);
    BattleEffectRuleStore emptyStore;
    const auto next = dispatchMergedFixture(
        emptyStore,
        context,
        random,
        std::span{ &newActive, std::size_t{ 1 } });
    REQUIRE(next.commands.size() == 1);
    const auto& shield = std::get<ChangeResourceEffectCommand>(
        next.commands.front().value);
    CHECK(shield.resolvedAmount() == 123);
}

TEST_CASE("BattleEffectSystem orders holder-local contribution sequences explicitly",
          "[battle][effect][status][ordering][holder]")
{
    const auto owner = makeUnit(1, 0, 1000, 1000);
    const auto firstHolder = makeUnit(2, 0, 1000, 1000);
    const auto secondHolder = makeUnit(3, 0, 1000, 1000);
    const auto enemy = makeUnit(4, 1, 1000, 1000);
    const std::vector units{ owner, firstHolder, secondHolder, enemy };

    ChangeResourceAction shield;
    shield.resource = BattleResource::Shield;
    shield.kind = ResourceChangeKind::Grant;
    shield.amount.flat = 1;
    auto rule = makeRule(
        1,
        EffectEvent::HitBeforeDamage,
        EffectSelector{ .kind = EffectSelectorKind::StatusHolder },
        { effectAction(shield) });
    rule.observation = EffectObservationScope::StatusSourceEventSource;
    auto behavior = std::make_shared<StatusBehaviorDefinition>();
    behavior->rules = { rule };
    std::vector firstRuntime(1, EffectRuleRuntimeState{});
    std::vector secondRuntime(1, EffectRuleRuntimeState{});
    const std::array active{
        statusBehaviorFixture({
            .binding = magicBinding(106),
            .producerRuleId = EffectRuleId{ 106 },
            .producerRuleOrder = 8,
            .producerActionOrder = 2,
            .holderUnitId = secondHolder.id,
            .sourceUnitId = owner.id,
            .kind = BattleStatusKind::Shadowless,
            .quantity = 1,
            .appliedSequence = 1,
            .behavior = behavior,
            .runtime = &secondRuntime,
        }, nullptr),
        statusBehaviorFixture({
            .binding = magicBinding(106),
            .producerRuleId = EffectRuleId{ 106 },
            .producerRuleOrder = 8,
            .producerActionOrder = 2,
            .holderUnitId = firstHolder.id,
            .sourceUnitId = owner.id,
            .kind = BattleStatusKind::Shadowless,
            .quantity = 1,
            .appliedSequence = 1,
            .behavior = behavior,
            .runtime = &firstRuntime,
        }, nullptr),
    };
    const auto context = makeContext(
        EffectEvent::HitBeforeDamage,
        magicBinding(106),
        owner,
        units,
        HitEventData{
            .provenance = attackProvenance(106),
            .targetUnitId = enemy.id,
        });
    BattleRuntimeRandom random(1);

    const auto dispatched = dispatchStatusFixture(
        context,
        random,
        active);

    REQUIRE(dispatched.commands.size() == 2);
    CHECK(dispatched.commands[0].metadata.targetUnitId == firstHolder.id);
    CHECK(dispatched.commands[1].metadata.targetUnitId == secondHolder.id);
    CHECK(effectExecutionOrderKey(dispatched.commands[0].metadata)
        < effectExecutionOrderKey(dispatched.commands[1].metadata));
}

TEST_CASE("BattleEffectSystem merges frame rules before ticking and skips dead owners and holders",
          "[battle][effect][status][frame][ordering][liveness]")
{
    const auto owner = makeUnit(1, 0, 1000, 1000);
    auto deadHolder = makeUnit(2, 1, 0, 1000);
    deadHolder.alive = false;
    const auto liveHolder = makeUnit(3, 1, 1000, 1000);
    const std::vector units{ owner, deadHolder, liveHolder };

    ChangeResourceAction shield;
    shield.resource = BattleResource::Shield;
    shield.kind = ResourceChangeKind::Grant;
    shield.amount.flat = 50;
    auto periodicRule = makeRule(
        2,
        EffectEvent::FrameAdvanced,
        EffectSelector{ .kind = EffectSelectorKind::StatusHolder },
        { effectAction(shield) });
    periodicRule.observation = EffectObservationScope::StatusHolderEventSource;
    periodicRule.chancePct = 50;
    auto behavior = std::make_shared<StatusBehaviorDefinition>();
    behavior->rules = { periodicRule };

    const auto liveBinding = magicBinding(106);
    BattleStatusEffectState liveEffects;
    liveEffects.statuses = {
        {
            .kind = BattleStatusKind::TrueQi,
            .behavior = behavior,
            .behaviorRuntime = { EffectRuleRuntimeState{} },
            .sourceUnitId = owner.id,
            .stacks = 1,
            .appliedSequence = 1,
        },
    };
    std::vector liveRuntime(1, EffectRuleRuntimeState{});
    const auto liveActive = statusBehaviorFixture({
        .binding = liveBinding,
        .producerRuleId = EffectRuleId{ 20 },
        .producerRuleOrder = 1,
        .holderUnitId = liveHolder.id,
        .sourceUnitId = owner.id,
        .kind = BattleStatusKind::TrueQi,
        .quantity = 1,
        .appliedSequence = 1,

        .behavior = behavior,
        .runtime = &liveRuntime,
    }, &liveEffects);

    RemoveStatusAction remove;
    remove.statuses = { BattleStatusKind::TrueQi };
    auto removeRule = makeRule(
        1,
        EffectEvent::FrameAdvanced,
        selfSelector(),
        { effectAction(remove) });
    const EffectSourceBinding liveHolderBinding{
        .kind = EffectSourceKind::Combo,
        .sourceId = 1,
        .ownerUnitId = liveHolder.id,
        .sourceTeam = liveHolder.team,
    };
    BattleEffectRuleStore store;
    store.append(liveHolderBinding, removeRule);

    auto deadOwnerRule = removeRule;
    deadOwnerRule.id = EffectRuleId{ 3 };
    const EffectSourceBinding deadHolderBinding{
        .kind = EffectSourceKind::Combo,
        .sourceId = 2,
        .ownerUnitId = deadHolder.id,
        .sourceTeam = deadHolder.team,
    };
    store.append(deadHolderBinding, deadOwnerRule);

    BattleStatusEffectState deadEffects = liveEffects;
    std::vector deadRuntime(1, EffectRuleRuntimeState{});
    const auto deadActive = statusBehaviorFixture({
        .binding = liveBinding,
        .producerRuleId = EffectRuleId{ 21 },
        .producerRuleOrder = 2,
        .holderUnitId = deadHolder.id,
        .sourceUnitId = owner.id,
        .kind = BattleStatusKind::TrueQi,
        .quantity = 1,
        .appliedSequence = 1,

        .behavior = behavior,
        .runtime = &deadRuntime,
    }, &deadEffects);
    const std::array active{ liveActive, deadActive };
    const auto context = makeContext(
        EffectEvent::FrameAdvanced,
        magicBinding(),
        owner,
        units,
        FrameTickEventData{ .deltaFrames = 1, .periodOrdinal = 1 });
    BattleRuntimeRandom random(1);

    const auto dispatched = dispatchMergedFixture(
        store,
        context,
        random,
        active,
        StatusBehaviorDispatchFilter::All,
        true);

    REQUIRE(dispatched.commands.size() == 1);
    CHECK(std::holds_alternative<RemoveStatusEffectCommand>(
        dispatched.commands.front().value));
    CHECK(dispatched.commands.front().metadata.binding.ownerUnitId == liveHolder.id);
    CHECK(random.rawDrawCount() == 0);
    CHECK(liveRuntime.front() == EffectRuleRuntimeState{});
    CHECK(deadRuntime.front() == EffectRuleRuntimeState{});
}

TEST_CASE("BattleEffectSystem same-name status presence cannot keep another producer behavior live",
          "[battle][effect][status][producer][liveness][isolation]")
{
    const auto holder = makeUnit(1, 0, 1000, 1000);
    const auto enemy = makeUnit(2, 1, 1000, 1000);
    const std::vector units{ holder, enemy };

    const auto behaviorGrantingShield = [](std::uint64_t ruleId, int amount)
    {
        ChangeResourceAction shield;
        shield.resource = BattleResource::Shield;
        shield.kind = ResourceChangeKind::Grant;
        shield.amount.flat = amount;
        auto rule = makeRule(
            ruleId,
            EffectEvent::HitBeforeDamage,
            EffectSelector{ .kind = EffectSelectorKind::StatusHolder },
            { effectAction(shield) });
        rule.observation = EffectObservationScope::StatusHolderEventSource;
        auto behavior = std::make_shared<StatusBehaviorDefinition>();
        behavior->rules = { std::move(rule) };
        return behavior;
    };
    const auto expiredBehavior = behaviorGrantingShield(1, 11);
    const auto liveBehavior = behaviorGrantingShield(2, 22);
    std::vector expiredRuntime{ EffectRuleRuntimeState{} };
    std::vector liveRuntime{ EffectRuleRuntimeState{} };
    BattleStatusEffectState holderEffects;
    holderEffects.statuses = {
        {
            .kind = BattleStatusKind::Shadowless,
            .behavior = liveBehavior,
            .behaviorRuntime = liveRuntime,
            .sourceUnitId = holder.id,
            .stacks = 1,
            .appliedSequence = 2,
        },
    };
    const std::array active{
        statusBehaviorFixture({
            .binding = magicBinding(105),
            .producerRuleId = EffectRuleId{ 1 },
            .producerRuleOrder = 1,
            .holderUnitId = holder.id,
            .sourceUnitId = holder.id,
            .kind = BattleStatusKind::Shadowless,
            .quantity = 1,
            .appliedSequence = 1,

            .behavior = expiredBehavior,
            .runtime = &expiredRuntime,
        }, &holderEffects),
        statusBehaviorFixture({
            .binding = magicBinding(105),
            .producerRuleId = EffectRuleId{ 2 },
            .producerRuleOrder = 2,
            .holderUnitId = holder.id,
            .sourceUnitId = holder.id,
            .kind = BattleStatusKind::Shadowless,
            .quantity = 1,
            .appliedSequence = 2,

            .behavior = liveBehavior,
            .runtime = &liveRuntime,
        }, &holderEffects),
    };
    const auto context = makeContext(
        EffectEvent::HitBeforeDamage,
        magicBinding(105),
        holder,
        units,
        HitEventData{
            .provenance = attackProvenance(105),
            .targetUnitId = enemy.id,
            .originalTargetUnitId = enemy.id,
            .damageKind = BattleDamageKind::Skill,
        });
    BattleRuntimeRandom random(1);

    const auto dispatched = dispatchStatusFixture(
        context, random, active);

    REQUIRE(dispatched.commands.size() == 1);
    const auto& shield = std::get<ChangeResourceEffectCommand>(
        dispatched.commands.front().value);
    CHECK(shield.resolvedAmount() == 22);
    CHECK(expiredRuntime.front() == EffectRuleRuntimeState{});
    CHECK(liveRuntime.front().activationCount == 1);
}

TEST_CASE("BattleEffectSystem skips a later contribution removed by an earlier status rule",
          "[battle][effect][status][snapshot][liveness][remove][random]")
{
    const auto owner = makeUnit(1, 0, 1000, 1000);
    const auto holder = makeUnit(2, 1, 1000, 1000);
    const std::vector units{ owner, holder };

    RemoveStatusAction remove;
    remove.statuses = { BattleStatusKind::TrueQi };
    auto removingRule = makeRule(
        1,
        EffectEvent::HitBeforeDamage,
        EffectSelector{ .kind = EffectSelectorKind::StatusHolder },
        { effectAction(remove) });
    removingRule.observation = EffectObservationScope::StatusHolderEventTarget;

    ChangeResourceAction shield;
    shield.resource = BattleResource::Shield;
    shield.kind = ResourceChangeKind::Grant;
    shield.amount.flat = 99;
    auto staleRule = makeRule(
        2,
        EffectEvent::HitBeforeDamage,
        EffectSelector{ .kind = EffectSelectorKind::StatusHolder },
        { effectAction(shield) });
    staleRule.observation = EffectObservationScope::StatusHolderEventTarget;
    staleRule.chancePct = 50;
    staleRule.maxActivations = 1;

    auto removingBehavior = std::make_shared<StatusBehaviorDefinition>();
    removingBehavior->rules = { removingRule };
    auto staleBehavior = std::make_shared<StatusBehaviorDefinition>();
    staleBehavior->rules = { staleRule };
    std::vector removingRuntime(1, EffectRuleRuntimeState{});
    std::vector staleRuntime(1, EffectRuleRuntimeState{});
    BattleStatusEffectState holderEffects;
    holderEffects.statuses = {
        { .kind = BattleStatusKind::Shadowless, .behavior = removingBehavior,
          .behaviorRuntime = removingRuntime, .sourceUnitId = owner.id,
          .stacks = 1, .appliedSequence = 1 },
        { .kind = BattleStatusKind::TrueQi, .behavior = staleBehavior,
          .behaviorRuntime = staleRuntime, .sourceUnitId = owner.id,
          .stacks = 2, .appliedSequence = 2 },
    };
    const std::array active{
        statusBehaviorFixture({
            .binding = magicBinding(106), .producerRuleId = { 106 },
            .producerRuleOrder = 1, .holderUnitId = holder.id,
            .sourceUnitId = owner.id, .kind = BattleStatusKind::Shadowless,
            .quantity = 1, .appliedSequence = 1,
            .behavior = removingBehavior, .runtime = &removingRuntime,
        }, &holderEffects),
        statusBehaviorFixture({
            .binding = magicBinding(106), .producerRuleId = { 107 },
            .producerRuleOrder = 2, .holderUnitId = holder.id,
            .sourceUnitId = owner.id, .kind = BattleStatusKind::TrueQi,
            .quantity = 2, .appliedSequence = 2,
            .behavior = staleBehavior, .runtime = &staleRuntime,
        }, &holderEffects),
    };
    const auto context = makeContext(
        EffectEvent::HitBeforeDamage,
        magicBinding(106),
        owner,
        units,
        HitEventData{
            .provenance = attackProvenance(106),
            .targetUnitId = holder.id,
        });
    BattleRuntimeRandom random(88);

    const auto dispatched = dispatchStatusFixture(
        context, random, active);

    REQUIRE(dispatched.commands.size() == 1);
    CHECK(std::holds_alternative<RemoveStatusEffectCommand>(
        dispatched.commands.front().value));
    CHECK(random.rawDrawCount() == 0);
    CHECK(staleRuntime.front() == EffectRuleRuntimeState{});
}

TEST_CASE("BattleEffectSystem does not evaluate losing attack interceptors",
          "[battle][effect][status][interceptor][ordering][random]")
{
    const auto owner = makeUnit(1, 0, 1000, 1000);
    const auto holder = makeUnit(2, 1, 1000, 1000);
    const std::vector units{ owner, holder };

    auto winnerRule = makeRule(
        1,
        EffectEvent::HitBeforeDamage,
        EffectSelector{ .kind = EffectSelectorKind::StatusHolder },
        { effectAction(MakeIncomingAttackMissAction{}) });
    winnerRule.observation = EffectObservationScope::StatusHolderEventTarget;
    auto loserRule = winnerRule;
    loserRule.id = EffectRuleId{ 2 };
    loserRule.chancePct = 50;
    loserRule.maxActivations = 1;
    auto winnerBehavior = std::make_shared<StatusBehaviorDefinition>();
    winnerBehavior->rules = { winnerRule };
    auto loserBehavior = std::make_shared<StatusBehaviorDefinition>();
    loserBehavior->rules = { loserRule };
    std::vector winnerRuntime(1, EffectRuleRuntimeState{});
    std::vector loserRuntime(1, EffectRuleRuntimeState{});
    BattleStatusEffectState holderEffects;
    holderEffects.statuses = {
        { .kind = BattleStatusKind::NextAttackMiss, .behavior = winnerBehavior,
          .behaviorRuntime = winnerRuntime, .sourceUnitId = owner.id,
          .stacks = 1, .appliedSequence = 1 },
        { .kind = BattleStatusKind::Blinded, .behavior = loserBehavior,
          .behaviorRuntime = loserRuntime, .sourceUnitId = owner.id,
          .stacks = 1, .appliedSequence = 2 },
    };
    const std::array active{
        statusBehaviorFixture({
            .binding = magicBinding(106), .producerRuleId = { 106 },
            .producerRuleOrder = 1, .holderUnitId = holder.id,
            .sourceUnitId = owner.id, .kind = BattleStatusKind::NextAttackMiss,
            .quantity = 1, .appliedSequence = 1,
            .behavior = winnerBehavior, .runtime = &winnerRuntime,
        }, &holderEffects),
        statusBehaviorFixture({
            .binding = magicBinding(106), .producerRuleId = { 107 },
            .producerRuleOrder = 2, .holderUnitId = holder.id,
            .sourceUnitId = owner.id, .kind = BattleStatusKind::Blinded,
            .quantity = 1, .appliedSequence = 2,
            .behavior = loserBehavior, .runtime = &loserRuntime,
        }, &holderEffects),
    };
    const auto context = makeContext(
        EffectEvent::HitBeforeDamage,
        magicBinding(106),
        owner,
        units,
        HitEventData{
            .provenance = attackProvenance(106),
            .targetUnitId = holder.id,
        });
    BattleRuntimeRandom random(99);

    const auto dispatched = dispatchStatusFixture(
        context,
        random,
        active,
        StatusBehaviorDispatchFilter::AttackInterceptorsOnly);

    REQUIRE(dispatched.commands.size() == 1);
    CHECK(std::holds_alternative<MakeIncomingAttackMissEffectCommand>(
        dispatched.commands.front().value));
    CHECK(random.rawDrawCount() == 0);
    CHECK(winnerRuntime.front().activationCount == 1);
    CHECK(loserRuntime.front() == EffectRuleRuntimeState{});
}

TEST_CASE("BattleEffectSystem saturates same-event poison aggregation",
          "[battle][effect][status][poison][aggregation][boundary]")
{
    const auto owner = makeUnit(1, 0, 1000, 1000);
    const auto enemy = makeUnit(2, 1, 1000, 1000);
    const std::vector units{ owner, enemy };

    ApplyStatusAction poison;
    poison.status = BattleStatusKind::Poison;
    poison.durationFrames = 90;
    poison.quantity = SetStatusTriggerCharges{ 3 };
    poison.reapplication = StatusReapplicationPolicy::KeepHigherDamage;
    poison.poisonSameEventMerge = PoisonSameEventMerge::SumDamagePercent;
    poison.behavior = poisonStatusBehavior(std::numeric_limits<int>::max());
    const auto firstRule = makeRule(
        1,
        EffectEvent::HitBeforeDamage,
        hitTargetSelector(),
        { effectAction(poison) });
    const auto secondRule = makeRule(
        2,
        EffectEvent::HitBeforeDamage,
        hitTargetSelector(),
        { effectAction(poison) });

    BattleEffectRuleStore store;
    store.append(magicBinding(21), firstRule);
    store.append(magicBinding(21), secondRule);
    BattleRuntimeRandom random(1);
    const auto provenance = attackProvenance(21);
    const auto context = makeContext(
        EffectEvent::HitBeforeDamage,
        magicBinding(21),
        owner,
        units,
        HitEventData{
            .provenance = provenance,
            .targetUnitId = enemy.id,
            .originalTargetUnitId = enemy.id,
            .contactPosition = enemy.position,
            .acceptedHit = true,
        });

    const auto dispatched = BattleEffectSystem{}.dispatch(store, context, random);

    REQUIRE(dispatched.commands.size() == 1);
    const auto& aggregated = std::get<ApplyStatusEffectCommand>(
        dispatched.commands.front().value);
    CHECK(poisonDamagePercent(aggregated.behavior)
        == std::numeric_limits<int>::max());
}

TEST_CASE("BattleEffectSystem evaluates formulas and deterministic selectors", "[battle][effect]")
{
    auto owner = makeUnit(1, 0, 700, 1000, 40, 100, Pointf{ 0.0f, 0.0f });
    owner.star = 3;
    owner.attack = 240;
    auto ally2 = makeUnit(2, 0, 200, 1000, 50, 100, Pointf{ 2.0f, 0.0f });
    ally2.shield = 500;
    const auto ally3 = makeUnit(3, 0, 100, 500, 10, 100, Pointf{ 1.0f, 0.0f });
    const auto enemy4 = makeUnit(4, 1, 900, 1000, 80, 100, Pointf{ 5.0f, 0.0f });
    const auto enemy5 = makeUnit(5, 1, 900, 1000, 80, 100, Pointf{ 3.0f, 0.0f });
    const auto enemy6 = makeUnit(6, 1, 900, 1000, 80, 100, Pointf{ -5.0f, 0.0f });
    const std::vector units{ owner, ally2, ally3, enemy4, enemy5, enemy6 };
    auto context = makeContext(
        EffectEvent::UltimateCommitted,
        magicBinding(),
        owner,
        units,
        CastCommitEventData{ .provenance = castProvenance(59), .targetUnitId = 4 });

    BattleRuntimeRandom random(1);
    EffectSelector lowest;
    lowest.kind = EffectSelectorKind::LowestHpAllies;
    lowest.count = 2;
    CHECK(BattleEffectSystem::selectTargets(lowest, context, random) == std::vector{ 2, 3 });

    EffectSelector nearest;
    nearest.kind = EffectSelectorKind::NearestEnemies;
    nearest.count = 2;
    CHECK(BattleEffectSystem::selectTargets(nearest, context, random) == std::vector{ 5, 4 });

    EffectSelector highestMp;
    highestMp.kind = EffectSelectorKind::HighestMpEnemy;
    CHECK(BattleEffectSystem::selectTargets(highestMp, context, random) == std::vector{ 4 });

    EffectSelector farthest;
    farthest.kind = EffectSelectorKind::FarthestEnemy;
    CHECK(BattleEffectSystem::selectTargets(farthest, context, random) == std::vector{ 4 });

    EffectNumber scissor;
    scissor.base = EffectNumberBase::TargetCurrentShield;
    scissor.multiplierBase = EffectNumberBase::SourceStar;
    scissor.percent = 20;
    CHECK(BattleEffectSystem::evaluateNumber(scissor, context, ally2) == 300);

    auto woundedOwner = owner;
    woundedOwner.attack = 3;
    woundedOwner.hp = 1;
    woundedOwner.maxHp = 100;
    const std::vector woundedUnits{ woundedOwner, ally2 };
    const auto woundedContext = makeContext(
        EffectEvent::UltimateCommitted,
        magicBinding(25),
        woundedOwner,
        woundedUnits,
        CastCommitEventData{ .provenance = castProvenance(25), .targetUnitId = ally2.id });
    EffectNumber missingHpAttack;
    missingHpAttack.base = EffectNumberBase::SourceAttack;
    missingHpAttack.multiplierBase = EffectNumberBase::SourceMissingHpRatio;
    missingHpAttack.percent = 45;
    CHECK(BattleEffectSystem::evaluateNumber(
        missingHpAttack,
        woundedContext,
        ally2) == 1);
    missingHpAttack.rounding = EffectRounding::Ceil;
    CHECK(BattleEffectSystem::evaluateNumber(
        missingHpAttack,
        woundedContext,
        ally2) == 2);

    EffectNumber composite;
    composite.base = EffectNumberBase::TargetMaxHp;
    composite.flat = 60;
    composite.percent = 7;
    CHECK(BattleEffectSystem::evaluateNumber(composite, context, ally2) == 130);

    EffectNumber negative;
    negative.base = EffectNumberBase::TargetMaxHp;
    negative.percent = -10;
    negative.rounding = EffectRounding::Floor;
    const auto oddTarget = makeUnit(9, 1, 333, 333);
    CHECK(BattleEffectSystem::evaluateNumber(negative, context, oddTarget) == -34);
}

TEST_CASE("BattleEffectSystem safely rounds saturated bound ratios consistently",
          "[battle][effect][number][rounding]")
{
    const auto owner = makeUnit(1, 0, 1000, 1000);
    const auto target = makeUnit(2, 1, 1000, 1000);
    const std::vector units{ owner, target };
    const auto context = makeContext(
        EffectEvent::UltimateCommitted,
        magicBinding(),
        owner,
        units,
        CastCommitEventData{ .provenance = castProvenance(59), .targetUnitId = target.id });

    EffectNumber number;
    number.base = EffectNumberBase::BoundRatio;
    number.boundNumerator = std::numeric_limits<std::int64_t>::max() / 200 + 1;
    number.boundDenominator = std::numeric_limits<std::int64_t>::max();
    number.percent = 100;
    number.rounding = EffectRounding::Nearest;

    const auto runtimeValue = BattleEffectSystem::evaluateNumber(number, context, target);
    const auto constantValue = effectiveConstantEffectNumberValue(number);
    REQUIRE(constantValue);
    CHECK(runtimeValue == 1);
    CHECK(*constantValue == runtimeValue);

    number.boundNumerator = -number.boundNumerator;
    const auto negativeRuntimeValue = BattleEffectSystem::evaluateNumber(number, context, target);
    const auto negativeConstantValue = effectiveConstantEffectNumberValue(number);
    REQUIRE(negativeConstantValue);
    CHECK(negativeRuntimeValue == -1);
    CHECK(*negativeConstantValue == negativeRuntimeValue);
}

TEST_CASE("BattleEffectSystem numeric catalog rows agree with runtime evaluation",
          "[battle][effect][number][catalog]")
{
    auto owner = makeUnit(1, 0, 400, 1000, 30, 80);
    owner.star = 3;
    owner.attack = 240;
    owner.statusDetails.push_back({
        .state = BattleStatusKind::TrueQi,
        .sourceUnitId = owner.id,
        .stacks = 6,
    });
    const auto target = [&]
    {
        auto result = makeUnit(2, 1, 123, 777, 20, 100);
        result.shield = 222;
        result.activeCooldown = 33;
        return result;
    }();
    const std::vector units{ owner, target };
    auto context = makeContext(
        EffectEvent::DamageResolved,
        magicBinding(),
        owner,
        units,
        DamageResultEventData{ .finalHpDamage = 17 });
    context.header.formulaInputs.storedStateValue = 321;
    context.header.statusContribution = EffectStatusContributionContext{
        .holderUnitId = target.id,
        .sourceUnitId = owner.id,
        .kind = BattleStatusKind::TrueQi,
        .quantity = 4,
    };

    struct Case
    {
        EffectNumberBase base;
        int expected;
        std::optional<BattleStatusKind> status;
        bool boundRatio = false;
    };
    const std::array cases{
        Case{ EffectNumberBase::Constant, 0 },
        Case{ EffectNumberBase::SourceStar, 3 },
        Case{ EffectNumberBase::SourceAttack, 240 },
        Case{ EffectNumberBase::SourceMaxHp, 1000 },
        Case{ EffectNumberBase::SourceMissingHpRatio, 6 },
        Case{ EffectNumberBase::SourceCurrentMpRatio, 3 },
        Case{ EffectNumberBase::TargetMaxHp, 777 },
        Case{ EffectNumberBase::TargetCurrentHp, 123 },
        Case{ EffectNumberBase::TargetCurrentShield, 222 },
        Case{ EffectNumberBase::TargetCurrentCooldown, 33 },
        Case{ EffectNumberBase::FinalHpDamage, 17 },
        Case{ EffectNumberBase::SourceStatusQuantity, 6,
              BattleStatusKind::TrueQi },
        Case{ EffectNumberBase::CurrentContributionQuantity, 4 },
        Case{ EffectNumberBase::StoredStateValue, 321 },
        Case{ EffectNumberBase::ApplicationTargetMaxHp, 777 },
        Case{ EffectNumberBase::BoundRatio, 777, std::nullopt, true },
    };
    for (const auto& test : cases)
    {
        EffectNumber number;
        number.base = test.base;
        number.percent = test.base == EffectNumberBase::SourceMissingHpRatio
                || test.base == EffectNumberBase::SourceCurrentMpRatio
            ? 1000
            : 100;
        number.status = test.status;
        if (test.boundRatio)
        {
            number.boundNumerator = 777;
            number.boundDenominator = 1;
        }
        CHECK(BattleEffectSystem::evaluateNumber(number, context, target)
            == test.expected);
        CHECK(effectNumberEvaluationKind(test.base)
            != EffectNumberEvaluationKind::Count);
    }
}

TEST_CASE("BattleEffectSystem orders MP selectors by current MP with deterministic unit ID ties",
          "[battle][effect][selector]")
{
    const auto owner = makeUnit(1, 0, 1000, 1000, 90, 100);
    const auto ally2 = makeUnit(2, 0, 1000, 1000, 30, 30);
    const auto ally3 = makeUnit(3, 0, 1000, 1000, 40, 200);
    const auto ally4 = makeUnit(4, 0, 1000, 1000, 30, 60);
    const auto enemy5 = makeUnit(5, 1, 1000, 1000, 70, 70);
    const auto enemy6 = makeUnit(6, 1, 1000, 1000, 80, 200);
    const auto enemy7 = makeUnit(7, 1, 1000, 1000, 80, 100);
    const std::vector units{ owner, ally2, ally3, ally4, enemy5, enemy6, enemy7 };
    const auto context = makeContext(
        EffectEvent::UltimateCommitted,
        magicBinding(133),
        owner,
        units,
        CastCommitEventData{ .provenance = castProvenance(133), .targetUnitId = enemy5.id });

    BattleRuntimeRandom random(1);
    EffectSelector lowest;
    lowest.kind = EffectSelectorKind::LowestMpAllies;
    lowest.count = 3;
    CHECK(BattleEffectSystem::selectTargets(lowest, context, random) == std::vector{ 2, 4, 3 });

    EffectSelector highest;
    highest.kind = EffectSelectorKind::HighestMpEnemy;
    CHECK(BattleEffectSystem::selectTargets(highest, context, random) == std::vector{ 6 });
}

TEST_CASE("BattleEffectSystem selects all living units across teams and can exclude the effect owner", "[battle][effect][selector]")
{
    const auto owner = makeUnit(1, 0, 1000, 1000);
    const auto ally = makeUnit(2, 0, 1000, 1000);
    const auto enemy = makeUnit(3, 1, 1000, 1000);
    auto deadAlly = makeUnit(4, 0, 0, 1000);
    deadAlly.alive = false;
    auto deadEnemy = makeUnit(5, 1, 0, 1000);
    deadEnemy.alive = false;
    const std::vector units{ owner, ally, enemy, deadAlly, deadEnemy };
    const auto context = makeContext(
        EffectEvent::UltimateCommitted,
        magicBinding(98),
        owner,
        units,
        CastCommitEventData{ .provenance = castProvenance(98), .targetUnitId = enemy.id });

    EffectSelector selector;
    selector.kind = EffectSelectorKind::AllLivingUnits;
    selector.excludeOwner = true;
    BattleRuntimeRandom random(1);

    CHECK(BattleEffectSystem::selectTargets(selector, context, random) == std::vector{ ally.id, enemy.id });
}

TEST_CASE("BattleEffectSystem reserves one target slot for a required hit target", "[battle][effect][selector_required]")
{
    const auto owner = makeUnit(1, 0, 1000, 1000, 0, 100, Pointf{ 0.0f, 0.0f });
    const auto enemy2 = makeUnit(2, 1, 1000, 1000, 0, 100, Pointf{ 1.0f, 0.0f });
    const auto enemy3 = makeUnit(3, 1, 1000, 1000, 0, 100, Pointf{ 2.0f, 0.0f });
    const auto enemy4 = makeUnit(4, 1, 1000, 1000, 0, 100, Pointf{ 2.5f, 0.0f });
    const auto hitTarget = makeUnit(9, 1, 1000, 1000, 0, 100, Pointf{ 0.0f, 0.0f });
    const std::vector units{ owner, enemy2, enemy3, enemy4, hitTarget };
    const auto context = makeContext(
        EffectEvent::MainProjectileBeforeDamage,
        magicBinding(79),
        owner,
        units,
        HitEventData{
            .provenance = attackProvenance(79),
            .targetUnitId = hitTarget.id,
            .originalTargetUnitId = hitTarget.id,
            .contactPosition = hitTarget.position,
        });

    EffectSelector selector;
    selector.kind = EffectSelectorKind::UnitsInRadius;
    selector.count = 3;
    selector.radiusTiles = 3;
    selector.team = EffectTeamFilter::Enemy;
    selector.requiredTarget = EffectRequiredTarget::HitTarget;
    BattleRuntimeRandom random(1);

    CHECK(BattleEffectSystem::selectTargets(selector, context, random)
          == std::vector{ hitTarget.id, enemy2.id, enemy3.id });
    CHECK(BattleEffectSystem::selectTargets(
        selector,
        context,
        random,
        [&](const EffectUnitSnapshot& candidate)
        {
            return candidate.id != hitTarget.id;
        }).empty());
}

TEST_CASE("BattleEffectSystem copy state machine applies its own source count after random living-unit selection", "[battle][effect][selector]")
{
    auto owner = makeUnit(1, 0, 1000, 1000);
    owner.ultimateMagicId = 98;
    auto ally = makeUnit(2, 0, 1000, 1000);
    ally.ultimateMagicId = 71;
    auto enemy = makeUnit(3, 1, 1000, 1000);
    enemy.ultimateMagicId = 72;
    auto deadEnemy = makeUnit(4, 1, 0, 1000);
    deadEnemy.alive = false;
    deadEnemy.ultimateMagicId = 73;
    const std::vector units{ owner, ally, enemy, deadEnemy };
    const auto context = makeContext(
        EffectEvent::UltimateCommitted,
        magicBinding(98),
        owner,
        units,
        CastCommitEventData{ .provenance = castProvenance(98), .targetUnitId = enemy.id });

    CopyAttackDefinitionAction copy;
    copy.sourceUnits.kind = EffectSelectorKind::AllLivingUnits;
    copy.sourceUnits.tieBreak = EffectTieBreak::BattleRandom;
    copy.sourceUnits.excludeOwner = true;
    copy.filter.conditions = {
        CopiedMagicCondition::HasUltimateAttackDefinition,
        CopiedMagicCondition::ExcludesRecursiveEffects,
    };
    copy.copyCount = 1;
    const auto rule = makeRule(
        1,
        EffectEvent::UltimateCommitted,
        selfSelector(),
        { stateAction(copy) });

    BattleEffectRuleStore store;
    store.append(magicBinding(98), rule);
    BattleRuntimeRandom random(1);
    const auto result = BattleEffectSystem{}.dispatch(store, context, random);
    REQUIRE(result.commands.size() == 1);
    const auto& command = std::get<StateMachineEffectCommand>(result.commands.front().value);
    REQUIRE(std::get<CopyAttackDefinitionCommand>(command.value).sourceUnitIds.size() == 1);
    CHECK(std::get<CopyAttackDefinitionCommand>(command.value).sourceUnitIds.front() != owner.id);
    CHECK(std::get<CopyAttackDefinitionCommand>(command.value).sourceUnitIds.front() != deadEnemy.id);
    CHECK((std::get<CopyAttackDefinitionCommand>(command.value).sourceUnitIds.front() == ally.id
        || std::get<CopyAttackDefinitionCommand>(command.value).sourceUnitIds.front() == enemy.id));

    const std::vector noCandidates{ owner, deadEnemy };
    const auto emptyContext = makeContext(
        EffectEvent::UltimateCommitted,
        magicBinding(98),
        owner,
        noCandidates,
        CastCommitEventData{ .provenance = castProvenance(98), .targetUnitId = deadEnemy.id });
    BattleEffectRuleStore emptyStore;
    emptyStore.append(magicBinding(98), rule);
    BattleRuntimeRandom emptyRandom(1);
    const auto emptyResult = BattleEffectSystem{}.dispatch(emptyStore, emptyContext, emptyRandom);
    REQUIRE(emptyResult.commands.size() == 1);
    const auto& emptyCommand = std::get<StateMachineEffectCommand>(emptyResult.commands.front().value);
    CHECK(std::get<CopyAttackDefinitionCommand>(emptyCommand.value).sourceUnitIds.empty());
}

TEST_CASE("BattleEffectSystem copy filter excludes recursive magic before random selection",
          "[battle][effect][selector][copy][filter]")
{
    auto owner = makeUnit(1, 0, 1000, 1000);
    owner.ultimateMagicId = 98;
    auto recursiveCandidate = makeUnit(2, 0, 1000, 1000);
    recursiveCandidate.ultimateMagicId = 43;
    const std::vector units{ owner, recursiveCandidate };
    const auto context = makeContext(
        EffectEvent::UltimateCommitted,
        magicBinding(98),
        owner,
        units,
        CastCommitEventData{
            .provenance = castProvenance(98),
            .targetUnitId = recursiveCandidate.id,
        });

    CopyAttackDefinitionAction copy;
    copy.sourceUnits.kind = EffectSelectorKind::AllLivingUnits;
    copy.sourceUnits.tieBreak = EffectTieBreak::BattleRandom;
    copy.sourceUnits.excludeOwner = true;
    copy.filter.conditions = {
        CopiedMagicCondition::HasUltimateAttackDefinition,
        CopiedMagicCondition::ExcludesRecursiveEffects,
    };
    const auto copyRule = makeRule(
        1,
        EffectEvent::UltimateCommitted,
        selfSelector(),
        { stateAction(copy) });

    BorrowEffectRulesAction borrow;
    borrow.sourceUnits.kind = EffectSelectorKind::Enemies;
    borrow.sourceCount.flat = 1;
    borrow.filter.allowedActionCategories = {
        BorrowedRuleActionCategory::ResourceChange,
    };
    borrow.propagation = CastPropagationPolicy::BorrowedUltimateRules;
    const auto recursiveRule = makeRule(
        2,
        EffectEvent::CastPlanned,
        selfSelector(),
        { stateAction(borrow) });

    BattleEffectRuleStore store;
    store.append(magicBinding(98), copyRule);
    auto recursiveBinding = magicBinding(43);
    recursiveBinding.ownerUnitId = recursiveCandidate.id;
    store.append(recursiveBinding, recursiveRule);
    BattleRuntimeRandom random(1);
    const auto result = BattleEffectSystem{}.dispatch(store, context, random);

    REQUIRE(result.commands.size() == 1);
    const auto& command = std::get<StateMachineEffectCommand>(
        result.commands.front().value);
    CHECK(std::get<CopyAttackDefinitionCommand>(command.value).sourceUnitIds.empty());
}

TEST_CASE("BattleEffectSystem gates rules by conditions chance propagation and maximum count", "[battle][effect]")
{
    auto owner = makeUnit(1, 0, 400, 1000);
    owner.statusDetails.push_back({ .state = BattleStatusKind::TrueQi });
    auto target = makeUnit(2, 1, 200, 1000);
    const std::vector units{ owner, target };
    auto context = makeContext(
        EffectEvent::MainProjectileBeforeDamage,
        magicBinding(77),
        owner,
        units,
        HitEventData{
            .provenance = attackProvenance(77, CastPropagationPolicy::SourceRules, 2),
            .targetUnitId = target.id,
            .originalTargetUnitId = 2,
            .damageKind = BattleDamageKind::Skill,
        });

    ModifyDamageAction modifier;
    modifier.amount.flat = 25;
    auto rule = makeRule(
        1,
        EffectEvent::MainProjectileBeforeDamage,
        hitTargetSelector(),
        { effectAction(modifier) },
        {
            IsUltimateCondition{},
            CastUsesEffectSourceMagicCondition{},
            IsMainProjectileCondition{},
            IsRootAttackCondition{},
            SourceHpRatioAtMostCondition{ 50 },
            TargetHpRatioAtMostCondition{ 25 },
            SourceHasStateCondition{ BattleStatusKind::TrueQi },
            AttackOrdinalEqualsCondition{ 2 },
        });
    rule.chancePct = 17;
    rule.maxActivations = 1;

    BattleEffectRuleStore store;
    store.append(magicBinding(77), rule);
    BattleRuntimeRandom random(5489);
    BattleEffectSystem system;
    const auto first = system.dispatch(store, context, random);
    CHECK(first.commands.size() == 1);
    CHECK(store.activationCount(magicBinding(77), EffectRuleId{ 1 }) == 1);
    CHECK(random.rawDrawCount() == 1);

    const auto second = system.dispatch(store, context, random);
    CHECK(second.commands.empty());
    CHECK(random.rawDrawCount() == 1);

    BattleEffectRuleStore suppressedStore;
    auto alwaysRule = rule;
    alwaysRule.id = EffectRuleId{ 2 };
    alwaysRule.chancePct = 100;
    alwaysRule.maxActivations = 0;
    alwaysRule.conditions.clear();
    suppressedStore.append(magicBinding(77), alwaysRule);
    auto suppressedContext = context;
    std::get<HitEventData>(suppressedContext.payload).provenance.propagation =
        CastPropagationPolicy::SuppressUltimateRules;
    BattleRuntimeRandom suppressedRandom(1);
    CHECK(system.dispatch(suppressedStore, suppressedContext, suppressedRandom).commands.empty());

    auto wrongMagicContext = context;
    std::get<HitEventData>(wrongMagicContext.payload).provenance = attackProvenance(78);
    BattleRuntimeRandom wrongMagicRandom(1);
    CHECK(system.dispatch(suppressedStore, wrongMagicContext, wrongMagicRandom).commands.empty());

    auto normalCastContext = context;
    auto& normalProvenance = std::get<HitEventData>(normalCastContext.payload).provenance;
    normalProvenance.cast.ultimate = false;
    normalProvenance.cast.origin = CastOriginKind::Normal;
    BattleRuntimeRandom normalCastRandom(1);
    CHECK(system.dispatch(suppressedStore, normalCastContext, normalCastRandom).commands.empty());
}

TEST_CASE("BattleEffectSystem matches a cast against the effect source relationally",
          "[battle][effect][condition]")
{
    const auto owner = makeUnit(1, 0, 1000, 1000);
    const auto target = makeUnit(2, 1, 1000, 1000);
    const std::vector units{owner, target};
    const auto binding = magicBinding(77);
    ChangeResourceAction restore;
    restore.resource = BattleResource::Mp;
    restore.kind = ResourceChangeKind::Restore;
    restore.amount.flat = 10;
    const auto rule = makeRule(
        1,
        EffectEvent::MainProjectileBeforeDamage,
        selfSelector(),
        {effectAction(restore)},
        {CastUsesEffectSourceMagicCondition{}});

    const auto dispatch = [&](int castMagicId)
    {
        BattleEffectRuleStore store;
        store.append(binding, rule);
        BattleRuntimeRandom random(1);
        auto provenance = attackProvenance(castMagicId);
        provenance.cast.sourceUnitId = owner.id;
        return BattleEffectSystem{}.dispatch(store, makeContext(
            EffectEvent::MainProjectileBeforeDamage,
            binding,
            owner,
            units,
            HitEventData{
                .provenance = provenance,
                .targetUnitId = target.id,
                .originalTargetUnitId = target.id,
                .damageKind = BattleDamageKind::Skill,
            }), random);
    };

    CHECK(dispatch(77).commands.size() == 1);
    CHECK(dispatch(78).commands.empty());
}

TEST_CASE("BattleEffectSystem bound magic attack commit still requires its matching ultimate",
          "[battle][effect][magic][commit]")
{
    const auto owner = makeUnit(1, 0, 1000, 1000, 50, 100);
    const auto target = makeUnit(2, 1, 1000, 1000);
    const std::vector units{ owner, target };

    ChangeResourceAction restore;
    restore.resource = BattleResource::Mp;
    restore.kind = ResourceChangeKind::Restore;
    restore.amount.flat = 10;
    const auto rule = makeRule(
        1,
        EffectEvent::AttackCommitted,
        selfSelector(),
        { effectAction(restore) });
    BattleEffectRuleStore store;
    store.append(magicBinding(133), rule);

    const auto dispatch = [&](int magicId, bool ultimate)
    {
        auto provenance = castProvenance(magicId);
        provenance.ultimate = ultimate;
        const auto context = makeContext(
            EffectEvent::AttackCommitted,
            magicBinding(133),
            owner,
            units,
            CastCommitEventData{
                .provenance = provenance,
                .targetUnitId = target.id,
            });
        BattleRuntimeRandom random(1);
        return BattleEffectSystem{}.dispatch(store, context, random);
    };

    CHECK(dispatch(133, true).commands.size() == 1);
    CHECK(dispatch(132, true).commands.empty());
    CHECK(dispatch(133, false).commands.empty());
}

TEST_CASE("Exact runtime candidates exclude unrelated events and owners without dropping observers",
          "[battle][effect][exact-runtime]")
{
    BattleEffectRuleStore store;
    BattleEffectSystem system;
    const auto owner = magicBinding();
    CHECK_FALSE(system.hasExactRuntimeRuleCandidates(store, EffectEvent::CastPlanned, 1));

    ChangeResourceAction shield;
    shield.resource = BattleResource::Shield;
    shield.kind = ResourceChangeKind::Grant;
    shield.amount.flat = 10;
    store.append(owner, makeRule(1, EffectEvent::CastPlanned, selfSelector(), { effectAction(shield) }));
    CHECK_FALSE(system.hasExactRuntimeRuleCandidates(store, EffectEvent::CastPlanned, 1));

    ModifyCastAction dash;
    dash.mobility = CastMobilityPolicy::DashAttack;
    const auto exact = makeRule(2, EffectEvent::CastPlanned, selfSelector(), { effectAction(dash) });
    store.append(owner, exact);
    CHECK(system.hasExactRuntimeRuleCandidates(store, EffectEvent::CastPlanned, 1));
    CHECK_FALSE(system.hasExactRuntimeRuleCandidates(store, EffectEvent::CastPlanned, 2));
    CHECK_FALSE(system.hasExactRuntimeRuleCandidates(store, EffectEvent::AttackCommitted, 1));

    ForceMoveAction knockback;
    knockback.direction = ForceMoveDirection::AwayFromSource;
    knockback.distancePixels = 120;
    knockback.lockFrames = 5;
    auto observed = makeRule(
        3, EffectEvent::MainProjectileBeforeDamage,
        EffectSelector{ .kind = EffectSelectorKind::HitTarget }, { effectAction(knockback) });
    observed.observation = EffectObservationScope::OwnerTeamEventSource;
    store.append(owner, observed);
    CHECK(system.hasExactRuntimeRuleCandidates(store, EffectEvent::MainProjectileBeforeDamage, 2));

    store.clear();
    CHECK_FALSE(system.hasExactRuntimeRuleCandidates(store, EffectEvent::CastPlanned, 1));
    auto shared = owner;
    shared.ownerUnitId = -1;
    store.append(shared, exact);
    CHECK(system.hasExactRuntimeRuleCandidates(store, EffectEvent::CastPlanned, 2));
}

TEST_CASE("Status behavior event filters include interceptors in nested conditional branches",
          "[battle][effect][status][filter]")
{
    const auto matches = BattleEffectSystem::statusBehaviorRuleMatchesEvent;
    const auto event = EffectEvent::HitBeforeDamage;
    auto nested = std::make_shared<ConditionalEffectAction>();
    nested->whenFalse = { effectAction(MakeIncomingAttackMissAction{}) };
    auto conditional = std::make_shared<ConditionalEffectAction>();
    conditional->whenTrue = { effectAction(SuppressCurrentCastContactsAction{}) };
    conditional->whenFalse = { effectAction(nested) };
    auto rule = makeRule(1, event, selfSelector(), { effectAction(conditional) });

    CHECK(matches(rule, event, StatusBehaviorDispatchFilter::All));
    CHECK(matches(rule, event, StatusBehaviorDispatchFilter::AttackInterceptorsOnly));
    CHECK(matches(rule, event, StatusBehaviorDispatchFilter::OutgoingCastSuppressorsOnly));
    CHECK(matches(rule, event, StatusBehaviorDispatchFilter::IncomingAttackMissOnly));
    CHECK_FALSE(matches(rule, event, StatusBehaviorDispatchFilter::ExcludeAttackInterceptors));
    CHECK_FALSE(matches(rule, EffectEvent::FrameAdvanced, StatusBehaviorDispatchFilter::All));

    rule.actions = { effectAction(MakeIncomingAttackMissAction{}) };
    CHECK_FALSE(matches(rule, event, StatusBehaviorDispatchFilter::OutgoingCastSuppressorsOnly));
    rule.actions = { effectAction(SuppressCurrentCastContactsAction{}) };
    CHECK_FALSE(matches(rule, event, StatusBehaviorDispatchFilter::IncomingAttackMissOnly));
    rule.actions.clear();
    CHECK(matches(rule, event, StatusBehaviorDispatchFilter::ExcludeAttackInterceptors));
    CHECK_FALSE(matches(rule, event, StatusBehaviorDispatchFilter::AttackInterceptorsOnly));
    rule.event = EffectEvent::StatusPersistent;
    CHECK_FALSE(matches(rule, rule.event, StatusBehaviorDispatchFilter::All));
}

TEST_CASE("Exact runtime metadata rejects unrelated magic and propagation before snapshots",
          "[battle][effect][exact-runtime][metadata]")
{
    BattleEffectRuleStore store;
    BattleEffectSystem system;
    const auto owner = magicBinding();
    ModifyCastAction dash;
    dash.mobility = CastMobilityPolicy::DashAttack;
    auto rule = makeRule(1, EffectEvent::CastPlanned, selfSelector(), { effectAction(dash) });
    store.append(owner, rule);
    const auto candidate = [&](BattleCastProvenance cast, int frame = 10)
    {
        const EffectEventData event{
            .event = EffectEvent::CastPlanned,
            .header = { .frame = frame },
            .payload = CastPlanEventData{ .provenance = cast },
        };
        return system.hasExactRuntimeRuleCandidates(store, event, 1);
    };
    CHECK(candidate(castProvenance(59)));
    CHECK_FALSE(candidate(castProvenance(60)));
    auto normal = castProvenance(59);
    normal.ultimate = false;
    normal.origin = CastOriginKind::Normal;
    CHECK_FALSE(candidate(normal));
    CHECK_FALSE(candidate(castProvenance(59, CastPropagationPolicy::NoEffectRules)));
    auto reflection = castProvenance(59);
    reflection.origin = CastOriginKind::Reflection;
    CHECK_FALSE(candidate(reflection));
    rule.id = EffectRuleId{ 2 };
    rule.castMatch = EffectCastMatch::OwnerAnyCast;
    store.append(owner, rule);
    CHECK(candidate(normal));
}

TEST_CASE("BattleEffectSystem exact runtime query uses canonical cast eligibility without activating",
          "[battle][effect][exact_runtime]")
{
    const auto owner = makeUnit(1, 0, 1000, 1000, 100, 100);
    const auto enemy = makeUnit(2, 1, 1000, 1000);
    const std::vector units{ owner, enemy };

    ModifyCastAction mobility;
    mobility.mobility = CastMobilityPolicy::DashAttack;
    auto rule = makeRule(
        1,
        EffectEvent::CastPlanned,
        selfSelector(),
        { effectAction(mobility) });
    const auto binding = magicBinding(59);
    BattleEffectRuleStore store;
    store.append(binding, rule);

    auto context = makeContext(
        EffectEvent::CastPlanned,
        binding,
        owner,
        units,
        CastPlanEventData{
            .provenance = castProvenance(59),
            .preferredTargetUnitId = enemy.id,
        });
    BattleRuntimeRandom random(1);
    BattleEffectSystem system;

    const auto matches = system.queryExactRuntimeRules(store, context, random);
    REQUIRE(matches.size() == 1);
    CHECK(matches[0].bound->binding.kind == binding.kind);
    CHECK(matches[0].bound->binding.sourceId == binding.sourceId);
    CHECK(matches[0].bound->binding.ownerUnitId == binding.ownerUnitId);
    CHECK(matches[0].targetUnitIds == std::vector{ owner.id });
    CHECK(random.rawDrawCount() == 0);
    CHECK(store.activationCount(binding, rule.id) == 0);

    auto wrongMagic = context;
    std::get<CastPlanEventData>(wrongMagic.payload).provenance.magicId = 60;
    CHECK(system.queryExactRuntimeRules(store, wrongMagic, random).empty());

    auto normalCast = context;
    auto& normal = std::get<CastPlanEventData>(normalCast.payload).provenance;
    normal.ultimate = false;
    normal.origin = CastOriginKind::Normal;
    CHECK(system.queryExactRuntimeRules(store, normalCast, random).empty());

    auto suppressed = context;
    std::get<CastPlanEventData>(suppressed.payload).provenance.propagation =
        CastPropagationPolicy::SuppressUltimateRules;
    CHECK(system.queryExactRuntimeRules(store, suppressed, random).empty());

    auto anyCastRule = rule;
    anyCastRule.id = EffectRuleId{ 2 };
    anyCastRule.castMatch = EffectCastMatch::OwnerAnyCast;
    BattleEffectRuleStore anyCastStore;
    anyCastStore.append(binding, anyCastRule);
    CHECK(system.queryExactRuntimeRules(anyCastStore, normalCast, random).size() == 1);
}

TEST_CASE("BattleEffectSystem uses one source precedence for ordinary and exact rules",
          "[battle][effect][ordering]")
{
    const auto owner = makeUnit(1, 0, 1000, 1000, 100, 100);
    const auto enemy = makeUnit(2, 1, 1000, 1000);
    const std::vector units{ owner, enemy };
    const EffectSourceBinding combo{
        .kind = EffectSourceKind::Combo,
        .sourceId = 10,
        .ownerUnitId = owner.id,
        .sourceTeam = owner.team,
    };
    const EffectSourceBinding equipment{
        .kind = EffectSourceKind::Equipment,
        .sourceId = 20,
        .ownerUnitId = owner.id,
        .sourceTeam = owner.team,
    };

    ChangeResourceAction shield;
    shield.resource = BattleResource::Shield;
    shield.kind = ResourceChangeKind::Grant;
    shield.amount.flat = 1;
    auto ordinary = makeRule(
        1,
        EffectEvent::UltimateCommitted,
        selfSelector(),
        { effectAction(shield) });
    ordinary.chancePct = 60;

    BattleEffectRuleStore store;
    store.append(equipment, ordinary);
    store.append(combo, ordinary);

    const auto commitContext = makeContext(
        EffectEvent::UltimateCommitted,
        magicBinding(59),
        owner,
        units,
        CastCommitEventData{
            .provenance = castProvenance(59),
            .targetUnitId = enemy.id,
        });
    BattleRuntimeRandom random(1);
    const auto dispatched = BattleEffectSystem().dispatch(
        store,
        commitContext,
        random);
    REQUIRE(dispatched.commands.size() == 1);
    CHECK(dispatched.commands.front().metadata.binding.kind
          == EffectSourceKind::Combo);
    CHECK(random.rawDrawCount() == 2);

    ModifyCastAction mobility;
    mobility.mobility = CastMobilityPolicy::DashAttack;
    const auto exact = makeRule(
        2,
        EffectEvent::CastPlanned,
        selfSelector(),
        { effectAction(mobility) });
    store.append(equipment, exact);
    store.append(combo, exact);

    const auto planContext = makeContext(
        EffectEvent::CastPlanned,
        magicBinding(59),
        owner,
        units,
        CastPlanEventData{
            .provenance = castProvenance(59),
            .preferredTargetUnitId = enemy.id,
        });
    const auto exactMatches = BattleEffectSystem().queryExactRuntimeRules(
        store,
        planContext,
        random);
    REQUIRE(exactMatches.size() == 2);
    CHECK(exactMatches[0].bound->binding.kind == EffectSourceKind::Combo);
    CHECK(exactMatches[1].bound->binding.kind == EffectSourceKind::Equipment);
}

TEST_CASE("BattleEffectSystem evaluates configured and status chances in structured order",
          "[battle][effect][status][ordering][random]")
{
    const auto owner = makeUnit(1, 0, 1000, 1000);
    const auto enemy = makeUnit(2, 1, 1000, 1000);
    const std::vector units{ owner, enemy };

    ChangeResourceAction configuredShield;
    configuredShield.resource = BattleResource::Shield;
    configuredShield.kind = ResourceChangeKind::Grant;
    configuredShield.amount.flat = 1;
    auto configuredRule = makeRule(
        1,
        EffectEvent::UltimateCommitted,
        selfSelector(),
        { effectAction(configuredShield) });
    configuredRule.chancePct = 60;
    const EffectSourceBinding configuredBinding{
        .kind = EffectSourceKind::Combo,
        .sourceId = 10,
        .ownerUnitId = owner.id,
        .sourceTeam = owner.team,
    };
    BattleEffectRuleStore store;
    store.append(configuredBinding, configuredRule);

    auto statusShield = configuredShield;
    statusShield.amount.flat = 2;
    auto statusRule = makeRule(
        2,
        EffectEvent::UltimateCommitted,
        EffectSelector{ .kind = EffectSelectorKind::StatusHolder },
        { effectAction(statusShield) });
    statusRule.observation = EffectObservationScope::StatusHolderEventSource;
    statusRule.chancePct = 60;
    auto behavior = std::make_shared<StatusBehaviorDefinition>();
    behavior->rules = { statusRule };
    std::vector runtime{ EffectRuleRuntimeState{} };
    const auto active = statusBehaviorFixture({
        .binding = magicBinding(59),
        .producerRuleId = EffectRuleId{ 2 },
        .producerRuleOrder = 1,
        .holderUnitId = owner.id,
        .sourceUnitId = owner.id,
        .kind = BattleStatusKind::Shadowless,
        .quantity = 1,
        .appliedSequence = 1,
        .behavior = behavior,
        .runtime = &runtime,
    }, nullptr);
    const auto context = makeContext(
        EffectEvent::UltimateCommitted,
        magicBinding(59),
        owner,
        units,
        CastCommitEventData{
            .provenance = castProvenance(59),
            .targetUnitId = enemy.id,
        });
    BattleRuntimeRandom random(1);

    const auto dispatched = dispatchMergedFixture(
        store,
        context,
        random,
        std::span{ &active, std::size_t{ 1 } });

    REQUIRE(dispatched.commands.size() == 1);
    CHECK(dispatched.commands.front().metadata.binding.kind
        == EffectSourceKind::Combo);
    CHECK(std::get<ChangeResourceEffectCommand>(
        dispatched.commands.front().value).resolvedAmount() == 1);
    CHECK(random.rawDrawCount() == 2);
    CHECK(store.activationCount(configuredBinding, configuredRule.id) == 1);
    CHECK(runtime.front().activationCount == 0);
}

TEST_CASE("BattleEffectSystem orders every configured source kind by explicit precedence",
          "[battle][effect][ordering][source]")
{
    const auto owner = makeUnit(1, 0, 1000, 1000);
    const auto enemy = makeUnit(2, 1, 1000, 1000);
    const std::vector units{ owner, enemy };
    ChangeResourceAction shield;
    shield.resource = BattleResource::Shield;
    shield.kind = ResourceChangeKind::Grant;
    shield.amount.flat = 1;

    BattleEffectRuleStore store;
    const std::array appended{
        EffectSourceKind::Magic,
        EffectSourceKind::Neigong,
        EffectSourceKind::EquipmentSynergy,
        EffectSourceKind::Equipment,
        EffectSourceKind::Combo,
    };
    for (std::size_t index = 0; index < appended.size(); ++index)
    {
        const auto kind = appended[index];
        const EffectSourceBinding source{
            .kind = kind,
            .sourceId = kind == EffectSourceKind::Magic
                ? 59
                : 100 + static_cast<int>(index),
            .ownerUnitId = owner.id,
            .sourceTeam = owner.team,
        };
        store.append(source, makeRule(
            100 + index,
            EffectEvent::UltimateCommitted,
            selfSelector(),
            { effectAction(shield) }));
    }

    BattleRuntimeRandom random(1);
    const auto result = BattleEffectSystem().dispatch(
        store,
        makeContext(
            EffectEvent::UltimateCommitted,
            magicBinding(59),
            owner,
            units,
            CastCommitEventData{
                .provenance = castProvenance(59),
                .targetUnitId = enemy.id,
            }),
        random);

    REQUIRE(result.commands.size() == 5);
    const std::array expected{
        EffectSourceKind::Combo,
        EffectSourceKind::Equipment,
        EffectSourceKind::EquipmentSynergy,
        EffectSourceKind::Neigong,
        EffectSourceKind::Magic,
    };
    for (std::size_t index = 0; index < expected.size(); ++index)
        CHECK(result.commands[index].metadata.binding.kind == expected[index]);
}

TEST_CASE("BattleEffectRuleStore never renumbers stable order tokens across borrowed cycles",
          "[battle][effect][ordering][borrow]")
{
    ChangeResourceAction shield;
    shield.resource = BattleResource::Shield;
    shield.kind = ResourceChangeKind::Grant;
    shield.amount.flat = 1;
    auto sourceRule = makeRule(
        1,
        EffectEvent::HitBeforeDamage,
        hitTargetSelector(),
        { effectAction(shield) });
    auto neighborRule = sourceRule;
    neighborRule.id = EffectRuleId{ 2 };

    BattleEffectRuleStore store;
    store.append(EffectSourceBinding{
        .kind = EffectSourceKind::Magic,
        .sourceId = 70,
        .ownerUnitId = 2,
        .sourceTeam = 1,
    }, sourceRule);
    store.append(EffectSourceBinding{
        .kind = EffectSourceKind::Magic,
        .sourceId = 71,
        .ownerUnitId = 1,
        .sourceTeam = 0,
    }, neighborRule);
    REQUIRE(store.rules().size() == 2);
    const auto sourceOrder = store.rules()[0].order;
    const auto neighborOrder = store.rules()[1].order;

    BorrowedRuleFilter filter;
    filter.allowedActionCategories = {
        BorrowedRuleActionCategory::ResourceChange,
    };
    std::uint32_t previousBorrowedOrder = neighborOrder;
    for (std::uint64_t cycle = 1; cycle <= 3; ++cycle)
    {
        const BattleCastId cast(cycle);
        const std::array sourceUnitIds{ 2 };
        const auto added = store.bindBorrowedUltimateRules(
            cast,
            1,
            0,
            sourceUnitIds,
            filter,
            CastPropagationPolicy::BorrowedUltimateRules);
        REQUIRE(added.size() == 1);
        REQUIRE(store.rules().size() == 3);
        CHECK(store.rules()[0].order == sourceOrder);
        CHECK(store.rules()[1].order == neighborOrder);
        CHECK(store.rules()[2].order > previousBorrowedOrder);
        previousBorrowedOrder = store.rules()[2].order;

        store.removeCastScopedRules(cast);
        REQUIRE(store.rules().size() == 2);
        CHECK(store.rules()[0].order == sourceOrder);
        CHECK(store.rules()[1].order == neighborOrder);
    }
}

TEST_CASE("BattleEffectRuleStore preserves later rule timers and cooldowns when a borrowed rule is removed",
          "[battle][effect][borrow][interval][cooldown]")
{
    ChangeResourceAction shield;
    shield.resource = BattleResource::Shield;
    shield.kind = ResourceChangeKind::Grant;
    shield.amount.flat = 1;
    auto rule = makeRule(1, EffectEvent::UltimateCommitted, selfSelector(), { effectAction(shield) });
    auto sourceBinding = magicBinding(70);
    sourceBinding.ownerUnitId = 2;
    sourceBinding.sourceTeam = 1;
    BattleEffectRuleStore store;
    store.append(sourceBinding, rule);
    BorrowedRuleFilter filter;
    filter.allowedActionCategories = { BorrowedRuleActionCategory::ResourceChange };
    const std::array sourceUnitIds{ 2 };
    const BattleCastId borrowedCast{ 1 };
    REQUIRE(store.bindBorrowedUltimateRules(borrowedCast, 1, 0, sourceUnitIds, filter,
        CastPropagationPolicy::BorrowedUltimateRules).size() == 1);

    const auto binding = magicBinding(71);
    rule.id = EffectRuleId{ 2 };
    rule.event = EffectEvent::FrameAdvanced;
    rule.intervalFrames = 3;
    rule.sharedCooldownFrames = 5;
    rule.maxActivations = 2;
    store.append(binding, rule);
    const auto owner = makeUnit(1, 0, 1000, 1000);
    const std::vector units{ owner };
    auto context = makeContext(EffectEvent::FrameAdvanced, binding, owner, units,
        FrameTickEventData{ .deltaFrames = 1 });
    context.header.frame = 1;
    BattleRuntimeRandom random(1);
    BattleEffectSystem system;
    CHECK(system.dispatch(store, context, random).commands.empty());
    CHECK(store.runtime(binding, rule.id).intervalFramesRemaining == 2);
    store.recordRuntimeRuleActivation(binding, rule.id, 10);

    store.removeCastScopedRules(borrowedCast);
    REQUIRE(store.rules().size() == 2);
    CHECK(store.activationCount(binding, rule.id) == 1);
    CHECK(store.runtime(binding, rule.id).intervalFramesRemaining == 2);
    CHECK_FALSE(store.canActivateRuntimeRule(binding, rule.id, 14));
    CHECK(store.canActivateRuntimeRule(binding, rule.id, 15));
    context.header.frame = 15;
    std::get<FrameTickEventData>(context.payload).deltaFrames = 2;
    REQUIRE(system.dispatch(store, context, random).commands.size() == 1);
    CHECK(store.activationCount(binding, rule.id) == 2);
    CHECK(store.runtime(binding, rule.id).intervalFramesRemaining == 3);
    CHECK_FALSE(store.canActivateRuntimeRule(binding, rule.id, 20));
}

TEST_CASE("BattleEffectSystem inserts dynamically borrowed rules at their structured order",
          "[battle][effect][ordering][borrow][merged]")
{
    const auto owner = makeUnit(1, 0, 1000, 1000);
    const auto source = makeUnit(2, 1, 1000, 1000);
    const std::vector units{ owner, source };

    ChangeResourceAction borrowedShield;
    borrowedShield.resource = BattleResource::Shield;
    borrowedShield.kind = ResourceChangeKind::Grant;
    borrowedShield.amount.flat = 9;
    const auto sourceRule = makeRule(
        1,
        EffectEvent::CastPlanned,
        selfSelector(),
        { effectAction(borrowedShield) });
    const EffectSourceBinding sourceBinding{
        .kind = EffectSourceKind::Magic,
        .sourceId = 71,
        .ownerUnitId = source.id,
        .sourceTeam = source.team,
    };

    BorrowEffectRulesAction borrow;
    borrow.sourceUnits.kind = EffectSelectorKind::Enemies;
    borrow.sourceCount.flat = 1;
    borrow.filter.allowedActionCategories = {
        BorrowedRuleActionCategory::ResourceChange,
    };
    borrow.propagation = CastPropagationPolicy::BorrowedUltimateRules;
    const auto borrowRule = makeRule(
        2,
        EffectEvent::CastPlanned,
        selfSelector(),
        { stateAction(borrow) });
    const auto borrowerBinding = magicBinding(43);

    BattleEffectRuleStore store;
    store.append(sourceBinding, sourceRule);
    store.append(borrowerBinding, borrowRule);
    BattleRuntimeRandom random(1);
    const auto result = dispatchMergedFixture(
        store,
        makeContext(
            EffectEvent::CastPlanned,
            borrowerBinding,
            owner,
            units,
            CastPlanEventData{
                .provenance = castProvenance(43),
                .preferredTargetUnitId = source.id,
            }),
        random,
        {});

    REQUIRE(result.commands.size() == 2);
    CHECK(std::holds_alternative<StateMachineEffectCommand>(
        result.commands[0].value));
    const auto& borrowed = std::get<ChangeResourceEffectCommand>(
        result.commands[1].value);
    CHECK(borrowed.resolvedAmount() == 9);
    CHECK(result.commands[1].metadata.binding.ownerUnitId == owner.id);
    CHECK(result.commands[1].metadata.binding.sourceId == 71);
    CHECK(result.commands[1].metadata.binding.runtimeInstanceId != 0);
    CHECK(effectExecutionOrderKey(result.commands[0].metadata)
        < effectExecutionOrderKey(result.commands[1].metadata));
    CHECK(store.castScopedRuleCount(BattleCastId{ 1 }) == 1);
}

TEST_CASE("Effect rules reject mixed or nested exact runtime actions",
          "[battle][effect][exact_runtime][schema]")
{
    ModifyCastAction exact;
    exact.mobility = CastMobilityPolicy::DashAttack;

    auto mixedAction = exact;
    mixedAction.mpCost = EffectNumber{ .flat = 10 };
    auto rule = makeRule(
        1,
        EffectEvent::CastPlanned,
        selfSelector(),
        { effectAction(mixedAction) });
    std::string error;
    CHECK_FALSE(validateEffectRule(rule, error));

    ChangeResourceAction resource;
    resource.resource = BattleResource::Mp;
    resource.kind = ResourceChangeKind::Grant;
    resource.amount.flat = 10;
    rule.actions = { effectAction(exact), effectAction(resource) };
    CHECK_FALSE(validateEffectRule(rule, error));

    ModifyCastAction ranged;
    ranged.rangeMode = CastRangeMode::Ranged;
    rule.actions = { effectAction(ranged), effectAction(resource) };
    CHECK_FALSE(validateEffectRule(rule, error));

    auto conditional = std::make_shared<ConditionalEffectAction>();
    conditional->conditions = { IsUltimateCondition{} };
    conditional->whenTrue = { effectAction(exact) };
    rule.actions = { effectAction(conditional) };
    CHECK_FALSE(validateEffectRule(rule, error));
}

TEST_CASE("Effect rules reject unsupported accounting on exact runtime paths",
          "[battle][effect][exact_runtime][schema]")
{
    ModifyCastAction exact;
    exact.mobility = CastMobilityPolicy::DashAttack;
    const auto valid = makeRule(
        1,
        EffectEvent::CastPlanned,
        selfSelector(),
        { effectAction(exact) });
    std::string error;
    REQUIRE(validateEffectRule(valid, error));

    const auto rejectsAccounting = [&](EffectRule rule)
    {
        error.clear();
        CHECK_FALSE(validateEffectRule(rule, error));
        CHECK(error.find("觸發記帳") != std::string::npos);
    };

    auto chance = valid;
    chance.chancePct = 50;
    rejectsAccounting(std::move(chance));

    auto everyNth = valid;
    everyNth.everyNthEvent = 2;
    rejectsAccounting(std::move(everyNth));

    auto activationLimit = valid;
    activationLimit.activationLimit = EffectActivationLimit{
        .scope = EffectActivationScope::PerCastPerTarget,
        .maxEvaluations = 1,
    };
    rejectsAccounting(std::move(activationLimit));

    auto maxActivations = valid;
    maxActivations.maxActivations = 1;
    rejectsAccounting(std::move(maxActivations));

    auto sharedCooldown = valid;
    sharedCooldown.sharedCooldownFrames = 30;
    rejectsAccounting(std::move(sharedCooldown));

    ForceMoveAction knockback;
    knockback.direction = ForceMoveDirection::AwayFromSource;
    knockback.distancePixels = 120;
    knockback.lockFrames = 5;
    knockback.collision = ForceMoveCollision::StopBeforeBlocked;
    knockback.blocked = ForceMoveBlockedResult::Shorten;
    auto hitChance = makeRule(
        2,
        EffectEvent::MainProjectileBeforeDamage,
        EffectSelector{ .kind = EffectSelectorKind::HitTarget },
        { effectAction(knockback) });
    hitChance.chancePct = 25;
    error.clear();
    CHECK(validateEffectRule(hitChance, error));
}

TEST_CASE("BattleEffectSystem lets an active magic state observe the owner's later normal root attack",
          "[battle][effect][cast_match][sunflower]")
{
    auto owner = makeUnit(1, 0, 1000, 1000);
    owner.statusDetails.push_back({ .state = BattleStatusKind::Shadowless });
    const auto enemy = makeUnit(2, 1, 1000, 1000);
    const std::vector units{ owner, enemy };

    ModifyAttackAction echo;
    echo.pattern.kind = AttackPatternKind::EchoNearestOthers;
    echo.pattern.projectileCount = 2;
    echo.strengthPct = 50;
    echo.addToBaseAttack = true;
    echo.propagation = CastPropagationPolicy::NoEffectRules;
    auto rule = makeRule(
        1,
        EffectEvent::AttackSpawned,
        EffectSelector{ .kind = EffectSelectorKind::Enemies, .count = 1 },
        { effectAction(echo) },
        { IsRootAttackCondition{}, SourceHasStateCondition{ BattleStatusKind::Shadowless } });
    rule.castMatch = EffectCastMatch::OwnerAnyCast;

    BattleEffectRuleStore store;
    store.append(magicBinding(105), rule);
    auto provenance = attackProvenance(1);
    provenance.cast.ultimate = false;
    provenance.cast.origin = CastOriginKind::Normal;
    const auto context = makeContext(
        EffectEvent::AttackSpawned,
        magicBinding(105),
        owner,
        units,
        AttackEventData{
            .provenance = provenance,
            .originalTargetUnitId = enemy.id,
        });
    BattleEffectSystem system;
    BattleRuntimeRandom random(1);

    const auto echoDispatch = system.dispatch(store, context, random);
    REQUIRE(echoDispatch.commands.size() == 1);
    const auto& echoCommand = std::get<ModifyAttackEffectCommand>(
        echoDispatch.commands.front().value);
    CHECK(echoCommand.strengthPct == 50);
    CHECK(echoCommand.propagation == CastPropagationPolicy::NoEffectRules);

    auto foreignCastContext = context;
    std::get<AttackEventData>(foreignCastContext.payload)
        .provenance.cast.sourceUnitId = enemy.id;
    CHECK(system.dispatch(store, foreignCastContext, random).commands.empty());

    auto nonRootContext = context;
    std::get<AttackEventData>(nonRootContext.payload).provenance.rootAttack = false;
    CHECK(system.dispatch(store, nonRootContext, random).commands.empty());

    auto bounceRule = rule;
    bounceRule.id = EffectRuleId{ 2 };
    std::erase_if(bounceRule.conditions, [](const EffectCondition& condition)
    {
        return std::holds_alternative<IsRootAttackCondition>(condition);
    });
    BattleEffectRuleStore bounceStore;
    bounceStore.append(magicBinding(105), bounceRule);
    auto bounceContext = context;
    auto& bounce = std::get<AttackEventData>(bounceContext.payload).provenance;
    bounce.origin = BattleAttackOriginKind::Bounce;
    bounce.rootAttack = false;
    bounce.propagation = CastPropagationPolicy::SourceHitRulesOnly;
    CHECK(system.dispatch(bounceStore, bounceContext, random).commands.empty());

    auto suppressedContext = context;
    auto& suppressed = std::get<AttackEventData>(suppressedContext.payload).provenance;
    suppressed.propagation = CastPropagationPolicy::NoEffectRules;
    suppressed.cast.propagation = CastPropagationPolicy::NoEffectRules;
    CHECK(system.dispatch(store, suppressedContext, random).commands.empty());
}

TEST_CASE("BattleEffectSystem rejects owner-any-cast matching on non-magic bindings",
          "[battle][effect][cast_match][schema]")
{
    ModifyAttackAction echo;
    auto rule = makeRule(
        1,
        EffectEvent::AttackSpawned,
        selfSelector(),
        { effectAction(echo) });
    rule.castMatch = EffectCastMatch::OwnerAnyCast;
    const EffectSourceBinding comboBinding{
        .kind = EffectSourceKind::Combo,
        .sourceId = 1,
        .ownerUnitId = 1,
        .sourceTeam = 0,
    };

    BattleEffectRuleStore store;
    CHECK_THROWS_AS(store.append(comboBinding, rule), std::invalid_argument);
}

TEST_CASE("BattleEffectSystem limits chance evaluation once per cast and target before drawing", "[battle][effect][activation]")
{
    const auto owner = makeUnit(1, 0, 1000, 1000);
    const auto firstTarget = makeUnit(2, 1, 400, 1000);
    const auto secondTarget = makeUnit(3, 1, 400, 1000);
    const std::vector units{ owner, firstTarget, secondTarget };
    const auto binding = magicBinding(67);

    DealDamageAction execute;
    execute.amount.base = EffectNumberBase::TargetMaxHp;
    execute.amount.percent = 100;
    execute.kind = BattleDamageKind::Execute;
    auto rule = makeRule(
        1,
        EffectEvent::MainProjectileBeforeDamage,
        hitTargetSelector(),
        { effectAction(execute) },
        { TargetHpRatioAtMostCondition{ 50 } });
    rule.chancePct = 5;
    rule.activationLimit = EffectActivationLimit{
        .scope = EffectActivationScope::PerCastPerTarget,
        .maxEvaluations = 1,
    };

    BattleEffectRuleStore store;
    store.append(binding, rule);
    BattleEffectSystem system;
    BattleRuntimeRandom random(1);
    random.restore(2);  // seed 1 的下一次判定為 1.24%。

    const auto hitContext = [&](const EffectUnitSnapshot& target,
                                BattleCastId castId,
                                BattleAttackId attackId)
    {
        auto provenance = attackProvenance(67);
        provenance.cast.rootCastId = castId;
        provenance.cast.castId = castId;
        provenance.attackId = attackId;
        return makeContext(
            EffectEvent::MainProjectileBeforeDamage,
            binding,
            owner,
            units,
            HitEventData{
                .provenance = provenance,
                .targetUnitId = target.id,
                .originalTargetUnitId = target.id,
                .damageKind = BattleDamageKind::Skill,
            });
    };

    const BattleCastId firstCast{ 10 };
    const auto drawsBefore = random.rawDrawCount();
    const auto first = system.dispatch(
        store,
        hitContext(firstTarget, firstCast, BattleAttackId{ 100 }),
        random);
    REQUIRE(first.commands.size() == 1);
    CHECK(random.rawDrawCount() == drawsBefore + 1);

    const auto repeated = system.dispatch(
        store,
        hitContext(firstTarget, firstCast, BattleAttackId{ 101 }),
        random);
    CHECK(repeated.commands.empty());
    CHECK(random.rawDrawCount() == drawsBefore + 1);
    CHECK(store.activationEvaluationCount(binding, rule.id, firstCast, firstTarget.id) == 1);

    const auto otherTarget = system.dispatch(
        store,
        hitContext(secondTarget, firstCast, BattleAttackId{ 102 }),
        random);
    CHECK(otherTarget.commands.size() <= 1);
    CHECK(random.rawDrawCount() == drawsBefore + 2);
    CHECK(store.activationEvaluationCount(binding, rule.id, firstCast, secondTarget.id) == 1);

    const BattleCastId secondCast{ 11 };
    const auto laterCast = system.dispatch(
        store,
        hitContext(firstTarget, secondCast, BattleAttackId{ 103 }),
        random);
    CHECK(laterCast.commands.size() <= 1);
    CHECK(random.rawDrawCount() == drawsBefore + 3);
    CHECK(store.activationEvaluationCount(binding, rule.id, secondCast, firstTarget.id) == 1);

    auto settledProvenance = castProvenance(67);
    settledProvenance.rootCastId = firstCast;
    settledProvenance.castId = firstCast;
    const auto settled = makeContext(
        EffectEvent::CastSettled,
        binding,
        owner,
        units,
        CastAggregateEventData{ .provenance = settledProvenance });
    CHECK(system.dispatch(store, settled, random).commands.empty());
    CHECK(store.activationEvaluationCount(binding, rule.id, firstCast, firstTarget.id) == 0);
    CHECK(store.activationEvaluationCount(binding, rule.id, firstCast, secondTarget.id) == 0);
    CHECK(store.activationEvaluationCount(binding, rule.id, secondCast, firstTarget.id) == 1);

    BattleEffectRuleStore failedStore;
    failedStore.append(binding, rule);
    BattleRuntimeRandom failedRandom(1);  // 第一次判定為 58.45%。
    const BattleCastId failedCast{ 12 };
    CHECK(system.dispatch(
        failedStore,
        hitContext(firstTarget, failedCast, BattleAttackId{ 104 }),
        failedRandom).commands.empty());
    CHECK(failedRandom.rawDrawCount() == 1);
    CHECK(system.dispatch(
        failedStore,
        hitContext(firstTarget, failedCast, BattleAttackId{ 105 }),
        failedRandom).commands.empty());
    CHECK(failedRandom.rawDrawCount() == 1);
}

TEST_CASE("BattleEffectSystem distinguishes damage dealt from damage received", "[battle][effect]")
{
    const auto owner = makeUnit(1, 0, 1000, 1000);
    const auto enemy = makeUnit(2, 1, 1000, 1000);
    const std::vector units{ owner, enemy };
    const EffectSourceBinding binding{
        .kind = EffectSourceKind::Combo,
        .sourceId = 10,
        .ownerUnitId = owner.id,
        .sourceTeam = owner.team,
    };

    ChangeResourceAction restore;
    restore.resource = BattleResource::Mp;
    restore.kind = ResourceChangeKind::Restore;
    restore.amount.flat = 1;
    const auto dealtRule = makeRule(
        1,
        EffectEvent::DamageResolved,
        selfSelector(),
        { effectAction(restore) },
        { DamagePerspectiveCondition{ DamagePerspective::Dealt } });
    const auto receivedRule = makeRule(
        2,
        EffectEvent::DamageResolved,
        selfSelector(),
        { effectAction(restore) },
        { DamagePerspectiveCondition{ DamagePerspective::Received } });

    BattleEffectRuleStore store;
    store.append(binding, dealtRule);
    store.append(binding, receivedRule);
    BattleRuntimeRandom random(1);
    BattleEffectSystem system;

    const auto dealt = system.dispatch(store, makeContext(
        EffectEvent::DamageResolved,
        binding,
        owner,
        units,
        DamageResultEventData{
            .origin = EffectAttackDamageOrigin{ attackProvenance(59) },
            .attackerBefore = owner,
            .defenderBefore = enemy,
            .defenderAfter = enemy,
            .finalHpDamage = 100,
        }), random);
    REQUIRE(dealt.commands.size() == 1);
    CHECK(dealt.commands.front().metadata.ruleId == EffectRuleId{ 1 });

    auto incomingProvenance = attackProvenance(59);
    incomingProvenance.cast.sourceUnitId = enemy.id;
    const auto received = system.dispatch(store, makeContext(
        EffectEvent::DamageResolved,
        binding,
        owner,
        units,
        DamageResultEventData{
            .origin = EffectAttackDamageOrigin{ incomingProvenance },
            .attackerBefore = enemy,
            .defenderBefore = owner,
            .defenderAfter = owner,
            .finalHpDamage = 100,
        }), random);
    REQUIRE(received.commands.size() == 1);
    CHECK(received.commands.front().metadata.ruleId == EffectRuleId{ 2 });
}

TEST_CASE("BattleEffectSystem does not suppress defender and death-owner rules with attacker propagation",
          "[battle][effect][propagation][observation]")
{
    const auto defender = makeUnit(1, 0, 1000, 1000);
    const auto attacker = makeUnit(2, 1, 1000, 1000);
    const std::vector units{ defender, attacker };
    const auto defenderBinding = magicBinding(79);
    const EffectSourceBinding attackerBinding{
        .kind = EffectSourceKind::Magic,
        .sourceId = 88,
        .ownerUnitId = attacker.id,
        .sourceTeam = attacker.team,
    };

    ChangeResourceAction shield;
    shield.resource = BattleResource::Shield;
    shield.kind = ResourceChangeKind::Grant;
    shield.amount.flat = 9;

    auto hitRule = makeRule(
        1,
        EffectEvent::HitBeforeDamage,
        selfSelector(),
        { effectAction(shield) });
    hitRule.observation = EffectObservationScope::EventTarget;

    auto deathRule = makeRule(
        2,
        EffectEvent::UnitDied,
        selfSelector(),
        { effectAction(shield) });
    const auto sourceHitRule = makeRule(
        3,
        EffectEvent::HitBeforeDamage,
        selfSelector(),
        { effectAction(shield) });

    BattleEffectRuleStore store;
    store.append(defenderBinding, hitRule);
    store.append(defenderBinding, deathRule);
    for (const auto kind : {
             EffectSourceKind::Combo,
             EffectSourceKind::Equipment,
             EffectSourceKind::Neigong })
    {
        store.append({
            .kind = kind,
            .sourceId = static_cast<int>(kind) + 100,
            .ownerUnitId = attacker.id,
            .sourceTeam = attacker.team,
        }, sourceHitRule);
    }
    BattleRuntimeRandom random(1);
    BattleEffectSystem system;

    auto rootProvenance = attackProvenance(88, CastPropagationPolicy::SourceRules);
    rootProvenance.cast.sourceUnitId = attacker.id;
    const auto rootHit = system.dispatch(store, makeContext(
        EffectEvent::HitBeforeDamage,
        attackerBinding,
        attacker,
        units,
        HitEventData{
            .provenance = rootProvenance,
            .targetUnitId = defender.id,
            .originalTargetUnitId = defender.id,
            .damageKind = BattleDamageKind::Skill,
        }), random);
    REQUIRE(rootHit.commands.size() == 4);

    auto provenance = attackProvenance(88, CastPropagationPolicy::NoEffectRules);
    provenance.cast.sourceUnitId = attacker.id;
    const auto hit = system.dispatch(store, makeContext(
        EffectEvent::HitBeforeDamage,
        attackerBinding,
        attacker,
        units,
        HitEventData{
            .provenance = provenance,
            .targetUnitId = defender.id,
            .originalTargetUnitId = defender.id,
            .damageKind = BattleDamageKind::Skill,
        }), random);
    REQUIRE(hit.commands.size() == 1);
    CHECK(hit.commands.front().metadata.binding.ownerUnitId == defender.id);

    const auto death = system.dispatch(store, makeContext(
        EffectEvent::UnitDied,
        defenderBinding,
        defender,
        units,
        DeathEventData{
            .deadBefore = defender,
            .cause = EffectAttackDamageOrigin{ provenance },
        }), random);
    REQUIRE(death.commands.size() == 1);
    CHECK(death.commands.front().metadata.binding.ownerUnitId == defender.id);
}

TEST_CASE("BattleEffectSystem expands conditional actions in stable action and target order", "[battle][effect]")
{
    const auto owner = makeUnit(1, 0, 1000, 1000, 100, 100);
    const auto ally2 = makeUnit(2, 0, 800, 1000, 100, 100);
    const auto ally3 = makeUnit(3, 0, 800, 1000, 20, 100);
    const std::vector units{ owner, ally2, ally3 };
    auto context = makeContext(
        EffectEvent::UltimateCommitted,
        magicBinding(133),
        owner,
        units,
        CastCommitEventData{
            .provenance = castProvenance(133),
            .targetUnitId = 1,
            .mpBefore = 100,
            .resourcesBeforeCast = { { 2, 100, 100 }, { 3, 20, 100 } },
        });

    ChangeResourceAction shield;
    shield.resource = BattleResource::Shield;
    shield.kind = ResourceChangeKind::Grant;
    shield.amount.flat = 160;
    ChangeResourceAction mp;
    mp.resource = BattleResource::Mp;
    mp.kind = ResourceChangeKind::Restore;
    mp.amount.flat = 20;

    auto conditional = std::make_shared<ConditionalEffectAction>();
    conditional->conditions.push_back(TargetMpWasFullBeforeCastCondition{});
    conditional->whenTrue.push_back(effectAction(shield));
    conditional->whenFalse.push_back(effectAction(mp));

    EffectSelector selector;
    selector.kind = EffectSelectorKind::LowestMpAllies;
    selector.count = 2;
    selector.excludeOwner = true;
    const auto rule = makeRule(
        1,
        EffectEvent::UltimateCommitted,
        selector,
        { effectAction(conditional) },
        { IsUltimateCondition{}, CastUsesEffectSourceMagicCondition{} });

    BattleEffectRuleStore store;
    store.append(magicBinding(133), rule);
    BattleRuntimeRandom random(1);
    const auto result = BattleEffectSystem{}.dispatch(store, context, random);
    REQUIRE(result.commands.size() == 2);
    CHECK(result.commands[0].metadata.targetUnitId == 2);
    CHECK(std::get<ChangeResourceEffectCommand>(result.commands[0].value).resource == BattleResource::Shield);
    CHECK(result.commands[1].metadata.targetUnitId == 3);
    CHECK(std::get<ChangeResourceEffectCommand>(result.commands[1].value).resource == BattleResource::Mp);
    CHECK(result.commands[0].metadata.commandOrdinal == 0);
    CHECK(result.commands[1].metadata.commandOrdinal == 1);
}

TEST_CASE("BattleEffectSystem transfers persistent damage memory into one cast", "[battle][effect]")
{
    const auto owner = makeUnit(1, 0, 1000, 1000);
    const auto target = makeUnit(2, 1, 1000, 1000);
    const std::vector units{ owner, target };
    const auto binding = magicBinding(88);

    RecordMaximumDamageAction record;
    record.slot = EffectStateSlot::MaximumSkillHpDamage;
    record.channel = DamageChannel::Skill;
    const auto recordRule = makeRule(
        1,
        EffectEvent::DamageResolved,
        selfSelector(),
        { stateAction(record) });

    TransferStateValueAction transfer;
    transfer.sourceSlot = EffectStateSlot::MaximumSkillHpDamage;
    transfer.destinationSlot = EffectStateSlot::CastMaximumHpDamage;
    const auto transferRule = makeRule(
        2,
        EffectEvent::UltimateCommitted,
        selfSelector(),
        { stateAction(transfer) });

    ConsumeRecordedMaximumAction consume;
    consume.slot = EffectStateSlot::CastMaximumHpDamage;
    consume.destination = StateValueDestination::DamageAmount;
    consume.clearAfterConsume = false;
    const auto consumeRule = makeRule(
        3,
        EffectEvent::MainProjectileBeforeDamage,
        hitTargetSelector(),
        { stateAction(consume) });

    BattleEffectRuleStore store;
    store.append(binding, recordRule);
    store.append(binding, transferRule);
    store.append(binding, consumeRule);
    BattleRuntimeRandom random(1);
    BattleEffectSystem system;

    auto damageContext = makeContext(
        EffectEvent::DamageResolved,
        binding,
        owner,
        units,
        DamageResultEventData{
            .origin = EffectAttackDamageOrigin{ attackProvenance(88) },
            .attackerBefore = owner,
            .defenderBefore = target,
            .defenderAfter = target,
            .finalHpDamage = 420,
            .damageKind = BattleDamageKind::Skill,
        });
    const auto recorded = system.dispatch(store, damageContext, random);
    REQUIRE(recorded.commands.size() == 1);
    CHECK(store.stateValue(binding, EffectStateSlot::MaximumSkillHpDamage) == 420);

    auto commitContext = makeContext(
        EffectEvent::UltimateCommitted,
        binding,
        owner,
        units,
        CastCommitEventData{
            .provenance = castProvenance(88),
            .targetUnitId = target.id,
        });
    const auto transferred = system.dispatch(store, commitContext, random);
    REQUIRE(transferred.commands.size() == 1);
    const auto& transferCommand = std::get<StateMachineEffectCommand>(
        transferred.commands[0].value);
    CHECK(std::holds_alternative<EvaluatedStateEffectCommand>(transferCommand.value));
    CHECK(store.stateValue(binding, EffectStateSlot::MaximumSkillHpDamage) == 0);
    CHECK(store.stateValue(
        binding,
        EffectStateSlot::CastMaximumHpDamage,
        castProvenance(88).castId.value()) == 420);

    auto laterDamage = damageContext;
    std::get<DamageResultEventData>(laterDamage.payload).finalHpDamage = 700;
    system.dispatch(store, laterDamage, random);
    CHECK(store.stateValue(binding, EffectStateSlot::MaximumSkillHpDamage) == 700);

    auto hitContext = makeContext(
        EffectEvent::MainProjectileBeforeDamage,
        binding,
        owner,
        units,
        HitEventData{
            .provenance = attackProvenance(88),
            .targetUnitId = target.id,
            .originalTargetUnitId = 2,
            .damageKind = BattleDamageKind::Skill,
        });
    const auto consumed = system.dispatch(store, hitContext, random);
    REQUIRE(consumed.commands.size() == 1);
    const auto& command = std::get<StateMachineEffectCommand>(consumed.commands[0].value);
    CHECK(std::get<StateDamageEffectCommand>(command.value).amount == 420);
    CHECK(store.stateValue(
        binding,
        EffectStateSlot::CastMaximumHpDamage,
        castProvenance(88).castId.value()) == 420);
    CHECK(store.stateValue(binding, EffectStateSlot::MaximumSkillHpDamage) == 700);

    const auto secondHit = system.dispatch(store, hitContext, random);
    REQUIRE(secondHit.commands.size() == 1);
    const auto& secondCommand = std::get<StateMachineEffectCommand>(
        secondHit.commands[0].value);
    CHECK(std::get<StateDamageEffectCommand>(secondCommand.value).amount == 420);

    store.removeCastScopedRules(castProvenance(88).castId);
    CHECK(store.stateValue(
        binding,
        EffectStateSlot::CastMaximumHpDamage,
        castProvenance(88).castId.value()) == 0);
}

TEST_CASE("BattleEffectSystem routes recorded shield output through the typed resource command", "[battle][effect][state-machine][resource][ordering]")
{
    auto owner = makeUnit(1, 0, 1000, 1000, 40, 100);
    owner.alive = true;
    const std::vector units{ owner };
    const auto binding = magicBinding(36);
    auto context = makeContext(
        EffectEvent::UltimateCommitted,
        binding,
        owner,
        units,
        CastCommitEventData{
            .provenance = castProvenance(36),
            .targetUnitId = owner.id,
        });

    ConsumeRecordedMaximumAction consume;
    consume.slot = EffectStateSlot::MaximumSkillHpDamage;
    consume.destination = StateValueDestination::ShieldAmount;
    consume.percent = 50;
    ChangeResourceAction restoreMp;
    restoreMp.resource = BattleResource::Mp;
    restoreMp.kind = ResourceChangeKind::Restore;
    restoreMp.amount.flat = 7;
    const auto rule = makeRule(
        1,
        EffectEvent::UltimateCommitted,
        selfSelector(),
        { stateAction(consume), effectAction(restoreMp) });

    BattleEffectRuleStore store;
    store.append(binding, rule);
    store.setStateValue(binding, consume.slot, 240);
    BattleRuntimeRandom random(1);

    const auto result = BattleEffectSystem{}.dispatch(store, context, random);

    REQUIRE(result.commands.size() == 2);
    const auto& shield = std::get<ChangeResourceEffectCommand>(result.commands[0].value);
    CHECK(result.commands[0].metadata.actionOrder == 0);
    CHECK(shield.resource == BattleResource::Shield);
    CHECK(shield.kind == ResourceChangeKind::Grant);
    CHECK(shield.resolvedAmount() == 120);
    const auto& mp = std::get<ChangeResourceEffectCommand>(result.commands[1].value);
    CHECK(result.commands[1].metadata.actionOrder == 1);
    CHECK(mp.resource == BattleResource::Mp);
    CHECK(mp.resolvedAmount() == 7);
    CHECK(store.stateValue(binding, consume.slot) == 0);
}

TEST_CASE("BattleEffectSystem shares marked-hit observation and permanent cast progress", "[battle][effect][stacked-status]")
{
    const auto caster = makeUnit(1, 0, 1000, 1000);
    const auto ally = makeUnit(2, 0, 1000, 1000);
    auto enemy = makeUnit(3, 1, 1000, 1000);
    enemy.statusDetails.push_back({
        .state = BattleStatusKind::SevenStarMark,
        .sourceUnitId = caster.id,
        .stacks = 1,
    });
    const std::vector units{ caster, ally, enemy };
    const auto binding = magicBinding(39);

    ModifyDamageAction ignoreDefense;
    ignoreDefense.stage = DamageModifierStage::BeforeDefense;
    ignoreDefense.channel = DamageChannel::Skill;
    ignoreDefense.operation = DamageModifierOperation::IgnoreDefensePercent;
    ignoreDefense.amount.flat = 50;
    ConsumeStatusAction consume;
    consume.status = BattleStatusKind::SevenStarMark;
    consume.source = StatusSourceMatch::EffectOwner;
    ApplyStatusAction finalStun;
    finalStun.status = BattleStatusKind::Stun;
    finalStun.durationFrames = 30;
    finalStun.quantity = NoStatusQuantity{};
    finalStun.reapplication = StatusReapplicationPolicy::KeepLongerDuration;
    consume.whenDepleted = finalStun;
    auto observer = makeRule(
        1,
        EffectEvent::HitBeforeDamage,
        hitTargetSelector(),
        { effectAction(ignoreDefense), effectAction(consume) },
        { TargetHasStateFromEffectOwnerCondition{ BattleStatusKind::SevenStarMark } });
    observer.observation = EffectObservationScope::OwnerTeamEventSource;

    BattleEffectRuleStore store;
    store.append(binding, observer);
    BattleRuntimeRandom random(1);
    BattleEffectSystem system;
    auto unrelatedAttack = attackProvenance(777);
    unrelatedAttack.cast.sourceUnitId = ally.id;
    const auto observed = system.dispatch(store, makeContext(
        EffectEvent::HitBeforeDamage,
        binding,
        ally,
        units,
        HitEventData{
            .provenance = unrelatedAttack,
            .targetUnitId = enemy.id,
            .originalTargetUnitId = enemy.id,
            .damageKind = BattleDamageKind::Skill,
        }), random);
    REQUIRE(observed.commands.size() == 2);
    CHECK(observed.commands[0].metadata.binding.ownerUnitId == caster.id);
    CHECK(std::holds_alternative<ModifyDamageEffectCommand>(observed.commands[0].value));
    CHECK(std::holds_alternative<ConsumeStatusEffectCommand>(observed.commands[1].value));

    ChangeStateValueAction progress;
    progress.slot = EffectStateSlot::PermanentCastProgress;
    progress.delta = 1;
    progress.maximum = 5;
    const auto progressRule = makeRule(
        2,
        EffectEvent::UltimateCommitted,
        selfSelector(),
        { stateAction(progress) });
    ApplyStatusAction stun;
    stun.status = BattleStatusKind::Stun;
    stun.quantity = NoStatusQuantity{};
    stun.reapplication = StatusReapplicationPolicy::KeepLongerDuration;
    EffectNumber duration;
    duration.base = EffectNumberBase::StoredStateValue;
    duration.stateSlot = EffectStateSlot::PermanentCastProgress;
    duration.percent = 2500;
    duration.flat = 25;
    duration.maximum = 150;
    stun.duration = duration;
    const auto stunRule = makeRule(
        3,
        EffectEvent::MainProjectileBeforeDamage,
        hitTargetSelector(),
        { effectAction(stun) });

    BattleEffectRuleStore progressStore;
    const auto progressBinding = magicBinding(84);
    progressStore.append(progressBinding, progressRule);
    progressStore.append(progressBinding, stunRule);
    for (int cast = 0; cast < 6; ++cast)
    {
        const auto committed = system.dispatch(progressStore, makeContext(
            EffectEvent::UltimateCommitted,
            progressBinding,
            caster,
            units,
            CastCommitEventData{
                .provenance = castProvenance(84),
                .targetUnitId = enemy.id,
            }), random);
        REQUIRE(committed.commands.size() == 1);
    }
    CHECK(progressStore.stateValue(
        progressBinding,
        EffectStateSlot::PermanentCastProgress) == 5);
    const auto hit = system.dispatch(progressStore, makeContext(
        EffectEvent::MainProjectileBeforeDamage,
        progressBinding,
        caster,
        units,
        HitEventData{
            .provenance = attackProvenance(84),
            .targetUnitId = enemy.id,
            .originalTargetUnitId = enemy.id,
            .damageKind = BattleDamageKind::Skill,
        }), random);
    REQUIRE(hit.commands.size() == 1);
    const auto& stunCommand = std::get<ApplyStatusEffectCommand>(hit.commands[0].value);
    CHECK(stunCommand.durationFrames == 150);
    CHECK(progressStore.stateValue(
        progressBinding,
        EffectStateSlot::PermanentCastProgress) == 5);
}

TEST_CASE("BattleEffectSystem binds generic consume depletion status behavior at application",
          "[battle][effect][status][binding][consume]")
{
    const auto owner = makeUnit(1, 0, 1000, 1000);
    const auto target = makeUnit(2, 1, 750, 1500);
    const std::vector units{ owner, target };

    ModifyDamageAction cap;
    cap.perspective = DamageModifierPerspective::Incoming;
    cap.stage = DamageModifierStage::Final;
    cap.channel = DamageChannel::All;
    cap.operation = DamageModifierOperation::CapSingleHitAtValue;
    cap.amount.base = EffectNumberBase::ApplicationTargetMaxHp;
    cap.amount.percent = 10;
    cap.amount.minimum = 1;
    auto persistent = makeRule(
        91,
        EffectEvent::StatusPersistent,
        { .kind = EffectSelectorKind::StatusHolder },
        { effectAction(cap) });
    persistent.observation = EffectObservationScope::StatusHolderEventSource;
    auto behavior = std::make_shared<StatusBehaviorDefinition>();
    behavior->rules.push_back(std::move(persistent));

    ApplyStatusAction depleted;
    depleted.status = BattleStatusKind::Shadowless;
    depleted.duration = EffectNumber{
        .base = EffectNumberBase::TargetMaxHp,
        .percent = 10,
        .minimum = 1,
    };
    depleted.quantity = NoStatusQuantity{};
    depleted.reapplication = StatusReapplicationPolicy::RefreshDuration;
    depleted.behavior = std::move(behavior);

    ConsumeStatusAction consume;
    consume.status = BattleStatusKind::SevenStarMark;
    consume.quantity = 1;
    consume.source = StatusSourceMatch::EffectBinding;
    consume.whenDepleted = std::move(depleted);
    const auto rule = makeRule(
        90,
        EffectEvent::HitBeforeDamage,
        hitTargetSelector(),
        { effectAction(consume) });

    const auto source = magicBinding(39);
    BattleEffectRuleStore store;
    store.append(source, rule);
    BattleRuntimeRandom random(1);
    const auto result = BattleEffectSystem{}.dispatch(
        store,
        makeContext(
            EffectEvent::HitBeforeDamage,
            source,
            owner,
            units,
            HitEventData{
                .provenance = attackProvenance(39),
                .targetUnitId = target.id,
                .originalTargetUnitId = target.id,
                .damageKind = BattleDamageKind::Skill,
            }),
        random);

    REQUIRE(result.commands.size() == 1);
    const auto& command = std::get<ConsumeStatusEffectCommand>(
        result.commands.front().value);
    CHECK(command.request.filter.producerBinding == source);
    CHECK(command.request.stacks == 1);
    REQUIRE(command.whenDepleted);
    CHECK(command.whenDepleted->durationFrames == 150);
    CHECK(command.whenDepleted->stackLimit == 1);
    REQUIRE(command.whenDepleted->behavior);
    const auto& boundNumber = std::get<ModifyDamageAction>(
        command.whenDepleted->behavior->rules.front().actions.front().value).amount;
    CHECK(boundNumber.base == EffectNumberBase::BoundRatio);
    CHECK(boundNumber.boundNumerator == target.maxHp);
    CHECK(boundNumber.boundDenominator == 1);
}

TEST_CASE("BattleEffectSystem emits the four vertical slice command shapes", "[battle][effect][ultimate]")
{
    auto owner = makeUnit(1, 0, 900, 1000, 100, 100);
    owner.star = 2;
    const auto ally = makeUnit(2, 0, 100, 2000, 30, 100);
    const auto enemy = makeUnit(3, 1, 1000, 1000, 50, 100);
    const std::vector units{ owner, ally, enemy };
    BattleRuntimeRandom random(1);
    BattleEffectSystem system;

    SECTION("青囊奇術")
    {
        ChangeResourceAction heal;
        heal.resource = BattleResource::Hp;
        heal.kind = ResourceChangeKind::Restore;
        heal.amount.base = EffectNumberBase::TargetMaxHp;
        heal.amount.percent = 7;
        EffectSelector selector;
        selector.kind = EffectSelectorKind::LowestHpAllies;
        selector.count = 1;
        const auto rule = makeRule(1, EffectEvent::UltimateCommitted, selector,
                                   { effectAction(heal) });
        BattleEffectRuleStore store;
        store.append(magicBinding(61), rule);
        const auto context = makeContext(
            EffectEvent::UltimateCommitted, magicBinding(61), owner, units,
            CastCommitEventData{ .provenance = castProvenance(61), .targetUnitId = 3 });
        const auto result = system.dispatch(store, context, random);
        REQUIRE(result.commands.size() == 1);
        const auto& command = std::get<ChangeResourceEffectCommand>(result.commands[0].value);
        CHECK(result.commands[0].metadata.targetUnitId == 2);
        CHECK(command.resolvedAmount() == 140);
    }

    SECTION("神照功")
    {
        ModifyCastAction cast;
        cast.mpCost = EffectNumber{ .flat = 75 };
        const auto rule = makeRule(1, EffectEvent::CastPlanned, selfSelector(),
                                   { effectAction(cast) });
        BattleEffectRuleStore store;
        store.append(magicBinding(58), rule);
        const auto context = makeContext(
            EffectEvent::CastPlanned, magicBinding(58), owner, units,
            CastPlanEventData{ .provenance = castProvenance(58), .preferredTargetUnitId = 3 });
        const auto result = system.dispatch(store, context, random);
        REQUIRE(result.commands.size() == 1);
        CHECK(std::get<ModifyCastEffectCommand>(result.commands[0].value).mpCost == 75);
    }

    SECTION("九陰白骨爪")
    {
        ApplyStatusAction status;
        status.status = BattleStatusKind::WitheredBone;
        status.durationFrames = 120;
        status.quantity = NoStatusQuantity{};
        status.reapplication = StatusReapplicationPolicy::Implicit;
        status.behavior = makeCatalogOwnedStatusBehavior(status);
        const auto rule = makeRule(1, EffectEvent::MainProjectileBeforeDamage,
                                   hitTargetSelector(), { effectAction(status) });
        BattleEffectRuleStore store;
        store.append(magicBinding(79), rule);
        const auto context = makeContext(
            EffectEvent::MainProjectileBeforeDamage, magicBinding(79), owner, units,
            HitEventData{
                .provenance = attackProvenance(79),
                .targetUnitId = enemy.id,
                .originalTargetUnitId = 3,
            });
        const auto result = system.dispatch(store, context, random);
        REQUIRE(result.commands.size() == 1);
        const auto& command = std::get<ApplyStatusEffectCommand>(result.commands[0].value);
        CHECK(command.status == BattleStatusKind::WitheredBone);
        REQUIRE(command.behavior);
        const auto& persistent = command.behavior->rules.front();
        CHECK(std::get<ModifyDamageAction>(persistent.actions[0].value).amount.flat == 25);
        CHECK(std::get<ModifyHealTransactionAction>(persistent.actions[1].value).percent == 25);
    }

    SECTION("五虎斷門刀")
    {
        ModifyAttackAction attack;
        attack.pattern.kind = AttackPatternKind::Fan;
        attack.pattern.projectileCount = 5;
        attack.strengthPct = 60;
        attack.through = true;
        attack.mainProjectile = true;
        attack.sameTargetHitLimit = 1;
        const auto rule = makeRule(
            1,
            EffectEvent::CastPlanned,
            EffectSelector{ .kind = EffectSelectorKind::OriginalAttackTarget },
            { effectAction(attack) });
        BattleEffectRuleStore store;
        store.append(magicBinding(59), rule);
        const auto context = makeContext(
            EffectEvent::CastPlanned, magicBinding(59), owner, units,
            CastPlanEventData{ .provenance = castProvenance(59), .preferredTargetUnitId = 3 });
        const auto result = system.dispatch(store, context, random);
        REQUIRE(result.commands.size() == 1);
        const auto& command = std::get<ModifyAttackEffectCommand>(result.commands[0].value);
        CHECK(command.pattern.kind == AttackPatternKind::Fan);
        CHECK(command.pattern.projectileCount == 5);
        CHECK(command.strengthPct == 60);
        CHECK(command.sameTargetHitLimit == 1);
    }
}

TEST_CASE("BattleEffectSystem resolves a single living ally as an attack source",
          "[battle][effect][attack_source]")
{
    const auto owner = makeUnit(1, 0, 1000, 1000, 100, 100, Pointf{ 0.0f, 0.0f });
    auto ally = makeUnit(2, 0, 1000, 1000, 30, 100, Pointf{ 20.0f, 40.0f });
    ally.magicIds.push_back(62);
    const auto enemy = makeUnit(3, 1, 1000, 1000, 50, 100, Pointf{ 100.0f, 0.0f });
    const std::vector units{ owner, ally, enemy };

    ModifyAttackAction attack;
    attack.pattern.kind = AttackPatternKind::Preserve;
    attack.targets = AttackTargetPolicy::SameTarget;
    attack.addToBaseAttack = true;
    attack.source = EffectSelector{
        .kind = EffectSelectorKind::Allies,
        .count = 1,
        .excludeOwner = true,
        .requiredBoundMagic = true,
    };
    const auto rule = makeRule(
        1,
        EffectEvent::UltimateCommitted,
        EffectSelector{ .kind = EffectSelectorKind::OriginalAttackTarget },
        { effectAction(attack) });
    BattleEffectRuleStore store;
    store.append(magicBinding(62), rule);
    const auto context = makeContext(
        EffectEvent::UltimateCommitted,
        magicBinding(62),
        owner,
        units,
        CastCommitEventData{ .provenance = castProvenance(62), .targetUnitId = 3 });
    BattleRuntimeRandom random(1);

    const auto result = BattleEffectSystem().dispatch(store, context, random);

    REQUIRE(result.commands.size() == 1);
    const auto& command = std::get<ModifyAttackEffectCommand>(result.commands[0].value);
    REQUIRE(command.source);
    CHECK(command.source->unitId == 2);
    CHECK(command.source->position.x == 20.0f);
    CHECK(command.source->position.y == 40.0f);
}

TEST_CASE("BattleEffectSystem couple-blade branch replaces its solo fallback",
          "[battle][effect][attack_source][couple_blade]")
{
    const auto owner = makeUnit(1, 0, 1000, 1000, 100, 100, Pointf{ 0.0f, 0.0f });
    auto ally = makeUnit(2, 0, 1000, 1000, 30, 100, Pointf{ 20.0f, 40.0f });
    ally.magicIds.push_back(62);
    const auto enemy = makeUnit(3, 1, 1000, 1000, 50, 100, Pointf{ 100.0f, 0.0f });

    ModifyAttackAction combined;
    combined.pattern.kind = AttackPatternKind::Preserve;
    combined.strengthPct = 100;
    combined.mainProjectile = true;
    combined.targets = AttackTargetPolicy::SameTarget;
    combined.addToBaseAttack = true;
    combined.propagation = CastPropagationPolicy::SuppressUltimateRules;
    combined.source = EffectSelector{
        .kind = EffectSelectorKind::Allies,
        .count = 1,
        .excludeOwner = true,
        .requiredBoundMagic = true,
    };
    auto fallback = combined;
    fallback.strengthPct = 50;
    fallback.mainProjectile = false;
    fallback.source.reset();

    auto branch = std::make_shared<ConditionalEffectAction>();
    branch->conditions = { OtherLivingAllyUsesBoundMagicCondition{} };
    branch->whenTrue = { effectAction(combined) };
    branch->whenFalse = { effectAction(fallback) };
    const auto rule = makeRule(
        1,
        EffectEvent::UltimateCommitted,
        EffectSelector{ .kind = EffectSelectorKind::OriginalAttackTarget },
        { effectAction(branch) });
    const auto dispatch = [&](const std::vector<EffectUnitSnapshot>& units)
    {
        BattleEffectRuleStore store;
        store.append(magicBinding(62), rule);
        const auto context = makeContext(
            EffectEvent::UltimateCommitted,
            magicBinding(62),
            owner,
            units,
            CastCommitEventData{
                .provenance = castProvenance(62),
                .targetUnitId = enemy.id,
            });
        BattleRuntimeRandom random(1);
        return BattleEffectSystem().dispatch(store, context, random);
    };

    const auto withPartner = dispatch({ owner, ally, enemy });
    REQUIRE(withPartner.commands.size() == 1);
    const auto& combinedCommand = std::get<ModifyAttackEffectCommand>(
        withPartner.commands.front().value);
    REQUIRE(combinedCommand.source);
    CHECK(combinedCommand.source->unitId == ally.id);
    CHECK(combinedCommand.strengthPct == 100);
    CHECK(combinedCommand.mainProjectile);

    const auto solo = dispatch({ owner, enemy });
    REQUIRE(solo.commands.size() == 1);
    const auto& fallbackCommand = std::get<ModifyAttackEffectCommand>(
        solo.commands.front().value);
    CHECK_FALSE(fallbackCommand.source);
    CHECK(fallbackCommand.strengthPct == 50);
    CHECK_FALSE(fallbackCommand.mainProjectile);
}

static_assert(std::variant_size_v<EffectCommandValue> == 16);

TEST_CASE("BattleEffectRuleStore_BlinkAttackTargetModeIsOwnerScoped", "[battle][effects][rule_store]")
{
    Battle::BattleEffectRuleStore store;

    CHECK_FALSE(store.blinkAttackUsesWeakestTarget(7));
    CHECK_FALSE(store.blinkAttackUsesWeakestTarget(8));

    store.advanceBlinkAttackTargetMode(7);
    CHECK(store.blinkAttackUsesWeakestTarget(7));
    CHECK_FALSE(store.blinkAttackUsesWeakestTarget(8));

    store.advanceBlinkAttackTargetMode(7);
    CHECK_FALSE(store.blinkAttackUsesWeakestTarget(7));

    store.advanceBlinkAttackTargetMode(8);
    CHECK_FALSE(store.blinkAttackUsesWeakestTarget(7));
    CHECK(store.blinkAttackUsesWeakestTarget(8));
}

TEST_CASE("BattleEffectSystem binds neutralize-force MP recovery to its producer star",
          "[battle][effect][status][numbers]")
{
    for (int star : { 1, 2, 3 })
    {
        CAPTURE(star);
        auto owner = makeUnit(1, 0, 100, 100);
        owner.star = star;
        auto target = makeUnit(2, 1, 100, 100);
        target.star = 5;
        const std::vector units{ owner, target };
        ApplyStatusAction status;
        status.status = BattleStatusKind::NeutralizeForce;
        status.quantity = SetStatusTriggerCharges{ 1 };
        status.neutralizeMpRecovery = EffectNumber{
            .base = EffectNumberBase::SourceStar,
            .flat = 40,
            .percent = 1000,
        };
        status.behavior = makeCatalogOwnedStatusBehavior(status);
        const auto source = magicBinding(7);
        BattleEffectRuleStore store;
        store.append(source, makeRule(1, EffectEvent::MainProjectileBeforeDamage,
                                     hitTargetSelector(), { effectAction(status) }));
        BattleRuntimeRandom random(1);
        const auto result = BattleEffectSystem{}.dispatch(
            store,
            makeContext(EffectEvent::MainProjectileBeforeDamage, source, owner, units,
                        HitEventData{
                            .provenance = attackProvenance(7),
                            .targetUnitId = target.id,
                            .originalTargetUnitId = target.id,
                            .damageKind = BattleDamageKind::Skill,
                        }),
            random);
        REQUIRE(result.commands.size() == 1);
        const auto& application = std::get<ApplyStatusEffectCommand>(result.commands.front().value);
        REQUIRE(application.behavior);
        const auto& recovery = std::get<ChangeResourceAction>(
            application.behavior->rules.front().actions.front().value);
        CHECK(recovery.amount.base == EffectNumberBase::BoundRatio);
        CHECK(recovery.amount.boundNumerator == star);
        CHECK(recovery.amount.boundDenominator == 1);
        const auto context = makeContext(
            EffectEvent::HitBeforeDamage, source, target, units,
            HitEventData{
                .provenance = attackProvenance(7),
                .targetUnitId = owner.id,
                .originalTargetUnitId = owner.id,
                .damageKind = BattleDamageKind::Skill,
            });
        CHECK(BattleEffectSystem::evaluateNumber(recovery.amount, context, owner) == 40 + 10 * star);
    }
}

namespace
{
std::vector<EffectRule> namedRules(const YAML::Node& configured)
{
    std::vector<GameplayEffect> effects;
    std::vector<EffectRule> rules;
    std::uint64_t id{};
    REQUIRE(parseGameplayEffects(configured, effects, rules, id, "組合效果測試"));
    return rules;
}

std::vector<EffectRule> magicContractRules(int magicId)
{
    const auto contracts = YAML::LoadFile("tests/data/gameplay-effect-contracts.yaml");
    for (const auto& entry : contracts)
    {
        if (entry["來源"].as<std::string>() == "magic:" + std::to_string(magicId))
            return namedRules(entry["效果"]);
    }
    FAIL("找不到武功契約");
    return {};
}
}

TEST_CASE("Huanhua heals only two lowest HP allies using the caster star",
          "[battle][effect][ultimate][healing]")
{
    const auto rules = magicContractRules(134);
    REQUIRE(rules.size() == 1);
    for (int star : { 1, 2, 3 })
    {
        CAPTURE(star);
        auto owner = makeUnit(1, 0, 900, 1000);
        owner.star = star;
        auto first = makeUnit(2, 0, 100, 2000);
        first.star = 5;
        const auto second = makeUnit(3, 0, 100, 1000);
        const auto third = makeUnit(4, 0, 200, 1000);
        const auto enemy = makeUnit(5, 1, 1, 1000);
        const std::vector units{ owner, first, second, third, enemy };
        const auto source = magicBinding(134);
        BattleEffectRuleStore store;
        store.append(source, rules.front());
        BattleRuntimeRandom random(1);
        const auto result = BattleEffectSystem{}.dispatch(
            store, makeContext(EffectEvent::AttackCommitted, source, owner, units,
                               CastCommitEventData{ .provenance = castProvenance(134), .targetUnitId = 5 }),
            random);
        REQUIRE(result.commands.size() == 4);
        int firstHealing{};
        int secondHealing{};
        int cleanses{};
        for (const auto& command : result.commands)
        {
            const int target = command.metadata.targetUnitId;
            REQUIRE((target == 2 || target == 3));
            if (const auto* heal = std::get_if<ChangeResourceEffectCommand>(&command.value))
            {
                (target == 2 ? firstHealing : secondHealing) += heal->resolvedAmount();
            }
            else
            {
                ++cleanses;
            }
        }
        CHECK(firstHealing == 30 * star + 120);
        CHECK(secondHealing == 30 * star + 60);
        CHECK(cleanses == 2);
    }
}

TEST_CASE("Yijin shield scales with the caster star", "[battle][effect][ultimate][shield]")
{
    const auto rules = magicContractRules(108);
    REQUIRE(rules.size() == 2);
    for (int star : { 1, 2, 3 })
    {
        CAPTURE(star);
        auto owner = makeUnit(1, 0, 100, 100);
        owner.star = star;
        const std::vector units{ owner };
        const auto source = magicBinding(108);
        BattleEffectRuleStore store;
        for (const auto& rule : rules) store.append(source, rule);
        BattleRuntimeRandom random(1);
        const auto result = BattleEffectSystem{}.dispatch(
            store, makeContext(EffectEvent::AttackCommitted, source, owner, units,
                               CastCommitEventData{ .provenance = castProvenance(108), .targetUnitId = 1 }),
            random);
        REQUIRE(result.commands.size() == 2);
        const auto& shield = std::get<ChangeResourceEffectCommand>(result.commands.back().value);
        CHECK(result.commands.back().metadata.targetUnitId == owner.id);
        CHECK(shield.resource == BattleResource::Shield);
        CHECK(shield.resolvedAmount() == 70 * star);
    }
}

TEST_CASE("Composed resource formulas issue one transaction before healing modifiers", "[battle][effect][composition]")
{
    for (const auto resource : { BattleResource::Hp, BattleResource::Shield })
    {
        const auto configured = resource == BattleResource::Hp
            ? "[{類型: 出招治療自身, 固定治療: 11, 每星治療: 30, 生命治療百分比: 6}]"
            : "[{類型: 出招護盾, 固定護盾: 11, 每星護盾: 30, 生命護盾百分比: 6}]";
        const auto rules = namedRules(YAML::Load(configured));
        for (int star : { 1, 2, 3 })
        {
            auto owner = makeUnit(1, 0, 100, 1500);
            owner.star = star;
            const std::vector units{owner};
            const auto source = magicBinding(108);
            BattleEffectRuleStore store;
            for (const auto& rule : rules) store.append(source, rule);
            BattleRuntimeRandom random(1);
            const auto result = BattleEffectSystem{}.dispatch(
                store, makeContext(EffectEvent::AttackCommitted, source, owner, units,
                    CastCommitEventData{.provenance = castProvenance(108), .targetUnitId = 1}), random);
            REQUIRE(result.commands.size() == 1);
            const auto& command = std::get<ChangeResourceEffectCommand>(result.commands.front().value);
            CHECK(command.resource == resource);
            CHECK(command.resolvedAmount() == 11 + 30 * star + 90);
        }
    }
}

TEST_CASE("Independent cast attributes preserve refresh duration and stacking caps", "[battle][effect][composition]")
{
    const auto rules = namedRules(YAML::Load(R"(
- {類型: 出招臨時屬性加成, 屬性: 格擋率, 百分比: 25, 持續幀數: 60}
- {類型: 出招臨時屬性加成, 屬性: 速度, 百分比: 40, 持續幀數: 100}
- {類型: 出招疊加屬性, 屬性: 暴擊率, 每層百分比: 7, 層數上限: 5}
- {類型: 出招疊加屬性, 屬性: 暴擊傷害, 每層百分比: 9, 層數上限: 3}
)"));
    REQUIRE(rules.size() == 4);
    const auto& block = std::get<ModifyAttributeAction>(rules[0].actions.front().value);
    const auto& speed = std::get<ModifyAttributeAction>(rules[1].actions.front().value);
    const auto& critical = std::get<ModifyAttributeAction>(rules[2].actions.front().value);
    const auto& criticalDamage = std::get<ModifyAttributeAction>(rules[3].actions.front().value);
    CHECK(block.attribute == BattleAttribute::BlockChance);
    CHECK(block.operation == AttributeOperation::PercentagePointAdd);
    CHECK(block.durationFrames == 60);
    CHECK(block.stack == EffectStackPolicy::Refresh);
    CHECK(speed.operation == AttributeOperation::PercentAdd);
    CHECK(speed.durationFrames == 100);
    CHECK(critical.stack == EffectStackPolicy::AddStack);
    CHECK(critical.stackLimit == 5);
    CHECK(criticalDamage.attribute == BattleAttribute::CriticalDamage);
    CHECK(criticalDamage.stackLimit == 3);
}

TEST_CASE("Decoupled hit weakening retains its original event and modifier stage", "[battle][effect][composition]")
{
    const auto rules = namedRules(YAML::Load(R"(
- {類型: 主彈命中弱化傷害, 傷害百分比: -20, 持續幀數: 90}
- {類型: 命中削弱敵方傷害, 傷害百分比: -45, 持續幀數: 70}
)"));
    REQUIRE(rules.size() == 2);
    const auto& before = std::get<ModifyDamageAction>(rules[0].actions.front().value);
    const auto& after = std::get<ModifyDamageAction>(rules[1].actions.front().value);
    CHECK(rules[0].event == EffectEvent::MainProjectileBeforeDamage);
    CHECK(rules[0].conditions.empty());
    CHECK(before.stage == DamageModifierStage::Final);
    CHECK(before.stack == EffectStackPolicy::Refresh);
    CHECK(rules[1].event == EffectEvent::DamageResolved);
    CHECK(rules[1].conditions.size() == 2);
    CHECK(after.stage == DamageModifierStage::BeforeDefense);
    CHECK(after.stack == EffectStackPolicy::Independent);
}

TEST_CASE("Seven star volley counts allied combo casts and fires once per living member", "[battle][effect][seven-star]")
{
    const auto rules = namedRules(YAML::Load(R"(
- {類型: 七星歸一, 出招次數: 7, 武功威力: 300, 特效編號: 48}
)"));
    std::vector units{makeUnit(1, 0, 100, 100), makeUnit(2, 0, 100, 100),
        makeUnit(3, 0, 100, 100), makeUnit(4, 1, 100, 100)};
    units[0].comboIds.push_back(12);
    units[1].comboIds.push_back(12);
    units[3].comboIds.push_back(12);
    units[0].attack = 100;
    units[1].attack = 240;
    const EffectSourceBinding binding{.kind = EffectSourceKind::Combo, .sourceId = 12,
        .ownerUnitId = 1, .sourceTeam = 0};
    BattleEffectRuleStore store;
    for (int owner : {1, 2})
    {
        auto memberBinding = binding;
        memberBinding.ownerUnitId = owner;
        for (const auto& rule : rules) store.append(memberBinding, rule);
    }
    BattleRuntimeRandom random(1);
    const auto dispatch = [&](int caster, CastPropagationPolicy policy = CastPropagationPolicy::SourceRules)
    {
        auto provenance = castProvenance(39, policy);
        provenance.sourceUnitId = caster;
        return BattleEffectSystem{}.dispatch(store,
            makeContext(EffectEvent::AttackCommitted, binding, units[caster - 1], units,
                CastCommitEventData{.provenance = provenance, .targetUnitId = 4}), random);
    };
    for (int i = 0; i < 6; ++i) CHECK(dispatch(i % 2 + 1).commands.empty());
    CHECK(dispatch(3).commands.empty()); // 無關友軍不計數。
    CHECK(dispatch(4).commands.empty()); // 敵方同門不計數。
    CHECK(dispatch(1, CastPropagationPolicy::NoEffectRules).commands.empty());
    const auto volley = dispatch(2);
    REQUIRE(volley.commands.size() == 2);
    for (const auto& command : volley.commands)
    {
        const auto& attack = std::get<ModifyAttackEffectCommand>(command.value);
        REQUIRE(attack.source);
        CHECK(attack.source->unitId == command.metadata.binding.ownerUnitId);
        CHECK(attack.source->attack == units[attack.source->unitId - 1].attack);
        CHECK_FALSE(attack.damageOverride);
        CHECK(command.metadata.targetUnitId == 4);
        CHECK(attack.propagation == CastPropagationPolicy::NoEffectRules);
        CHECK(attack.independentProjectile.has_value());
    }
    for (int i = 0; i < 3; ++i) CHECK(dispatch(1).commands.empty());
    units[1].alive = false;
    units[1].hp = 0;
    for (int i = 0; i < 3; ++i) CHECK(dispatch(1).commands.empty());
    const auto survivorVolley = dispatch(1);
    REQUIRE(survivorVolley.commands.size() == 1);
    CHECK(survivorVolley.commands[0].metadata.binding.ownerUnitId == 1);
}

TEST_CASE("Separate aura components keep aligned periods and exclude their owner", "[battle][effect][composition]")
{
    const auto rules = namedRules(YAML::Load(R"(
- {類型: 友軍治療光環, 半徑格數: 6, 間隔幀數: 3, 治療點數: 25}
- {類型: 友軍減冷卻光環, 半徑格數: 6, 間隔幀數: 3, 冷卻百分比: 30}
)"));
    const auto owner = makeUnit(1, 0, 100, 100);
    auto ally = makeUnit(2, 0, 50, 100);
    const auto enemy = makeUnit(3, 1, 100, 100);
    const std::vector units{owner, ally, enemy};
    const auto source = magicBinding(108);
    BattleEffectRuleStore store;
    for (const auto& rule : rules) store.append(source, rule);
    BattleRuntimeRandom random(1);
    for (int tick = 1; tick <= 6; ++tick)
    {
        const auto result = BattleEffectSystem{}.dispatch(
            store, makeContext(EffectEvent::FrameAdvanced, source, owner, units,
                FrameTickEventData{.deltaFrames = 1}), random);
        if (tick % 3 != 0) CHECK(result.commands.empty());
        else
        {
            REQUIRE(result.commands.size() == 2);
            for (const auto& command : result.commands) CHECK(command.metadata.targetUnitId == ally.id);
            const auto& heal = std::get<ChangeResourceEffectCommand>(result.commands[0].value);
            CHECK(heal.resource == BattleResource::Hp);
            CHECK(heal.healKind == EffectHealKind::Aura);
            CHECK(heal.resolvedAmount() == 25);
            CHECK(std::get<ChangeResourceEffectCommand>(result.commands[1].value).resource == BattleResource::ActiveCooldown);
        }
    }
}
