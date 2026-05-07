#pragma once

#include "BattleSceneAct.h"
#include "battle/BattleAttackSystem.h"
#include "battle/BattleCastSystem.h"
#include "battle/BattleCore.h"
#include "battle/BattleProjectileTargetingSystem.h"

#include <array>
#include <cstddef>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

class BattleTracker;

namespace KysChess::BattleSceneBattleAdapter
{

struct BattleCastSkillAdapterInput
{
    Magic* magic = nullptr;
    double reach{};
    bool forceRanged = false;
    bool rangedStyle = false;
    int projectileSpeedMultiplierPct = 100;
    int meleeSplashCount = 0;
    int extraProjectileCount = 0;
    bool strengthenedMelee = false;
};

struct BattleCastAdapterInput
{
    Role* unit = nullptr;
    BattleCastSkillAdapterInput normalSkill;
    BattleCastSkillAdapterInput ultimateSkill;
    bool canStartAttack = true;
    bool movementDashActive = false;
    bool dashAttackEnabled = false;
    Pointf dashVelocity;
    int dashHitCount = 1;
    bool emitDashFollowUpSkillAttack = false;
    int dashFollowUpOperationType = -1;
    double meleeAttackReach{};
    double dashAttackReach{};
    int operationCount = 0;
    int cooldownReductionPct = 0;
};

struct BattleFrameApplyContext
{
    std::unordered_map<int, Role*> rolesByBattleId;
};

struct BattleFrameHitAdapterInput
{
    int attackId = -1;
    Role* attacker = nullptr;
    Role* defender = nullptr;
    Magic* magic = nullptr;
    Item* hiddenWeapon = nullptr;
    int resolvedMagicBaseDamage = 0;
    int resolvedHiddenWeaponDamage = 0;
    int sharedBleedMaxStacks = 1;
    int randomDamageVariance = 0;
    std::vector<double> percentRolls;
    int pendingDefenderHpDamage = 0;
};

struct BattleActionFrameAdapterConfig
{
    double maxEffectiveBattleReach = 0.0;
    double meleeAttackHitRadius = 0.0;
    double meleeAttackReach = 0.0;
    double dashAttackMeleeReach = 0.0;
    double blinkWeakTargetDefWeight = 0.0;
    int dashMomentumFrames = 0;
    int movementDashCooldownFrames = 0;
    int actionRecoveryFrames = 0;
    int hiddenWeaponTotalFrame = 0;
    int battleFrame = 0;
    float gravity = 0.0f;
    int projectileBounceRange = 0;
    int coordCount = 0;
};

struct BattleActionFrameAdapterContext
{
    const std::vector<Role*>* roles = nullptr;
    const Battle::BattleUnitStore* units = nullptr;
    RandomDouble* random = nullptr;
    std::unordered_map<int, Point> unitCells;
    const std::map<int, Battle::BattleMovementPhysicsState>* movementRuntime = nullptr;
    std::map<int, Battle::BattleCastResult>* pendingCastResults = nullptr;
    std::map<int, RoleComboState>* comboStates = nullptr;
    const std::map<int, Battle::MovementDecision>* movementDecisions = nullptr;
    std::set<int>* ultimateCasters = nullptr;
    std::vector<int> movementDashStartUnitIds;
    BattleActionFrameAdapterConfig config;
};

struct BattleMovementPhysicsFrameAdapterContext
{
    const std::vector<Role*>* roles = nullptr;
};

struct BattleActionFrameApplyResult
{
    std::vector<int> attackSoundIds;
    int blinkSoundCount = 0;
    std::vector<Battle::BattleAttackSpawnRequest> attackSpawnRequests;
    std::vector<Battle::BattleLogEvent> logEvents;
    std::vector<Battle::BattleVisualEvent> visualEvents;
    std::vector<int> clearMovementDashSpreadUnitIds;
    std::vector<int> faceTowardsNearestUnitIds;
};

struct BattleSelectedSkillActionResult
{
    Magic* magic = nullptr;
    BattleActionFrameApplyResult applyResult;
};

struct BattleLifecycleApplicationContext
{
    BattleTracker* tracker = nullptr;
    const std::vector<Role*>* roles = nullptr;
    int currentBattleResult = -1;
};

struct BattleLifecycleApplicationResult
{
    bool battleEnded = false;
    int battleResult = -1;
    bool unitDied = false;
};

Role* findRoleByBattleId(const std::vector<Role*>& roles, int unitId);

Battle::BattleCastConfig makeBattleCastConfig();
Battle::BattleCastGeometry makeBattleCastGeometry();
Magic* selectLowerPowerMagic(Role* role);
Magic* selectHigherPowerMagic(Role* role);
bool roleForcesRangedMagic(const std::map<int, RoleComboState>& comboStates, int unitId);
int forcedRangedMinSelectDistance(const std::map<int, RoleComboState>& comboStates, int unitId);
int projectileSpeedMultiplierPct(const std::map<int, RoleComboState>& comboStates, int unitId);
double battleBlinkReach(const Magic* magic);
bool isBattleRangedStyleMagic(const Magic* magic, bool forceRanged);
double effectiveBattleReach(
    const Magic* magic,
    bool forceRanged,
    int forcedRangedMinSelectDistance,
    int projectileSpeedMultiplierPct);
Battle::BattleCastSkillState makeBattleCastSkillState(Role* unit, const BattleCastSkillAdapterInput& input);
Battle::BattleCastInput makeBattleCastInput(const BattleCastAdapterInput& input);
void applyBattleCastStart(Role* unit, const Battle::BattleCastResult& result, int actType);
void applyBattleCastCommit(Role* unit, const Battle::BattleCastResult& result);

void configureBattleAttackWorld(Battle::BattleAttackWorld& world);
Battle::BattlePresentationColor makeBattlePresentationColor(Color color);

Battle::BattleRuntimeUnit makeBattleRuntimeUnit(
    Role* role,
    const RoleComboState* state,
    const Battle::BattleGridTransform& gridTransform);
void applyBattleFrameUnitRuntimeResult(Role* role, const Battle::BattleFrameUnitRuntimeResult& result);
void applyBattleProjectileCancelDamage(Role* role, int damage);
Battle::BattleActionCommitUnitSnapshot makeBattleActionCommitUnitSnapshot(Role* role);
Battle::BattleActionTargetSnapshot makeBattleActionTargetSnapshot(Role* role);
Battle::BattleActionItemSnapshot makeBattleActionItemSnapshot(Item* item);

Battle::BattleStatusUnitState makeBattleStatusUnit(Role* role, const RoleComboState& state);
void writeBattleStatusUnit(Role* role, RoleComboState& state, const Battle::BattleStatusUnitState& unit);
Battle::BattleDamageUnitState makeBattleDamageUnit(Role* role, const RoleComboState* state);
void writeBattleDamageUnit(Role* role, RoleComboState* state, const Battle::BattleDamageUnitState& unit);
Battle::BattleDamagePresentationStyle makeBattleDamagePresentationStyle(Role* role);
void appendBattleFrameHitInput(
    Battle::BattleFrameScratch& scratch,
    const BattleFrameHitAdapterInput& input);
void applyBattleMovementPhysicsFrameResults(
    const std::vector<Battle::BattleFrameMovementPhysicsUnitResult>& movementResults,
    const BattleMovementPhysicsFrameAdapterContext& context);
void populateBattleActionFrame(
    Battle::BattleFrameScratch& scratch,
    BattleActionFrameAdapterContext& context);
BattleActionFrameApplyResult applyBattleActionFrameResults(
    const std::vector<Battle::BattleFrameActionUnitResult>& actionResults,
    const BattleActionFrameAdapterContext& context);
BattleSelectedSkillActionResult commitBattleSelectedSkillAction(
    Role* role,
    Magic* magic,
    bool isUltimate,
    int operationType,
    const BattleActionFrameAdapterContext& context);
BattleLifecycleApplicationResult applyBattleLifecycleEvents(
    const BattleLifecycleApplicationContext& context,
    const std::vector<Battle::BattleGameplayEvent>& events);
int resolveBattleMagicBaseDamage(const Battle::BattleMagicBaseDamageInput& input);

}  // namespace KysChess::BattleSceneBattleAdapter
