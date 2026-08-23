#include "battle/BattleEffectSystem.h"
#include "battle/BattleRuntimeRandom.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

using namespace KysChess;
using namespace KysChess::Battle;

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
EffectEventContext makeContext(EffectEvent event,
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

TEST_CASE("BattleEffectSystem emits one poison application and damage transaction per source layer",
          "[battle][effect][status][poison_explosion]")
{
    auto owner = makeUnit(1, 0, 0, 1000);
    owner.alive = false;
    owner.statusDetails.push_back({
        .state = "毒爆",
        .sourceUnitId = owner.id,
        .stacks = 3,
        .potency = 240,
    });
    const auto enemy = makeUnit(2, 1, 1000, 1000);
    const std::vector units{ owner, enemy };

    EffectNumber layerCount;
    layerCount.base = EffectNumberBase::SourceStatusStacks;
    layerCount.status = "毒爆";
    layerCount.percent = 100;
    layerCount.minimum = 1;
    DealDamageAction damage;
    damage.amount.base = EffectNumberBase::SourceStatusPotency;
    damage.amount.status = "毒爆";
    damage.amount.percent = 100;
    damage.transactionCount = layerCount;
    damage.kind = BattleDamageKind::Pure;
    ApplyStatusAction poison;
    poison.status = BattleStatusKind::Poison;
    poison.durationFrames = 120;
    poison.applicationCount = layerCount;
    poison.stacks = 4;
    poison.potency.flat = 10;
    poison.stack = EffectStackPolicy::Replace;
    poison.stackLimit = 4;
    const auto rule = makeRule(
        1,
        EffectEvent::UnitDied,
        EffectSelector{ .kind = EffectSelectorKind::Enemies, .count = 1 },
        { effectAction(damage), effectAction(poison) },
        { SourceHasStateCondition{ "毒爆" } });

    BattleEffectRuleStore store;
    store.append(magicBinding(95), rule);
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

    const auto dispatched = BattleEffectSystem{}.dispatch(store, context, random);
    REQUIRE(dispatched.commands.size() == 4);
    const auto& damageCommand = std::get<DealDamageEffectCommand>(
        dispatched.commands[0].value);
    CHECK(damageCommand.amount == 240);
    CHECK(damageCommand.transactionCount == 3);
    for (std::size_t index = 1; index < dispatched.commands.size(); ++index)
    {
        CHECK(std::holds_alternative<ApplyStatusEffectCommand>(
            dispatched.commands[index].value));
        CHECK(dispatched.commands[index].metadata.commandOrdinal == index);
    }

    auto noLayers = owner;
    noLayers.statusDetails.clear();
    const std::vector noLayerUnits{ noLayers, enemy };
    auto noLayerContext = makeContext(
        EffectEvent::UnitDied,
        magicBinding(95),
        noLayers,
        noLayerUnits,
        DeathEventData{
            .deadBefore = noLayers,
            .deadAfter = noLayers,
            .cause = EffectEnvironmentDamageOrigin{},
        });
    CHECK(BattleEffectSystem{}.dispatch(store, noLayerContext, random).commands.empty());
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
    REQUIRE(command.selectedSourceUnitIds.size() == 1);
    CHECK(command.selectedSourceUnitIds.front() != owner.id);
    CHECK(command.selectedSourceUnitIds.front() != deadEnemy.id);
    CHECK((command.selectedSourceUnitIds.front() == ally.id
        || command.selectedSourceUnitIds.front() == enemy.id));
    CHECK(command.outputValue == 1);

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
    CHECK(emptyCommand.selectedSourceUnitIds.empty());
    CHECK(emptyCommand.outputValue == 0);
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
    CHECK(command.selectedSourceUnitIds.empty());
    CHECK(command.outputValue == 0);
}

TEST_CASE("BattleEffectSystem gates rules by conditions chance propagation and maximum count", "[battle][effect]")
{
    auto owner = makeUnit(1, 0, 400, 1000);
    owner.statusDetails.push_back({ .state = "真氣" });
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
            MagicIdEqualsCondition{ 77 },
            IsMainProjectileCondition{},
            IsRootAttackCondition{},
            SourceHpRatioAtMostCondition{ 50 },
            TargetHpRatioAtMostCondition{ 25 },
            SourceHasStateCondition{ "真氣" },
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
    owner.statusDetails.push_back({ .state = "無影" });
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
        { IsRootAttackCondition{}, SourceHasStateCondition{ "無影" } });
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
    CHECK(echoCommand.action.strengthPct == 50);
    CHECK(echoCommand.action.propagation == CastPropagationPolicy::NoEffectRules);

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
        { IsUltimateCondition{}, MagicIdEqualsCondition{ 133 } });

    BattleEffectRuleStore store;
    store.append(magicBinding(133), rule);
    BattleRuntimeRandom random(1);
    const auto result = BattleEffectSystem{}.dispatch(store, context, random);
    REQUIRE(result.commands.size() == 2);
    CHECK(result.commands[0].metadata.targetUnitId == 3);
    CHECK(std::get<ChangeResourceEffectCommand>(result.commands[0].value).action.resource == BattleResource::Mp);
    CHECK(result.commands[1].metadata.targetUnitId == 2);
    CHECK(std::get<ChangeResourceEffectCommand>(result.commands[1].value).action.resource == BattleResource::Shield);
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
    CHECK(transferCommand.stateValueBefore == 420);
    CHECK(transferCommand.outputValue == 420);
    CHECK(transferCommand.stateValueAfter == 0);
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
    CHECK(command.stateValueBefore == 420);
    CHECK(command.outputValue == 420);
    CHECK(command.stateValueAfter == 0);
    CHECK(store.stateValue(
        binding,
        EffectStateSlot::CastMaximumHpDamage,
        castProvenance(88).castId.value()) == 0);
    CHECK(store.stateValue(binding, EffectStateSlot::MaximumSkillHpDamage) == 700);
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
    CHECK(shield.action.resource == BattleResource::Shield);
    CHECK(shield.action.kind == ResourceChangeKind::Grant);
    CHECK(shield.amount == 120);
    const auto& mp = std::get<ChangeResourceEffectCommand>(result.commands[1].value);
    CHECK(result.commands[1].metadata.actionOrder == 1);
    CHECK(mp.action.resource == BattleResource::Mp);
    CHECK(mp.amount == 7);
    CHECK(store.stateValue(binding, consume.slot) == 0);
}

