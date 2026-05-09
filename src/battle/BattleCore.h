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
#include <cstdint>
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
    int speed = 0;
    int cooldown = 0;
    int cooldownMax = 0;
    bool haveAction = false;
    int actFrame = 0;
    BattleOperationType operationType = BattleOperationType::None;
    int actType = -1;
    int operationCount = 0;
    int physicalPower = 0;
    int invincible = 0;
    int hurtFrame = 0;
    int shield = 0;
    bool mpBlocked = false;
    int mpRecoveryBonusPct = 0;
    std::map<int, int> actPropertiesByMagicType;
    Pointf position;
    Pointf velocity;
    Pointf acceleration;
    Pointf facing;
    Point grid;
    bool canAttack = true;
    double reach = 0.0;
    CombatStyle style = CombatStyle::Melee;
    int star = 1;
    int cost = 0;
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
    void setPosition(int unitId, Pointf position);
    void setMotion(int unitId, Pointf position, Pointf velocity, Pointf acceleration);
};

int findNearestEnemyUnitId(const BattleUnitStore& units, int sourceUnitId);
int findFarthestEnemyUnitId(const BattleUnitStore& units, int sourceUnitId);

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

struct BattleRuntimeRandom
{
    std::uint32_t state = 0x6d2b79f5u;

    std::uint32_t nextRaw();
    double nextPercent();
    int nextInt(int upperBound);
};

struct BattleFrameKnockbackDelta
{
    int targetUnitId{};
    Pointf velocityDelta;
    double velocityCap = 0.0;
    bool grantHurtFrame = false;
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

struct BattleFrameDeathEffectTrackerResult
{
    int unitId{};
    int shieldOnAllyDeathTracker{};
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
    BattleUnitStore units;
    BattleWorldState world;
    BattleAttackWorld attacks;
    BattleRuntimeRandom random;

    struct DamageState
    {
        bool aggregatePendingTransactionsByDefender = false;
        std::vector<BattleDamageRuntimeUnit> unitExtras;
        std::map<int, BattleDamagePresentationStyle> presentationStylesByDefender;
        std::map<int, BattleDamageApplicationUnitEffects> unitEffects;
        std::vector<BattleDamageTransactionInput> pendingTransactions;
        std::vector<BattleDamagePresentationInput> pendingPresentation;
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
        std::vector<BattleFrameRescueUnitSnapshot> units;
        std::vector<BattleRescueCellSnapshot> cells;
        std::map<std::pair<int, int>, Pointf> positionsByCell;
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
        BattleMovementPhysicsCollisionWorld collision;
        std::vector<int> actionCastFrames;
        int dashMomentumFrames = 0;
    } movementPhysics;

    struct ActionState
    {
        std::map<int, BattleActionPlanSeed> planSeeds;
        std::map<int, BattleActionCommitInput> pendingCommitInputs;
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
    std::map<int, BattleMovementPhysicsState> movementRuntime;
    std::set<int> ultimateCasters;

    std::vector<BattleAttackSpawnRequest> pendingAttackSpawns;
};

struct BattleFrameResult
{
    BattlePresentationFrame frame;
    BattleTickResult movement;
    std::vector<BattleAttackEvent> attackEvents;
    BattleFrameApplications applications;
    std::vector<BattleProjectileCancelDamageCommand> projectileCancelDamageCommands;
    std::vector<BattleFrameUnitApplication> unitApplications;
    std::vector<BattleFrameMovementPhysicsUnitResult> movementPhysicsResults;
    std::vector<BattleFrameMovementPresentationUnitResult> movementPresentationResults;
    std::vector<BattleHitResolutionResult> hitResults;
    std::vector<BattleFrameActionUnitResult> actionResults;
    std::vector<BattleDamageTransactionResult> damageTransactions;
    std::vector<BattleFrameDamageRenderApplication> damageRenderApplications;
    std::vector<BattleRescueRepositionResult> rescueResults;
    std::vector<BattleTeamEffectEvent> teamEffectEvents;
    std::vector<BattleEffectCommand> effectCommands;
    std::vector<BattleFrameDeathEffectTrackerResult> deathEffectTrackers;
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

BattleDamageRuntimeUnit makeBattleDamageRuntimeUnit(const BattleDamageUnitState& unit);

}  // namespace KysChess::Battle
