#pragma once

#include "BattleSceneAct.h"
#include "battle/BattleAttackSystem.h"
#include "battle/BattleCastSystem.h"
#include "battle/BattleCore.h"
#include "battle/BattleProjectileTargetingSystem.h"

#include <array>
#include <cstddef>
#include <deque>
#include <functional>
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
    Role* target = nullptr;
    BattleCastSkillAdapterInput normalSkill;
    BattleCastSkillAdapterInput ultimateSkill;
    bool canStartAttack = true;
    bool movementDashActive = false;
    bool dashAttackEnabled = false;
    Pointf dashVelocity;
    int dashHitCount = 1;
    bool emitDashFollowUpSkillAttack = false;
    int dashFollowUpOperationType = -1;
    double targetDistance{};
    double meleeAttackReach{};
    double dashAttackReach{};
    int operationCount = 0;
    int cooldownReductionPct = 0;
};

struct BattleSceneFrameBundle
{
    Battle::BattleFrameState state;
    std::unordered_map<int, Role*> rolesByBattleId;
};

struct BattleFrameLegacyRoleSnapshot
{
    Role* role = nullptr;
    int unitId = -1;
    Point grid;
    bool alive = false;
    int movementDashFrames = 0;
    int movementDashCooldown = 0;
    int movementDashSpreadFrames = 0;
};

struct BattleFrameLegacyActionUnitSnapshot
{
    int unitId = -1;
    int nearestEnemyUnitId = -1;
    int farthestEnemyUnitId = -1;
    Magic* normalMagic = nullptr;
    Magic* ultimateMagic = nullptr;
    bool forceRangedMagic = false;
    int forcedRangedMinSelectDistance = 0;
    int projectileSpeedMultiplierPct = 100;
    int ultimateExtraProjectileCount = 0;
    double normalEffectiveReach = 0.0;
    double ultimateEffectiveReach = 0.0;
    bool normalRangedStyle = false;
    bool ultimateRangedStyle = false;
    double normalBlinkReach = 0.0;
    double ultimateBlinkReach = 0.0;
    std::array<int, 4> castFrameByOperation{};
    std::array<double, 2> randomUnitRolls{};
    int projectileBounceRoll = 0;
    int blinkRandomRoll = 0;
    int blinkCellRandomRoll = 0;
};

struct BattleFrameLegacyGridSnapshot
{
    std::vector<Battle::BattleRescueCellSnapshot> rescueCells;
    std::vector<Battle::BattleMovementPhysicsCollisionCellSnapshot> movementCells;
    std::map<std::pair<int, int>, Pointf> positionsByCell;
};

struct BattleFrameLegacySnapshot
{
    int battleFrame = 0;
    std::vector<Role*> roles;
    std::vector<BattleFrameLegacyRoleSnapshot> roleSnapshots;
    std::unordered_map<int, BattleFrameLegacyActionUnitSnapshot> actionsByUnitId;
    std::unordered_map<int, Role*> rolesByBattleId;
    std::map<int, RoleComboState>* comboStates = nullptr;
    BattleFrameLegacyGridSnapshot grid;
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
    const BattleFrameLegacySnapshot* snapshot = nullptr;
    std::unordered_map<Role*, Battle::BattleCastResult>* pendingCastResults = nullptr;
    std::map<int, RoleComboState>* comboStates = nullptr;
    const std::unordered_map<Role*, Battle::MovementDecision>* movementDecisions = nullptr;
    std::set<Role*>* ultimateCasters = nullptr;
    std::vector<int> movementDashStartUnitIds;
    BattleActionFrameAdapterConfig config;
};

struct BattleRescueFrameAdapterConfig
{
    double executeUnattendedRadius = 0.0;
    int counterAttackSkillId = -1;
    int counterAttackVisualEffectId = -1;
    double counterAttackProjectileSpeed = 0.0;
    double counterAttackMeleeOffset = 0.0;
    int counterAttackMinimumTotalFrames = 20;
    int counterAttackTotalFramePadding = 15;
};

struct BattleRescueFrameAdapterContext
{
    const std::vector<Role*>* roles = nullptr;
    const std::map<int, RoleComboState>* comboStates = nullptr;
    const BattleFrameLegacySnapshot* snapshot = nullptr;
    std::vector<Battle::BattleRescueCellSnapshot> cells;
    std::map<std::pair<int, int>, Pointf> positionsByCell;
    BattleRescueFrameAdapterConfig config;
};

struct BattleMovementPhysicsFrameAdapterConfig
{
    float gravity = -4.0f;
    float friction = 0.1f;
    int postDashSpreadFrames = 0;
    double tileWidth = 0.0;
    int coordCount = 0;
    double defaultSeparationDistance = 0.0;
    int dashMomentumFrames = 0;
    std::array<int, 4> castFrames{};
};

struct BattleMovementPhysicsFrameAdapterContext
{
    const std::vector<Role*>* roles = nullptr;
    const BattleFrameLegacySnapshot* snapshot = nullptr;
    std::vector<Battle::BattleMovementPhysicsCollisionCellSnapshot> cells;
    BattleMovementPhysicsFrameAdapterConfig config;
};

