#include "BattleSceneBattleAdapter.h"

#include "Scene.h"
#include "Event.h"
#include "Save.h"
#include "BattleScenePresentationConstants.h"
#include "BattleStatsView.h"
#include "ChessEftIds.h"
#include "GameUtil.h"
#include "battle/BattleCombatIntent.h"
#include "battle/BattleDamageSystem.h"
#include "battle/BattleProjectileTargetingSystem.h"

#include <algorithm>
#include <cassert>
#include <format>
#include <iterator>
#include <limits>
#include <set>
#include <utility>

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

Color damageTextColor(const Role* role, bool emphasized)
{
    bool friendlyTarget = role && role->Team == 0;
    if (friendlyTarget)
    {
        return emphasized ? Color{ 255, 45, 85, 255 } : Color{ 255, 90, 79, 255 };
    }
    return emphasized ? Color{ 47, 128, 255, 255 } : Color{ 102, 207, 255, 255 };
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

Battle::BattlePresentationColor makeBattlePresentationColor(Color color)
{
    return { color.r, color.g, color.b, color.a };
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

Battle::BattleTeamEffectWorld makeBattleTeamEffectWorld(
    const std::vector<Role*>& roles,
    const std::map<int, RoleComboState>& states)
{
    Battle::BattleTeamEffectWorld world;
    for (auto role : roles)
    {
        assert(role);
        auto stateIt = states.find(role->ID);
        assert(stateIt != states.end());

        Battle::BattleTeamEffectUnit unit;
        unit.id = role->ID;
        unit.team = role->Team;
        unit.alive = role->Dead == 0;
        unit.hp = role->HP;
        unit.maxHp = role->MaxHP;
        unit.mp = role->MP;
        unit.maxMp = GameUtil::MAX_MP;
        unit.cooldown = role->CoolDown;
        unit.shield = stateIt->second.shield;
        unit.mpBlocked = stateIt->second.mpBlockTimer > 0;
        unit.mpRecoveryBonusPct = stateIt->second.mpRecoveryBonusPct;
        unit.x = role->Pos.x;
        unit.y = role->Pos.y;
        world.units.push_back(unit);
    }
    return world;
}

const Battle::BattleTeamEffectUnit& findBattleTeamEffectUnit(
    const Battle::BattleTeamEffectWorld& world,
    int unitId)
{
    auto it = std::find_if(world.units.begin(), world.units.end(), [&](const Battle::BattleTeamEffectUnit& unit)
        {
            return unit.id == unitId;
        });
    assert(it != world.units.end());
    return *it;
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

Battle::BattleStatusUnitState makeBattleStatusUnit(Role* role, const RoleComboState& state)
{
    Battle::BattleStatusUnitState unit;
    unit.id = role ? role->ID : -1;
    unit.alive = role && role->Dead == 0;
    unit.hp = role ? role->HP : 0;
    unit.maxHp = role ? role->MaxHP : 0;
    unit.attack = role ? role->Attack : 0;
    unit.invincible = role ? role->Invincible : 0;
    unit.poisonTimer = state.poisonTimer;
    unit.poisonTickPct = state.poisonTickDmg;
    unit.poisonSourceId = state.poisonSourceId;
    unit.bleedStacks = state.bleedStacks;
    unit.bleedTimer = state.bleedTimer;
    unit.bleedSourceId = state.bleedSourceId;
    unit.frozenTimer = role ? role->Frozen : 0;
    unit.frozenMaxTimer = role ? role->FrozenMax : 0;
    unit.freezeReductionPct = state.freezeReductionPct;
    unit.shieldFreezeResPct = state.shieldFreezeResPct;
    unit.controlImmunityFrames = state.controlImmunityFrames;
    unit.mpBlockTimer = state.mpBlockTimer;
    unit.damageImmunityAfterFrames = state.damageImmunityAfterFrames;
    unit.damageImmunityDuration = state.damageImmunityDuration;
    unit.damageImmunityTimer = state.damageImmunityTimer;

    for (const auto& buff : state.tempAttackBuffs)
    {
        unit.tempAttackBuffs.push_back({ buff.attackBonus, buff.remainingFrames });
    }
    for (const auto& debuff : state.dmgReduceDebuffs)
    {
        unit.damageReduceDebuffs.push_back({ debuff.remainingFrames, debuff.pct });
    }
    return unit;
}

void writeBattleStatusUnit(Role* role, RoleComboState& state, const Battle::BattleStatusUnitState& unit)
{
    if (role)
    {
        role->Attack = unit.attack;
        role->Invincible = unit.invincible;
        role->Frozen = unit.frozenTimer;
        role->FrozenMax = unit.frozenMaxTimer;
    }

    state.poisonTimer = unit.poisonTimer;
    state.poisonTickDmg = unit.poisonTickPct;
    state.poisonSourceId = unit.poisonSourceId;
    state.bleedStacks = unit.bleedStacks;
    state.bleedTimer = unit.bleedTimer;
    state.bleedSourceId = unit.bleedSourceId;
    state.controlImmunityFrames = unit.controlImmunityFrames;
    state.mpBlockTimer = unit.mpBlockTimer;
    state.damageImmunityTimer = unit.damageImmunityTimer;

    state.tempAttackBuffs.clear();
    for (const auto& buff : unit.tempAttackBuffs)
    {
        state.tempAttackBuffs.push_back({ buff.attackBonus, buff.remainingFrames });
    }

    state.dmgReduceDebuffs.clear();
    for (const auto& debuff : unit.damageReduceDebuffs)
    {
        state.dmgReduceDebuffs.push_back({ debuff.remainingFrames, debuff.pct });
    }
}

Battle::BattleDamageUnitState makeBattleDamageUnit(Role* role, const RoleComboState* state)
{
    Battle::BattleDamageUnitState unit;
    unit.id = role ? role->ID : -1;
    unit.alive = role && role->Dead == 0;
    unit.hp = role ? role->HP : 0;
    unit.maxHp = role ? role->MaxHP : 0;
    unit.mp = role ? role->MP : 0;
    unit.maxMp = role ? role->MaxMP : 0;
    unit.attack = role ? role->Attack : 0;
    unit.invincible = role ? role->Invincible : 0;
    if (!state)
    {
        return unit;
    }

    unit.hurtInvincFrames = state->hurtInvincFrames;
    unit.shield = state->shield;
    unit.blockFirstHitsRemaining = state->blockFirstHitsRemaining;
    unit.deathPrevention = state->deathPrevention;
    unit.deathPreventionUsed = state->deathPreventionUsed;
    unit.deathPreventionFrames = state->deathPreventionFrames;
    unit.killHealPct = state->killHealPct;
    unit.killInvincFrames = state->killInvincFrames;
    unit.bloodlustAttackPerKill = state->bloodlustATKPerKill;
    unit.mpBlocked = state->mpBlockTimer > 0;
    unit.mpRecoveryBonusPct = state->mpRecoveryBonusPct;
    return unit;
}

void writeBattleDamageUnit(Role* role, RoleComboState* state, const Battle::BattleDamageUnitState& unit)
{
    if (role)
    {
        role->HP = unit.hp;
        role->MP = unit.mp;
        role->Attack = unit.attack;
        role->Invincible = unit.invincible;
        role->Dead = unit.alive ? 0 : 1;
    }

    if (!state)
    {
        return;
    }
    state->shield = unit.shield;
    state->blockFirstHitsRemaining = unit.blockFirstHitsRemaining;
    state->deathPreventionUsed = unit.deathPreventionUsed;
}

Battle::BattleCooldownState makeBattleCooldownState(Role* role)
{
    Battle::BattleCooldownState state;
    if (!role)
    {
        state.alive = false;
        return state;
    }
    state.alive = role->Dead == 0;
    state.cooldown = role->CoolDown;
    state.cooldownMax = role->CoolDownMax;
    state.haveAction = role->HaveAction;
    state.operationType = Battle::battleOperationFromLegacy(role->OperationType);
    state.actType = role->ActType;
    return state;
}

void writeBattleCooldownState(Role* role, const Battle::BattleCooldownState& state)
{
    if (role)
    {
        role->CoolDown = state.cooldown;
    }
}

Battle::BattleDamagePresentationStyle makeBattleDamagePresentationStyle(Role* role)
{
    assert(role);

    Battle::BattleDamagePresentationStyle style;
    style.normalDamageColor = makeBattlePresentationColor(damageTextColor(role, false));
    style.emphasizedDamageColor = makeBattlePresentationColor(damageTextColor(role, true));
    style.executeTextColor = makeBattlePresentationColor({ 255, 136, 48, 255 });
    style.normalDamageTextSize = NORMAL_DAMAGE_TEXT_SIZE;
    style.emphasizedDamageTextSize = ULT_DAMAGE_TEXT_SIZE;
    style.executeTextSize = ULT_DAMAGE_TEXT_SIZE;
    return style;
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

void populateBattleFrameHitUnits(
    Battle::BattleFrameState& frameState,
    const std::vector<Role*>& roles)
{
    frameState.hits.units.clear();
    frameState.hits.units.reserve(roles.size());
    for (auto* role : roles)
    {
        frameState.hits.units.push_back(makeBattleHitUnitSnapshot(role));
    }
}

void appendBattleFrameHitInput(
    Battle::BattleFrameState& frameState,
    const BattleFrameHitAdapterInput& input)
{
    assert(input.attackId >= 0);
    assert(input.attacker);
    assert(input.defender);

    frameState.hits.scalars.push_back({
        input.attackId,
        input.attacker->ID,
        input.defender->ID,
        input.resolvedMagicBaseDamage,
        input.resolvedHiddenWeaponDamage,
        input.sharedBleedMaxStacks,
        input.randomDamageVariance,
        input.percentRolls,
        input.pendingDefenderHpDamage,
    });
    frameState.hits.skills.push_back({
        input.attackId,
        input.attacker->ID,
        input.defender->ID,
        makeBattleHitSkillSnapshot(
            input.attacker,
            input.defender,
            input.magic,
            input.resolvedMagicBaseDamage),
    });
    frameState.hits.items.push_back({
        input.attackId,
        input.attacker->ID,
        input.defender->ID,
        makeBattleHitItemSnapshot(
            input.hiddenWeapon,
            input.resolvedHiddenWeaponDamage),
    });
}

int legacyOperationForAttackArea(int attackAreaType)
{
    return Battle::toLegacyOperationType(
        Battle::BattleCombatIntentPlanner().operationTypeForAttackArea(attackAreaType));
}

const BattleFrameLegacyActionUnitSnapshot& requireActionSnapshot(
    const BattleActionFrameAdapterContext& context,
    int unitId)
{
    assert(context.snapshot);
    auto it = context.snapshot->actionsByUnitId.find(unitId);
    assert(it != context.snapshot->actionsByUnitId.end());
    return it->second;
}

Role* findSnapshotRole(const BattleActionFrameAdapterContext& context, int unitId)
{
    if (unitId < 0)
    {
        return nullptr;
    }
    assert(context.snapshot);
    auto it = context.snapshot->rolesByBattleId.find(unitId);
    assert(it != context.snapshot->rolesByBattleId.end());
    return it->second;
}

Pointf positionForCell(const BattleFrameLegacySnapshot& snapshot, int x, int y, int coordCount)
{
    auto it = snapshot.grid.positionsByCell.find({ x, y });
    if (it != snapshot.grid.positionsByCell.end())
    {
        return it->second;
    }
    return {
        static_cast<float>(-y * BATTLE_TILE_W + x * BATTLE_TILE_W + coordCount * BATTLE_TILE_W),
        static_cast<float>(y * BATTLE_TILE_W + x * BATTLE_TILE_W),
        0.0f,
    };
}

bool cellWalkable(const BattleFrameLegacySnapshot& snapshot, int x, int y)
{
    auto it = std::find_if(
        snapshot.grid.movementCells.begin(),
        snapshot.grid.movementCells.end(),
        [&](const Battle::BattleMovementPhysicsCollisionCellSnapshot& cell)
        {
            return cell.x == x && cell.y == y;
        });
    return it != snapshot.grid.movementCells.end() && it->walkable;
}

BattleCastSkillAdapterInput makeActionFrameSkillInput(
    Role* role,
    Magic* magic,
    bool ultimate,
    const BattleActionFrameAdapterContext& context)
{
    BattleCastSkillAdapterInput skill;
    if (!magic)
    {
        return skill;
    }

    const auto& action = requireActionSnapshot(context, role->ID);
    const bool forceRanged = action.forceRangedMagic;
    const int projectileSpeedMultiplierPct = action.projectileSpeedMultiplierPct;
    skill.magic = magic;
    skill.reach = ultimate ? action.ultimateEffectiveReach : action.normalEffectiveReach;
    skill.forceRanged = forceRanged;
    skill.rangedStyle = ultimate ? action.ultimateRangedStyle : action.normalRangedStyle;
    skill.projectileSpeedMultiplierPct = projectileSpeedMultiplierPct;
    skill.meleeSplashCount = ultimate && magic->AttackAreaType == 0 ? 1 : 0;
    skill.extraProjectileCount = ultimate ? action.ultimateExtraProjectileCount : 0;
    return skill;
}

int rollDashHitCount(Role* role, Magic* selectedMagic, const BattleActionFrameAdapterContext& context)
{
    int dashHitCount = 1;
    if (selectedMagic)
    {
        const auto& action = requireActionSnapshot(context, role->ID);
        const double multiHitScore = (role->Speed + role->getActProperty(selectedMagic->MagicType)) / 180.0;
        if (action.randomUnitRolls[0] < multiHitScore)
        {
            dashHitCount++;
        }
        if (action.randomUnitRolls[1] < multiHitScore * 0.5)
        {
            dashHitCount++;
        }
    }
    return dashHitCount;
}

Battle::BattleCastInput makeActionFrameCastInput(
    Role* role,
    Role* target,
    Magic* normalMagic,
    Magic* ultimateMagic,
    bool canStartAttack,
    bool movementDashActive,
    const BattleActionFrameAdapterContext& context)
{
    assert(role);
    assert(target);
    assert(context.comboStates);

    const bool isUltimate = role->MP >= role->MaxMP;
    Magic* selectedMagic = isUltimate && ultimateMagic ? ultimateMagic : normalMagic;
    Pointf dashVelocity = target->Pos - role->Pos;
    if (dashVelocity.norm() > 0.01)
    {
        dashVelocity.normTo(context.config.meleeAttackHitRadius / context.config.dashMomentumFrames);
    }

    bool dashAttackEnabled = false;
    int cooldownReductionPct = 0;
    auto comboIt = context.comboStates->find(role->ID);
    if (comboIt != context.comboStates->end())
    {
        dashAttackEnabled = comboIt->second.dashAttack;
        cooldownReductionPct = comboIt->second.cdrPct;
    }

    BattleCastAdapterInput castAdapterInput;
    castAdapterInput.unit = role;
    castAdapterInput.target = target;
    castAdapterInput.normalSkill = makeActionFrameSkillInput(role, normalMagic, false, context);
    castAdapterInput.ultimateSkill = makeActionFrameSkillInput(role, ultimateMagic, true, context);
    castAdapterInput.canStartAttack = canStartAttack;
    castAdapterInput.movementDashActive = movementDashActive;
    castAdapterInput.dashAttackEnabled = dashAttackEnabled;
    castAdapterInput.dashVelocity = dashVelocity;
    castAdapterInput.dashHitCount = rollDashHitCount(role, selectedMagic, context);
    castAdapterInput.emitDashFollowUpSkillAttack = dashAttackEnabled && selectedMagic;
    castAdapterInput.dashFollowUpOperationType = selectedMagic ? legacyOperationForAttackArea(selectedMagic->AttackAreaType) : -1;
    castAdapterInput.targetDistance = EuclidDis(role->Pos, target->Pos);
    castAdapterInput.meleeAttackReach = context.config.meleeAttackReach;
    castAdapterInput.dashAttackReach = context.config.dashAttackMeleeReach;
    castAdapterInput.operationCount = role->OperationCount;
    castAdapterInput.cooldownReductionPct = cooldownReductionPct;
    return makeBattleCastInput(castAdapterInput);
}

Battle::BattleBlinkGeometryInput makeBlinkGeometryInput(
    Role* role,
    double reach,
    const BattleActionFrameAdapterContext& context);

const BattleFrameLegacyRoleSnapshot& requireLegacyRoleSnapshot(
    const BattleFrameLegacySnapshot& snapshot,
    int unitId)
{
    auto it = std::find_if(
        snapshot.roleSnapshots.begin(),
        snapshot.roleSnapshots.end(),
        [&](const BattleFrameLegacyRoleSnapshot& roleSnapshot)
        {
            return roleSnapshot.unitId == unitId;
        });
    assert(it != snapshot.roleSnapshots.end());
    return *it;
}

void captureActionComboState(
    Battle::BattleActionCommitInput& actionInput,
    Role* role,
    const BattleActionFrameAdapterContext& context)
{
    auto comboIt = context.comboStates->find(role->ID);
    if (comboIt == context.comboStates->end())
    {
        return;
    }

    actionInput.combo = comboIt->second;
    auto prime = Battle::collectFrameProjectileBouncePrime(
        actionInput.combo,
        role->ID,
        requireActionSnapshot(context, role->ID).projectileBounceRoll,
        context.config.projectileBounceRange);
    actionInput.projectileBouncePrime = {
        prime.count,
        prime.chancePct,
        prime.rollPct,
        prime.range,
    };
}

void populateActionCommitInputForRole(
    Battle::BattleFrameActionUnitInput& unitInput,
    Role* role,
    const BattleActionFrameAdapterContext& context)
{
    if (!role->HaveAction)
    {
        return;
    }

    if (role->OperationType != 3)
    {
        role->Velocity = { 0, 0, 0 };
    }

    if (role->OperationType >= 0 && role->OperationType <= 3
        && role->ActFrame == unitInput.state.castFrame)
    {
        role->PreActTimer = context.config.battleFrame;
        Magic* magic = role->UsingMagic;
        auto pendingCast = context.pendingCastResults->find(role);
        assert(pendingCast != context.pendingCastResults->end());
        assert(magic);
        auto castResult = pendingCast->second;
        assert(castResult.decision.canCast);

        Battle::BattleActionCommitInput actionInput;
        actionInput.unit = makeBattleActionCommitUnitSnapshot(role);
        actionInput.hasCast = true;
        actionInput.cast = std::move(castResult);
        const auto& action = requireActionSnapshot(context, role->ID);
        actionInput.blinkRandomRoll = action.blinkRandomRoll;
        actionInput.blinkCellRandomRoll = action.blinkCellRandomRoll;
        actionInput.blinkReach = castResult.decision.ultimate
            ? action.ultimateBlinkReach
            : action.normalBlinkReach;
        actionInput.blinkWeakTargetDefWeight = context.config.blinkWeakTargetDefWeight;
        actionInput.blinkGeometry = makeBlinkGeometryInput(role, actionInput.blinkReach, context);
        actionInput.strengthenedMeleeOperationCountThreshold = STRENGTHENED_MELEE_OPERATION_COUNT_THRESHOLD;
        for (auto target : *context.roles)
        {
            actionInput.targets.push_back(makeBattleActionTargetSnapshot(target));
        }

        captureActionComboState(actionInput, role, context);
        unitInput.hasPendingActionInput = true;
        unitInput.pendingActionInput = std::move(actionInput);
    }

    if (role->UsingItem)
    {
        Item* item = role->UsingItem;
        Battle::BattleActionCommitInput actionInput;
        actionInput.unit = makeBattleActionCommitUnitSnapshot(role);
        actionInput.hasItem = true;
        actionInput.item = makeBattleActionItemSnapshot(item);
        actionInput.strengthenedMeleeOperationCountThreshold = STRENGTHENED_MELEE_OPERATION_COUNT_THRESHOLD;
        actionInput.hiddenWeaponTotalFrame = context.config.hiddenWeaponTotalFrame;
        auto hiddenWeaponVelocity = role->RealTowards;
        if (auto* target = findSnapshotRole(context, requireActionSnapshot(context, role->ID).farthestEnemyUnitId))
        {
            hiddenWeaponVelocity = target->Pos - role->Pos;
        }
        hiddenWeaponVelocity.normTo(10);
        actionInput.hiddenWeaponVelocity = hiddenWeaponVelocity;
        unitInput.state.castFrame = role->ActFrame;
        unitInput.hasPendingActionInput = true;
        unitInput.pendingActionInput = std::move(actionInput);
    }
}

void populateCastPlanInputForRole(
    Battle::BattleFrameActionUnitInput& unitInput,
    Role* role,
    BattleActionFrameAdapterContext& context)
{
    if (role->Dead != 0 || role->HaveAction)
    {
        return;
    }

    bool canStartAttack = role->CoolDown == 0;
    Magic* equippedMagic = role->UsingMagic;
    const auto& action = requireActionSnapshot(context, role->ID);
    Magic* normalMagic = equippedMagic ? equippedMagic : action.normalMagic;
    Magic* ultimateMagic = equippedMagic ? equippedMagic : action.ultimateMagic;
    Role* target = findSnapshotRole(context, action.nearestEnemyUnitId);
    if (!target)
    {
        role->Velocity = { 0, 0, 0 };
        role->FindingWay = 0;
        role->OperationType = -1;
        return;
    }

    if (requireLegacyRoleSnapshot(*context.snapshot, role->ID).movementDashFrames > 0)
    {
        return;
    }

    auto coreDecisionIt = context.movementDecisions->find(role);
    if (coreDecisionIt != context.movementDecisions->end())
    {
        const auto& coreDecision = coreDecisionIt->second;
        bool coreWantsMove = coreDecision.action == Battle::MovementAction::Move
            || coreDecision.action == Battle::MovementAction::Dash;
        auto coreVelocity = coreDecision.velocity;
        if (coreWantsMove && coreVelocity.norm() > 0.01)
        {
            role->OperationType = -1;
            role->FindingWay = 0;
            role->Velocity = coreDecision.velocity;
            role->RealTowards = coreDecision.velocity;
            role->RealTowards.normTo(1);
            if (coreDecision.action == Battle::MovementAction::Dash)
            {
                context.movementDashStartUnitIds.push_back(role->ID);
            }
            return;
        }
    }

    role->RealTowards = target->Pos - role->Pos;
    if (role->RealTowards.norm() > 0.01)
    {
        role->RealTowards.normTo(1);
    }

    unitInput.canPlanCast = true;
    unitInput.castInput = makeActionFrameCastInput(
        role,
        target,
        normalMagic,
        ultimateMagic,
        canStartAttack,
        false,
        context);
}

Battle::BattleFrameActionUnitState makeActionFrameUnitState(
    Role* role,
    const BattleActionFrameAdapterContext& context)
{
    assert(role);

    Battle::BattleFrameActionUnitState state;
    state.haveAction = role->HaveAction != 0;
    state.actFrame = role->ActFrame;
    state.actType = role->ActType;
    state.operationType = Battle::battleOperationFromLegacy(role->OperationType);
    state.cooldownFrames = role->CoolDown;
    state.recoveryFrames = role->OperationType == 3
        ? context.config.dashMomentumFrames
        : context.config.actionRecoveryFrames;
    if (role->OperationType >= 0 && role->OperationType <= 3 && role->ActType >= 0)
    {
        state.castFrame = requireActionSnapshot(context, role->ID).castFrameByOperation[role->OperationType];
    }
    return state;
}

Battle::BattleBlinkGeometryInput makeBlinkGeometryInput(
    Role* role,
    double reach,
    const BattleActionFrameAdapterContext& context)
{
    assert(role);
    assert(context.snapshot);

    Battle::BattleBlinkGeometryInput geometry;
    auto current = requireLegacyRoleSnapshot(*context.snapshot, role->ID).grid;
    geometry.currentGridX = current.x;
    geometry.currentGridY = current.y;

    int gridReach = std::max(1, static_cast<int>(reach / BATTLE_TILE_W) + 1);
    std::set<std::pair<int, int>> visited;
    for (auto target : *context.roles)
    {
        if (target == role || target->Dead != 0 || target->Team == role->Team)
        {
            continue;
        }

        auto targetPos45 = requireLegacyRoleSnapshot(*context.snapshot, target->ID).grid;
        for (int dx = -gridReach; dx <= gridReach; ++dx)
        {
            for (int dy = -gridReach; dy <= gridReach; ++dy)
            {
                int x = targetPos45.x + dx;
                int y = targetPos45.y + dy;
                if (!visited.emplace(x, y).second)
                {
                    continue;
                }

                bool occupied = false;
                for (auto other : *context.roles)
                {
                    if (other == role || other->Dead != 0)
                    {
                        continue;
                    }
                    auto rolePos45 = requireLegacyRoleSnapshot(*context.snapshot, other->ID).grid;
                    if (rolePos45.x == x && rolePos45.y == y)
                    {
                        occupied = true;
                        break;
                    }
                }

                geometry.cells.push_back({
                    x,
                    y,
                    positionForCell(*context.snapshot, x, y, context.config.coordCount),
                    cellWalkable(*context.snapshot, x, y),
                    occupied,
                });
            }
        }
    }
    return geometry;
}

void applyActionFrameUnitState(Role* role, const Battle::BattleFrameActionUnitState& state)
{
    assert(role);

    role->HaveAction = state.haveAction ? 1 : 0;
    role->ActFrame = state.actFrame;
    role->ActType = state.actType;
    role->OperationType = Battle::toLegacyOperationType(state.operationType);
}

void applyBlinkTeleportDelta(
    Role* role,
    const Battle::BattleBlinkTeleportDelta& teleport,
    const BattleActionFrameAdapterContext& context,
    BattleActionFrameApplyResult& result)
{
    assert(role);
    auto* target = findRoleByBattleId(*context.roles, teleport.targetUnitId);
    Battle::BattlePresentationEvent presentation;
    presentation.type = Battle::BattlePresentationEventType::StatusLog;
    presentation.sourceUnitId = role->ID;
    presentation.targetUnitId = target->ID;
    presentation.text = teleport.selectedWeakest ? "閃擊追殺" : "閃擊突襲";
    result.presentationEvents.push_back(std::move(presentation));

    role->setPositionOnly(teleport.gridX, teleport.gridY);
    role->Pos.x = teleport.position.x;
    role->Pos.y = teleport.position.y;
    role->Pos.z = 0;
    role->Velocity = { 0, 0, 0 };
    role->Acceleration = { 0, 0, context.config.gravity };
    role->FindingWay = 0;
    role->RealTowards = teleport.facing;
    result.faceTowardsNearestUnitIds.push_back(role->ID);
    result.blinkSoundCount++;
}

void populateBattleFrameRescueState(
    Battle::BattleFrameState& frameState,
    const BattleRescueFrameAdapterContext& context)
{
    assert(context.roles);
    assert(context.comboStates);
    assert(context.snapshot);

    frameState.rescue.units.clear();
    frameState.rescue.cells = context.cells;
    frameState.rescue.positionsByCell = context.positionsByCell;
    frameState.rescue.executeUnattendedRadius = context.config.executeUnattendedRadius;
    frameState.rescue.counterAttack.skillId = context.config.counterAttackSkillId;
    frameState.rescue.counterAttack.visualEffectId = context.config.counterAttackVisualEffectId;
    frameState.rescue.counterAttack.projectileSpeed = context.config.counterAttackProjectileSpeed;
    frameState.rescue.counterAttack.meleeAttackEffectOffset = context.config.counterAttackMeleeOffset;
    frameState.rescue.counterAttack.minimumTotalFrames = context.config.counterAttackMinimumTotalFrames;
    frameState.rescue.counterAttack.totalFramePadding = context.config.counterAttackTotalFramePadding;

    frameState.rescue.units.reserve(context.roles->size());
    for (auto role : *context.roles)
    {
        assert(role);
        Battle::BattleFrameRescueUnitSnapshot snapshot;
        snapshot.unit.id = role->ID;
        snapshot.unit.team = role->Team;
        snapshot.unit.alive = role->Dead == 0;
        snapshot.unit.hp = role->HP;
        snapshot.unit.maxHp = role->MaxHP;
        snapshot.unit.invincible = role->Invincible;
        snapshot.unit.cell = requireLegacyRoleSnapshot(*context.snapshot, role->ID).grid;
        snapshot.position = role->Pos;
        auto stateIt = context.comboStates->find(role->ID);
        if (stateIt != context.comboStates->end())
        {
            snapshot.unit.isSummonedClone = stateIt->second.isSummonedClone;
            snapshot.unit.forcePullProtect = stateIt->second.forcePullProtect;
            snapshot.unit.forcePullProtectRemaining = stateIt->second.forcePullProtectRemaining;
            snapshot.unit.forcePullExecute = stateIt->second.forcePullExecute;
            snapshot.unit.forcePullExecuteRemaining = stateIt->second.forcePullExecuteRemaining;
        }
        frameState.rescue.units.push_back(std::move(snapshot));
    }
}

void populateBattleMovementPhysicsFrame(
    Battle::BattleFrameState& frameState,
    const BattleMovementPhysicsFrameAdapterContext& context)
{
    assert(context.roles);
    assert(context.snapshot);

    frameState.movementPhysics.config.gravity = context.config.gravity;
    frameState.movementPhysics.config.friction = context.config.friction;
    frameState.movementPhysics.config.postDashSpreadFrames = context.config.postDashSpreadFrames;
    frameState.movementPhysics.collision.tileWidth = context.config.tileWidth;
    frameState.movementPhysics.collision.coordCount = context.config.coordCount;
    frameState.movementPhysics.collision.defaultSeparationDistance = context.config.defaultSeparationDistance;
    frameState.movementPhysics.collision.units.clear();
    frameState.movementPhysics.collision.cells = context.cells;
    frameState.movementPhysics.units.clear();

    for (auto role : *context.roles)
    {
        assert(role);
        frameState.movementPhysics.collision.units.push_back({
            role->ID,
            role->Dead == 0,
            role->Pos,
        });
    }
    for (auto role : *context.roles)
    {
        assert(role);
        const auto& roleSnapshot = requireLegacyRoleSnapshot(*context.snapshot, role->ID);
        Battle::BattleFrameMovementPhysicsUnitInput input;
        input.unitId = role->ID;
        input.frozenFrames = role->Frozen;
        input.state.position = role->Pos;
        input.state.velocity = role->Velocity;
        input.state.acceleration = role->Acceleration;
        input.state.movementDashFrames = roleSnapshot.movementDashFrames;
        input.state.movementDashCooldown = roleSnapshot.movementDashCooldown;
        input.state.movementDashSpreadFrames = roleSnapshot.movementDashSpreadFrames;

        if (role->OperationType == 3 && role->HaveAction)
        {
            assert(role->OperationType >= 0 && role->OperationType < static_cast<int>(context.config.castFrames.size()));
            const int dashStartFrame = context.config.castFrames[role->OperationType];
            const int dashEndFrame = dashStartFrame + context.config.dashMomentumFrames;
            input.actionDashActive = role->ActFrame >= dashStartFrame && role->ActFrame <= dashEndFrame;
            if (role->ActFrame > dashEndFrame)
            {
                input.state.velocity = { 0, 0, 0 };
            }
        }

        frameState.movementPhysics.units.push_back(std::move(input));
    }
}

void applyBattleMovementPhysicsFrameResults(
    const Battle::BattleFrameState& frameState,
    const BattleMovementPhysicsFrameAdapterContext& context)
{
    assert(context.roles);

    for (const auto& result : frameState.movementPhysics.committedResults)
    {
        auto* role = findRoleByBattleId(*context.roles, result.unitId);
        role->Frozen = result.frozenFrames;
        role->Pos = result.state.position;
        role->Velocity = result.state.velocity;
        role->Acceleration = result.state.acceleration;
    }
}

void populateBattleActionFrame(
    Battle::BattleFrameState& frameState,
    BattleActionFrameAdapterContext& context)
{
    assert(context.roles);
    assert(context.pendingCastResults);
    assert(context.comboStates);
    assert(context.movementDecisions);

    frameState.actions.units.clear();
    frameState.actions.units.reserve(context.roles->size());
    for (auto role : *context.roles)
    {
        assert(role);
        Battle::BattleFrameActionUnitInput unitInput;
        unitInput.unitId = role->ID;
        unitInput.state = makeActionFrameUnitState(role, context);
        populateActionCommitInputForRole(unitInput, role, context);
        populateCastPlanInputForRole(unitInput, role, context);
        frameState.actions.units.push_back(std::move(unitInput));
    }
}

BattleActionFrameApplyResult applyBattleActionFrameResults(
    const Battle::BattleFrameState& frameState,
    const BattleActionFrameAdapterContext& context)
{
    assert(context.roles);
    assert(context.pendingCastResults);
    assert(context.comboStates);
    assert(context.ultimateCasters);

    BattleActionFrameApplyResult result;
    for (const auto& action : frameState.actions.unitResults)
    {
        auto* role = findRoleByBattleId(*context.roles, action.unitId);

        if (action.castStarted)
        {
            auto* magic = action.castResult.decision.skillId >= 0
                ? Save::getInstance()->getMagic(action.castResult.decision.skillId)
                : nullptr;
            if (action.castResult.decision.equipSkill)
            {
                assert(magic);
                role->UsingMagic = magic;
            }
            assert(magic);
            role->UsingMagic = magic;
            applyBattleCastStart(role, action.castResult, magic->MagicType);
            (*context.pendingCastResults)[role] = action.castResult;
            result.clearMovementDashSpreadUnitIds.push_back(role->ID);
        }
        else
        {
            applyActionFrameUnitState(role, action.state);
        }

        if (!action.actionCommitted)
        {
            continue;
        }

        auto comboIt = context.comboStates->find(role->ID);
        if (comboIt != context.comboStates->end())
        {
            comboIt->second = action.actionResult.combo;
        }

        for (const auto& teleport : action.actionResult.blinkTeleports)
        {
            applyBlinkTeleportDelta(role, teleport, context, result);
        }

        if (action.actionInput.hasCast)
        {
            Magic* magic = role->UsingMagic;
            assert(magic);
            result.attackSoundIds.push_back(magic->SoundID);
            role->OperationCount = action.actionResult.operationCount;
            applyBattleCastCommit(role, action.actionInput.cast);
            context.pendingCastResults->erase(role);
            context.ultimateCasters->erase(role);
        }

        if (action.actionInput.hasItem)
        {
            Item* item = role->UsingItem;
            assert(item);
            for (const auto& command : action.actionResult.itemUseCommands)
            {
                assert(command.itemId == item->ID);
                role->useItem(item);
            }
            for (const auto& delta : action.actionResult.itemCountDeltas)
            {
                Event::getInstance()->addItemWithoutHint(delta.itemId, delta.delta);
            }
            role->UsingItem = nullptr;
        }
    }
    return result;
}

BattleSelectedSkillActionResult commitBattleSelectedSkillAction(
    Role* role,
    Magic* magic,
    bool isUltimate,
    int operationType,
    const BattleActionFrameAdapterContext& context)
{
    assert(context.roles);
    assert(context.comboStates);

    BattleSelectedSkillActionResult result;
    if (!role || !magic)
    {
        return result;
    }

    auto* target = findSnapshotRole(context, requireActionSnapshot(context, role->ID).nearestEnemyUnitId);
    if (!target)
    {
        return result;
    }

    auto castInput = makeActionFrameCastInput(
        role,
        target,
        magic,
        magic,
        true,
        false,
        context);
    const int resolvedOperationType = operationType >= 0
        ? operationType
        : legacyOperationForAttackArea(magic->AttackAreaType);

    Battle::BattleActionCommitInput actionInput;
    actionInput.unit = makeBattleActionCommitUnitSnapshot(role);
    actionInput.strengthenedMeleeOperationCountThreshold = STRENGTHENED_MELEE_OPERATION_COUNT_THRESHOLD;
    captureActionComboState(actionInput, role, context);

    Battle::BattleFrameState actionFrameState;
    Battle::BattleFrameActionUnitInput unitInput;
    unitInput.unitId = role->ID;
    unitInput.hasSelectedCastInput = true;
    unitInput.selectedCastInput = std::move(castInput);
    unitInput.selectedCastUltimate = isUltimate;
    unitInput.selectedOperationType = Battle::battleOperationFromLegacy(resolvedOperationType);
    unitInput.selectedActionInput = std::move(actionInput);
    actionFrameState.actions.units.push_back(std::move(unitInput));
    auto actionFrameResult = Battle::BattleFrameRunner().advanceFrame(actionFrameState);
    assert(actionFrameState.actions.unitResults.size() == 1);
    const auto& actionUnitResult = actionFrameState.actions.unitResults.front();
    assert(actionUnitResult.actionCommitted);
    const auto& actionResult = actionUnitResult.actionResult;
    auto comboIt = context.comboStates->find(role->ID);
    if (comboIt != context.comboStates->end())
    {
        comboIt->second = actionResult.combo;
    }

    result.magic = magic;
    result.applyResult.attackSoundIds.push_back(magic->SoundID);
    result.applyResult.presentationEvents.insert(
        result.applyResult.presentationEvents.end(),
        actionFrameResult.frame.presentationEvents.begin(),
        actionFrameResult.frame.presentationEvents.end());
    result.applyResult.attackSpawnRequests = actionResult.attackSpawnRequests;
    role->OperationCount = actionResult.operationCount;
    return result;
}

BattleLifecycleApplicationResult applyBattleLifecycleEvents(
    const BattleLifecycleApplicationContext& context,
    const std::vector<Battle::BattleGameplayEvent>& events)
{
    assert(context.tracker);
    assert(context.roles);

    BattleLifecycleApplicationResult result;
    for (const auto& event : events)
    {
        switch (event.type)
        {
        case Battle::BattleGameplayEventType::UnitDied:
        {
            auto* killer = event.sourceUnitId >= 0
                ? findRoleByBattleId(*context.roles, event.sourceUnitId)
                : nullptr;
            auto* victim = findRoleByBattleId(*context.roles, event.targetUnitId);
            context.tracker->recordKill(killer, victim, event.frame);
            context.tracker->recordDeath(victim, event.frame);
            result.unitDied = true;
            break;
        }
        case Battle::BattleGameplayEventType::BattleEnded:
            if (context.currentBattleResult == -1)
            {
                result.battleEnded = true;
                result.battleResult = event.amount;
                context.tracker->recordBattleEnd(event.frame, event.amount);
            }
            break;
        default:
            break;
        }
    }
    return result;
}

int resolveBattleMagicBaseDamage(const Battle::BattleMagicBaseDamageInput& input)
{
    return Battle::BattleDamageSystem().resolveMagicBaseDamage(input);
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
