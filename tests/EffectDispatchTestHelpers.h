#pragma once

#include "BattleRuntimeRecordTestHelpers.h"
#include "battle/BattleEffectEventBridge.h"

#include <algorithm>

namespace KysChess::Battle::Test
{

struct StatusBehaviorFixture
{
    ActiveStatusBehaviorView view;
    const BattleStatusEffectState* effects{};
};

inline StatusBehaviorFixture statusBehaviorFixture(
    ActiveStatusBehaviorView view, const BattleStatusEffectState* effects)
{
    return { std::move(view), effects };
}

inline BattleRuntimeState dispatchRuntimeFixture(
    const EffectEventContext& event, std::span<const StatusBehaviorFixture> behaviors)
{
    BattleRuntimeState state;
    state.movement.frame = event.header.frame;
    state.gridTransform.tileWidth = event.header.battle.tileWidth();
    for (const auto& snapshot : event.header.battle.units())
    {
        BattleRuntimeUnit unit;
        unit.id = snapshot.id;
        unit.team = snapshot.team;
        unit.alive = snapshot.alive;
        unit.vitals = { snapshot.hp, snapshot.maxHp, snapshot.mp, snapshot.maxMp };
        unit.stats.attack = snapshot.attack;
        unit.shield = snapshot.shield;
        unit.invincible = snapshot.invincible ? 1 : 0;
        unit.motion.position = snapshot.position;
        appendRuntimeRecord(state.units, std::move(unit));
        auto& effects = state.units.require(snapshot.id).status.effects;
        effects.statusShield = snapshot.statusShield;
        effects.staggerShield = snapshot.staggerShield;
    }
    for (const auto& fixture : behaviors)
    {
        auto& effects = state.units.require(fixture.view.holderUnitId).status.effects;
        if (fixture.effects) effects = *fixture.effects;
    }
    for (const auto& fixture : behaviors)
    {
        const auto& view = fixture.view;
        if (fixture.effects) continue;
        auto& effects = state.units.require(view.holderUnitId).status.effects;
        if (std::ranges::find(effects.statuses, view.appliedSequence,
                &BattleStatusContribution::appliedSequence) != effects.statuses.end()) continue;
        EffectCommandMetadata producer{
            .binding = view.binding,
            .ruleId = view.producerRuleId,
            .ruleOrder = view.producerRuleOrder,
            .authoredActionOrder = view.producerActionOrder,
            .targetUnitId = view.holderUnitId,
        };
        const auto provenance = BattleEffectCommandSystem::statusProducerProvenance(producer);
        effects.statuses.push_back({
            .kind = view.kind,
            .producer = provenance.producer,
            .producerFamily = provenance.producerFamily,
            .behavior = view.behavior,
            .behaviorRuntime = *view.runtime,
            .sourceUnitId = view.sourceUnitId,
            .remainingFrames = 120,
            .maximumFrames = 120,
            .stacks = view.quantity,
            .origin = provenance.origin,
            .appliedSequence = view.appliedSequence,
        });
        effects.nextStatusSequence = std::max(effects.nextStatusSequence, view.appliedSequence + 1);
    }
    return state;
}

inline BattleEffectDispatchResult dispatchMergedFixture(
    BattleEffectRuleStore& store, const EffectEventContext& event,
    BattleRuntimeRandom& random, std::span<const StatusBehaviorFixture> fixtures,
    StatusBehaviorDispatchFilter filter = StatusBehaviorDispatchFilter::All,
    bool includeAllFrameOwners = false)
{
    auto state = dispatchRuntimeFixture(event, fixtures);
    state.effectRules = store;
    BattleEffectDispatchPrediction prediction(state);
    std::vector<ActiveStatusBehaviorView> views;
    for (const auto& fixture : fixtures) views.push_back(fixture.view);
    return BattleEffectSystem().dispatchMerged(
        store, event, random, views, filter, includeAllFrameOwners, &prediction.hooks());
}

inline BattleEffectDispatchResult dispatchStatusFixture(
    const EffectEventContext& event, BattleRuntimeRandom& random,
    std::span<const StatusBehaviorFixture> fixtures,
    StatusBehaviorDispatchFilter filter = StatusBehaviorDispatchFilter::All)
{
    BattleEffectRuleStore store;
    return dispatchMergedFixture(store, event, random, fixtures, filter);
}

}  // namespace KysChess::Battle::Test
