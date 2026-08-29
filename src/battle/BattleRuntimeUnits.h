#pragma once

#include "BattleLimits.h"
#include "BattleOutcome.h"

#include "BattleAreaEffectSystem.h"
#include "BattleAttackSystem.h"
#include "BattleDamageQueue.h"
#include "BattleDamageSystem.h"
#include "BattleEffectCommandSystem.h"
#include "BattleEffectSystem.h"
#include "BattleHitResolver.h"
#include "BattleMovement.h"
#include "BattleRescueRepositionSystem.h"
#include "BattleRuntimeActions.h"
#include "BattleRuntimeQueues.h"
#include "BattleRuntimeRandom.h"
#include "BattleStatusSystem.h"
#include "BattleTypes.h"
#include "BattleUnitStore.h"
#include "../ChessBattleEffectTypes.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace KysChess::Battle
{

struct BattleRuntimeState;

struct BattleRuntimeUnitFrameTickConfig
{
    int frame{};
    int mpRegenIntervalFrames = 3;
    int physicalPowerRegenIntervalFrames = 3;
    int mpRecoveryBonusPct{};
};

struct BattleRuntimeUnitFrameTickResult
{
    bool skillFinished{};
};

struct BattleRuntimeUnitActionState
{
    std::optional<BattleActionPlanSeed> planSeed;
    std::optional<BattlePendingCastAction> pendingCast;
    bool ultimateCaster = false;
    bool cooldownFinishUltimate = false;
};

struct BattleRescueUnitRuntime
{
    int forcePullProtectRemaining = 0;
    int forcePullExecuteRemaining = 0;

    bool operator==(const BattleRescueUnitRuntime&) const = default;
};

struct BattleComboRuntimeFacts
{
    std::set<int> memberComboIds;
    std::set<int> appliedComboIds;

    bool isMember(int comboId) const
    {
        return memberComboIds.contains(comboId);
    }

    bool hasApplied(int comboId) const
    {
        return appliedComboIds.contains(comboId);
    }
};

struct BattleRuntimeUnitRecord
{
    BattleRuntimeUnit core;
    BattleComboRuntimeFacts comboFacts;
    BattleStatusRuntimeUnit status;
    BattleDamageRuntimeUnit damage;
    BattleMovementAgentState movement;
    BattleRescueUnitRuntime rescue;
    BattleRuntimeUnitActionState action;

    int id() const { return core.id; }
    bool alive() const { return core.alive; }

    BattleRuntimeUnitFrameTickResult advanceFrameTick(const BattleRuntimeUnitFrameTickConfig& config);

    const BattleActionPlanSeed* actionPlan() const
    {
        return action.planSeed ? &*action.planSeed : nullptr;
    }

    void setActionPlan(BattleActionPlanSeed seed)
    {
        action.planSeed = std::move(seed);
    }

    auto pendingCast(this auto& self)
    {
        return self.action.pendingCast ? &*self.action.pendingCast : nullptr;
    }

    void setPendingCast(BattlePendingCastAction pending)
    {
        assert(pending.effectCast.provenance.valid());
        assert(pending.effectCast.provenance.sourceUnitId == id());
        assert(pending.skillPlan.id == -1);
        action.pendingCast = std::move(pending);
    }

    void clearPendingCast()
    {
        action.pendingCast.reset();
    }

    BattlePendingCastAction takePendingCast()
    {
        assert(action.pendingCast);
        auto pending = std::move(*action.pendingCast);
        action.pendingCast.reset();
        return pending;
    }

    void markUltimateCaster()
    {
        action.ultimateCaster = true;
        setSkillCooldownUltimate(true);
    }

    void clearUltimateCaster()
    {
        action.ultimateCaster = false;
    }

    bool isUltimateCaster() const
    {
        return action.ultimateCaster;
    }

    void setSkillCooldownUltimate(bool ultimate)
    {
        action.cooldownFinishUltimate = ultimate;
    }

    void clearSkillCooldownSource()
    {
        action.cooldownFinishUltimate = false;
    }

    bool isSkillCooldownUltimate() const
    {
        return action.cooldownFinishUltimate;
    }

    void clearActionOwners()
    {
        clearPendingCast();
        clearUltimateCaster();
        clearSkillCooldownSource();
    }

    const BattleStatusEffectState& statusEffects() const { return status.effects; }
    bool frozen() const { return status.effects.has(BattleStatusKind::Stun); }
    int frozenFrames() const { return status.effects.remainingFrames(BattleStatusKind::Stun); }
    bool mpBlocked() const { return status.effects.has(BattleStatusKind::MpBlocked); }

    void clearFrozen()
    {
        status.effects.clear(BattleStatusKind::Stun);
    }

    void setMpBlockFrames(int frames)
    {
        status.effects.setFrames(BattleStatusKind::MpBlocked, frames);
    }

    void commitFrozenPhysicsFrames(int frozenFrames)
    {
        assert(frozenFrames >= 0);
        if (frozenFrames == 0)
        {
            clearFrozen();
            return;
        }
        auto* stun = status.effects.find(BattleStatusKind::Stun);
        assert(stun);
        stun->remainingFrames = frozenFrames;
    }

    BattleStatusUnitState statusDamageState() const
    {
        return makeBattleStatusUnitState(status, core);
    }

    void writeStatusDamageResult(const BattleStatusUnitState& unit)
    {
        writeBattleStatusRuntimeUnit(status, unit);
    }

    BattleDamageUnitState damageState(int mpRecoveryBonusPct) const
    {
        assert(mpRecoveryBonusPct >= 0);
        auto unit = makeBattleDamageUnitState(core, &damage);
        unit.mpBlocked = mpBlocked();
        unit.mpRecoveryBonusPct = mpRecoveryBonusPct;
        return unit;
    }

    void writeDamageResult(const BattleDamageUnitState& unit)
    {
        writeBattleDamageRuntimeUnit(damage, unit);
    }

    int forcePullProtectRemaining() const
    {
        return rescue.forcePullProtectRemaining;
    }

    int forcePullExecuteRemaining() const
    {
        return rescue.forcePullExecuteRemaining;
    }

    void clearForcePullProtect()
    {
        rescue.forcePullProtectRemaining = 0;
    }

    void applyRescueCounterDelta(const BattleRescueCounterDelta& delta)
    {
        assert(delta.unitId == id());
        rescue.forcePullProtectRemaining = std::max(
            0,
            rescue.forcePullProtectRemaining + delta.protectRemainingDelta);
        rescue.forcePullExecuteRemaining = std::max(
            0,
            rescue.forcePullExecuteRemaining + delta.executeRemainingDelta);
    }
};

class BattleRuntimeUnits
{
    static constexpr std::size_t MissingRecordIndex = std::numeric_limits<std::size_t>::max();

    std::vector<BattleRuntimeUnitRecord> records_;
    std::vector<std::size_t> recordIndexById_;

    auto recordById(this auto& self, int unitId)
    {
        using RecordPointer = decltype(self.records_.data());
        if (unitId < 0 || static_cast<std::size_t>(unitId) >= self.recordIndexById_.size())
        {
            return RecordPointer{};
        }
        const auto index = self.recordIndexById_[unitId];
        if (index == MissingRecordIndex)
        {
            return RecordPointer{};
        }
        assert(index < self.records_.size());
        assert(self.records_[index].id() == unitId);
        return &self.records_[index];
    }

public:
    void reserve(std::size_t count)
    {
        records_.reserve(count);
        recordIndexById_.reserve(count);
    }

    void append(BattleRuntimeUnitRecord record)
    {
        assert(record.id() >= 0);
        assert(recordById(record.id()) == nullptr);
        const auto unitId = static_cast<std::size_t>(record.id());
        if (recordIndexById_.size() <= unitId)
        {
            recordIndexById_.resize(unitId + 1, MissingRecordIndex);
        }
        records_.push_back(std::move(record));
        recordIndexById_[unitId] = records_.size() - 1;
    }

    decltype(auto) require(this auto& self, int unitId)
    {
        auto* record = self.recordById(unitId);
        assert(record != nullptr);
        return *record;
    }

    decltype(auto) requireCore(this auto& self, int unitId)
    {
        return (self.require(unitId).core);
    }

    void writeDamageUnit(const BattleDamageUnitState& source)
    {
        auto& unit = requireCore(source.id);
        unit.alive = source.alive;
        unit.vitals = source.vitals;
        unit.stats.attack = source.attack;
        unit.invincible = source.invincible;
        unit.shield = source.shield;
    }

    void setPosition(int unitId, Pointf position, const BattleGridTransform& gridTransform)
    {
        auto& unit = requireCore(unitId);
        unit.motion.position = position;
        unit.grid = gridTransform.toGrid(position);
    }

    void setMotion(
        int unitId,
        Pointf position,
        Pointf velocity,
        Pointf acceleration,
        const BattleGridTransform& gridTransform,
        bool updateFacingFromVelocity)
    {
        auto& unit = requireCore(unitId);
        unit.motion.position = position;
        unit.motion.velocity = velocity;
        unit.motion.acceleration = acceleration;
        if (updateFacingFromVelocity)
        {
            const float velocityLength = velocity.norm();
            if (velocityLength > 0.01f)
            {
                unit.motion.facing = velocity;
                const float scale = 1.0f / velocityLength;
                unit.motion.facing.x *= scale;
                unit.motion.facing.y *= scale;
                unit.motion.facing.z *= scale;
            }
        }
        unit.grid = gridTransform.toGrid(position);
    }

    std::size_t size() const { return records_.size(); }
    bool empty() const { return records_.empty(); }

    std::size_t pendingCastCount() const
    {
        return static_cast<std::size_t>(std::ranges::count_if(
            records_,
            [](const BattleRuntimeUnitRecord& record)
            {
                return record.pendingCast() != nullptr;
            }));
    }

    std::size_t ultimateCasterCount() const
    {
        return static_cast<std::size_t>(std::ranges::count_if(
            records_,
            [](const BattleRuntimeUnitRecord& record)
            {
                return record.isUltimateCaster();
            }));
    }

    auto all(this auto& self)
    {
        return self.records_ | std::views::all;
    }

    auto cores(this auto& self)
    {
        return self.records_
            | std::views::transform(
                [](auto& record) -> decltype(auto)
                {
                    return (record.core);
                });
    }

    auto live(this auto& self)
    {
        return self.records_
            | std::views::filter([](const BattleRuntimeUnitRecord& record) { return record.core.alive; });
    }

    auto dead(this auto& self)
    {
        return self.records_
            | std::views::filter([](const BattleRuntimeUnitRecord& record) { return !record.core.alive; });
    }
};

struct BattleFrameRescueUnitSnapshot
{
    BattleRescueUnitSnapshot unit;
    Pointf position;
};

struct BattleFrameRescueCounterAttackConfig
{
    int skillId = -1;
    int visualEffectId = -1;
    double projectileSpeed = 0.0;
    double meleeAttackEffectOffset = 0.0;
    int minimumTotalFrames = 20;
    int totalFramePadding = 15;
};

struct BattleEffectCastRuntimeContext
{
    int originalTargetUnitId = -1;
    EffectResourcesBeforeCastSnapshot resourcesBeforeCast;
    BattleCastSkillState skill;
    BattleOperationType operationType = BattleOperationType::None;
};

struct BattleEffectPerCastDamageKey
{
    BattleCastId castId;
    EffectSourceKind sourceKind{};
    int sourceId{};
    std::uint64_t sourceInstanceId{};
    EffectRuleId ruleId;
    int targetUnitId = -1;

    auto operator<=>(const BattleEffectPerCastDamageKey&) const = default;
};

struct BattleQueuedEffectCommandBatch
{
    std::vector<EffectCommand> commands;
    BattleEffectCommandContext context;
};

struct BattleEffectDamageContinuationRuntime
{
    int remainingDamageTransactions{};
    BattleQueuedEffectCommandBatch commandBatch;
};

struct BattleEnemyTopDebuffReportState
{
    int value{};
    int sourceTeam = -1;
};

struct BattleEffectIntegrationRuntimeState
{
    std::uint64_t nextEventOrdinal = 1;
    std::uint64_t nextDamageTransactionId = 1;
    std::uint64_t nextDamageContinuationId = 1;
    int nextSharedHitGroupId = 1;
    std::map<BattleCastId, BattleEffectCastRuntimeContext> casts;
    std::set<BattleEffectPerCastDamageKey> appliedPerCastDamage;
    std::vector<BattleQueuedEffectCommandBatch> queuedCommandBatches;
    std::map<std::uint64_t, BattleEffectDamageContinuationRuntime> damageContinuations;
    std::map<int, BattleEnemyTopDebuffReportState> reportedEnemyTopDebuffs;
};

// Persistent battle facts live here. One-frame queues and presentation accumulation
// belong in BattleFrameContext inside BattleCore.cpp. Do not add cached copies of
// combo/status/action facts here unless all mutations to the source fact update the
// cache through the same owner.
struct BattleRuntimeState
{
    BattleGridTransform gridTransform;
    BattleRuntimeUnits units;
    BattleMovementState movement;
    BattleAttackState attacks;
    BattleCastLifecycle castLifecycle;
    BattleHealRuntimeState heals;
    BattleRuntimeRandom random;
    BattleAreaEffectState areas;
    BattleEffectRuleStore effectRules;
    BattleEffectCommandRuntimeState effectCommands;
    BattleEffectIntegrationRuntimeState effectIntegration;
    std::map<std::pair<EffectSourceKind, int>, std::string> effectSourceNames;
    std::set<int> antiComboIds;

    struct DamageState
    {
        bool sortPendingDamageByDefenderMagnitude = false;
        std::map<int, BattleDamagePresentationStyle> presentationStylesByDefender;
    } damage;

    struct StatusState
    {
        BattleStatusSystemConfig config;
    } status;

    struct RescueState
    {
        std::vector<BattleRescueCellSnapshot> cells;
        double executeUnattendedRadius = 0.0;
        BattleFrameRescueCounterAttackConfig counterAttack;
    } rescue;

    struct BattleResultState
    {
        bool ended = false;
        int winningTeam = -1;
        bool eventEmitted = false;
        int endedFrame = -1;
        BattleOutcome outcome = BattleOutcome::InProgress;
    } result;

    int maximumFrames = kBattleFrameLimit;

    struct MovementPhysicsState
    {
        BattleMovementPhysicsConfig config;
        BattleMovementPhysicsTerrain terrain;
    } movementPhysics;

    BattleRuntimeActions action;

    BattleProjectileFollowUpContext projectileFollowUps;
    BattleNextFrameQueues nextFrame;
};

}  // namespace KysChess::Battle
