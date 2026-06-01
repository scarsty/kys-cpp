#pragma once

#include "BattleAttackSystem.h"
#include "BattleDamageQueue.h"
#include "BattleDamageSystem.h"
#include "BattleDeathEffectSystem.h"
#include "BattleHitResolver.h"
#include "BattleMovement.h"
#include "BattleRescueRepositionSystem.h"
#include "BattleRuntimeActions.h"
#include "BattleRuntimeQueues.h"
#include "BattleRuntimeRandom.h"
#include "BattleStatusSystem.h"
#include "BattleTypes.h"
#include "BattleUnitStore.h"
#include "../ChessBattleEffects.h"

#include <algorithm>
#include <cassert>
#include <map>
#include <optional>
#include <ranges>
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
};

struct BattleRescueUnitRuntime
{
    int forcePullProtectRemaining = 0;
    int forcePullExecuteRemaining = 0;
};

struct BattleRuntimeUnitRecord
{
    BattleRuntimeUnit core;
    RoleComboState combo;
    BattleStatusRuntimeUnit status;
    BattleDamageRuntimeUnit damage;
    BattleMovementAgentState movement;
    BattleDeathEffectExtras deathEffects;
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
        seed.unitId = id();
        action.planSeed = std::move(seed);
    }

    auto pendingCast(this auto& self)
    {
        return self.action.pendingCast ? &*self.action.pendingCast : nullptr;
    }

    void setPendingCast(BattlePendingCastAction pending)
    {
        pending.unitId = id();
        action.pendingCast = std::move(pending);
    }

    void clearPendingCast()
    {
        action.pendingCast.reset();
    }

    void markUltimateCaster()
    {
        action.ultimateCaster = true;
    }

    void clearUltimateCaster()
    {
        action.ultimateCaster = false;
    }

    bool isUltimateCaster() const
    {
        return action.ultimateCaster;
    }

    void clearActionOwners()
    {
        clearPendingCast();
        clearUltimateCaster();
    }

    void clearPendingSkillHeal()
    {
        combo.setTypePending(EffectType::OnSkillTeamHeal, false);
    }

    int sumAlways(EffectType type) const
    {
        return combo.sumAlways(type);
    }

    int maxAlways(EffectType type) const
    {
        return combo.maxAlways(type);
    }

    bool hasAlways(EffectType type) const
    {
        return combo.hasAlways(type);
    }

    const RoleComboEffectInstance* firstAlways(EffectType type) const
    {
        return combo.firstAlways(type);
    }

    BattleDamageModifierState damageModifiers() const
    {
        return makeBattleDamageModifierState(&combo);
    }

    void grantRuntimeComboEffect(const ComboEffectSnapshot& effect, int comboId)
    {
        combo.grantRuntimeEffect(effect, comboId);
    }

    bool hasComboApplied(int comboId) const
    {
        return combo.hasComboApplied(comboId);
    }

    const BattleStatusEffectState& statusEffects() const { return status.effects; }
    bool frozen() const { return status.effects.frozenTimer > 0; }
    int frozenFrames() const { return status.effects.frozenTimer; }
    bool mpBlocked() const { return status.effects.mpBlockTimer > 0; }

    void clearFrozen()
    {
        status.effects.frozenTimer = 0;
        status.effects.frozenMaxTimer = 0;
    }

    void setMpBlockFrames(int frames)
    {
        status.effects.mpBlockTimer = frames;
    }

    void addTempAttackBuff(int attackBonus, int durationFrames)
    {
        status.effects.tempAttackBuffs.push_back({
            attackBonus,
            durationFrames,
        });
    }

    void commitFrozenPhysicsFrames(int frozenFrames)
    {
        status.effects.frozenTimer = frozenFrames;
    }

    BattleStatusUnitState statusDamageState() const
    {
        return makeBattleStatusUnitState(status, core);
    }

    void writeStatusDamageResult(const BattleStatusUnitState& unit)
    {
        writeBattleStatusRuntimeUnit(status, unit);
    }

    BattleDamageUnitState damageState() const
    {
        auto unit = makeBattleDamageUnitState(core, &damage);
        unit.mpBlocked = status.effects.mpBlockTimer > 0;
        unit.mpRecoveryBonusPct = sumAlways(EffectType::MPRecoveryBonus);
        return unit;
    }

    void writeDamageResult(const BattleDamageUnitState& unit)
    {
        writeBattleDamageRuntimeUnit(damage, unit);
    }

    void transferDeathAppliedEffect(const ComboEffectSnapshot& effect)
    {
        deathEffects.appliedEffects.push_back(effect);
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
public:
    void reserve(std::size_t count)
    {
        records_.reserve(count);
    }

    void append(BattleRuntimeUnitRecord record)
    {
        assert(record.id() >= 0);
        assert(recordById(record.id()) == nullptr);
        records_.push_back(std::move(record));
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
        const BattleGridTransform& gridTransform)
    {
        auto& unit = requireCore(unitId);
        unit.motion.position = position;
        unit.motion.velocity = velocity;
        unit.motion.acceleration = acceleration;
        if (velocity.norm() > 0.01f)
        {
            unit.motion.facing = velocity;
            unit.motion.facing.normTo(1.0f);
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

private:
    auto recordById(this auto& self, int unitId)
    {
        auto it = std::ranges::find_if(
            self.records_,
            [unitId](const BattleRuntimeUnitRecord& record)
            {
                return record.id() == unitId;
            });
        return it == self.records_.end() ? nullptr : &*it;
    }

    std::vector<BattleRuntimeUnitRecord> records_;
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
    BattleRuntimeRandom random;

    struct DamageState
    {
        bool sortPendingDamageByDefenderMagnitude = false;
        std::map<int, BattleDamagePresentationStyle> presentationStylesByDefender;
    } damage;

    struct StatusState
    {
        BattleStatusSystemConfig config;
    } status;

    struct DeathEffectState
    {
        BattleDeathEffectStore store;
    } deathEffects;

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
    } result;

    struct TeamEffectState
    {
        double healAuraRadius = 0.0;
    } teamEffects;

    struct MovementPhysicsState
    {
        BattleMovementPhysicsConfig config;
        BattleMovementPhysicsTerrain terrain;
        std::vector<int> actionCastFrames;
        int dashMomentumFrames = 0;
    } movementPhysics;

    BattleRuntimeActions action;

    BattleProjectileFollowUpContext projectileFollowUps;
    BattleNextFrameQueues nextFrame;
};

}  // namespace KysChess::Battle
