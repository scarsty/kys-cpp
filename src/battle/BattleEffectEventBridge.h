#pragma once

#include "BattleDamageSystem.h"
#include "BattleRuntimeEffects.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace KysChess::Battle
{

struct BattleRuntimeState;

// The caller owns the ordering policy.  In particular, eventOrdinal is never
// inferred from the frame or from an unrelated transaction identifier.
struct BattleEffectEventHeaderInput
{
    int frame{};
    std::uint64_t eventOrdinal{};
    int ownerUnitId = -1;
    EffectFormulaInputs formulaInputs;
    std::optional<int> executionFrame;
};

// BattleCastLifecycle intentionally tracks only lineage and aggregate values.
// Event-time targeting and resource snapshots therefore remain required input
// at the boundary that converts a lifecycle event to an effect event.
struct BattleCastLifecycleEffectInput
{
    int originalTargetUnitId = -1;
    EffectResourcesBeforeCastSnapshot resourcesBeforeCast;
};

// BattleDamageTransactionResult contains the committed result, but it cannot
// reconstruct either pre-transaction unit snapshot or the two earlier damage
// stages.  Requiring them here prevents evaluation-time live reads from being
// mistaken for transaction snapshots.
struct BattleDamageResolvedEffectInput
{
    std::uint64_t transactionId{};
    std::optional<EffectUnitSnapshot> attackerBefore;
    EffectUnitSnapshot defenderBefore;
    int rawDamage{};
    int resolvedDamage{};
};

// Owns every unit snapshot referenced by the context's read view.  context()
// returns a cheap typed value whose span remains valid while this object lives.
class BattleEffectOwnedEvent
{
public:
    BattleEffectOwnedEvent(
        const BattleRuntimeState& runtime,
        BattleEffectEventHeaderInput header,
        EffectEvent event,
        EffectEventPayload payload);

    BattleEffectOwnedEvent(const BattleEffectOwnedEvent&) = delete;
    BattleEffectOwnedEvent& operator=(const BattleEffectOwnedEvent&) = delete;
    BattleEffectOwnedEvent(BattleEffectOwnedEvent&&) = default;
    BattleEffectOwnedEvent& operator=(BattleEffectOwnedEvent&&) = default;

    EffectEvent event() const;
    const EffectEventPayload& payload() const;
    EffectEventContext context() const &;
    EffectEventContext context() && = delete;
    EffectEventContext context() const && = delete;

private:
    BattleEffectRuntimeSnapshot battle_;
    EffectEventData data_;
};

// 生產 dispatch 與測試使用相同的 reducer；不維護第二套命令模擬器。
class BattleEffectDispatchPrediction
{
public:
    explicit BattleEffectDispatchPrediction(const BattleRuntimeState& source);
    ~BattleEffectDispatchPrediction();
    BattleEffectDispatchPrediction(const BattleEffectDispatchPrediction&) = delete;
    BattleEffectDispatchPrediction& operator=(const BattleEffectDispatchPrediction&) = delete;
    const StatusBehaviorDispatchLiveness& hooks() const { return hooks_; }

private:
    void reduceRuleCommands(std::span<const EffectCommand> commands);
    std::unique_ptr<BattleRuntimeState> runtime_;
    StatusBehaviorDispatchLiveness hooks_;
};

class BattleEffectEventBridge
{
public:
    BattleEffectOwnedEvent makeEvent(
        const BattleRuntimeState& runtime,
        BattleEffectEventHeaderInput header,
        EffectEvent event,
        EffectEventPayload payload) const;

    BattleEffectDispatchResult dispatch(
        BattleRuntimeState& runtime,
        const BattleEffectOwnedEvent& event) const;

    BattleEffectDispatchResult dispatch(
        BattleRuntimeState& runtime,
        BattleEffectEventHeaderInput header,
        EffectEvent event,
        EffectEventPayload payload) const;

    // FrameAdvanced is the only owner-periodic event. This entry point merges
    // every living configured owner with all live status contributions into
    // one structured frame-start stream.
    BattleEffectDispatchResult dispatchFrameAdvanced(
        BattleRuntimeState& runtime,
        const BattleEffectOwnedEvent& event) const;

    BattleEffectDispatchResult dispatchActiveStatusBehaviors(
        BattleRuntimeState& runtime,
        const BattleEffectOwnedEvent& event,
        StatusBehaviorDispatchFilter filter) const;

    BattleEffectOwnedEvent makeCastLifecycleEvent(
        const BattleRuntimeState& runtime,
        BattleEffectEventHeaderInput header,
        const BattleCastLifecycleEvent& event,
        BattleCastLifecycleEffectInput input) const;

    BattleEffectDispatchResult dispatchCastLifecycleEvent(
        BattleRuntimeState& runtime,
        BattleEffectEventHeaderInput header,
        const BattleCastLifecycleEvent& event,
        BattleCastLifecycleEffectInput input) const;

    BattleEffectOwnedEvent makeDamageResolvedEvent(
        const BattleRuntimeState& runtime,
        BattleEffectEventHeaderInput header,
        const BattleDamageTransactionResult& transaction,
        EffectDamageOrigin origin,
        BattleDamageResolvedEffectInput input) const;

    BattleEffectDispatchResult dispatchDamageResolvedEvent(
        BattleRuntimeState& runtime,
        BattleEffectEventHeaderInput header,
        const BattleDamageTransactionResult& transaction,
        EffectDamageOrigin origin,
        BattleDamageResolvedEffectInput input) const;

    // Cancellation paths that do not emit CastSettled must release the same
    // temporary aliases explicitly. Normal settlement performs this itself.
    void releaseCastScopedRules(
        BattleRuntimeState& runtime,
        BattleCastId castId) const;
};

}  // namespace KysChess::Battle