struct BattleActionFrameApplyResult
{
    std::vector<int> attackSoundIds;
    int blinkSoundCount = 0;
    std::vector<Battle::BattleAttackSpawnRequest> attackSpawnRequests;
    std::vector<Battle::BattlePresentationEvent> presentationEvents;
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
    std::function<void()> onUnitDied;
};

struct BattleLifecycleApplicationResult
{
    bool battleEnded = false;
    int battleResult = -1;
};

Role* findRoleByBattleId(const std::vector<Role*>& roles, int unitId);

Battle::BattleCastConfig makeBattleCastConfig();
Battle::BattleCastGeometry makeBattleCastGeometry();
Battle::BattleCastSkillState makeBattleCastSkillState(Role* unit, const BattleCastSkillAdapterInput& input);
Battle::BattleCastInput makeBattleCastInput(const BattleCastAdapterInput& input);
void applyBattleCastStart(Role* unit, const Battle::BattleCastResult& result, int actType);
void applyBattleCastCommit(Role* unit, const Battle::BattleCastResult& result);

Battle::BattleAttackWorld makeBattleAttackWorld(
    const std::vector<Role*>& roles,
    const Battle::BattleAttackWorld& activeWorld,
    const std::unordered_map<int, std::set<int>>& sharedHitGroupTargets);
Battle::BattleTeamEffectWorld makeBattleTeamEffectWorld(
    const std::vector<Role*>& roles,
    const std::map<int, RoleComboState>& states);
const Battle::BattleTeamEffectUnit& findBattleTeamEffectUnit(
    const Battle::BattleTeamEffectWorld& world,
    int unitId);
Battle::BattlePresentationColor makeBattlePresentationColor(Color color);

Battle::BattleFrameUnitRuntimeInput makeBattleFrameUnitRuntimeInput(
    Role* role,
    int frame,
    int mpRegenIntervalFrames,
    int physicalPowerRegenIntervalFrames);
void applyBattleFrameUnitRuntimeResult(Role* role, const Battle::BattleFrameUnitRuntimeResult& result);
void applyBattleProjectileCancelDamage(Role* role, int damage);
Battle::BattleActionCommitUnitSnapshot makeBattleActionCommitUnitSnapshot(Role* role);
Battle::BattleActionTargetSnapshot makeBattleActionTargetSnapshot(Role* role);
Battle::BattleActionItemSnapshot makeBattleActionItemSnapshot(Item* item);

Battle::BattleHitUnitSnapshot makeBattleHitUnitSnapshot(Role* unit);
Battle::BattleHitSkillSnapshot makeBattleHitSkillSnapshot(Role* attacker,
                                                          Role* defender,
                                                          Magic* magic,
                                                          int resolvedBaseDamage);
Battle::BattleHitItemSnapshot makeBattleHitItemSnapshot(Item* item, int resolvedDamage);
Battle::BattleStatusUnitState makeBattleStatusUnit(Role* role, const RoleComboState& state);
void writeBattleStatusUnit(Role* role, RoleComboState& state, const Battle::BattleStatusUnitState& unit);
Battle::BattleDamageUnitState makeBattleDamageUnit(Role* role, const RoleComboState* state);
void writeBattleDamageUnit(Role* role, RoleComboState* state, const Battle::BattleDamageUnitState& unit);
Battle::BattleCooldownState makeBattleCooldownState(Role* role);
void writeBattleCooldownState(Role* role, const Battle::BattleCooldownState& state);
Battle::BattleDamagePresentationStyle makeBattleDamagePresentationStyle(Role* role);
void populateBattleFrameHitUnits(
    Battle::BattleFrameState& frameState,
    const std::vector<Role*>& roles);
void appendBattleFrameHitInput(
    Battle::BattleFrameState& frameState,
    const BattleFrameHitAdapterInput& input);
void populateBattleFrameRescueState(
    Battle::BattleFrameState& frameState,
    const BattleRescueFrameAdapterContext& context);
void populateBattleMovementPhysicsFrame(
    Battle::BattleFrameState& frameState,
    const BattleMovementPhysicsFrameAdapterContext& context);
void applyBattleMovementPhysicsFrameResults(
    const Battle::BattleFrameState& frameState,
    const BattleMovementPhysicsFrameAdapterContext& context);
void populateBattleActionFrame(
    Battle::BattleFrameState& frameState,
    BattleActionFrameAdapterContext& context);
BattleActionFrameApplyResult applyBattleActionFrameResults(
    const Battle::BattleFrameState& frameState,
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
std::vector<int> selectBattleNearbyProjectileTargets(
    const Battle::BattleProjectileTargetWorld& world,
    int sourceUnitId,
    int centerTargetUnitId,
    int rangePixels);
std::vector<int> selectBattleAreaImpactTargets(
    const Battle::BattleProjectileTargetWorld& world,
    int originUnitId,
    int areaSize,
    int maxTargets,
    int trackedTargetUnitId);

}  // namespace KysChess::BattleSceneBattleAdapter
