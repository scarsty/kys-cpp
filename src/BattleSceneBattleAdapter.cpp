#include "BattleSceneBattleAdapter.h"

#include "Scene.h"
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
constexpr std::array<int, 4> LEGACY_CAST_FRAMES = { 25, 30, 20, 25 };
constexpr int DEFAULT_FORCED_RANGED_MIN_SELECT_DISTANCE = 6;
constexpr double MAX_EFFECTIVE_BATTLE_REACH = 480.0;
constexpr double ENGAGEMENT_CELL_DEADBAND = BATTLE_TILE_W / 2.0;
constexpr double MELEE_ATTACK_EFFECT_OFFSET = BATTLE_TILE_W * 2.0;
constexpr double MELEE_ATTACK_HIT_RADIUS = BATTLE_TILE_W * 2.0;
constexpr double MELEE_ATTACK_SAFETY_MARGIN = MELEE_ATTACK_HIT_RADIUS - (BATTLE_TILE_W - ENGAGEMENT_CELL_DEADBAND / 2.0);
constexpr double MELEE_ATTACK_REACH = MELEE_ATTACK_EFFECT_OFFSET + MELEE_ATTACK_HIT_RADIUS - MELEE_ATTACK_SAFETY_MARGIN;
constexpr double RANGED_ATTACK_SAFETY_MARGIN = MELEE_ATTACK_HIT_RADIUS - ENGAGEMENT_CELL_DEADBAND;

template<typename Cmp>
Magic* selectBattleMagic(Role* role, Cmp cmp)
{
    assert(role);
    auto learnedMagics = role->getLearnedMagics();
    if (learnedMagics.empty())
    {
        return nullptr;
    }

    Magic* chosen = learnedMagics.front();
    double power = role->getMagicPower(chosen);
    for (size_t i = 1; i < learnedMagics.size(); ++i)
    {
        double candidatePower = role->getMagicPower(learnedMagics[i]);
        if (cmp(candidatePower, power))
        {
            power = candidatePower;
            chosen = learnedMagics[i];
        }
    }
    return chosen;
}

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

