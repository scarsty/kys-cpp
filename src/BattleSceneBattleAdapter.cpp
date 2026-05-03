#include "BattleSceneBattleAdapter.h"

#include "ChessCombo.h"
#include "Scene.h"

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

bool attackHasExecuteEffect(const BattleSceneAct::AttackEffect& ae)
{
    if (!ae.Attacker)
    {
        return false;
    }

    auto& cs = KysChess::ChessCombo::getActiveStates();
    auto it = cs.find(ae.Attacker->ID);
    if (it == cs.end())
    {
        return false;
    }

    for (auto& effect : it->second.triggeredEffects)
    {
        if (effect.trigger == KysChess::Trigger::OnHit
            && effect.type == KysChess::EffectType::Execute
            && effect.triggerValue > 0
            && effect.value > 0)
        {
            return true;
        }
    }
    return false;
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

Battle::BattleAttackInstance makeBattleAttackInstance(
    const BattleSceneAct::AttackEffect& effect,
    int attackId)
{
    Battle::BattleAttackInstance attack;
    attack.id = attackId;
    attack.state.attackerUnitId = effect.Attacker ? effect.Attacker->ID : -1;
    attack.state.preferredTargetUnitId = effect.PreferredTarget ? effect.PreferredTarget->ID : -1;
    attack.state.requirePreferredTarget = effect.RequirePreferredTarget != 0;
    attack.frame = effect.Frame;
    attack.state.totalFrame = effect.TotalFrame;
    attack.noHurt = effect.NoHurt != 0;
    attack.state.track = effect.Track != 0;
    attack.state.through = effect.Through != 0;
    attack.state.ultimate = effect.IsUltimate != 0;
    attack.state.executeCanHitInvincible = attackHasExecuteEffect(effect);
    attack.state.ignoreProjectileCancel = effect.IgnoreProjectileCancel != 0 || effect.UsingMagic == nullptr;
    attack.state.sharedHitGroupId = effect.SharedHitGroupId;
    attack.state.visualEffectId = effect.VisualEffectId;
    attack.state.operationType = Battle::battleOperationFromLegacy(effect.OperationType);
    attack.state.hiddenWeaponItemId = effect.UsingHiddenWeapon ? effect.UsingHiddenWeapon->ID : -1;
    attack.state.scriptedDamage = effect.ScriptedDamage;
    attack.state.scriptedStunFrames = effect.ScriptedStunFrames;
    attack.state.scriptedBleedStacks = effect.ScriptedBleedStacks;
    attack.state.bounceRemaining = effect.BounceRemaining;
    attack.state.bounceRange = effect.BounceRange;
    attack.state.bounceChancePct = effect.BounceChancePct;
    attack.state.bounceRollPct = effect.BounceRollPct;
    attack.state.position = effect.Pos;
    attack.state.velocity = effect.Velocity;
    attack.acceleration = effect.Acceleration;
    attack.spiralMotion = effect.SpiralMotion != 0;
    attack.spiralCenter = effect.SpiralCenter;
    attack.spiralRadius = effect.SpiralRadius;
    attack.spiralRadiusGrowth = effect.SpiralRadiusGrowth;
    attack.spiralAngle = effect.SpiralAngle;
    attack.spiralAngularVelocity = effect.SpiralAngularVelocity;
    for (const auto& [role, count] : effect.Defender)
    {
        if (role && count > 0)
        {
            attack.hitUnitIds.push_back(role->ID);
        }
    }
    return attack;
}

void writeBattleAttackInstance(
    BattleSceneAct::AttackEffect& effect,
    const Battle::BattleAttackInstance& attack,
    const std::vector<Role*>& roles)
{
    effect.Frame = attack.frame;
    effect.NoHurt = attack.noHurt ? 1 : 0;
    effect.Pos = attack.state.position;
    effect.Velocity = attack.state.velocity;
    effect.Acceleration = attack.acceleration;
    effect.SpiralCenter = attack.spiralCenter;
    effect.SpiralRadius = attack.spiralRadius;
    effect.SpiralRadiusGrowth = attack.spiralRadiusGrowth;
    effect.SpiralAngle = attack.spiralAngle;
    effect.SpiralAngularVelocity = attack.spiralAngularVelocity;

    effect.Defender.clear();
    for (int unitId : attack.hitUnitIds)
    {
        effect.Defender[findRoleByBattleId(roles, unitId)] = 1;
    }
}

BattleSceneAct::AttackEffect makeSpawnedBattleAttackEffect(
    const std::deque<BattleSceneAct::AttackEffect>& effects,
    const Battle::BattleAttackInstance& attack,
    const std::vector<Role*>& roles)
{
    assert(attack.spawnedFromAttackId >= 0);
    assert(static_cast<size_t>(attack.spawnedFromAttackId) < effects.size());

    auto effect = effects[attack.spawnedFromAttackId];
    effect.PreferredTarget = attack.state.preferredTargetUnitId >= 0
        ? findRoleByBattleId(roles, attack.state.preferredTargetUnitId)
        : nullptr;
    effect.RequirePreferredTarget = attack.state.requirePreferredTarget ? 1 : 0;
    effect.OperationType = Battle::toLegacyOperationType(attack.state.operationType);
    effect.TotalFrame = attack.state.totalFrame;
    effect.Track = attack.state.track ? 1 : 0;
    effect.Through = attack.state.through ? 1 : 0;
    effect.IsUltimate = attack.state.ultimate ? 1 : 0;
    effect.IsMain = 0;
    effect.Weaken = 0;
    effect.SharedHitGroupId = attack.state.sharedHitGroupId;
    effect.IgnoreProjectileCancel = attack.state.ignoreProjectileCancel ? 1 : 0;
    effect.BounceRemaining = attack.state.bounceRemaining;
    effect.BounceChancePct = attack.state.bounceChancePct;
    effect.BounceRollPct = attack.state.bounceRollPct;
    effect.BounceRange = attack.state.bounceRange;
    effect.VisualEffectId = attack.state.visualEffectId;
    writeBattleAttackInstance(effect, attack, roles);
    return effect;
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

std::vector<BattleSceneAct::AttackEffect> makeBattleCastAttackEffects(
    Role* unit,
    Magic* magic,
    const Battle::BattleCastResult& result,
    const std::vector<Role*>& roles)
{
    assert(unit);
    assert(magic);
    assert(result.decision.canCast);

    std::vector<BattleSceneAct::AttackEffect> effects;
    effects.reserve(result.attackSpawnRequests.size());
    for (const auto& request : result.attackSpawnRequests)
    {
        const auto& state = request.initial;
        assert(state.totalFrame > 0);

        BattleSceneAct::AttackEffect effect;
        effect.Pos = state.position;
        effect.Velocity = state.velocity;
        effect.Attacker = unit;
        effect.PreferredTarget = state.preferredTargetUnitId >= 0
            ? findRoleByBattleId(roles, state.preferredTargetUnitId)
            : nullptr;
        effect.RequirePreferredTarget = state.requirePreferredTarget ? 1 : 0;
        effect.UsingMagic = magic;
        effect.Frame = request.initialFrame;
        effect.TotalFrame = state.totalFrame;
        effect.OperationType = Battle::toLegacyOperationType(state.operationType);
        effect.Strengthen = state.strengthMultiplier;
        effect.Track = state.track ? 1 : 0;
        effect.Through = state.through ? 1 : 0;
        effect.IsUltimate = state.ultimate ? 1 : 0;
        effect.SharedHitGroupId = state.sharedHitGroupId;
        effect.IgnoreProjectileCancel = state.ignoreProjectileCancel ? 1 : 0;
        effect.BounceRemaining = state.bounceRemaining;
        effect.BounceChancePct = state.bounceChancePct;
        effect.BounceRollPct = state.bounceRollPct;
        effect.BounceRange = state.bounceRange;
        effect.setEft(state.visualEffectId);
        effects.push_back(std::move(effect));
    }
    return effects;
}

Battle::BattleAttackWorld makeBattleAttackWorld(
    const std::vector<Role*>& roles,
    const std::deque<BattleSceneAct::AttackEffect>& effects,
    size_t effectCount,
    const std::unordered_map<int, std::set<int>>& sharedHitGroupTargets)
{
    Battle::BattleAttackWorld world;
    world.hitRadius = BATTLE_TILE_W * 2.0;
    world.minimumVectorNorm = MINIMUM_VECTOR_NORM;
    world.projectileGraceFrames = 5;
    world.bounceSpawnDistance = BATTLE_TILE_W * 1.5;
    world.defaultProjectileSpeed = BATTLE_TILE_W / 3.0;
    world.spendNonThroughOnHit = false;
    world.nextAttackId = static_cast<int>(effectCount);
    for (auto role : roles)
    {
        if (role)
        {
            world.units.push_back(makeBattleAttackUnit(role));
        }
    }

    for (size_t i = 0; i < effectCount; ++i)
    {
        if (effects[i].VisualOnly)
        {
            continue;
        }
        world.attacks.push_back(makeBattleAttackInstance(effects[i], static_cast<int>(i)));
    }

    for (const auto& [groupId, targets] : sharedHitGroupTargets)
    {
        auto& output = world.sharedHitGroupTargets[groupId];
        output.insert(output.end(), targets.begin(), targets.end());
    }
    return world;
}

void writeBattleAttackWorld(
    const Battle::BattleAttackWorld& world,
    std::deque<BattleSceneAct::AttackEffect>& effects,
    const std::vector<Role*>& roles,
    std::unordered_map<int, std::set<int>>& sharedHitGroupTargets)
{
    for (const auto& attack : world.attacks)
    {
        assert(attack.id >= 0);
        if (static_cast<size_t>(attack.id) < effects.size())
        {
            writeBattleAttackInstance(effects[attack.id], attack, roles);
            continue;
        }

        assert(static_cast<size_t>(attack.id) == effects.size());
        effects.push_back(makeSpawnedBattleAttackEffect(effects, attack, roles));
    }

    sharedHitGroupTargets.clear();
    for (const auto& [groupId, targets] : world.sharedHitGroupTargets)
    {
        auto& output = sharedHitGroupTargets[groupId];
        output.insert(targets.begin(), targets.end());
    }
}

}  // namespace KysChess::BattleSceneBattleAdapter
