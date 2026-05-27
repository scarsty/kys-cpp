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

struct BattleRuntimeUnitActionState
{
    std::optional<BattleActionPlanSeed> planSeed;
    std::optional<BattlePendingCastAction> pendingCast;
    bool ultimateCaster = false;
};

struct BattleRescueUnitRuntime
{
    int unitId = -1;
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

    const BattleActionPlanSeed* actionPlan() const
    {
        return action.planSeed ? &*action.planSeed : nullptr;
    }

    void setActionPlan(BattleActionPlanSeed seed)
    {
        seed.unitId = id();
        action.planSeed = std::move(seed);
    }

    BattlePendingCastAction* pendingCast()
    {
        return action.pendingCast ? &*action.pendingCast : nullptr;
    }

    const BattlePendingCastAction* pendingCast() const
    {
        return action.pendingCast ? &*action.pendingCast : nullptr;
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
};

class BattleRuntimeUnitComboView
{
public:
    explicit BattleRuntimeUnitComboView(RoleComboState& combo)
        : combo_(&combo)
    {
    }

    void clearPendingSkillHeal() const
    {
        combo_->onSkillTeamHealPending = false;
    }

    int sumAlways(EffectType type) const
    {
        return sumAlwaysEffectValue(*combo_, type);
    }

    int maxAlways(EffectType type) const
    {
        return maxAlwaysEffectValue(*combo_, type);
    }

    bool hasAlways(EffectType type) const
    {
        return firstAlwaysEffect(*combo_, type) != nullptr;
    }

    void applyEffect(const AppliedEffectInstance& effect, int comboId) const
    {
        KysChess::ChessBattleEffects::applyEffect(*combo_, effect, comboId);
    }

    bool hasAppliedEffect(EffectType type, int sourceComboId) const
    {
        return std::any_of(
            combo_->appliedEffects.begin(),
            combo_->appliedEffects.end(),
            [type, sourceComboId](const AppliedEffectInstance& effect)
            {
                return effect.type == type && effect.sourceComboId == sourceComboId;
            });
    }

    bool hasComboApplied(int comboId) const
    {
        return std::any_of(
            combo_->appliedEffects.begin(),
            combo_->appliedEffects.end(),
            [comboId](const AppliedEffectInstance& effect)
            {
                return effect.sourceComboId == comboId;
            });
    }

    const AppliedEffectInstance* firstAlways(EffectType type) const
    {
        return firstAlwaysEffect(*combo_, type);
    }

    BattleDamageModifierState damageModifiers() const
    {
        return makeBattleDamageModifierState(combo_);
    }

private:
    RoleComboState* combo_{};
};

class BattleRuntimeUnitStatusView
{
public:
    explicit BattleRuntimeUnitStatusView(BattleStatusRuntimeUnit& status)
        : status_(&status)
    {
    }

    const BattleStatusEffectState& effects() const { return status_->effects; }
    bool frozen() const { return status_->effects.frozenTimer > 0; }
    int frozenFrames() const { return status_->effects.frozenTimer; }
    bool mpBlocked() const { return status_->effects.mpBlockTimer > 0; }

    void setMpBlockFrames(int frames) const
    {
        status_->effects.mpBlockTimer = frames;
    }

    void addTempAttackBuff(int attackBonus, int durationFrames) const
    {
        status_->effects.tempAttackBuffs.push_back({
            attackBonus,
            durationFrames,
        });
    }

    void writeDamageResult(const BattleStatusUnitState& unit) const
    {
        writeBattleStatusRuntimeUnit(*status_, unit);
    }

    BattleStatusUnitState damageState(const BattleRuntimeUnit& unit) const
    {
        return makeBattleStatusUnitState(*status_, unit);
    }

    void clearFrozen() const
    {
        status_->effects.frozenTimer = 0;
        status_->effects.frozenMaxTimer = 0;
    }

    void setFrozenForTest(int timer, int maxTimer) const
    {
        status_->effects.frozenTimer = timer;
        status_->effects.frozenMaxTimer = maxTimer;
    }

