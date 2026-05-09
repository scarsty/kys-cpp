#include "BattleSceneBattleAdapter.h"

#include "Scene.h"
#include "Save.h"
#include "BattleScenePresentationConstants.h"
#include "BattleStatsView.h"
#include "ChessCombo.h"
#include "ChessEquipment.h"
#include "ChessEftIds.h"
#include "ChessNeigong.h"
#include "GameUtil.h"
#include "battle/BattleCombatIntent.h"
#include "battle/BattleDamageSystem.h"
#include "battle/BattleProjectileTargetingSystem.h"

#include <algorithm>
#include <cassert>
#include <set>
#include <string>
#include <utility>

namespace KysChess::BattleSceneBattleAdapter
{

void configureBattleAttackWorld(
    Battle::BattleAttackWorld& world,
    const Battle::BattleRuntimeRulesConfig& rules);
Battle::BattleRuntimeUnit makeBattleRuntimeUnit(
    Role* role,
    const RoleComboState* state,
    const Battle::BattleGridTransform& gridTransform);
Battle::BattleStatusUnitState makeBattleStatusUnit(Role* role, const RoleComboState& state);
Battle::BattleDamageUnitState makeBattleDamageUnit(Role* role, const RoleComboState* state);
Battle::BattleDamagePresentationStyle makeBattleDamagePresentationStyle(Role* role);

namespace
{
constexpr double ROLE_MOVE_SPEED_DIVISOR = 22.0;

struct BattleActionPlanInputContext
{
    const std::vector<Role*>* roles = nullptr;
    Battle::BattleActionRulesConfig actionRules;
    Battle::BattleCastConfig castConfig;
    Battle::BattleCastGeometry castGeometry;
};

void initializeBattleActionPlanInputs(
    Battle::BattleRuntimeState::ActionState& action,
    BattleActionPlanInputContext& context);

class RoleBattleProjection
{
public:
    explicit RoleBattleProjection(Role* role)
        : role_(role)
    {
        assert(role_);
    }

    int id() const { return role_->ID; }
    int realRoleId() const { return role_->RealID; }
    const std::string& name() const { return role_->Name; }
    int team() const { return role_->Team; }
    bool alive() const { return role_->Dead == 0; }
    int hp() const { return role_->HP; }
    int maxHp() const { return role_->MaxHP; }
    int mp() const { return role_->MP; }
    int maxMp() const { return role_->MaxMP; }
    int attack() const { return role_->Attack; }
    int defence() const { return role_->Defence; }
    int speed() const { return role_->Speed; }
    int star() const { return role_->Star; }
    int invincible() const { return role_->Invincible; }
    int hurtFrame() const { return role_->HurtFrame; }
    int frozen() const { return role_->Frozen; }
    int frozenMax() const { return role_->FrozenMax; }
    Pointf position() const { return role_->Pos; }
    Pointf velocity() const { return role_->Velocity; }
    Pointf acceleration() const { return role_->Acceleration; }
    Pointf facing() const { return role_->RealTowards; }

