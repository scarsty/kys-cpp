#pragma once

// Cross-TU internals of the BattleCore pipeline. One owning TU per symbol
// (owner comments land with the physical split). Stage entry points are called
// by BattleFrameRunner; the rest are genuine cross-owner dependency edges.

#include "BattleAttackSystem.h"
#include "BattleCastLifecycle.h"
#include "BattleEffectEventBridge.h"
#include "BattleEffectSystem.h"
#include "BattleFrameContext.h"
#include "BattleStatusSystem.h"

#include <memory_resource>
#include <span>
#include <string>
#include <vector>

namespace KysChess::Battle::CoreDetail
{

inline constexpr int CoreRoleStatusEffectFrames = 48;
CastPlanEventData makeCastPlanEventData(
    int sourceUnitId,
    int magicId,
    bool ultimate,
    int preferredTargetUnitId,
    int mpBefore,
    int maxMp,
    int normalCastMpDelta,
    bool forceRanged,
    const EffectResourcesBeforeCastSnapshot& resourcesBeforeCast,
    const BattleCastProvenance* provenance = nullptr);

void applyRuntimeUnitMpDelta(BattleRuntimeState& state, BattleRuntimeUnit& unit, int mpDelta);
BattleRuntimeUnitsAdvanceResult advanceRuntimeUnits(BattleRuntimeState& state);
EffectResourcesBeforeCastSnapshot snapshotEffectResourcesBeforeCast(
    const BattleRuntimeState& state);
BattleEffectEventHeaderInput nextEffectEventHeader(
    BattleRuntimeState& state,
    int ownerUnitId,
    EffectFormulaInputs formulaInputs = {});
BattleCastProvenance plannedEffectCastProvenance(
    int sourceUnitId,
    int magicId,
    bool ultimate);
CastPlanEventData makeCastPlanEventData(
    const BattleCastInput& input,
    bool ultimate,
    const EffectResourcesBeforeCastSnapshot& resourcesBeforeCast,
    const BattleCastProvenance* provenance = nullptr);
BattleEffectDispatchResult dispatchCastPlannedEffects(
    BattleRuntimeState& state,
    const BattleCastInput& input,
    bool ultimate,
    const EffectResourcesBeforeCastSnapshot& resourcesBeforeCast,
    const BattleCastProvenance& provenance);
void applyEffectAttackDirectives(
    std::span<BattleAttackSpawnRequest> requests,
    const BattleEffectAttackApplyResult& result);
BattleCastStart beginEffectRootCast(
    BattleCastLifecycle& lifecycle,
    int sourceUnitId,
    int magicId,
    bool ultimate);
void cancelEffectRootCast(
    BattleRuntimeState& state,
    const BattleCastStart& start);
void reserveEffectRootCastAttacks(
    BattleCastLifecycle& lifecycle,
    const BattleCastStart& start,
    std::span<BattleAttackSpawnRequest> requests);
std::vector<BattleEffectDispatchResult> dispatchCastCommittedEffects(
    BattleRuntimeState& state,
    const BattlePendingCastAction& pending,
    const BattleCastResult& cast);
void appendFramePendingDamage(
    BattleRuntimeState& state,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
    BattleDamageRequest request,
    std::optional<BattleDamagePresentationInput> presentation = std::nullopt,
    int executeThresholdPct = 0,
    bool canTriggerDefenderBlock = false,
    BattleAttackProvenance provenance = {},
    std::optional<EffectDamageOrigin> effectOrigin = std::nullopt,
    CastWorkToken delayedCastWork = {});
void reduceCommandsBeforeMovement(
    BattleRuntimeState& state,
    BattleFrameContext& frame);
void reduceCommandsBeforeAttacks(BattleRuntimeState& state, BattleFrameContext& frame);
void appendAreaDamagePulses(BattleRuntimeState& state, BattleFrameContext& frame);
void reduceCommandsAfterAttackHits(BattleRuntimeState& state, BattleFrameContext& frame);
void reduceCommandsAfterDamageLifecycle(BattleRuntimeState& state, BattleFrameContext& frame);
void completeCastCommitBarriers(
    BattleRuntimeState& state,
    BattleFrameContext& frame);
void appendDamageAbsorptionSettlements(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
    std::span<const BattleDamageAbsorptionInstance> absorptions,
    int settlementFrame);
void reduceEffectCommandBatches(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
    std::uint64_t reductionReceiptId = 0,
    BattleEffectCommandReduction* reductionReceipt = nullptr);
void reduceEffectCommand(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
    const EffectCommand& command,
    BattleEffectCommandReduction* reductionReceipt = nullptr);
void completeEffectDamageContinuation(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
    std::uint64_t continuationId);
std::vector<BattleFrameEffectCommandBatch> dispatchFrameAdvancedEffects(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    int upcomingFrame);
void applyLateFrameMpRestores(BattleRuntimeState& state, BattleFrameContext& frame);
std::vector<BattleStatusEvent> advanceStatus(
    BattleRuntimeState& state,
    std::vector<BattlePendingDamageIntent>& pendingDamage);
void applyKnockbackImpulse(
    BattleRuntimeState& state,
    const BattleKnockbackCommand& knockback);
void advanceActionFrameUnits(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    const BattleTickResult& movement);
void advanceAttacksAndResolveHits(
    BattleRuntimeState& state,
    BattleFrameContext& frame);
void applyDamageAndLifecycle(
    BattleRuntimeState& state,
    BattleFrameContext& frame);
void dispatchReadyCastLifecycleEffects(
    BattleRuntimeState& state,
    BattleFrameContext& frame);
void emitPresentationFrame(BattleRuntimeState& state, BattleFrameContext& frame);

bool tryCommitAutoUltimate(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    int unitId,
    bool consumeMp,
    bool announceAutoUltimate,
    std::pmr::memory_resource* frameMemoryResource,
    std::vector<int>& attackSoundIds,
    std::vector<BattleAttackSpawnRequest>& attackSpawns,
    std::vector<BattleGameplayEvent>& gameplayEvents,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents,
    bool* submitted = nullptr);

void refreshRuntimeMovementProfiles(BattleRuntimeState& state);

BattleCastSkillState makePendingCastSkillPlan(BattleCastSkillState skill);

std::optional<BattleCastInput> tryMakeRuntimeCastInputForPendingCast(
    BattleRuntimeState& state,
    const BattlePendingCastAction& pending,
    std::pmr::memory_resource* frameMemoryResource);

const BattleCastSkillState& selectedCastSkill(const BattleCastInput& input, bool ultimate);

void cancelDeadRuntimeActions(BattleRuntimeState& state);

bool runtimeDashAttackEnabled(BattleRuntimeState& state, int ownerUnitId);


BattleVisualEvent roleEffectEvent(int targetUnitId, int effectId, int durationFrames);
void appendStatusEventLog(
    std::vector<BattleLogEvent>& logEvents,
    int sourceUnitId,
    int targetUnitId,
    std::string text,
    BattleStatusSemanticId statusId = BattleStatusSemanticId::None,
    BattleResourceSemanticId resourceId = BattleResourceSemanticId::None,
    int amount = 0);
void reserveTrackedProjectileFollowUps(
    BattleRuntimeState& state,
    BattleProjectileFollowUpExpansion& expansion);
DamageChannel effectDamageChannel(BattleDamageKind kind);

int mpRecoveryBonusPct(BattleRuntimeState& state, int unitId);

CastPropagationPolicy derivedAttackPropagation(
    const BattleAttackProvenance& sourceAttack);

// Damage owns materialization of typed gameplay damage commands because HP
// command conversion also builds Damage-owned presentation styling.
bool tryAppendFrameDamageTransaction(
    BattleRuntimeState& state,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
    const BattleHpDamageCommand& command);

bool tryAppendFrameDamageTransaction(
    BattleRuntimeState& state,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
    const BattleMpDamageCommand& command);

bool tryAppendFrameDamageTransaction(
    BattleRuntimeState& state,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
    const BattleAcceptedHitSideEffectCommand& command);

void appendHealEventLog(
    std::vector<BattleLogEvent>& logEvents,
    int sourceUnitId,
    int targetUnitId,
    int amount,
    std::string text,
    BattleResourceSemanticId resourceId = BattleResourceSemanticId::HitPoints);

void appendPoisonEffectLogEvents(
    std::vector<BattleLogEvent>& logEvents,
    const EffectCommandMetadata& metadata,
    const ApplyStatusEffectCommand& command,
    const BattleStatusApplyEffectResult& result);

CastWorkToken reserveEffectDamageDescendantWork(
    BattleRuntimeState& state,
    const EffectExecutionInputs& context,
    const BattleAttackProvenance& provenance);

void appendEffectDamageOutput(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
    const BattleEffectDamageRequestOutput& output,
    const EffectExecutionInputs& context);


void updateFrameBattleResultAfterDamage(BattleRuntimeState& state, BattleFrameContext& frame);

BattleDamageUnitState makeBattleDamageUnitStateFromRuntime(
    const BattleRuntimeUnit& unit,
    const BattleDamageRuntimeUnit* runtime);

void writeBattleDamageRuntimeUnitImpl(BattleDamageRuntimeUnit& runtime, const BattleDamageUnitState& unit);

BattleCooldownState makeBattleFrameCooldownStateImpl(const BattleRuntimeUnit& unit);

void applyStatusTickDamagePresentation(
    const BattleRuntimeState& state,
    BattleDamageKind kind,
    int targetUnitId,
    BattleDamagePresentationInput& presentation);


}  // namespace KysChess::Battle::CoreDetail
