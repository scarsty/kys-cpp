#pragma once

#include "BattleSceneAct.h"
#include "battle/BattleAttackSystem.h"
#include "battle/BattleCastSystem.h"
#include "battle/BattleCore.h"
#include "battle/BattleDamageApplicationSystem.h"
#include "battle/BattleProjectileTargetingSystem.h"
#include "battle/BattleTeamEffectSystem.h"
#include "battle/BattleHitResolver.h"

#include <cstddef>
#include <deque>
#include <optional>
#include <set>
#include <unordered_map>
#include <vector>

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

enum class BattleTeamEffectCommitType
{
    Heal,
    MpRestore,
    Shield,
};

struct BattleTeamEffectCommitRequest
{
    BattleTeamEffectCommitType type = BattleTeamEffectCommitType::Heal;
    int sourceUnitId = -1;
    int flatHeal = 0;
    int pctHeal = 0;
    int amount = 0;
    bool refreshOnly = false;
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
Battle::BattleDamageRequest makeBattleMpLeechDamageRequest(int damage);
std::optional<Battle::BattleDamageApplicationUnitEffects> makeBattleDamageApplicationUnitEffects(
    const RoleComboState& state);
std::vector<Battle::BattleComboFrameRuntimeEvent> advanceBattleComboFrameRuntime(
    RoleComboState& state,
    const Battle::BattleComboFrameRuntimeInput& input);
Battle::BattleDodgeResolution resolveBattleDodge(
    const RoleComboState& state,
    int attackerUnitId,
    double rollPercent);
Battle::BattleProjectileBouncePrime collectBattleProjectileBouncePrime(
    const RoleComboState& state,
    int attackerUnitId,
    int rollPct,
    int defaultRange);
int collectBattleExtraProjectileCount(RoleComboState& state, int unitId, int baseCount);
bool battleComboHasExecute(const RoleComboState& state, int attackerUnitId);
double resolveBattleArmorPenetratedDefense(
    const RoleComboState& state,
    int attackerUnitId,
    int targetUnitId,
    double defense,
    double rollPercent);
int resolveBattleMagicBaseDamage(const Battle::BattleMagicBaseDamageInput& input);
std::vector<Battle::BattleTeamEffectEvent> applyBattleTeamEffect(
    Battle::BattleTeamEffectWorld& world,
    const BattleTeamEffectCommitRequest& request);
std::vector<Battle::BattleTeamEffectEvent> applyBattleSelfHeal(
    Battle::BattleTeamEffectWorld& world,
    int sourceUnitId,
    int pctHeal);
std::vector<Battle::BattleTeamEffectEvent> applyBattleHealAura(
    Battle::BattleTeamEffectWorld& world,
    int sourceUnitId,
    int pctHeal,
    int flatHeal,
    double range,
    int cooldownReductionPct);
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