TEST_CASE("BattleEffectSystem shares marked-hit observation and permanent cast progress", "[battle][effect][stacked-status]")
{
    const auto caster = makeUnit(1, 0, 1000, 1000);
    const auto ally = makeUnit(2, 0, 1000, 1000);
    auto enemy = makeUnit(3, 1, 1000, 1000);
    enemy.statusDetails.push_back({
        .state = "七星",
        .sourceUnitId = caster.id,
        .stacks = 1,
        .potency = 50,
        .secondaryPotency = 30,
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
    finalStun.stack = EffectStackPolicy::Refresh;
    consume.whenDepleted = finalStun;
    auto observer = makeRule(
        1,
        EffectEvent::HitBeforeDamage,
        hitTargetSelector(),
        { effectAction(ignoreDefense), effectAction(consume) },
        { TargetHasStateFromEffectOwnerCondition{ "七星" } });
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
    stun.stack = EffectStackPolicy::Refresh;
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
    CHECK(stunCommand.evaluatedDurationFrames == 150);
    CHECK(progressStore.stateValue(
        progressBinding,
        EffectStateSlot::PermanentCastProgress) == 5);
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
        CHECK(command.amount == 140);
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
        status.potency.flat = 25;
        status.secondaryPotency.flat = 75;
        status.stack = EffectStackPolicy::Refresh;
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
        CHECK(command.action.status == BattleStatusKind::WitheredBone);
        CHECK(command.potency == 25);
        CHECK(command.secondaryPotency == 75);
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
        CHECK(command.action.pattern.kind == AttackPatternKind::Fan);
        CHECK(command.action.pattern.projectileCount == 5);
        CHECK(command.action.strengthPct == 60);
        CHECK(command.action.sameTargetHitLimit == 1);
    }
}

TEST_CASE("BattleEffectSystem resolves a single living ally as an attack source",
          "[battle][effect][attack_source]")
{
    const auto owner = makeUnit(1, 0, 1000, 1000, 100, 100, Pointf{ 0.0f, 0.0f });
    auto ally = makeUnit(2, 0, 1000, 1000, 30, 100, Pointf{ 20.0f, 40.0f });
    ally.magicIds.insert(62);
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
        .requiredMagicId = 62,
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
    ally.magicIds.insert(62);
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
        .requiredMagicId = 62,
    };
    auto fallback = combined;
    fallback.strengthPct = 50;
    fallback.mainProjectile = false;
    fallback.source.reset();

    auto branch = std::make_shared<ConditionalEffectAction>();
    branch->conditions = { OtherLivingAllyUsesMagicCondition{ 62 } };
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
    CHECK(combinedCommand.action.strengthPct == 100);
    CHECK(combinedCommand.action.mainProjectile);

    const auto solo = dispatch({ owner, enemy });
    REQUIRE(solo.commands.size() == 1);
    const auto& fallbackCommand = std::get<ModifyAttackEffectCommand>(
        solo.commands.front().value);
    CHECK_FALSE(fallbackCommand.source);
    CHECK(fallbackCommand.action.strengthPct == 50);
    CHECK_FALSE(fallbackCommand.action.mainProjectile);
}

static_assert(std::variant_size_v<EffectCommandValue> == 13);
