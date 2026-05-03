#pragma once

#include "BattleAttackSystem.h"
#include "BattleCastSystem.h"
#include "BattleComboTriggerSystem.h"
#include "BattleDamageSystem.h"
#include "BattleDeathEffectSystem.h"
#include "BattleMovement.h"
#include "BattlePresentation.h"
#include "BattleStatusSystem.h"

#include <map>
#include <vector>

namespace KysChess::Battle
{

class BattleCore
{
public:
    explicit BattleCore(BattleWorldState& world);

    BattleTickResult tickMovement();

private:
    BattleWorldState& world_;
};

struct BattleFrameUnitRuntimeState
{
    int cooldown = 0;
    int actFrame = 0;
    int actType = -1;
    BattleOperationType operationType = BattleOperationType::None;
    bool haveAction = false;
    int physicalPower = 0;
};

struct BattleFrameUnitRuntimeInput
{
    BattleFrameUnitRuntimeState state;
    int frame = 0;
    bool frozen = false;
    int mpRegenIntervalFrames = 0;
    int physicalPowerRegenIntervalFrames = 0;
};

struct BattleFrameUnitRuntimeResult
{
    BattleFrameUnitRuntimeState state;
    int mpDelta = 0;
    bool skillFinished = false;
    bool resetDashVelocity = false;
};

class BattleFrameUnitRuntimeSystem
{
public:
    BattleFrameUnitRuntimeResult advance(const BattleFrameUnitRuntimeInput& input) const;
};

struct BattleFrameState
{
    BattleWorldState world;
    BattleAttackWorld attacks;
    struct CastState
    {
        std::vector<BattleCastInput> pendingInputs;
        std::vector<BattleCastResult> committedResults;
    } casts;

    struct DamageState
    {
        std::vector<BattleDamageTransactionInput> pendingTransactions;
        std::vector<BattleDamageTransactionResult> committedTransactions;
    } damage;

    struct StatusState
    {
        BattleStatusSystemConfig config;
        std::vector<BattleStatusUnitState> units;
        std::vector<BattleStatusEvent> events;
    } status;

    struct ComboTriggerState
    {
        std::map<int, RoleComboState> units;
        std::vector<BattleComboTriggerEvent> events;
    } combo;

    struct DeathEffectState
    {
        BattleDeathEffectWorld world;
        std::vector<BattleDeathEffectEvent> events;
    } deathEffects;

    struct BattleResultState
    {
        bool ended = false;
        int winningTeam = -1;
        bool eventEmitted = false;
        int endedFrame = -1;
        std::map<int, int> pendingAliveByTeam;
    } result;

    std::vector<BattleAttackSpawnRequest> pendingAttackSpawns;
};

struct BattleFrameResult
{
    BattlePresentationFrame frame;
    BattleTickResult movement;
    std::vector<BattleAttackEvent> attackEvents;
};

class BattleFrameRunner
{
public:
    BattleFrameResult advanceFrame(BattleFrameState& state) const;
};

}  // namespace KysChess::Battle
