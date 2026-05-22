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
#include "BattleUnitStore.h"
#include "BattleTeamEffectSystem.h"
#include "BattleUnitValues.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace KysChess::Battle
{

class BattleCore
{
public:
    explicit BattleCore(BattleMovementFrameInput& world);

    BattleTickResult tickMovement();

private:
    BattleMovementFrameInput& world_;
};

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

struct BattleFrameUnitApplication
{
    int unitId{};
    int cooldown{};
    int actFrame{};
    int actType{};
    int finalMp{};
    bool resetDashVelocity{};
};

struct BattleActionRulesConfig
{
    double tileWidth = 0.0;
    double maxEffectiveBattleReach = 0.0;
    double meleeAttackHitRadius = 0.0;
    double meleeAttackReach = 0.0;
    double dashAttackMeleeReach = 0.0;
    double blinkWeakTargetDefWeight = 0.0;
    int dashMomentumFrames = 0;
    int movementDashCooldownFrames = 0;
    int actionRecoveryFrames = 0;
    int dashRecoveryFrames = 0;
    int strengthenedMeleeOperationCountThreshold = 0;
    int projectileBounceRange = 0;
    int coordCount = 0;
};

struct BattleActionSkillSeed
{
    int id = -1;
    std::string name;
    int soundId = -1;
    int hurtType = 0;
    int attackAreaType = -1;
    int magicType = -1;
    int visualEffectId = -1;
    int selectDistance = 1;
    int actProperty = 0;
    int magicPower = 0;
};

struct BattleActionPlanSeed
{
    int unitId{};
    bool hasEquippedSkill = false;
    BattleActionSkillSeed normalSkill;
    BattleActionSkillSeed ultimateSkill;
};

struct BattlePendingCastAction
{
    int unitId = -1;
    int targetUnitId = -1;
    bool ultimate = false;
    BattleOperationType operationType = BattleOperationType::None;
    BattleCastSkillState skill;
};

struct BattleFrameActionUnitResult
{
    int unitId{};
    BattleUnitFrameTickState state;
    bool castStarted = false;
    bool actionCommitted = false;
    bool castCommitted = false;
    BattleActionCommitInput actionInput;
    BattleCastResult castResult;
    BattleActionCommitResult actionResult;
};

class BattleRuntimeRandom
{
public:
    explicit BattleRuntimeRandom(unsigned int seed = 1);

    unsigned int seed() const;
    double nextPercent();
    int nextInt(int upperBound);
    bool chance(int chancePct);
    int symmetricInt(int exclusiveBound);

private:
    unsigned int seed_ = 1;
    std::mt19937 rand_;
};

struct BattleFrameKnockbackDelta
{
    int targetUnitId{};
    Pointf velocityDelta;
    double velocityCap = 0.0;
};

struct BattleFrameMpRestoreDelta
{
    int unitId{};
    int amount{};
};

struct BattleFrameUnitHealDelta
{
    int sourceUnitId{};
    int targetUnitId{};
    int amount{};
};

struct BattleFrameLastAttackerDelta
{
    int targetUnitId{};
    int attackerUnitId{};
};

struct BattleFrameRumbleEvent
{
    int lowFrequency{};
    int highFrequency{};
    int durationMs{};
};

struct BattleFrameApplications
{
    std::vector<BattleFrameKnockbackDelta> knockbacks;
    std::vector<BattleFrameMpRestoreDelta> mpRestores;
    std::vector<BattleFrameUnitHealDelta> unitHeals;
    std::vector<BattleFrameLastAttackerDelta> lastAttackers;
    std::vector<int> attackSoundIds;
    std::vector<BattleFrameRumbleEvent> rumbles;
};

struct BattleFrameComboRenderApplication
{
    int unitId{};
    int shield{};
    int blockFirstHitsRemaining{};
};

struct BattleFrameRenderStatusUnit
{
    int unitId{};
    int invincible{};
    int frozenFrames{};
    int frozenMaxFrames{};
};

struct BattleFrameStateApplications
{
    std::vector<BattleFrameRenderStatusUnit> statusRenderUnits;
    std::vector<BattleFrameComboRenderApplication> comboRenderUnits;
};

struct BattleFrameMovementPresentationUnitResult
{
    int unitId{};
    Pointf position;
    Pointf velocity;
    Pointf acceleration;
    Pointf facing;
    int frozenFrames{};
};

struct BattleFrameDamageRenderUnit
{
    int unitId{};
    int hp{};
    int mp{};
    int invincible{};
    bool alive{};
};

struct BattleFrameDamageRenderApplication
{
    BattleFrameDamageRenderUnit defender;
    BattleFrameDamageRenderUnit attacker;
    int frozenFrames{};
    int frozenMaxFrames{};
    int cooldown{};
    int committedHpDamage{};
    bool killed{};
    bool critical{};
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
    BattleUnitStore unitStore;
    BattleMovementState movement;
    BattleAttackState attacks;
    BattleRuntimeRandom random;

    struct DamageState
    {
        bool sortPendingDamageByDefenderMagnitude = false;
        std::vector<BattleDamageRuntimeUnit> unitExtras;
        std::map<int, BattleDamagePresentationStyle> presentationStylesByDefender;
        std::map<int, BattleDamageApplicationUnitEffects> unitEffects;
        std::vector<BattlePendingDamageIntent> pendingDamage;
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
        std::vector<BattleGameplayCommand> pendingCommands;
    } teamEffects;

    struct EffectState
    {
        std::map<int, int> activationCounts;
    } effects;

    struct MovementPhysicsState
    {
        BattleMovementPhysicsConfig config;
        BattleMovementPhysicsTerrain terrain;
        std::vector<int> actionCastFrames;
        int dashMomentumFrames = 0;
    } movementPhysics;

    struct ActionState
    {
        std::map<int, BattleActionPlanSeed> planSeeds;
        std::map<int, BattlePendingCastAction> pendingCasts;
        BattleCastConfig castConfig;
        BattleCastGeometry castGeometry;
        BattleActionRulesConfig actionRules;
        std::vector<int> castFrames;
        int actionRecoveryFrames = 0;
        int dashRecoveryFrames = 0;
        double blinkWeakTargetDefWeight = 0.0;
        int strengthenedMeleeOperationCountThreshold = 0;
        int projectileBounceRange = 0;
    } action;

    BattleProjectileFollowUpContext projectileFollowUps;
    std::set<int> ultimateCasters;

    std::vector<BattleAttackSpawnRequest> pendingAttackSpawns;
};

struct BattleFrameResult
{
    BattlePresentationFrame frame;
    std::vector<BattleAttackEvent> attackEvents;
    BattleFrameApplications applications;
    std::vector<BattleProjectileCancelDamageCommand> projectileCancelDamageCommands;
    std::vector<BattleFrameUnitApplication> unitApplications;
    std::vector<BattleFrameMovementPresentationUnitResult> movementPresentationResults;
    std::vector<BattleHitResolutionResult> hitResults;
    std::vector<BattleFrameActionUnitResult> actionResults;
    std::vector<BattleDamageTransactionResult> damageTransactions;
    std::vector<BattleFrameDamageRenderApplication> damageRenderApplications;
    std::vector<BattleRescueRepositionResult> rescueResults;
    std::vector<BattleTeamEffectEvent> teamEffectEvents;
    std::vector<BattleEffectCommand> effectCommands;
    BattleFrameStateApplications stateApplications;
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
int collectFrameExtraProjectileCount(
    KysChess::RoleComboState& state,
    BattleRuntimeRandom& random,
    int unitId,
    int baseCount);
std::vector<BattleGameplayCommand> collectFrameCastScopedComboCommands(
    KysChess::RoleComboState& state,
    BattleRuntimeRandom& random,
    int unitId,
    double projectileSpeed);
bool frameComboHasExecute(const KysChess::RoleComboState& state, int attackerUnitId);
double resolveFrameArmorPenetratedDefense(
    const KysChess::RoleComboState& state,
    int attackerUnitId,
    int targetUnitId,
    double defense,
    BattleRuntimeRandom& random);

class BattleFrameRunner
{
public:
    BattleFrameResult runFrame(BattleRuntimeState& runtime) const;
};

BattleDamageRuntimeUnit makeBattleDamageRuntimeUnit(const BattleDamageUnitState& unit);

}  // namespace KysChess::Battle
