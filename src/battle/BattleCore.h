#pragma once

#include "BattleAttackSystem.h"
#include "BattleCastSystem.h"
#include "BattleComboTriggerSystem.h"
#include "BattleDamageApplicationSystem.h"
#include "BattleDamageSystem.h"
#include "BattleDeathEffectSystem.h"
#include "BattleEffectSystem.h"
#include "BattleHitResolver.h"
#include "BattleMovement.h"
#include "BattlePresentation.h"
#include "BattleStatusSystem.h"
#include "BattleTeamEffectSystem.h"

#include <cstddef>
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

struct BattleFrameRuntimeUnitInput
{
    int unitId = -1;
    BattleFrameUnitRuntimeInput input;
    int hp = 0;
    int maxHp = 0;
    bool alive = false;
    bool lastAlive = false;
};

struct BattleFrameRuntimeUnitResult
{
    int unitId = -1;
    BattleFrameUnitRuntimeResult result;
    std::vector<BattleComboFrameRuntimeEvent> comboEvents;
    RoleComboState comboState;
};

struct BattleProjectileCancelBaseDamage
{
    int attackId = -1;
    int otherAttackId = -1;
    int baseDamage = 0;
};

struct BattleFrameActionUnitState
{
    bool haveAction = false;
    int actFrame = 0;
    int actType = -1;
    int castFrame = 0;
    BattleOperationType operationType = BattleOperationType::None;
    int cooldownFrames = 0;
    int recoveryFrames = 0;
};

struct BattleFrameActionUnitInput
{
    int unitId = -1;
    BattleFrameActionUnitState state;
    bool canPlanCast = false;
    BattleCastInput castInput;
    bool hasSelectedCastInput = false;
    BattleCastInput selectedCastInput;
    bool selectedCastUltimate = false;
    BattleOperationType selectedOperationType = BattleOperationType::None;
    BattleActionCommitInput selectedActionInput;
    bool hasPendingActionInput = false;
    BattleActionCommitInput pendingActionInput;
};

struct BattleFrameActionUnitResult
{
    int unitId = -1;
    BattleFrameActionUnitState state;
    bool castStarted = false;
    bool actionCommitted = false;
    bool castCommitted = false;
    BattleActionCommitInput actionInput;
    BattleCastResult castResult;
    BattleActionCommitResult actionResult;
};

struct BattleFrameHitScalarInput
{
    int attackId = -1;
    int attackerUnitId = -1;
    int defenderUnitId = -1;
    int resolvedMagicBaseDamage = 0;
    int resolvedHiddenWeaponDamage = 0;
    int sharedBleedMaxStacks = 1;
    int randomDamageVariance = 0;
    std::vector<double> percentRolls;
    int pendingDefenderHpDamage = 0;
};

struct BattleFrameHitSkillInput
{
    int attackId = -1;
    int attackerUnitId = -1;
    int defenderUnitId = -1;
    BattleHitSkillSnapshot skill;
};

struct BattleFrameHitItemInput
{
    int attackId = -1;
    int attackerUnitId = -1;
    int defenderUnitId = -1;
    BattleHitItemSnapshot item;
};

struct BattleFrameState
{
    BattleWorldState world;
    BattleAttackWorld attacks;
    struct RuntimeState
    {
        std::vector<BattleFrameRuntimeUnitInput> units;
        std::vector<BattleFrameRuntimeUnitResult> committedResults;
        std::vector<double> percentRolls;
        std::size_t nextPercentRoll = 0;
    } runtime;

    struct DamageState
    {
        bool aggregatePendingTransactionsByDefender = false;
        std::vector<BattleDamageUnitState> units;
        std::map<int, BattleCooldownState> cooldowns;
        std::vector<BattleStatusUnitState> statusUnits;
        std::map<int, BattleDamagePresentationStyle> presentationStylesByDefender;
        std::map<int, BattleDamageApplicationUnitEffects> unitEffects;
        std::vector<BattleDamageTransactionInput> pendingTransactions;
        std::vector<BattleDamagePresentationInput> pendingPresentation;
        std::vector<BattleDamageTransactionResult> committedTransactions;
        std::vector<BattleDamageLifecycleEvent> lifecycleEvents;
        std::vector<BattlePresentationEvent> presentationEvents;
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

    struct ProjectileCancelState
    {
        std::vector<BattleProjectileCancelBaseDamage> baseDamages;
        std::vector<BattleProjectileCancelDamageCommand> committedCommands;
    } projectileCancel;

    struct TeamEffectState
    {
        BattleTeamEffectWorld world;
        double healAuraRadius = 0.0;
        std::vector<BattleGameplayCommand> pendingCommands;
        std::vector<BattleTeamEffectEvent> committedEvents;
    } teamEffects;

    struct EffectState
    {
        BattleEffectWorld world;
        std::vector<BattleEffectCommand> committedCommands;
    } effects;

    struct ActionState
    {
        std::vector<BattleFrameActionUnitInput> units;
        std::vector<BattleFrameActionUnitResult> unitResults;
    } actions;

    struct HitState
    {
        std::vector<BattleHitUnitSnapshot> units;
        std::vector<BattleFrameHitSkillInput> skills;
        std::vector<BattleFrameHitItemInput> items;
        std::vector<BattleFrameHitScalarInput> scalars;
        std::vector<BattleHitResolutionResult> committedResults;
    } hits;

    BattleProjectileFollowUpContext projectileFollowUps;

    std::vector<BattleAttackSpawnRequest> pendingAttackSpawns;
};

struct BattleFrameResult
{
    BattlePresentationFrame frame;
    BattleTickResult movement;
    std::vector<BattleAttackEvent> attackEvents;
    std::vector<BattleGameplayCommand> commands;
};

struct BattleTeamEffectCommandApplication
{
    std::vector<BattleTeamEffectEvent> events;
    std::vector<BattlePresentationEvent> presentationEvents;
};

BattleTeamEffectCommandApplication applyBattleTeamEffectCommand(
    BattleTeamEffectWorld& world,
    const BattleGameplayCommand& command);

class BattleFrameRunner
{
public:
    BattleFrameResult advanceFrame(BattleFrameState& state) const;
};

}  // namespace KysChess::Battle