    void commitFrozenPhysicsFrames(int frozenFrames) const
    {
        status_->effects.frozenTimer = frozenFrames;
    }

private:
    BattleStatusRuntimeUnit* status_{};
};

class BattleRuntimeUnitDeathEffectsView
{
public:
    explicit BattleRuntimeUnitDeathEffectsView(BattleDeathEffectExtras& extras)
        : extras_(&extras)
    {
    }

    void transferAppliedEffect(const AppliedEffectInstance& effect) const
    {
        extras_->appliedEffects.push_back(effect);
    }

private:
    BattleDeathEffectExtras* extras_{};
};

class BattleRuntimeUnitRescueView
{
public:
    explicit BattleRuntimeUnitRescueView(BattleRuntimeState& state, int unitId)
        : state_(&state),
          unitId_(unitId)
    {
    }

    int forcePullProtectRemaining() const;
    int forcePullExecuteRemaining() const;
    void clearForcePullProtect() const;
    void applyCounterDelta(const BattleRescueCounterDelta& delta) const;

private:
    BattleRuntimeState* state_{};
    int unitId_{};
};

// Runtime unit membership is fixed after clone initialization. Handles and unit
// ranges are frame-local views over stable unit/status/damage/combo/death/rescue/
// movement rows. Movement rows remain present for dead units; agent.active tells
// movement phases whether the unit currently participates in planning/physics.
//
// Today: BattleRuntimeUnitHandle is a view over horizontal stores.
// Future ownership migration is reasonable only if every per-unit store has the
// same lifetime as BattleRuntimeUnit and system-wide processors can still iterate
// efficiently without rebuilding temporary horizontal arrays each frame.
class BattleRuntimeUnitHandle
{
public:
    BattleRuntimeUnitHandle(BattleRuntimeUnit& core,
                            BattleStatusRuntimeUnit* status,
                            RoleComboState* combo,
                            BattleDamageRuntimeUnit* damage,
                            BattleMovementAgentState* movement,
                            BattleDeathEffectExtras* deathEffects,
                            BattleRuntimeState* state)
        : core_(&core),
          status_(status),
          combo_(combo),
          damage_(damage),
          movement_(movement),
          deathEffects_(deathEffects),
          state_(state)
    {
    }

    int id() const { return core_->id; }
    bool alive() const { return core_->alive; }

    BattleRuntimeUnit& core() const { return *core_; }
    BattleRuntimeUnitStatusView status() const { return BattleRuntimeUnitStatusView(*status_); }
    BattleRuntimeUnitComboView combo() const { return BattleRuntimeUnitComboView(*combo_); }
    BattleDamageRuntimeUnit& damage() const { return *damage_; }
    BattleMovementAgentState& movement() const
    {
        assert(movement_ != nullptr);
        return *movement_;
    }
    BattleRuntimeUnitDeathEffectsView deathEffects() const { return BattleRuntimeUnitDeathEffectsView(*deathEffects_); }
    BattleRuntimeUnitRescueView rescue() const;
    int mpRecoveryBonusPct() const { return combo().sumAlways(EffectType::MPRecoveryBonus); }

private:
    BattleRuntimeUnit* core_{};
    BattleStatusRuntimeUnit* status_{};
    RoleComboState* combo_{};
    BattleDamageRuntimeUnit* damage_{};
    BattleMovementAgentState* movement_{};
    BattleDeathEffectExtras* deathEffects_{};
    BattleRuntimeState* state_{};
};

class BattleRuntimeUnits
{
public:
    explicit BattleRuntimeUnits(BattleRuntimeState& state)
        : state_(&state)
    {
    }

    BattleRuntimeUnitHandle require(int unitId) const;
    auto all() const;
    auto live() const;
    auto dead() const;

private:
    BattleRuntimeState& state() const;
    BattleRuntimeUnitHandle makeHandle(BattleRuntimeUnit& unit) const;

    BattleRuntimeState* state_{};
};

class BattleRuntimeUnitRecords
{
public:
    void reserve(std::size_t count)
    {
        records_.reserve(count);
    }

