#pragma once

#include "BattleDamageSystem.h"
#include "BattleDeathEffectSystem.h"
#include "BattleMovement.h"
#include "BattleRescueRepositionSystem.h"
#include "BattleRuntimeActions.h"
#include "BattleStatusSystem.h"
#include "BattleTypes.h"
#include "BattleUnitStore.h"
#include "../ChessBattleEffects.h"

#include <algorithm>
#include <cassert>
#include <ranges>

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

    BattleStatusEffectState& effects() const { return status_->effects; }
    bool frozen() const { return status_->effects.frozenTimer > 0; }
    bool mpBlocked() const { return status_->effects.mpBlockTimer > 0; }

    int mpRecoveryBonusPct(const BattleRuntimeUnitComboView& combo) const
    {
        return mpBlocked() ? 0 : combo.sumAlways(EffectType::MPRecoveryBonus);
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

}  // namespace KysChess::Battle
