#pragma once

#include "BattleSceneAct.h"
#include "battle/BattleAttackSystem.h"
#include "battle/BattleCastSystem.h"
#include "battle/BattleCore.h"
#include "battle/BattleHitResolver.h"

#include <cstddef>
#include <deque>
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

}  // namespace KysChess::BattleSceneBattleAdapter
