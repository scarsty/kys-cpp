#include "BattleSceneBattleAdapter.h"

#include "Scene.h"
#include "battle/BattleComboTriggerSystem.h"
#include "battle/BattleDamageSystem.h"
#include "battle/BattleProjectileTargetingSystem.h"
#include "battle/BattleTeamEffectSystem.h"

#include <algorithm>
#include <cassert>

namespace KysChess::BattleSceneBattleAdapter
{
namespace
{
constexpr int BATTLE_TILE_W = Scene::TILE_W;
constexpr double MINIMUM_VECTOR_NORM = 0.0001;
constexpr int ACTION_RECOVERY_FRAMES = 4;
constexpr int DASH_MOMENTUM_FRAMES = 5;
constexpr int NORMAL_CAST_MP_DELTA = 5;
constexpr int COOLDOWN_AFTER_CAST_PADDING = 2;
constexpr int COOLDOWN_MAX_SPEED = 150;
constexpr double SPEED_COOLDOWN_REDUCTION_RATIO = 0.5;
constexpr int MELEE_HIT_TOTAL_FRAME = 10;
constexpr int STRENGTHENED_MELEE_TOTAL_FRAME = 30;
constexpr double STRENGTHENED_MELEE_SELECT_DISTANCE_DIVISOR = 2.0;
constexpr float STRENGTHENED_MELEE_MULTIPLIER = 2.0f;
constexpr int STRENGTHENED_MELEE_OPERATION_COUNT_THRESHOLD = 2;
constexpr int MELEE_SPLASH_TOTAL_FRAME = 60;
constexpr int MELEE_SPLASH_INITIAL_FRAME = 5;
constexpr double MELEE_SPLASH_PROJECTILE_SPEED = 3.0;
constexpr float MELEE_SPLASH_STRENGTH_MULTIPLIER = 0.5f;
constexpr int TRACKING_PROJECTILE_TOTAL_FRAME = 120;
constexpr int DASH_HIT_TOTAL_FRAME = 30;
constexpr double DASH_HIT_POSITION_SPACING = 2.0;
constexpr int DASH_HIT_FRAME_STEP = 3;

Battle::BattleAttackUnit makeBattleAttackUnit(Role* role)
{
    assert(role);

    Battle::BattleAttackUnit unit;
    unit.id = role->ID;
    unit.team = role->Team;
    unit.alive = role->Dead == 0;
    unit.invincible = role->Invincible != 0;
    unit.hurtFrame = role->HurtFrame != 0;
    unit.position = role->Pos;
    return unit;
}

}  // namespace

Role* findRoleByBattleId(const std::vector<Role*>& roles, int unitId)
{
    auto it = std::find_if(roles.begin(), roles.end(), [&](Role* role)
        {
            return role && role->ID == unitId;
        });
    assert(it != roles.end());
    assert(*it);
    return *it;
}

Battle::BattleCastConfig makeBattleCastConfig()
{
    Battle::BattleCastConfig config;
    config.castFrames = { 25, 30, 20, 25 };
    config.baseCooldownFrames = { 105, 185, 115, 45 };
    config.minimumCooldownFrames = { 60, 70, 70, 45 };
    config.cooldownActPropertyDivisors = { 2, 1, 2, 0 };
    config.recoveryFrames = {
        ACTION_RECOVERY_FRAMES,
        ACTION_RECOVERY_FRAMES,
        ACTION_RECOVERY_FRAMES,
        DASH_MOMENTUM_FRAMES,
    };
    config.maxCooldownSpeed = COOLDOWN_MAX_SPEED;
    config.speedCooldownReductionRatio = SPEED_COOLDOWN_REDUCTION_RATIO;
    config.minimumCooldownAfterCastPadding = COOLDOWN_AFTER_CAST_PADDING;
    config.normalCastMpDelta = NORMAL_CAST_MP_DELTA;
    config.minimumFacingNorm = MINIMUM_VECTOR_NORM;
    config.meleeHitTotalFrame = MELEE_HIT_TOTAL_FRAME;
    config.strengthenedMeleeTotalFrame = STRENGTHENED_MELEE_TOTAL_FRAME;
    config.strengthenedMeleeSelectDistanceDivisor = STRENGTHENED_MELEE_SELECT_DISTANCE_DIVISOR;
    config.strengthenedMeleeMultiplier = STRENGTHENED_MELEE_MULTIPLIER;
    config.meleeSplashTotalFrame = MELEE_SPLASH_TOTAL_FRAME;
    config.meleeSplashInitialFrame = MELEE_SPLASH_INITIAL_FRAME;
    config.meleeSplashStrengthMultiplier = MELEE_SPLASH_STRENGTH_MULTIPLIER;
    config.trackingProjectileTotalFrame = TRACKING_PROJECTILE_TOTAL_FRAME;
    config.dashHitTotalFrame = DASH_HIT_TOTAL_FRAME;
    config.strengthenedMeleeOperationCountThreshold = STRENGTHENED_MELEE_OPERATION_COUNT_THRESHOLD;
    return config;
}

Battle::BattleCastGeometry makeBattleCastGeometry()
{
    Battle::BattleCastGeometry geometry;
    geometry.meleeAttackEffectOffset = BATTLE_TILE_W * 2.0;
    geometry.projectileSpeed = BATTLE_TILE_W / 3.0;
    geometry.projectileSpawnOffset = BATTLE_TILE_W * 2.0;
    geometry.projectileBaseTravel = BATTLE_TILE_W * 5.0;
    geometry.projectileTravelPerSelectDistance = BATTLE_TILE_W;
    geometry.meleeSplashProjectileSpeed = MELEE_SPLASH_PROJECTILE_SPEED;
    geometry.dashHitPositionSpacing = DASH_HIT_POSITION_SPACING;
    geometry.dashHitFrameStep = DASH_HIT_FRAME_STEP;
    return geometry;
}

Battle::BattleCastSkillState makeBattleCastSkillState(Role* unit, const BattleCastSkillAdapterInput& input)
{
    Battle::BattleCastSkillState skill;
    if (!input.magic)
    {
        return skill;
    }
    assert(unit);
    skill.id = input.magic->ID;
    skill.name = input.magic->Name;
    skill.attackAreaType = input.magic->AttackAreaType;
    skill.magicType = input.magic->MagicType;
    skill.visualEffectId = input.magic->EffectID;
    skill.selectDistance = input.magic->SelectDistance;
    skill.projectileSpeedMultiplierPct = input.projectileSpeedMultiplierPct;
    skill.actProperty = unit->getActProperty(input.magic->MagicType);
    skill.meleeSplashCount = input.meleeSplashCount;
    skill.extraProjectileCount = input.extraProjectileCount;
    skill.strengthenedMelee = input.strengthenedMelee;
    skill.reach = input.reach;
    skill.forceRanged = input.forceRanged;
    skill.rangedStyle = input.rangedStyle;
    return skill;
}

Battle::BattleCastInput makeBattleCastInput(const BattleCastAdapterInput& input)
{
    assert(input.unit);

    Battle::BattleCastInput castInput;
    castInput.config = makeBattleCastConfig();
    castInput.geometry = makeBattleCastGeometry();
    castInput.unit.id = input.unit->ID;
    castInput.unit.position = input.unit->Pos;
    castInput.unit.facing = input.unit->RealTowards;
    castInput.unit.alive = input.unit->Dead == 0;
    castInput.unit.frozen = input.unit->Frozen != 0;
    castInput.unit.stunned = input.unit->HurtFrame != 0;
    castInput.unit.canStartAttack = input.canStartAttack;
    castInput.unit.mp = input.unit->MP;
    castInput.unit.maxMp = input.unit->MaxMP;
    castInput.unit.speed = input.unit->Speed;
    castInput.unit.cooldownReductionPct = input.cooldownReductionPct;
    castInput.unit.operationCount = input.operationCount;
    castInput.unit.meleeAttackReach = input.meleeAttackReach;
    castInput.unit.dashAttackReach = input.dashAttackReach;
    castInput.unit.hasEquippedSkill = input.unit->UsingMagic != nullptr;
    castInput.unit.movementDashActive = input.movementDashActive;
    castInput.unit.dashAttackEnabled = input.dashAttackEnabled;
    castInput.unit.dashVelocity = input.dashVelocity;
    castInput.unit.dashHitCount = input.dashHitCount;
    castInput.unit.emitDashFollowUpSkillAttack = input.emitDashFollowUpSkillAttack;
    castInput.unit.dashFollowUpOperationType = Battle::battleOperationFromLegacy(input.dashFollowUpOperationType);
    castInput.normalSkill = makeBattleCastSkillState(input.unit, input.normalSkill);
    castInput.ultimateSkill = makeBattleCastSkillState(input.unit, input.ultimateSkill);
    castInput.targetUnitId = input.target ? input.target->ID : -1;
    castInput.targetPosition = input.target ? input.target->Pos : Pointf{};
    castInput.targetDistance = input.targetDistance;
    return castInput;
}

void applyBattleCastStart(Role* unit, const Battle::BattleCastResult& result, int actType)
{
    assert(unit);
    assert(result.decision.canCast);
    unit->OperationType = Battle::toLegacyOperationType(result.decision.operationType);
    unit->CoolDown = result.cooldownDelta;
    unit->CoolDownMax = result.cooldownDelta;
    unit->ActType = actType;
    unit->ActFrame = 0;
    unit->HaveAction = 1;
    unit->Velocity = { 0, 0, 0 };
    unit->FindingWay = 0;
}

void applyBattleCastCommit(Role* unit, const Battle::BattleCastResult& result)
{
    assert(unit);
    unit->MP += result.mpDelta;
    unit->limit();
    unit->UsingMagic = nullptr;
}

Battle::BattleAttackWorld makeBattleAttackWorld(
    const std::vector<Role*>& roles,
    const Battle::BattleAttackWorld& activeWorld,
    const std::unordered_map<int, std::set<int>>& sharedHitGroupTargets)
{
    Battle::BattleAttackWorld world = activeWorld;
    world.hitRadius = BATTLE_TILE_W * 2.0;
    world.minimumVectorNorm = MINIMUM_VECTOR_NORM;
    world.projectileGraceFrames = 5;
    world.bounceSpawnDistance = BATTLE_TILE_W * 1.5;
    world.defaultProjectileSpeed = BATTLE_TILE_W / 3.0;
    world.spendNonThroughOnHit = false;
    world.units.clear();
    for (auto role : roles)
    {
        if (role)
        {
            world.units.push_back(makeBattleAttackUnit(role));
        }
    }

    world.sharedHitGroupTargets.clear();
    for (const auto& [groupId, targets] : sharedHitGroupTargets)
    {
        auto& output = world.sharedHitGroupTargets[groupId];
        output.insert(output.end(), targets.begin(), targets.end());
    }
    return world;
}

Battle::BattleFrameUnitRuntimeInput makeBattleFrameUnitRuntimeInput(
    Role* role,
    int frame,
    int mpRegenIntervalFrames,
    int physicalPowerRegenIntervalFrames)
{
    assert(role);

    Battle::BattleFrameUnitRuntimeInput input;
    input.state.cooldown = role->CoolDown;
    input.state.actFrame = role->ActFrame;
    input.state.actType = role->ActType;
    input.state.operationType = Battle::battleOperationFromLegacy(role->OperationType);
    input.state.haveAction = role->HaveAction != 0;
    input.state.physicalPower = role->PhysicalPower;
    input.frame = frame;
    input.frozen = role->Frozen > 0;
    input.mpRegenIntervalFrames = mpRegenIntervalFrames;
    input.physicalPowerRegenIntervalFrames = physicalPowerRegenIntervalFrames;
    return input;
}

void applyBattleFrameUnitRuntimeResult(Role* role, const Battle::BattleFrameUnitRuntimeResult& result)
{
    assert(role);

    role->CoolDown = result.state.cooldown;
    role->ActFrame = result.state.actFrame;
    role->ActType = result.state.actType;
    role->OperationType = Battle::toLegacyOperationType(result.state.operationType);
    role->HaveAction = result.state.haveAction ? 1 : 0;
    role->PhysicalPower = result.state.physicalPower;
    if (result.resetDashVelocity)
    {
        role->Velocity = { 0, 0, 0 };
    }
}

void applyBattleProjectileCancelDamage(Role* role, int damage)
{
    assert(role);
    assert(damage >= 0);
    role->CancelDmg += damage;
}

Battle::BattleActionCommitUnitSnapshot makeBattleActionCommitUnitSnapshot(Role* role)
{
    assert(role);

    Battle::BattleActionCommitUnitSnapshot snapshot;
    snapshot.id = role->ID;
    snapshot.team = role->Team;
    snapshot.position = role->Pos;
    snapshot.facing = role->RealTowards;
    snapshot.operationCount = role->OperationCount;
    return snapshot;
}

Battle::BattleActionTargetSnapshot makeBattleActionTargetSnapshot(Role* role)
{
    assert(role);

    Battle::BattleActionTargetSnapshot snapshot;
    snapshot.id = role->ID;
    snapshot.team = role->Team;
    snapshot.alive = role->Dead == 0;
    snapshot.hp = role->HP;
    snapshot.maxHp = role->MaxHP;
    snapshot.defence = role->Defence;
    snapshot.invincible = role->Invincible;
    snapshot.position = role->Pos;
    return snapshot;
}

Battle::BattleActionItemSnapshot makeBattleActionItemSnapshot(Item* item)
{
    assert(item);

    Battle::BattleActionItemSnapshot snapshot;
    snapshot.id = item->ID;
    snapshot.itemType = item->ItemType;
    snapshot.hiddenWeaponEffectId = item->HiddenWeaponEffectID;
    return snapshot;
}

Battle::BattleHitUnitSnapshot makeBattleHitUnitSnapshot(Role* unit)
{
    assert(unit);

    Battle::BattleHitUnitSnapshot snapshot;
    snapshot.id = unit->ID;
    snapshot.team = unit->Team;
    snapshot.alive = unit->Dead == 0;
    snapshot.hp = unit->HP;
    snapshot.maxHp = unit->MaxHP;
    snapshot.mp = unit->MP;
    snapshot.maxMp = unit->MaxMP;
    snapshot.attack = unit->Attack;
    snapshot.defence = unit->Defence;
    snapshot.speed = unit->Speed;
    snapshot.invincible = unit->Invincible;
    snapshot.hurtFrame = unit->HurtFrame;
    snapshot.cooldown = unit->CoolDown;
    snapshot.cooldownMax = unit->CoolDownMax;
    snapshot.haveAction = unit->HaveAction != 0;
    snapshot.operationType = Battle::battleOperationFromLegacy(unit->OperationType);
    snapshot.actType = unit->ActType;
    snapshot.position = unit->Pos;
    snapshot.facing = unit->RealTowards;
    return snapshot;
}

Battle::BattleHitSkillSnapshot makeBattleHitSkillSnapshot(Role* attacker,
                                                          Role* defender,
                                                          Magic* magic,
                                                          int resolvedBaseDamage)
{
    Battle::BattleHitSkillSnapshot snapshot;
    if (!magic)
    {
        return snapshot;
    }
    assert(attacker);
    assert(defender);

    snapshot.id = magic->ID;
    snapshot.name = magic->Name;
    snapshot.hurtType = magic->HurtType;
    snapshot.magicType = magic->MagicType;
    snapshot.effectId = magic->EffectID;
    snapshot.attackerActProperty = attacker->getActProperty(magic->MagicType);
    snapshot.defenderActProperty = defender->getActProperty(magic->MagicType);
    snapshot.magicPower = attacker->getMagicPower(magic);
    snapshot.resolvedBaseDamage = resolvedBaseDamage;
    return snapshot;
}

Battle::BattleHitItemSnapshot makeBattleHitItemSnapshot(Item* item, int resolvedDamage)
{
    Battle::BattleHitItemSnapshot snapshot;
    if (!item)
    {
        return snapshot;
    }

    snapshot.id = item->ID;
    snapshot.name = item->Name;
    snapshot.hiddenWeaponEffectId = item->HiddenWeaponEffectID;
    snapshot.resolvedDamage = resolvedDamage;
    return snapshot;
}

Battle::BattleDamageRequest makeBattleMpLeechDamageRequest(int damage)
{
    assert(damage >= 0);

    Battle::BattleDamageRequest request;
    request.mpDamage = damage;
    request.mpOnHit = static_cast<int>(damage * 0.8);
    return request;
}

std::optional<Battle::BattleDamageApplicationUnitEffects> makeBattleDamageApplicationUnitEffects(
    const RoleComboState& state)
{
    if (state.deathAOEPct <= 0)
    {
        return std::nullopt;
    }

    return Battle::BattleDamageApplicationUnitEffects{
        state.deathAOEPct,
        state.deathAOEStunFrames,
        state.deathAOEMaxTargets,
    };
}

std::vector<Battle::BattleComboFrameRuntimeEvent> advanceBattleComboFrameRuntime(
    RoleComboState& state,
    const Battle::BattleComboFrameRuntimeInput& input)
{
    return Battle::BattleComboTriggerSystem().advanceFrameRuntime(state, input);
}

Battle::BattleDodgeResolution resolveBattleDodge(
    const RoleComboState& state,
    int attackerUnitId,
    double rollPercent)
{
    return Battle::BattleComboTriggerSystem().resolveDodge(state, attackerUnitId, rollPercent);
}

Battle::BattleProjectileBouncePrime collectBattleProjectileBouncePrime(
    const RoleComboState& state,
    int attackerUnitId,
    int rollPct,
    int defaultRange)
{
    return Battle::BattleComboTriggerSystem().collectProjectileBouncePrime(
        state,
        {
            attackerUnitId,
            rollPct,
            defaultRange,
        });
}

int collectBattleExtraProjectileCount(RoleComboState& state, int unitId, int baseCount)
{
    return Battle::BattleComboTriggerSystem().collectExtraProjectileCount(
        state,
        { Battle::BattleComboTriggerHook::AfterSkillCast, unitId, -1 },
        baseCount,
        []() { return 0.0; });
}

bool battleComboHasExecute(const RoleComboState& state, int attackerUnitId)
{
    return Battle::BattleComboTriggerSystem().hasExecuteCombo(state, attackerUnitId);
}

double resolveBattleArmorPenetratedDefense(
    const RoleComboState& state,
    int attackerUnitId,
    int targetUnitId,
    double defense,
    double rollPercent)
{
    return Battle::BattleComboTriggerSystem().resolveArmorPenetratedDefense(
        state,
        { attackerUnitId, targetUnitId, defense },
        [rollPercent]() { return rollPercent; }).defense;
}

int resolveBattleMagicBaseDamage(const Battle::BattleMagicBaseDamageInput& input)
{
    return Battle::BattleDamageSystem().resolveMagicBaseDamage(input);
}

std::vector<Battle::BattleTeamEffectEvent> applyBattleTeamEffect(
    Battle::BattleTeamEffectWorld& world,
    const BattleTeamEffectCommitRequest& request)
{
    assert(request.sourceUnitId >= 0);
    switch (request.type)
    {
    case BattleTeamEffectCommitType::Heal:
        return Battle::BattleTeamEffectSystem().applyTeamHeal(
            world,
            request.sourceUnitId,
            request.flatHeal,
            request.pctHeal);
    case BattleTeamEffectCommitType::MpRestore:
        return Battle::BattleTeamEffectSystem().applyTeamMp(
            world,
            request.sourceUnitId,
            request.amount);
    case BattleTeamEffectCommitType::Shield:
        return Battle::BattleTeamEffectSystem().applyTeamShield(
            world,
            request.sourceUnitId,
            request.amount,
            request.refreshOnly);
    }
    assert(false);
    return {};
}

std::vector<Battle::BattleTeamEffectEvent> applyBattleSelfHeal(
    Battle::BattleTeamEffectWorld& world,
    int sourceUnitId,
    int pctHeal)
{
    return Battle::BattleTeamEffectSystem().applySelfHeal(world, sourceUnitId, pctHeal);
}

std::vector<Battle::BattleTeamEffectEvent> applyBattleHealAura(
    Battle::BattleTeamEffectWorld& world,
    int sourceUnitId,
    int pctHeal,
    int flatHeal,
    double range,
    int cooldownReductionPct)
{
    return Battle::BattleTeamEffectSystem().applyHealAura(
        world,
        sourceUnitId,
        pctHeal,
        flatHeal,
        range,
        cooldownReductionPct);
}

std::vector<int> selectBattleNearbyProjectileTargets(
    const Battle::BattleProjectileTargetWorld& world,
    int sourceUnitId,
    int centerTargetUnitId,
    int rangePixels)
{
    return Battle::BattleProjectileTargetingSystem().selectNearbyTargets(
        world,
        sourceUnitId,
        centerTargetUnitId,
        rangePixels);
}

std::vector<int> selectBattleAreaImpactTargets(
    const Battle::BattleProjectileTargetWorld& world,
    int originUnitId,
    int areaSize,
    int maxTargets,
    int trackedTargetUnitId)
{
    return Battle::BattleProjectileTargetingSystem().selectAreaImpactTargets(
        world,
        originUnitId,
        areaSize,
        maxTargets,
        trackedTargetUnitId);
}

}  // namespace KysChess::BattleSceneBattleAdapter