Pointf facingDirection(int faceTowards)
{
    switch (faceTowards)
    {
    case 0:
        return { 1.0f, 0.0f, 0.0f };
    case 1:
        return { 0.0f, 1.0f, 0.0f };
    case 2:
        return { -1.0f, 0.0f, 0.0f };
    case 3:
        return { 0.0f, -1.0f, 0.0f };
    default:
        return { 1.0f, 0.0f, 0.0f };
    }
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

BattleRuntimeCreationResult createInitializedBattleRuntimeSession(const BattleRuntimeBuildContext& context)
{
    assert(context.comboStates);

    Battle::BattleRuntimeInit init;
    init.runtime.units.gridTransform = context.gridTransform;
    init.runtime.combo.units = *context.comboStates;
    init.runtime.combo.events.clear();
    init.runtime.units.units.reserve(context.roles.size());
    init.runtime.status.units.reserve(context.roles.size());

    std::unordered_map<int, Role*> rolesByBattleId;
    rolesByBattleId.reserve(context.roles.size());
    for (auto* role : context.roles)
    {
        assert(role);
        rolesByBattleId.emplace(role->ID, role);
        auto stateIt = context.comboStates->find(role->ID);
        init.runtime.units.units.push_back(makeBattleRuntimeUnit(
            role,
            stateIt != context.comboStates->end() ? &stateIt->second : nullptr,
            context.gridTransform));
        if (stateIt != context.comboStates->end())
        {
            init.runtime.status.units.push_back(Battle::makeBattleStatusRuntimeUnit(
                makeBattleStatusUnit(role, stateIt->second)));
        }
    }

    BattleRuntimeCreationResult result{
        Battle::BattleRuntimeSession(std::move(init)),
        {},
        std::move(rolesByBattleId),
    };
    result.initializationResult = result.session.releaseInitializationResult();
    return result;
}

void commitFinalSetupPlacementToRuntime(
    Battle::BattleRuntimeState& runtime,
    const BattleSetupPlacementInput& input)
{
    runtime.units.gridTransform = input.gridTransform;
    for (const auto& role : input.roles)
    {
        const auto position = Pointf{
            static_cast<float>(-role.y * input.gridTransform.tileWidth + role.x * input.gridTransform.tileWidth + input.gridTransform.coordCount * input.gridTransform.tileWidth),
            static_cast<float>(role.y * input.gridTransform.tileWidth + role.x * input.gridTransform.tileWidth),
            0.0f,
        };
        runtime.units.setPosition(role.unitId, position);
        auto& unit = runtime.units.requireUnit(role.unitId);
        unit.facing = facingDirection(role.faceTowards);

        auto worldIt = std::find_if(
            runtime.world.units.begin(),
            runtime.world.units.end(),
            [unitId = role.unitId](const Battle::BattleUnitState& unitState)
            {
                return unitState.id == unitId;
            });
        if (worldIt != runtime.world.units.end())
        {
            worldIt->position = position;
            worldIt->velocity = { 0, 0, 0 };
        }
    }
}

Battle::BattlePresentationColor makeBattlePresentationColor(Color color)
{
    return { color.r, color.g, color.b, color.a };
}

Battle::BattleCastConfig makeBattleCastConfig()
{
    Battle::BattleCastConfig config;
    config.castFrames = LEGACY_CAST_FRAMES;
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
    geometry.dashVelocityMagnitude = MELEE_ATTACK_HIT_RADIUS / DASH_MOMENTUM_FRAMES;
    geometry.dashHitFrameStep = DASH_HIT_FRAME_STEP;
    return geometry;
}

int strengthenedMeleeOperationCountThreshold()
{
    return STRENGTHENED_MELEE_OPERATION_COUNT_THRESHOLD;
}

Magic* selectLowerPowerMagic(Role* role)
{
    return selectBattleMagic(role, std::less<double>{});
}

Magic* selectHigherPowerMagic(Role* role)
{
    return selectBattleMagic(role, std::greater<double>{});
}

bool roleForcesRangedMagic(const std::map<int, RoleComboState>& comboStates, int unitId)
{
    auto it = comboStates.find(unitId);
    return it != comboStates.end() && it->second.forceRangedAttack;
}

int forcedRangedMinSelectDistance(const std::map<int, RoleComboState>& comboStates, int unitId)
{
    auto it = comboStates.find(unitId);
    if (it == comboStates.end() || it->second.forceRangedMinSelectDistance <= 0)
    {
        return DEFAULT_FORCED_RANGED_MIN_SELECT_DISTANCE;
    }
    return std::max(1, it->second.forceRangedMinSelectDistance);
}

int projectileSpeedMultiplierPct(const std::map<int, RoleComboState>& comboStates, int unitId)
{
    auto it = comboStates.find(unitId);
    if (it == comboStates.end())
    {
        return 100;
    }
    return std::max(100, it->second.projectileSpeedMultiplierPct);
}

bool isForcedRangedMagic(const Magic* magic, bool forceRanged)
{
    return magic && forceRanged && magic->AttackAreaType == 0;
}

bool isProjectileStyleMagic(const Magic* magic, bool forceRanged)
{
    return magic
        && (magic->AttackAreaType == 1
            || magic->AttackAreaType == 2
            || isForcedRangedMagic(magic, forceRanged));
}

bool isBattleRangedStyleMagic(const Magic* magic, bool forceRanged)
{
    return magic
        && (magic->AttackAreaType == 1
            || magic->AttackAreaType == 2
            || magic->AttackAreaType == 3
            || isForcedRangedMagic(magic, forceRanged));
}

int effectiveProjectileSelectDistance(const Magic* magic, bool forcedRanged, int forcedRangedMinSelectDistance)
{
    assert(magic);
    int selectDistance = std::max(1, magic->SelectDistance);
    if (forcedRanged && magic->AttackAreaType == 0)
    {
        selectDistance = std::max(selectDistance, std::max(1, forcedRangedMinSelectDistance));
    }
    return selectDistance;
}

double battleBlinkReach(const Magic* magic)
{
    const auto geometry = makeBattleCastGeometry();
    if (!magic)
    {
        return BATTLE_TILE_W * 3.0;
    }
    if (magic->AttackAreaType == 3)
    {
        return 180.0;
    }
    if (magic->AttackAreaType == 1 || magic->AttackAreaType == 2)
    {
        const double reach = geometry.projectileSpawnOffset
            + geometry.projectileBaseTravel
            + (magic->SelectDistance - 1) * geometry.projectileTravelPerSelectDistance;
        return std::min(MAX_EFFECTIVE_BATTLE_REACH, reach - 10.0);
    }
    return std::max(BATTLE_TILE_W * 3.0, static_cast<double>(magic->SelectDistance * BATTLE_TILE_W));
}

double effectiveBattleReach(
    const Magic* magic,
    bool forceRanged,
    int forcedRangedMinSelectDistance,
    int projectileSpeedMultiplierPct)
{
    const auto geometry = makeBattleCastGeometry();
    if (!magic)
    {
        return BATTLE_TILE_W * 2.0;
    }
    if (magic->AttackAreaType == 3)
    {
        return 180.0;
    }
    if (isProjectileStyleMagic(magic, forceRanged))
    {
        const int selectDistance = effectiveProjectileSelectDistance(
            magic,
            isForcedRangedMagic(magic, forceRanged),
            forcedRangedMinSelectDistance);
        const double projectileReach = geometry.projectileSpawnOffset
            + (geometry.projectileBaseTravel + (selectDistance - 1) * geometry.projectileTravelPerSelectDistance)
                * projectileSpeedMultiplierPct / 100.0;
        return std::max(BATTLE_TILE_W * 2.0, projectileReach - RANGED_ATTACK_SAFETY_MARGIN);
    }
    return MELEE_ATTACK_REACH;
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
    skill.soundId = input.magic->SoundID;
    skill.hurtType = input.magic->HurtType;
    skill.attackAreaType = input.magic->AttackAreaType;
    skill.magicType = input.magic->MagicType;
    skill.visualEffectId = input.magic->EffectID;
    skill.selectDistance = input.magic->SelectDistance;
    skill.projectileSpeedMultiplierPct = input.projectileSpeedMultiplierPct;
    skill.actProperty = unit->getActProperty(input.magic->MagicType);
    skill.magicPower = unit->getMagicPower(input.magic);
    skill.meleeSplashCount = input.meleeSplashCount;
    skill.extraProjectileCount = input.extraProjectileCount;
    skill.strengthenedMelee = input.strengthenedMelee;
    skill.reach = input.reach;
    skill.blinkReach = input.blinkReach;
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

void configureBattleAttackWorld(Battle::BattleAttackWorld& world)
{
    world.hitRadius = BATTLE_TILE_W * 2.0;
    world.minimumVectorNorm = MINIMUM_VECTOR_NORM;
    world.projectileGraceFrames = 5;
    world.bounceSpawnDistance = BATTLE_TILE_W * 1.5;
    world.defaultProjectileSpeed = BATTLE_TILE_W / 3.0;
    world.spendNonThroughOnHit = false;
}

Battle::BattleRuntimeUnit makeBattleRuntimeUnit(
    Role* role,
    const RoleComboState* state,
    const Battle::BattleGridTransform& gridTransform)
{
    assert(role);

    Battle::BattleRuntimeUnit unit;
    unit.id = role->ID;
    unit.realRoleId = role->RealID;
    unit.name = role->Name;
    unit.team = role->Team;
    unit.alive = role->Dead == 0;
    unit.hp = role->HP;
    unit.maxHp = role->MaxHP;
    unit.mp = role->MP;
    unit.maxMp = role->MaxMP;
    unit.attack = role->Attack;
    unit.defence = role->Defence;
    unit.speed = role->Speed;
    unit.cooldown = role->CoolDown;
    unit.cooldownMax = role->CoolDownMax;
    unit.haveAction = role->HaveAction != 0;
    unit.actFrame = role->ActFrame;
    unit.operationType = Battle::battleOperationFromLegacy(role->OperationType);
    unit.actType = role->ActType;
    unit.operationCount = role->OperationCount;
    unit.physicalPower = role->PhysicalPower;
    unit.invincible = role->Invincible;
    unit.hurtFrame = role->HurtFrame;
    for (int magicType = 0; magicType <= 4; ++magicType)
    {
        unit.actPropertiesByMagicType.emplace(magicType, role->getActProperty(magicType));
    }
    unit.position = role->Pos;
    unit.velocity = role->Velocity;
    unit.acceleration = role->Acceleration;
    unit.facing = role->RealTowards;
    unit.grid = gridTransform.toGrid(role->Pos);
    if (state)
    {
        unit.shield = state->shield;
        unit.mpBlocked = state->mpBlockTimer > 0;
        unit.mpRecoveryBonusPct = state->mpRecoveryBonusPct;
    }
    return unit;
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

int legacyOperationForAttackArea(int attackAreaType)
{
    return Battle::toLegacyOperationType(
        Battle::BattleCombatIntentPlanner().operationTypeForAttackArea(attackAreaType));
}

double actionRandomRoll(const BattleActionFrameAdapterContext& context)
{
    assert(context.random);
    return context.random->rand();
}

int actionRandomInt(const BattleActionFrameAdapterContext& context, int upperBound)
{
    assert(context.random);
    return context.random->rand_int(upperBound);
}

int actionUltimateExtraProjectileCount(Role* role, const BattleActionFrameAdapterContext& context)
{
    assert(context.comboStates);
    auto comboIt = context.comboStates->find(role->ID);
    assert(comboIt != context.comboStates->end());
    auto& combo = comboIt->second;
    return Battle::collectFrameExtraProjectileCount(
        combo,
        role->ID,
        std::max(0, combo.ultimateExtraProjectiles));
}

Pointf positionForCell(int x, int y, int coordCount)
{
    return {
        static_cast<float>(-y * BATTLE_TILE_W + x * BATTLE_TILE_W + coordCount * BATTLE_TILE_W),
        static_cast<float>(y * BATTLE_TILE_W + x * BATTLE_TILE_W),
        0.0f,
    };
}

bool cellWalkable(int x, int y, int coordCount)
{
    return x >= 0 && y >= 0 && x < coordCount && y < coordCount;
}

BattleCastSkillAdapterInput makeActionFrameSkillInput(
    Role* role,
    Magic* magic,
    bool ultimate,
    const BattleActionFrameAdapterContext& context,
    bool consumeFrameSkillBonuses)
{
    BattleCastSkillAdapterInput skill;
    if (!magic)
    {
        return skill;
    }

    assert(context.comboStates);
    const bool forceRanged = roleForcesRangedMagic(*context.comboStates, role->ID);
    const int speedMultiplierPct = projectileSpeedMultiplierPct(*context.comboStates, role->ID);
    skill.magic = magic;
    skill.reach = std::min(
        effectiveBattleReach(
            magic,
            forceRanged,
            forcedRangedMinSelectDistance(*context.comboStates, role->ID),
            speedMultiplierPct),
        context.config.maxEffectiveBattleReach);
    skill.forceRanged = forceRanged;
    skill.rangedStyle = isBattleRangedStyleMagic(magic, forceRanged);
    skill.projectileSpeedMultiplierPct = speedMultiplierPct;
    skill.meleeSplashCount = ultimate && magic->AttackAreaType == 0 ? 1 : 0;
    skill.extraProjectileCount = ultimate && consumeFrameSkillBonuses
        ? actionUltimateExtraProjectileCount(role, context)
        : 0;
    skill.blinkReach = battleBlinkReach(magic);
    return skill;
}

int rollDashHitCount(Role* role, Magic* selectedMagic, const BattleActionFrameAdapterContext& context)
{
    int dashHitCount = 1;
    if (selectedMagic)
    {
        const double multiHitScore = (role->Speed + role->getActProperty(selectedMagic->MagicType)) / 180.0;
        if (actionRandomRoll(context) < multiHitScore)
        {
            dashHitCount++;
        }
        if (actionRandomRoll(context) < multiHitScore * 0.5)
        {
            dashHitCount++;
        }
    }
    return dashHitCount;
}

Battle::BattleCastInput makeActionFrameCastInput(
    Role* role,
    Magic* normalMagic,
    Magic* ultimateMagic,
    bool canStartAttack,
    bool movementDashActive,
    const BattleActionFrameAdapterContext& context,
    bool consumeFrameSkillBonuses = true)
{
    assert(role);
    assert(context.comboStates);

    const bool isUltimate = role->MP >= role->MaxMP;
    Magic* selectedMagic = isUltimate && ultimateMagic ? ultimateMagic : normalMagic;
    Pointf dashVelocity = role->RealTowards;
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
    castAdapterInput.normalSkill = makeActionFrameSkillInput(role, normalMagic, false, context, consumeFrameSkillBonuses);
    castAdapterInput.ultimateSkill = makeActionFrameSkillInput(role, ultimateMagic, true, context, consumeFrameSkillBonuses);
    castAdapterInput.canStartAttack = canStartAttack;
    castAdapterInput.movementDashActive = movementDashActive;
    castAdapterInput.dashAttackEnabled = dashAttackEnabled;
    castAdapterInput.dashVelocity = dashVelocity;
    castAdapterInput.dashHitCount = rollDashHitCount(role, selectedMagic, context);
    castAdapterInput.emitDashFollowUpSkillAttack = dashAttackEnabled && selectedMagic;
    castAdapterInput.dashFollowUpOperationType = selectedMagic ? legacyOperationForAttackArea(selectedMagic->AttackAreaType) : -1;
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

Battle::BattleBlinkGeometryInput makeBlinkGeometryInput(
    Role* role,
    double reach,
    const BattleActionFrameAdapterContext& context)
{
    assert(role);
    assert(context.units);

    Battle::BattleBlinkGeometryInput geometry;
    auto current = context.units->requireUnit(role->ID).grid;
    geometry.currentGridX = current.x;
    geometry.currentGridY = current.y;

    int gridReach = std::max(1, static_cast<int>(reach / BATTLE_TILE_W) + 1);
    std::set<std::pair<int, int>> visited;
    for (const auto& target : context.units->units)
    {
        if (target.id == role->ID || !target.alive || target.team == role->Team)
        {
            continue;
        }

        auto targetPos45 = target.grid;
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
                for (const auto& other : context.units->units)
                {
                    if (other.id == role->ID || !other.alive)
                    {
                        continue;
                    }
                    auto rolePos45 = other.grid;
                    if (rolePos45.x == x && rolePos45.y == y)
                    {
                        occupied = true;
                        break;
                    }
                }

                geometry.cells.push_back({
                    x,
                    y,
                    positionForCell(x, y, context.config.coordCount),
                    cellWalkable(x, y, context.config.coordCount),
                    occupied,
                });
            }
        }
    }
    return geometry;
}

void applyActionFrameUnitState(Role* role, const Battle::BattleFrameUnitRuntimeState& state)
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

void applyBattleMovementPhysicsFrameResults(
    const std::vector<Battle::BattleFrameMovementPhysicsUnitResult>& movementResults,
    const std::vector<Role*>& roles)
{
    for (const auto& result : movementResults)
    {
        auto* role = findRoleByBattleId(roles, result.unitId);
        role->Frozen = result.frozenFrames;
        role->Pos = result.state.position;
        role->Velocity = result.state.velocity;
        role->Acceleration = result.state.acceleration;
    }
}

void applyBattleMovementFrameResults(
    const Battle::BattleTickResult& movement,
    const std::vector<Role*>& roles)
{
    for (const auto& unit : movement.snapshot.units)
    {
        auto* role = findRoleByBattleId(roles, unit.id);
        role->Pos = unit.position;
        role->Velocity = unit.velocity;
        auto velocity = unit.velocity;
        if (velocity.norm() > 0.01)
        {
            role->RealTowards = velocity;
            role->RealTowards.normTo(1);
        }
    }
}

void initializeBattleActionPlanInputs(
    Battle::BattleRuntimeState& runtime,
    BattleActionFrameAdapterContext& context)
{
    assert(context.roles);
    assert(context.comboStates);

    runtime.action.castPlanInputs.clear();
    for (auto role : *context.roles)
    {
        assert(role);
        Magic* equippedMagic = role->UsingMagic;
        Magic* normalMagic = equippedMagic ? equippedMagic : selectLowerPowerMagic(role);
        Magic* ultimateMagic = equippedMagic ? equippedMagic : selectHigherPowerMagic(role);
        runtime.action.castPlanInputs[role->ID] = makeActionFrameCastInput(
            role,
            normalMagic,
            ultimateMagic,
            true,
            false,
            context,
            false);
    }
}

BattleActionFrameApplyResult applyBattleActionFrameResults(
    const std::vector<Battle::BattleFrameActionUnitResult>& actionResults,
    const BattleActionFrameAdapterContext& context)
{
    assert(context.roles);
    assert(context.ultimateCasters);

    BattleActionFrameApplyResult result;
    for (const auto& action : actionResults)
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

        for (const auto& teleport : action.actionResult.blinkTeleports)
        {
            applyBlinkTeleportDelta(role, teleport, context, result);
        }

        if (action.actionInput.hasCast)
        {
            Magic* magic = role->UsingMagic;
            assert(magic);
            role->PreActTimer = context.config.battleFrame;
            result.attackSoundIds.push_back(magic->SoundID);
            role->OperationCount = action.actionResult.operationCount;
            applyBattleCastCommit(role, action.actionInput.cast);
            context.ultimateCasters->erase(role->ID);
        }

    }
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
            result.diedUnitIds.push_back(event.targetUnitId);
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

}  // namespace KysChess::BattleSceneBattleAdapter
