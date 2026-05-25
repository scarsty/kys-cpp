#pragma once

#include "BattleAttackSystem.h"
#include "BattleCastSystem.h"
#include "BattleComboTriggerSystem.h"
#include "BattleDamageQueue.h"
#include "BattleDamageSystem.h"
#include "BattleDeathEffectSystem.h"
#include "BattleHitResolver.h"
#include "BattleMovement.h"
#include "BattlePresentation.h"
#include "BattleRescueRepositionSystem.h"
#include "BattleRuntimeActions.h"
#include "BattleRuntimeQueues.h"
#include "BattleRuntimeRandom.h"
#include "BattleRuntimeUnits.h"
#include "BattleStatusSystem.h"
#include "BattleUnitStore.h"
#include "BattleTeamEffectSystem.h"
#include "BattleUnitValues.h"

#include <cassert>
#include <cstddef>
#include <map>
#include <ranges>
#include <vector>

namespace KysChess::Battle
{

struct BattleUnitFrameTickState
{
    int cooldown = 0;
    int actFrame = 0;
    int actType = -1;
    BattleOperationType operationType = BattleOperationType::None;
    bool haveAction = false;
    int physicalPower = 0;
};

struct BattleUnitFrameTickInput
{
    BattleUnitFrameTickState state;
    int frame = 0;
    bool frozen = false;
    int mpRegenIntervalFrames = 0;
    int physicalPowerRegenIntervalFrames = 0;
};

struct BattleUnitFrameTickResult
{
    BattleUnitFrameTickState state;
    int mpDelta = 0;
    bool skillFinished = false;
    bool resetDashVelocity = false;
};

class BattleUnitFrameTickSystem
{
public:
    BattleUnitFrameTickResult advance(const BattleUnitFrameTickInput& input) const;
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

class BattleFrameRunner
{
public:
    BattlePresentationFrame runFrame(BattleRuntimeState& runtime) const;
};

}  // namespace KysChess::Battle
