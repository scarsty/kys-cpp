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
#include "BattleRescueRepositionSystem.h"
#include "BattleStatusSystem.h"
#include "BattleTeamEffectSystem.h"

#include <cstddef>
#include <map>
#include <set>
#include <string>
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

struct BattleGridTransform
{
    double tileWidth = 0.0;
    int coordCount = 0;

    Point toGrid(Pointf position) const;
};

struct BattleRuntimeUnit
{
    int id = -1;
    int realRoleId = -1;
    std::string name;
    int team = 0;
    bool alive = true;
    int hp = 0;
    int maxHp = 0;
    int mp = 0;
    int maxMp = 0;
    int attack = 0;
    int defence = 0;
    int cooldown = 0;
    int cooldownMax = 0;
    int physicalPower = 0;
    int invincible = 0;
    int shield = 0;
    bool mpBlocked = false;
    int mpRecoveryBonusPct = 0;
    Pointf position;
    Pointf velocity;
    Pointf acceleration;
    Pointf facing;
    Point grid;
    bool canAttack = true;
    double reach = 0.0;
    CombatStyle style = CombatStyle::Melee;
};

struct BattleUnitStore
{
    BattleGridTransform gridTransform;
    std::vector<BattleRuntimeUnit> units;

    BattleRuntimeUnit* findUnit(int unitId);
    const BattleRuntimeUnit* findUnit(int unitId) const;
    BattleRuntimeUnit& requireUnit(int unitId);
    const BattleRuntimeUnit& requireUnit(int unitId) const;
    void writeDamageUnit(const BattleDamageUnitState& source);
    void writeStatusUnit(const BattleStatusUnitState& source);
    void setPosition(int unitId, Pointf position);
    void setMotion(int unitId, Pointf position, Pointf velocity, Pointf acceleration);
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

struct BattleFrameKnockbackDelta
{
    int targetUnitId = -1;
    Pointf velocityDelta;
    double velocityCap = 0.0;
    bool grantHurtFrame = false;
};

struct BattleFrameMpRestoreDelta
{
    int unitId = -1;
    int amount = 0;
};

struct BattleFrameUnitHealDelta
{
    int sourceUnitId = -1;
    int targetUnitId = -1;
    int amount = 0;
};

struct BattleFrameUnitShieldDelta
{
    int sourceUnitId = -1;
    int targetUnitId = -1;
    int amount = 0;
};

struct BattleFrameTempAttackBuffDelta
{
    int unitId = -1;
    int attackBonus = 0;
    int defenceBonus = 0;
    int durationFrames = 0;
    bool permanent = false;
};

struct BattleFrameLastAttackerDelta
{
    int targetUnitId = -1;
    int attackerUnitId = -1;
};

struct BattleFrameAutoUltimateRequest
{
    int unitId = -1;
    bool consumeMp = false;
};

struct BattleFrameRumbleEvent
{
    int lowFrequency = 0;
    int highFrequency = 0;
    int durationMs = 0;
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

struct BattleRuntimeState
{
    BattleUnitStore units;
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
        std::vector<BattleLogEvent> logEvents;
        std::vector<BattleVisualEvent> visualEvents;
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
        BattleDeathEffectStore store;
        std::vector<BattleDeathEffectEvent> events;
    } deathEffects;

    struct RescueState
    {
        std::vector<BattleFrameRescueUnitSnapshot> units;
        std::vector<BattleRescueCellSnapshot> cells;
        std::map<std::pair<int, int>, Pointf> positionsByCell;
        double executeUnattendedRadius = 0.0;
        BattleFrameRescueCounterAttackConfig counterAttack;
        std::vector<BattleRescueRepositionResult> committedResults;
    } rescue;

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
        double healAuraRadius = 0.0;
        std::vector<BattleGameplayCommand> pendingCommands;
        std::vector<BattleTeamEffectEvent> committedEvents;
    } teamEffects;

    struct EffectState
    {
        std::map<int, int> activationCounts;
        std::vector<BattleEffectCommand> committedCommands;
    } effects;

    struct ActionState
    {
        std::vector<BattleFrameActionUnitInput> units;
        std::vector<BattleFrameActionUnitResult> unitResults;
    } actions;

    struct MovementPhysicsState
    {
        BattleMovementPhysicsConfig config;
        BattleMovementPhysicsCollisionWorld collision;
        std::vector<BattleFrameMovementPhysicsUnitInput> units;
        std::vector<BattleFrameMovementPhysicsUnitResult> committedResults;
    } movementPhysics;

    struct HitState
    {
        std::vector<BattleHitUnitSnapshot> units;
        std::vector<BattleFrameHitSkillInput> skills;
        std::vector<BattleFrameHitItemInput> items;
        std::vector<BattleFrameHitScalarInput> scalars;
        std::vector<BattleHitResolutionResult> committedResults;
    } hits;

    BattleProjectileFollowUpContext projectileFollowUps;
    std::map<int, BattleMovementPhysicsState> movementRuntime;
    std::map<int, MovementDecision> movementDecisions;
    std::map<int, BattleCastResult> pendingCastResults;
    std::set<int> ultimateCasters;

    struct ApplicationState
    {
        std::vector<BattleFrameKnockbackDelta> knockbacks;
        std::vector<BattleFrameMpRestoreDelta> mpRestores;
        std::vector<BattleFrameUnitHealDelta> unitHeals;
        std::vector<BattleFrameUnitShieldDelta> unitShields;
        std::vector<BattleFrameTempAttackBuffDelta> tempAttackBuffs;
        std::vector<BattleFrameLastAttackerDelta> lastAttackers;
        std::vector<BattleFrameAutoUltimateRequest> autoUltimateRequests;
        std::vector<BattleFrameRumbleEvent> rumbles;
    } applications;

    std::vector<BattleAttackSpawnRequest> pendingAttackSpawns;
};

struct BattleFrameResult
{
    BattlePresentationFrame frame;
    BattleTickResult movement;
    std::vector<BattleAttackEvent> attackEvents;
    // 遷移期間的不完整快照退路；場景與 adapter 不得消費這些命令。
    std::vector<BattleGameplayCommand> commands;
};

struct BattleTeamEffectCommandApplication
{
    std::vector<BattleTeamEffectEvent> events;
    std::vector<BattleLogEvent> logEvents;
};

BattleTeamEffectCommandApplication applyBattleTeamEffectCommand(
    BattleUnitStore& units,
    const BattleGameplayCommand& command);

BattleProjectileBouncePrime collectFrameProjectileBouncePrime(
    const KysChess::RoleComboState& state,
    int attackerUnitId,
    int rollPct,
    int defaultRange);
int collectFrameExtraProjectileCount(KysChess::RoleComboState& state, int unitId, int baseCount);
bool frameComboHasExecute(const KysChess::RoleComboState& state, int attackerUnitId);
double resolveFrameArmorPenetratedDefense(
    const KysChess::RoleComboState& state,
    int attackerUnitId,
    int targetUnitId,
    double defense,
    double rollPercent);

class BattleFrameRunner
{
public:
    BattleFrameResult runFrame(BattleRuntimeState& runtime) const;
};

}  // namespace KysChess::Battle