    Battle::BattleUnitState worldUnit() const;
    Battle::BattleUnitState initializedWorldUnit(
        const std::map<int, RoleComboState>& comboStates,
        const Battle::BattleWorldState& world,
        const std::map<int, Battle::BattleMovementPhysicsState>& movementRuntime,
        const Battle::BattleActionRulesConfig& actionRules) const;
    Battle::BattleRuntimeUnit runtimeUnit(
        const RoleComboState* state,
        const Battle::BattleGridTransform& gridTransform) const;
    Battle::BattleStatusUnitState statusUnit(const RoleComboState& state) const;
    Battle::BattleDamageUnitState damageUnit(const RoleComboState* state) const;

private:
    Role* role_ = nullptr;
};

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

Color damageTextColor(const Role* role, bool emphasized)
{
    bool friendlyTarget = role && role->Team == 0;
    if (friendlyTarget)
    {
        return emphasized ? Color{ 255, 45, 85, 255 } : Color{ 255, 90, 79, 255 };
    }
    return emphasized ? Color{ 47, 128, 255, 255 } : Color{ 102, 207, 255, 255 };
}

Battle::BattleUnitState RoleBattleProjection::worldUnit() const
{
    Battle::BattleUnitState unit;
    unit.id = id();
    unit.realRoleId = realRoleId();
    unit.name = name();
    unit.team = team();
    unit.alive = alive();
    unit.position = position();
    unit.velocity = velocity();
    unit.speed = speed() / ROLE_MOVE_SPEED_DIVISOR;
    unit.star = star();
    unit.canAttack = role_->CoolDown == 0;
    return unit;
}

bool isSummonedCloneRole(const Role* role)
{
    if (!role)
    {
        return false;
    }
    const auto& states = KysChess::ChessCombo::getActiveStates();
    const auto it = states.find(role->ID);
    return it != states.end() && it->second.isSummonedClone;
}

int getComboLookupId(const Role* role)
{
    if (!role)
    {
        return -1;
    }
    if (isSummonedCloneRole(role))
    {
        return -1;
    }
    return role->RealID >= 0 ? role->RealID : role->ID;
}

Battle::BattleUnitState RoleBattleProjection::initializedWorldUnit(
    const std::map<int, RoleComboState>& comboStates,
    const Battle::BattleWorldState& world,
    const std::map<int, Battle::BattleMovementPhysicsState>& movementRuntime,
    const Battle::BattleActionRulesConfig& actionRules) const
{
    const auto comboIt = comboStates.find(id());
    const bool dashAttackEnabled = comboIt != comboStates.end() && comboIt->second.dashAttack;

    auto unit = worldUnit();
    unit.reach = actionRules.meleeAttackReach;
    unit.style = Battle::CombatStyle::Melee;
    unit.taXue = dashAttackEnabled;
    unit.dashAttack = dashAttackEnabled;

    const auto existingMovement = std::find_if(
        world.units.begin(),
        world.units.end(),
        [unitId = id()](const Battle::BattleUnitState& existing)
        {
            return existing.id == unitId;
        });
    if (existingMovement != world.units.end())
    {
        unit.assignedSlot = existingMovement->assignedSlot;
        unit.slotSwitchCooldownRemaining = existingMovement->slotSwitchCooldownRemaining;
    }

    const auto movementIt = movementRuntime.find(id());
    if (movementIt != movementRuntime.end())
    {
        unit.dashFramesRemaining = movementIt->second.movementDashFrames;
        unit.dashCooldownRemaining = movementIt->second.movementDashCooldown;
    }

    return unit;
}

Battle::BattleUnitState makeBattleWorldUnit(Role* role)
{
    return RoleBattleProjection(role).worldUnit();
}

Battle::BattleUnitState makeInitializedBattleWorldUnit(
    Role* role,
    const std::map<int, RoleComboState>& comboStates,
    const Battle::BattleWorldState& world,
    const std::map<int, Battle::BattleMovementPhysicsState>& movementRuntime,
    const Battle::BattleActionRulesConfig& actionRules)
{
    return RoleBattleProjection(role).initializedWorldUnit(
        comboStates,
        world,
        movementRuntime,
        actionRules);
}

Battle::BattleDeathEffectStore makeBattleDeathEffectStore(
    const std::vector<Role*>& roles,
    const std::map<int, RoleComboState>& states)
{
    Battle::BattleDeathEffectStore store;
    const auto& allCombos = KysChess::ChessCombo::getAllCombos();
    for (int comboId = 0; comboId < static_cast<int>(allCombos.size()); ++comboId)
    {
        if (!allCombos[comboId].isAntiCombo)
        {
            store.regularSynergyComboIds.insert(comboId);
        }
    }

    for (auto* role : roles)
    {
        assert(role);
        const auto stateIt = states.find(role->ID);
        assert(stateIt != states.end());

        Battle::BattleDeathEffectExtras extras;
        extras.id = role->ID;
        extras.shieldPctMaxHp = stateIt->second.shieldPctMaxHP;
        extras.shieldOnAllyDeathTracker = stateIt->second.shieldOnAllyDeathTracker;
        extras.appliedEffects = stateIt->second.appliedEffects;

        const int comboLookupId = getComboLookupId(role);
        if (comboLookupId >= 0)
        {
            extras.comboIds = KysChess::ChessCombo::getCombosForRole(comboLookupId);
        }

        store.units.push_back(std::move(extras));
    }

    return store;
}

std::vector<Role*> makeOrderedRuntimeRoles(
    const Battle::BattleRuntimeState& runtime,
    const std::unordered_map<int, Role*>& rolesByBattleId)
{
    std::vector<Role*> roles;
    roles.reserve(runtime.units.units.size());
    for (const auto& unit : runtime.units.units)
    {
        const auto roleIt = rolesByBattleId.find(unit.id);
        assert(roleIt != rolesByBattleId.end());
        assert(roleIt->second);
        roles.push_back(roleIt->second);
    }
    return roles;
}

std::vector<Battle::BattleSetupComboDefinition> makeBattleSetupComboDefinitions()
{
    std::vector<Battle::BattleSetupComboDefinition> definitions;
    for (const auto& combo : KysChess::ChessCombo::getAllCombos())
    {
        Battle::BattleSetupComboDefinition definition;
        definition.id = combo.id;
        definition.name = combo.name;
        definition.memberRoleIds = combo.memberRoleIds;
        definition.isAntiCombo = combo.isAntiCombo;
        definition.starSynergyBonus = combo.starSynergyBonus;
        definition.thresholds.reserve(combo.thresholds.size());
        for (const auto& threshold : combo.thresholds)
        {
            definition.thresholds.push_back({ threshold.count, threshold.effects });
        }
        definitions.push_back(std::move(definition));
    }
    return definitions;
}

std::vector<Battle::BattleSetupEquipmentDefinition> makeBattleSetupEquipmentDefinitions()
{
    std::vector<Battle::BattleSetupEquipmentDefinition> definitions;
    for (const auto& equipment : KysChess::ChessEquipment::getAll())
    {
        definitions.push_back({
            equipment.itemId,
            equipment.equipType,
            equipment.effects,
            equipment.actAsComboNames,
        });
    }
    return definitions;
}

std::vector<Battle::BattleSetupEquipmentSynergyDefinition> makeBattleSetupEquipmentSynergyDefinitions()
{
    std::vector<Battle::BattleSetupEquipmentSynergyDefinition> definitions;
    for (const auto& synergy : KysChess::ChessEquipment::getAllSynergies())
    {
        definitions.push_back({
            synergy.roleIds,
            synergy.equipmentId,
            synergy.effects,
            synergy.actAsComboNames,
        });
    }
    return definitions;
}

std::vector<Battle::BattleSetupNeigongDefinition> makeBattleSetupNeigongDefinitions()
{
    std::vector<Battle::BattleSetupNeigongDefinition> definitions;
    for (const auto& neigong : KysChess::ChessNeigong::getPool())
    {
        definitions.push_back({ neigong.magicId, neigong.effects });
    }
    return definitions;
}

Battle::BattleRuntimeSetupSeed makeBattleRuntimeSetupSeed(const BattleRuntimeSceneSetupInput& context)
{
    Battle::BattleRuntimeSetupSeed setup;
    setup.allyRoster = context.allyRoster;
    setup.enemyRoster = context.enemyRoster;
    setup.comboDefinitions = makeBattleSetupComboDefinitions();
    setup.equipmentDefinitions = makeBattleSetupEquipmentDefinitions();
    setup.equipmentSynergies = makeBattleSetupEquipmentSynergyDefinitions();
    setup.neigongDefinitions = makeBattleSetupNeigongDefinitions();
    setup.obtainedNeigongMagicIds = context.obtainedNeigongMagicIds;
    setup.nextCloneUnitId = context.nextCloneUnitId;

    for (const auto& unit : context.allyRoster)
    {
        setup.units.push_back({
            unit.unitId,
            unit.realRoleId,
            unit.team,
            unit.star,
            unit.cost,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            {},
        });
    }
    for (const auto& unit : context.enemyRoster)
    {
        setup.units.push_back({
            unit.unitId,
            unit.realRoleId,
            unit.team,
            unit.star,
            unit.cost,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            {},
        });
    }

    for (const auto& roster : { &context.allyRoster, &context.enemyRoster })
    {
        for (const auto& unit : *roster)
        {
            const auto roleIt = std::find_if(
                context.roles.begin(),
                context.roles.end(),
                [&unit](Role* role)
                {
                    return role && role->ID == unit.unitId;
                });
            if (roleIt == context.roles.end() || !*roleIt)
            {
                continue;
            }

            const auto seedIt = std::find_if(
                setup.units.begin(),
                setup.units.end(),
                [&unit](const Battle::BattleInitializationUnitSeed& seed)
                {
                    return seed.unitId == unit.unitId;
                });
            if (seedIt == setup.units.end())
            {
                continue;
            }

            seedIt->baseMaxHp = (*roleIt)->MaxHP;
            seedIt->baseAttack = (*roleIt)->Attack;
            seedIt->baseDefence = (*roleIt)->Defence;
            seedIt->baseSpeed = (*roleIt)->Speed;
            seedIt->baseFist = (*roleIt)->Fist;
            seedIt->baseSword = (*roleIt)->Sword;
            seedIt->baseKnife = (*roleIt)->Knife;
            seedIt->baseUnusual = (*roleIt)->Unusual;
            seedIt->baseHiddenWeapon = (*roleIt)->HiddenWeapon;
        }
    }

    for (const auto& unit : context.allyRoster)
    {
        setup.cloneSources.push_back({
            unit.unitId,
            unit.realRoleId,
            0,
            unit.star,
            unit.chessInstanceId,
            unit.sourceOrder,
        });
    }
    for (auto& source : setup.cloneSources)
    {
        const auto roleIt = std::find_if(
            context.roles.begin(),
            context.roles.end(),
            [&source](Role* role)
            {
                return role && role->ID == source.sourceUnitId;
            });
        if (roleIt != context.roles.end() && *roleIt)
        {
            source.power = (*roleIt)->MaxHP + (*roleIt)->Attack + (*roleIt)->Defence;
        }
    }
    for (const auto& [x, y] : context.cloneSpawnCells)
    {
        bool occupied = false;
        for (auto* role : context.roles)
        {
            if (!role || role->Dead != 0)
            {
                continue;
            }
            if (role->X() == x && role->Y() == y)
            {
                occupied = true;
                break;
            }
        }
        setup.cloneCells.push_back({ x, y, true, occupied });
    }

    return setup;
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
    const auto& setup = context.setup;
    assert(setup.comboStates);

    Battle::BattleRuntimeInit init;
    init.runtime.units.gridTransform = context.rules.gridTransform;
    init.runtime.combo.units = *setup.comboStates;
    init.runtime.units.units.reserve(setup.roles.size());
    init.runtime.status.units.reserve(setup.roles.size());
    init.runtime.world.units.reserve(setup.roles.size());
    init.setup = makeBattleRuntimeSetupSeed(setup);

    std::unordered_map<int, Role*> rolesByBattleId;
    rolesByBattleId.reserve(setup.roles.size());
    for (auto* role : setup.roles)
    {
        assert(role);
        rolesByBattleId.emplace(role->ID, role);
        auto stateIt = setup.comboStates->find(role->ID);
        init.runtime.units.units.push_back(makeBattleRuntimeUnit(
            role,
            stateIt != setup.comboStates->end() ? &stateIt->second : nullptr,
            context.rules.gridTransform));
        if (stateIt != setup.comboStates->end())
        {
            init.runtime.status.units.push_back(Battle::makeBattleStatusRuntimeUnit(
                makeBattleStatusUnit(role, stateIt->second)));
        }
        else
        {
            init.runtime.status.units.push_back(Battle::BattleStatusRuntimeUnit{ .id = role->ID });
        }
        init.runtime.world.units.push_back(makeBattleWorldUnit(role));
    }

    BattleRuntimeCreationResult result{
        Battle::BattleRuntimeSession(std::move(init)),
        {},
        std::move(rolesByBattleId),
    };
    result.initializationResult = result.session.releaseInitializationResult();
    return result;
}

void commitInitializedBattleRuntimeConfiguration(
    Battle::BattleRuntimeSession& session,
    const BattleRuntimeBuildContext& context,
    const std::unordered_map<int, Role*>& rolesByBattleId)
{
    const auto& runtime = session.runtime();
    const auto orderedRoles = makeOrderedRuntimeRoles(runtime, rolesByBattleId);
    Battle::BattleRuntimeSetupConfiguration config;

    config.world = runtime.world;
    config.world.frame = context.setup.battleFrame;
    config.world.config = context.rules.movementConfig;
    config.world.terrainCells = context.setup.terrainCells;

    const auto existingWorld = runtime.world;
    config.world.units.clear();
    config.world.units.reserve(orderedRoles.size());
    for (auto* role : orderedRoles)
    {
        assert(role);
        if (role->Dead != 0)
        {
            continue;
        }
        config.world.units.push_back(makeInitializedBattleWorldUnit(
            role,
            runtime.combo.units,
            existingWorld,
            runtime.movementRuntime,
            context.rules.action));
    }

    config.attacks = runtime.attacks;
    configureBattleAttackWorld(config.attacks, context.rules);

    config.teamEffects.healAuraRadius = context.rules.teamEffectHealAuraRadius;
    config.deathEffects.store = makeBattleDeathEffectStore(orderedRoles, runtime.combo.units);

    config.rescue.positionsByCell = context.setup.rescuePositionsByCell;
    config.rescue.cells = context.setup.rescueCells;
    config.rescue.executeUnattendedRadius = context.rules.rescueExecuteUnattendedRadius;
    config.rescue.counterAttack = context.rules.rescueCounterAttack;
    config.rescue.counterAttack.skillId = context.setup.rescueCounterAttackSkillId;

    config.movementPhysics.config = context.rules.movementPhysicsConfig;
    config.movementPhysics.collision = context.rules.movementCollisionWorld;
    config.movementPhysics.actionCastFrames.assign(
        context.rules.castConfig.castFrames.begin(),
        context.rules.castConfig.castFrames.end());
    config.movementPhysics.dashMomentumFrames = context.rules.movementPhysicsDashMomentumFrames;

    config.action.castFrames.assign(
        context.rules.castConfig.castFrames.begin(),
        context.rules.castConfig.castFrames.end());
    config.action.actionRecoveryFrames = context.rules.action.actionRecoveryFrames;
    config.action.dashRecoveryFrames = context.rules.action.dashRecoveryFrames;
    config.action.blinkWeakTargetDefWeight = context.rules.action.blinkWeakTargetDefWeight;
    config.action.strengthenedMeleeOperationCountThreshold = context.rules.action.strengthenedMeleeOperationCountThreshold;
    config.action.projectileBounceRange = context.rules.action.projectileBounceRange;

    config.projectileFollowUps = context.rules.projectileFollowUps;

    config.damage.aggregatePendingTransactionsByDefender = true;
    config.result.pendingAliveByTeam = context.setup.pendingAliveByTeam;
    for (auto* role : orderedRoles)
    {
        assert(role);
        const auto stateIt = runtime.combo.units.find(role->ID);
        config.damage.unitExtras.push_back(Battle::makeBattleDamageRuntimeUnit(
            makeBattleDamageUnit(
                role,
                stateIt != runtime.combo.units.end() ? &stateIt->second : nullptr)));
        config.damage.presentationStylesByDefender.emplace(role->ID, makeBattleDamagePresentationStyle(role));
        if (stateIt != runtime.combo.units.end() && stateIt->second.deathAOEPct > 0)
        {
            config.damage.unitEffects.emplace(
                role->ID,
                Battle::BattleDamageApplicationUnitEffects{
                    stateIt->second.deathAOEPct,
                    stateIt->second.deathAOEStunFrames,
                    stateIt->second.deathAOEMaxTargets,
                });
        }
    }

    BattleActionPlanInputContext actionContext;
    actionContext.roles = &orderedRoles;
    actionContext.actionRules = context.rules.action;
    actionContext.castConfig = context.rules.castConfig;
    actionContext.castGeometry = context.rules.castGeometry;
    initializeBattleActionPlanInputs(config.action, actionContext);

    session.commitSetupConfiguration(std::move(config));
}

void applyBattleInitializationResult(
    const Battle::BattleInitializationResult& result,
    const BattleInitializationApplyContext& context)
{
    assert(context.battleRoles);
    assert(context.friendsObj);
    assert(context.comboStates);
    assert(context.rolesByBattleId);

    *context.comboStates = result.comboStates;

    for (const auto& delta : result.roleDeltas)
    {
        auto* role = findRoleByBattleId(*context.battleRoles, delta.unitId);
        role->Star = delta.star;
        role->MaxHP = delta.maxHp;
        role->HP = delta.hp;
        role->Attack = delta.attack;
        role->Defence = delta.defence;
        role->Speed = delta.speed;
        role->Fist = delta.fist;
        role->Sword = delta.sword;
        role->Knife = delta.knife;
        role->Unusual = delta.unusual;
        role->HiddenWeapon = delta.hiddenWeapon;
    }

    for (const auto& intent : result.cloneIntents)
    {
        auto roleIt = context.rolesByBattleId->find(intent.sourceUnitId);
        assert(roleIt != context.rolesByBattleId->end());
        auto* source = roleIt->second;
        assert(source);

        context.friendsObj->push_back(*source);
        auto* clone = &context.friendsObj->back();
        clone->ID = intent.cloneUnitId;
        clone->RealID = source->RealID;
        clone->Auto = 2;
        clone->Team = source->Team;
        clone->Dead = 0;
        clone->LastAttacker = nullptr;
        clone->Velocity = { 0, 0, 0 };
        clone->Acceleration = { 0, 0, 0 };
        clone->UsingMagic = nullptr;
        clone->HaveAction = 0;
        clone->ActFrame = 0;
        clone->CoolDown = 0;
        clone->CoolDownMax = 0;
        clone->Frozen = 0;
        clone->FrozenMax = 0;
        clone->Invincible = 0;
        clone->HurtFrame = 0;
        clone->Shake = 0;
        clone->FindingWay = 0;
        clone->OperationCount = 0;
        clone->setPositionOnly(intent.gridX, intent.gridY);
        clone->MaxHP = intent.roleValues.maxHp;
        clone->HP = intent.roleValues.hp;
        clone->Attack = intent.roleValues.attack;
        clone->Defence = intent.roleValues.defence;
        clone->Speed = intent.roleValues.speed;
        context.battleRoles->push_back(clone);
        (*context.rolesByBattleId)[intent.cloneUnitId] = clone;
    }

    for (const auto& delta : result.enemyTopDebuffs)
    {
        auto* role = findRoleByBattleId(*context.battleRoles, delta.unitId);
        role->Attack += delta.attackDelta;
        role->Defence += delta.defenceDelta;
    }
}

Battle::BattleSetupPlacementInput makeBattleSetupPlacementInput(const std::vector<Role*>& roles)
{
    Battle::BattleSetupPlacementInput input;
    input.units.reserve(roles.size());
    for (auto* role : roles)
    {
        assert(role);
        input.units.push_back({
            role->ID,
            role->X(),
            role->Y(),
            role->FaceTowards,
        });
    }
    return input;
}

Battle::BattlePresentationColor makeBattlePresentationColor(Color color)
{
    return { color.r, color.g, color.b, color.a };
}

Magic* selectLowerPowerMagic(Role* role)
{
    return selectBattleMagic(role, std::less<double>{});
}

Magic* selectHigherPowerMagic(Role* role)
{
    return selectBattleMagic(role, std::greater<double>{});
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

void configureBattleAttackWorld(Battle::BattleAttackWorld& world, const Battle::BattleRuntimeRulesConfig& rules)
{
    world.hitRadius = rules.action.meleeAttackHitRadius;
    world.minimumVectorNorm = rules.minimumVectorNorm;
    world.projectileGraceFrames = 5;
    world.bounceSpawnDistance = rules.projectileFollowUps.areaSpawnDistance;
    world.defaultProjectileSpeed = rules.projectileFollowUps.projectileSpeed;
    world.spendNonThroughOnHit = false;
}

Battle::BattleRuntimeUnit RoleBattleProjection::runtimeUnit(
    const RoleComboState* state,
    const Battle::BattleGridTransform& gridTransform) const
{
    Battle::BattleRuntimeUnit unit;
    unit.id = id();
    unit.realRoleId = realRoleId();
    unit.name = name();
    unit.team = team();
    unit.alive = alive();
    unit.hp = hp();
    unit.maxHp = maxHp();
    unit.mp = mp();
    unit.maxMp = maxMp();
    unit.attack = attack();
    unit.defence = defence();
    unit.speed = speed();
    unit.cooldown = role_->CoolDown;
    unit.cooldownMax = role_->CoolDownMax;
    unit.haveAction = role_->HaveAction != 0;
    unit.actFrame = role_->ActFrame;
    unit.operationType = Battle::battleOperationFromLegacy(role_->OperationType);
    unit.actType = role_->ActType;
    unit.operationCount = role_->OperationCount;
    unit.physicalPower = role_->PhysicalPower;
    unit.invincible = invincible();
    unit.hurtFrame = hurtFrame();
    for (int magicType = 0; magicType <= 4; ++magicType)
    {
        unit.actPropertiesByMagicType.emplace(magicType, role_->getActProperty(magicType));
    }
    unit.position = position();
    unit.velocity = velocity();
    unit.acceleration = acceleration();
    unit.facing = facing();
    unit.grid = gridTransform.toGrid(position());
    if (state)
    {
        unit.shield = state->shield;
        unit.mpBlocked = state->mpBlockTimer > 0;
        unit.mpRecoveryBonusPct = state->mpRecoveryBonusPct;
    }
    return unit;
}

Battle::BattleRuntimeUnit makeBattleRuntimeUnit(
    Role* role,
    const RoleComboState* state,
    const Battle::BattleGridTransform& gridTransform)
{
    return RoleBattleProjection(role).runtimeUnit(state, gridTransform);
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

Battle::BattleStatusUnitState RoleBattleProjection::statusUnit(const RoleComboState& state) const
{
    Battle::BattleStatusUnitState unit;
    unit.id = id();
    unit.alive = alive();
    unit.hp = hp();
    unit.maxHp = maxHp();
    unit.attack = attack();
    unit.invincible = invincible();
    unit.poisonTimer = state.poisonTimer;
    unit.poisonTickPct = state.poisonTickDmg;
    unit.poisonSourceId = state.poisonSourceId;
    unit.bleedStacks = state.bleedStacks;
    unit.bleedTimer = state.bleedTimer;
    unit.bleedSourceId = state.bleedSourceId;
    unit.frozenTimer = frozen();
    unit.frozenMaxTimer = frozenMax();
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

Battle::BattleStatusUnitState makeBattleStatusUnit(Role* role, const RoleComboState& state)
{
    return RoleBattleProjection(role).statusUnit(state);
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

Battle::BattleDamageUnitState RoleBattleProjection::damageUnit(const RoleComboState* state) const
{
    Battle::BattleDamageUnitState unit;
    unit.id = id();
    unit.alive = alive();
    unit.hp = hp();
    unit.maxHp = maxHp();
    unit.mp = mp();
    unit.maxMp = maxMp();
    unit.attack = attack();
    unit.invincible = invincible();
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

Battle::BattleDamageUnitState makeBattleDamageUnit(Role* role, const RoleComboState* state)
{
    return RoleBattleProjection(role).damageUnit(state);
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

Battle::BattleActionSkillSeed makeBattleActionSkillSeed(Role* role, Magic* magic)
{
    Battle::BattleActionSkillSeed seed;
    if (!magic)
    {
        return seed;
    }
    seed.id = magic->ID;
    seed.name = magic->Name;
    seed.soundId = magic->SoundID;
    seed.hurtType = magic->HurtType;
    seed.attackAreaType = magic->AttackAreaType;
    seed.magicType = magic->MagicType;
    seed.visualEffectId = magic->EffectID;
    seed.selectDistance = magic->SelectDistance;
    seed.actProperty = role->getActProperty(magic->MagicType);
    seed.magicPower = role->getMagicPower(magic);
    return seed;
}

Battle::BattleActionPlanSeed makeBattleActionPlanSeed(
    Role* role,
    Magic* normalMagic,
    Magic* ultimateMagic,
    bool hasEquippedSkill)
{
    assert(role);
    Battle::BattleActionPlanSeed seed;
    seed.unitId = role->ID;
    seed.hasEquippedSkill = hasEquippedSkill;
    seed.normalSkill = makeBattleActionSkillSeed(role, normalMagic);
    seed.ultimateSkill = makeBattleActionSkillSeed(role, ultimateMagic);
    return seed;
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
    const BattleActionFrameApplyContext& context,
    BattleActionFrameApplyResult& result)
{
    assert(role);
    role->setPositionOnly(teleport.gridX, teleport.gridY);
    role->Pos.x = teleport.position.x;
    role->Pos.y = teleport.position.y;
    role->Pos.z = 0;
    role->Velocity = { 0, 0, 0 };
    role->Acceleration = { 0, 0, context.gravity };
    role->FindingWay = 0;
    role->RealTowards = teleport.facing;
    result.faceTowardsNearestUnitIds.push_back(role->ID);
    result.blinkSoundCount++;
}

void applyBattleMovementPresentationResults(
    const std::vector<Battle::BattleFrameMovementPresentationUnitResult>& movementResults,
    const std::vector<Role*>& roles)
{
    for (const auto& result : movementResults)
    {
        auto* role = findRoleByBattleId(roles, result.unitId);
        role->Frozen = result.frozenFrames;
        role->Pos = result.position;
        role->Velocity = result.velocity;
        role->Acceleration = result.acceleration;
        auto facing = result.facing;
        if (facing.norm() > 0.01)
        {
            role->RealTowards = facing;
        }
    }
}

namespace
{
void initializeBattleActionPlanInputs(
    Battle::BattleRuntimeState::ActionState& action,
    BattleActionPlanInputContext& context)
{
    assert(context.roles);

    action.planSeeds.clear();
    action.castConfig = context.castConfig;
    action.castGeometry = context.castGeometry;
    action.actionRules = context.actionRules;
    for (auto role : *context.roles)
    {
        assert(role);
        Magic* equippedMagic = role->UsingMagic;
        Magic* normalMagic = equippedMagic ? equippedMagic : selectLowerPowerMagic(role);
        Magic* ultimateMagic = equippedMagic ? equippedMagic : selectHigherPowerMagic(role);
        action.planSeeds[role->ID] = makeBattleActionPlanSeed(
            role,
            normalMagic,
            ultimateMagic,
            equippedMagic != nullptr);
    }
}
}  // namespace

BattleActionFrameApplyResult applyBattleActionFrameResults(
    const std::vector<Battle::BattleFrameActionUnitResult>& actionResults,
    const BattleActionFrameApplyContext& context)
{
    assert(context.roles);

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
            role->PreActTimer = context.battleFrame;
            role->OperationCount = action.actionResult.operationCount;
            applyBattleCastCommit(role, action.actionInput.cast);
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
