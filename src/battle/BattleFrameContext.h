#pragma once

// Core-pipeline-internal frame state for BattleFrameRunner. Persistent gameplay
// lives in BattleRuntimeState; anything consumed within one frame belongs here.
// Do not pass this type to subsystem classes outside the battle core pipeline.

#include "BattleAttackSystem.h"
#include "BattleCastLifecycle.h"
#include "BattleDamageQueue.h"
#include "BattleEffectCommandSystem.h"
#include "BattleEffectSystem.h"
#include "BattleHitResolver.h"
#include "BattlePresentation.h"
#include "BattleRuntimeQueues.h"
#include "BattleRuntimeUnits.h"
#include "BattleUnitStore.h"
#include "BattleUnitValues.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <string>
#include <utility>
#include <vector>

namespace KysChess::Battle
{

struct BattleFrameMpRestore
{
    int unitId{};
    int amount{};
    std::string reason;
};

struct BattleFrameEffectCommandBatch
{
    std::vector<EffectCommand> commands;
    std::uint64_t reductionReceiptId{};
};

template <typename T>
using BattleFrameVector = std::pmr::vector<T>;

struct BattleRuntimeUnitsAdvanceResult
{
    std::vector<BattleFrameEffectCommandBatch> cooldownFinishedEffects;
};

struct UnitMotionSnapshot
{
    int unitId = -1;
    BattleUnitMotion motion;
};

using UnitMotionSnapshotList = std::pmr::vector<UnitMotionSnapshot>;

UnitMotionSnapshotList makeUnitMotionSnapshot(
    const BattleRuntimeUnits& units,
    std::pmr::memory_resource* frameMemoryResource);

const BattleUnitMotion& motionSnapshotForUnit(
    const UnitMotionSnapshotList& snapshots,
    const BattleRuntimeUnit& fallback);

enum class BattleSemanticCueFamily : std::uint8_t
{
    SwordIntent,
    Positive,
    Protection,
    Poison,
    Bleed,
    Control,
    Curse,
    Cleanse,
};

struct BattleSemanticCueRequest
{
    int targetUnitId = -1;
    BattleSemanticCueFamily family{};
};

inline int semanticCuePriority(BattleSemanticCueFamily family)
{
    switch (family)
    {
    case BattleSemanticCueFamily::Control: return 90;
    case BattleSemanticCueFamily::Protection: return 80;
    case BattleSemanticCueFamily::Poison:
    case BattleSemanticCueFamily::Bleed: return 70;
    case BattleSemanticCueFamily::Curse: return 60;
    case BattleSemanticCueFamily::Positive: return 50;
    case BattleSemanticCueFamily::SwordIntent: return 55;
    case BattleSemanticCueFamily::Cleanse: return 40;
    }
    assert(false);
    return 0;
}




class BattleFrameContext
{
public:
    static BattleFrameContext begin(
        BattleRuntimeState& state,
        BattlePresentationFrame recycledPresentation,
        std::byte* frameMemoryStorage,
        std::size_t frameMemoryBytes)
    {
        return BattleFrameContext(
            state,
            std::move(recycledPresentation),
            frameMemoryStorage,
            frameMemoryBytes);
    }

    std::vector<BattleAttackSpawnRequest>& currentFrameAttacks() { return attackSpawns_; }
    std::vector<BattlePendingDamageIntent>& currentFrameDamage() { return pendingDamage_; }

    void queueCommand(BattleGameplayCommand command)
    {
        frameCommands_.push_back(std::move(command));
    }

    void queueEffectCommands(
        std::vector<EffectCommand> commands,
        std::uint64_t reductionReceiptId = 0)
    {
        if (commands.empty())
        {
            return;
        }
        effectCommandBatches_.push_back({
            std::move(commands),
            reductionReceiptId,
        });
    }

    BattleFrameVector<BattleFrameEffectCommandBatch> drainEffectCommandBatches()
    {
        return drainFrameVector(effectCommandBatches_);
    }

    BattleFrameVector<BattleGameplayCommand> drainCommands()
    {
        return drainFrameVector(frameCommands_);
    }

    std::vector<BattleAttackSpawnRequest> drainCurrentFrameAttacks()
    {
        return std::exchange(attackSpawns_, {});
    }

    std::vector<BattlePendingDamageIntent> drainCurrentFrameDamage()
    {
        return std::exchange(pendingDamage_, {});
    }

    BattleFrameVector<BattleAreaProjectileFollowUp> drainAreaProjectileFollowUps()
    {
        return drainFrameVector(areaProjectileFollowUps_);
    }

    void queueCastCommitBarrier(CastWorkToken barrier)
    {
        assert(barrier.valid());
        castCommitBarriers_.push_back(barrier);
    }

    BattleFrameVector<CastWorkToken> drainCastCommitBarriers()
    {
        return drainFrameVector(castCommitBarriers_);
    }

