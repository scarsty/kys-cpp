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
#include <ranges>
#include <vector>

namespace KysChess::Battle
{

struct BattleRuntimeState;

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
// ranges are frame-local views over stable unit/status/damage/combo/death rows.
// Movement agent access is phase-sensitive because dead-unit cleanup may erase
// movement agents for non-corpse units.
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
                            BattleRuntimeActions* action,
                            BattleRuntimeState* state)
        : core_(&core),
          status_(status),
          combo_(combo),
          damage_(damage),
          movement_(movement),
          deathEffects_(deathEffects),
          action_(action),
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
    BattleRuntimeUnitActionView action() const { return BattleRuntimeUnitActionView(*action_, id()); }
    BattleRuntimeUnitRescueView rescue() const;
    int mpRecoveryBonusPct() const { return combo().sumAlways(EffectType::MPRecoveryBonus); }

private:
    BattleRuntimeUnit* core_{};
    BattleStatusRuntimeUnit* status_{};
    RoleComboState* combo_{};
    BattleDamageRuntimeUnit* damage_{};
    BattleMovementAgentState* movement_{};
    BattleDeathEffectExtras* deathEffects_{};
    BattleRuntimeActions* action_{};
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
