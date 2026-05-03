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
    attack.attackerUnitId = effect.Attacker ? effect.Attacker->ID : -1;
    attack.preferredTargetUnitId = effect.PreferredTarget ? effect.PreferredTarget->ID : -1;
    attack.requirePreferredTarget = effect.RequirePreferredTarget != 0;
    attack.frame = effect.Frame;
    attack.totalFrame = effect.TotalFrame;
    attack.noHurt = effect.NoHurt != 0;
    attack.track = effect.Track != 0;
    attack.through = effect.Through != 0;
    attack.ultimate = effect.IsUltimate != 0;
    attack.executeCanHitInvincible = attackHasExecuteEffect(effect);
    attack.ignoreProjectileCancel = effect.IgnoreProjectileCancel != 0 || effect.UsingMagic == nullptr;
    attack.sharedHitGroupId = effect.SharedHitGroupId;
    attack.visualEffectId = effect.VisualEffectId;
    attack.operationKind = effect.OperationType;
    attack.position = effect.Pos;
    attack.velocity = effect.Velocity;
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
    effect.Pos = attack.position;
    effect.Velocity = attack.velocity;
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

Battle::BattleAttackWorld makeBattleAttackWorld(
    const std::vector<Role*>& roles,
    const std::deque<BattleSceneAct::AttackEffect>& effects,
    size_t effectCount,
    const std::unordered_map<int, std::set<int>>& sharedHitGroupTargets)
{
    Battle::BattleAttackWorld world;
    world.hitRadius = BATTLE_TILE_W * 2.0;
    world.projectileGraceFrames = 5;
    world.spendNonThroughOnHit = false;
    for (auto role : roles)
    {
        if (role)
        {
            world.units.push_back(makeBattleAttackUnit(role));
        }
    }

    for (size_t i = 0; i < effectCount; ++i)
    {
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
    assert(world.attacks.size() <= effects.size());
    for (const auto& attack : world.attacks)
    {
        assert(attack.id >= 0);
        assert(static_cast<size_t>(attack.id) < effects.size());
        writeBattleAttackInstance(effects[attack.id], attack, roles);
    }

    sharedHitGroupTargets.clear();
    for (const auto& [groupId, targets] : world.sharedHitGroupTargets)
    {
        auto& output = sharedHitGroupTargets[groupId];
        output.insert(targets.begin(), targets.end());
    }
}

}  // namespace KysChess::BattleSceneBattleAdapter