    void append(BattleRuntimeUnitRecord record)
    {
        assert(record.id() >= 0);
        assert(find(record.id()) == nullptr);
        records_.push_back(std::move(record));
    }

    BattleRuntimeUnitRecord* find(int unitId)
    {
        auto it = std::ranges::find_if(
            records_,
            [unitId](const BattleRuntimeUnitRecord& record)
            {
                return record.id() == unitId;
            });
        return it == records_.end() ? nullptr : &*it;
    }

    const BattleRuntimeUnitRecord* find(int unitId) const
    {
        auto it = std::ranges::find_if(
            records_,
            [unitId](const BattleRuntimeUnitRecord& record)
            {
                return record.id() == unitId;
            });
        return it == records_.end() ? nullptr : &*it;
    }

    BattleRuntimeUnitRecord& require(int unitId)
    {
        auto* record = find(unitId);
        assert(record != nullptr);
        return *record;
    }

    const BattleRuntimeUnitRecord& require(int unitId) const
    {
        const auto* record = find(unitId);
        assert(record != nullptr);
        return *record;
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

    auto all()
    {
        return records_ | std::views::all;
    }

    auto all() const
    {
        return records_ | std::views::all;
    }

    auto live()
    {
        return records_
            | std::views::filter([](const BattleRuntimeUnitRecord& record) { return record.core.alive; });
    }

    auto live() const
    {
        return records_
            | std::views::filter([](const BattleRuntimeUnitRecord& record) { return record.core.alive; });
    }

    auto dead()
    {
        return records_
            | std::views::filter([](const BattleRuntimeUnitRecord& record) { return !record.core.alive; });
    }

    auto dead() const
    {
        return records_
            | std::views::filter([](const BattleRuntimeUnitRecord& record) { return !record.core.alive; });
    }

private:
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
    BattleRuntimeUnits units();
    BattleUnitStore unitStore;
    BattleRuntimeUnitRecords unitRecords;
    BattleMovementState movement;
    BattleAttackState attacks;
    BattleRuntimeRandom random;

    struct DamageState
    {
        bool sortPendingDamageByDefenderMagnitude = false;
        std::vector<BattleDamageRuntimeUnit> unitExtras;
        std::map<int, BattleDamagePresentationStyle> presentationStylesByDefender;
    } damage;

    struct StatusState
    {
        BattleStatusSystemConfig config;
        std::vector<BattleStatusRuntimeUnit> units;
    } status;

    struct ComboTriggerState
    {
        std::map<int, RoleComboState> units;
    } combo;

    struct DeathEffectState
    {
        BattleDeathEffectStore store;
    } deathEffects;

    struct RescueState
    {
        struct RescueUnitRuntime
        {
            int unitId = -1;
            int forcePullProtectRemaining = 0;
            int forcePullExecuteRemaining = 0;
        };

        std::vector<BattleRescueCellSnapshot> cells;
        std::vector<RescueUnitRuntime> units;
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

inline BattleRuntimeUnits BattleRuntimeState::units()
{
    return BattleRuntimeUnits(*this);
}

inline auto BattleRuntimeUnits::all() const
{
    auto* runtime = &state();
    return runtime->unitStore.units
        | std::views::transform(
            [runtime](BattleRuntimeUnit& unit)
            {
                return BattleRuntimeUnits(*runtime).makeHandle(unit);
            });
}

inline auto BattleRuntimeUnits::live() const
{
    auto* runtime = &state();
    return runtime->unitStore.units
        | std::views::filter([](const BattleRuntimeUnit& unit) { return unit.alive; })
        | std::views::transform(
            [runtime](BattleRuntimeUnit& unit)
            {
                return BattleRuntimeUnits(*runtime).makeHandle(unit);
            });
}

inline auto BattleRuntimeUnits::dead() const
{
    auto* runtime = &state();
    return runtime->unitStore.units
        | std::views::filter([](const BattleRuntimeUnit& unit) { return !unit.alive; })
        | std::views::transform(
            [runtime](BattleRuntimeUnit& unit)
            {
                return BattleRuntimeUnits(*runtime).makeHandle(unit);
            });
}

}  // namespace KysChess::Battle