    BattleFrameVector<BattleFrameMpRestore> drainLateMpRestores()
    {
        return drainFrameVector(lateMpRestores_);
    }

    const UnitMotionSnapshotList& frameStartMotion() const { return frameStartMotion_; }
    std::pmr::memory_resource* frameMemoryResource() { return &frameMemoryResource_; }

    BattleFrameVector<BattleGameplayCommand>& mutableCommandsForReducer() { return frameCommands_; }
    BattleFrameVector<BattleAreaProjectileFollowUp>& mutableAreaProjectileFollowUps() { return areaProjectileFollowUps_; }
    BattleFrameVector<BattleFrameMpRestore>& mutableLateMpRestores() { return lateMpRestores_; }

    void queueSemanticCue(int targetUnitId, BattleSemanticCueFamily family)
    {
        // 暫停負面狀態的角色特效，避免反覆閃爍，也不讓它們排擠其他提示。
        switch (family)
        {
        case BattleSemanticCueFamily::Poison:
        case BattleSemanticCueFamily::Bleed:
        case BattleSemanticCueFamily::Control:
        case BattleSemanticCueFamily::Curse:
            return;
        default:
            break;
        }
        auto existing = std::find_if(
            semanticCues_.begin(),
            semanticCues_.end(),
            [&](const BattleSemanticCueRequest& cue)
            {
                return cue.targetUnitId == targetUnitId;
            });
        if (existing == semanticCues_.end())
        {
            semanticCues_.push_back({ targetUnitId, family });
            return;
        }
        if (semanticCuePriority(family) > semanticCuePriority(existing->family))
        {
            existing->family = family;
        }
    }

    BattleFrameVector<BattleSemanticCueRequest> drainSemanticCues()
    {
        return drainFrameVector(semanticCues_);
    }

private:
    explicit BattleFrameContext(
        BattleRuntimeState& state,
        BattlePresentationFrame recycledPresentation,
        std::byte* frameMemoryStorage,
        std::size_t frameMemoryBytes)
        : frameMemoryResource_(frameMemoryStorage, frameMemoryBytes)
        , frameCommands_(&frameMemoryResource_)
        , areaProjectileFollowUps_(&frameMemoryResource_)
        , lateMpRestores_(&frameMemoryResource_)
        , attackSpawns_(state.nextFrame.drainAttacks())
        , pendingDamage_(state.nextFrame.drainDamage())
        , castCommitBarriers_(&frameMemoryResource_)
        , effectCommandBatches_(&frameMemoryResource_)
        , semanticCues_(&frameMemoryResource_)
        , frameStartMotion_(makeUnitMotionSnapshot(state.units, &frameMemoryResource_))
        , gameplayEvents(std::move(recycledPresentation.gameplayEvents))
        , logEvents(std::move(recycledPresentation.logEvents))
        , visualEvents(std::move(recycledPresentation.visualEvents))
        , attackSoundIds(std::move(recycledPresentation.attackSoundIds))
        , rumbles(std::move(recycledPresentation.rumbles))
        , attackEvents(&frameMemoryResource_)
    {
        gameplayEvents.clear();
        logEvents.clear();
        visualEvents.clear();
        attackSoundIds.clear();
        rumbles.clear();
    }

    template <typename T>
    BattleFrameVector<T> drainFrameVector(BattleFrameVector<T>& source)
    {
        BattleFrameVector<T> drained(&frameMemoryResource_);
        drained.swap(source);
        return drained;
    }

    std::pmr::monotonic_buffer_resource frameMemoryResource_;
    BattleFrameVector<BattleGameplayCommand> frameCommands_;
    BattleFrameVector<BattleAreaProjectileFollowUp> areaProjectileFollowUps_;
    BattleFrameVector<BattleFrameMpRestore> lateMpRestores_;
    std::vector<BattleAttackSpawnRequest> attackSpawns_;
    std::vector<BattlePendingDamageIntent> pendingDamage_;
    BattleFrameVector<CastWorkToken> castCommitBarriers_;
    BattleFrameVector<BattleFrameEffectCommandBatch> effectCommandBatches_;
    BattleFrameVector<BattleSemanticCueRequest> semanticCues_;
    UnitMotionSnapshotList frameStartMotion_;

public:
    BattlePresentationFrame result;
    std::vector<BattleGameplayEvent> gameplayEvents;
    std::vector<BattleLogEvent> logEvents;
    std::vector<BattleVisualEvent> visualEvents;
    std::vector<int> attackSoundIds;
    std::vector<BattleFrameRumbleEvent> rumbles;
    int blinkSoundCount{};
    BattleFrameVector<BattleAttackEvent> attackEvents;
};

BattlePresentationFrame consumeBattleFrameContext(BattleFrameContext&& frame);

}  // namespace KysChess::Battle
