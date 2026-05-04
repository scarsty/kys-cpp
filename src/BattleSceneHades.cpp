#include "BattleSceneHades.h"
#include "Audio.h"
#include "BattleRoleManager.h"
#include "BattleSceneBattleAdapter.h"
#include "BattleScenePresentationConstants.h"
#include "battle/BattleAttackSystem.h"
#include "battle/BattleCombatIntent.h"
#include "battle/BattleComboTriggerSystem.h"
#include "battle/BattleCore.h"
#include "battle/BattleDamageApplicationSystem.h"
#include "battle/BattleDamageSystem.h"
#include "battle/BattleProjectileTargetingSystem.h"
#include "battle/BattleStatusSystem.h"
#include "battle/BattleTeamEffectSystem.h"
#include "ChessBalance.h"
#include "ChessCombo.h"
#include "ChessEftIds.h"
#include "ChessEquipment.h"
#include "ChessNeigong.h"
#include "ChessUiCommon.h"
#include "Engine.h"
#include "Event.h"
#include "Font.h"
#include "GameUtil.h"
#include "Head.h"
#include "MainScene.h"
#include "SystemSettings.h"
#include "TeamMenu.h"
#include "Weather.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <variant>

namespace
{
using KysChess::BattleSceneBattleAdapter::findRoleByBattleId;
using KysChess::BattleSceneBattleAdapter::applyBattleCastCommit;
using KysChess::BattleSceneBattleAdapter::applyBattleCastStart;
using KysChess::BattleSceneBattleAdapter::applyBattleFrameUnitRuntimeResult;
using KysChess::BattleSceneBattleAdapter::applyBattleProjectileCancelDamage;
using KysChess::BattleSceneBattleAdapter::BattleCastAdapterInput;
using KysChess::BattleSceneBattleAdapter::BattleCastSkillAdapterInput;
using KysChess::BattleSceneBattleAdapter::makeBattleCastInput;
using KysChess::BattleSceneBattleAdapter::makeBattleAttackWorld;
using KysChess::BattleSceneBattleAdapter::makeBattleActionCommitUnitSnapshot;
using KysChess::BattleSceneBattleAdapter::makeBattleActionItemSnapshot;
using KysChess::BattleSceneBattleAdapter::makeBattleActionTargetSnapshot;
using KysChess::BattleSceneBattleAdapter::makeBattleFrameUnitRuntimeInput;
using KysChess::BattleSceneBattleAdapter::makeBattleHitItemSnapshot;
using KysChess::BattleSceneBattleAdapter::makeBattleHitSkillSnapshot;
using KysChess::BattleSceneBattleAdapter::makeBattleHitUnitSnapshot;
using KysChess::BattleSceneBattleAdapter::makeBattleDamageApplicationUnitEffects;

constexpr int BATTLE_TILE_W = Scene::TILE_W;
constexpr int PROJECTILE_SPEED = BATTLE_TILE_W / 3;
constexpr int PROJECTILE_BASE_TRAVEL = BATTLE_TILE_W * 5;  // travel distance at sd=1 (excluding spawn offset)
constexpr int PROJECTILE_TRAVEL_PER_SD = BATTLE_TILE_W;    // extra travel per sd
constexpr double PROJECTILE_BOUNCE_RANGE = 90.0;
constexpr int HURT_FLASH_DURATION = 15;
constexpr int HURT_FLASH_PERIOD = 3;
constexpr int BLINK_SOUND_EFFECT_ID = 11;
constexpr int CAMERA_DEATH_ZOOM_FRAMES = 30;           // death-focus hold time; original behavior used 30
constexpr int CAMERA_BATTLE_END_ZOOM_FRAMES = 30;      // end-of-battle hold time; original behavior used 60
constexpr float CAMERA_DEATH_ZOOM_SCALE = 0.5f;        // 0.5 == original half-screen zoom; 1.0 disables zoom
constexpr int CAMERA_ZOOM_EASE_FRAMES = 0;             // 0 restores the old hard snap zoom; higher = softer zoom edges
constexpr int CAMERA_DEATH_SLOW_FRAMES = 10;           // slow-motion duration after a death
constexpr int CAMERA_BATTLE_END_SLOW_FRAMES = 30;      // slow-motion duration when the battle ends
constexpr int CAMERA_SLOW_STEP_INTERVAL = 4;           // lower = less slow-motion, higher = slower motion
constexpr float CAMERA_AUTO_FOLLOW_LERP = 1.0f;        // 1.0 restores the old instant auto-follow snap
constexpr float CAMERA_DEATH_FOCUS_LERP = 1.0f;        // 1.0 restores the old instant death-focus snap
constexpr float CAMERA_DEATH_RETARGET_BLEND = 0.0f;    // 0.0 restores the old immediate retarget on repeated deaths
constexpr int STATUS_TEXT_SIZE = 24;
constexpr int EMPHASIS_TEXT_SIZE = 36;
constexpr int NORMAL_DAMAGE_TEXT_SIZE = 30;
constexpr int ULT_DAMAGE_TEXT_SIZE = 44;
constexpr double BLINK_WEAK_TARGET_DEF_WEIGHT = 4.0;
constexpr int PATH_REFRESH_FRAMES = 12;
constexpr int DASH_MOMENTUM_FRAMES = 5;
constexpr int MOVEMENT_DASH_COOLDOWN_FRAMES = 18;
constexpr int POST_DASH_SPREAD_FRAMES = 12;
constexpr int ACTION_RECOVERY_FRAMES = 4;
constexpr double MAX_EFFECTIVE_BATTLE_REACH = 480.0;
constexpr double ENGAGEMENT_CELL_DEADBAND = BATTLE_TILE_W / 2.0;
constexpr double ENGAGEMENT_CELL_ARRIVE_DISTANCE = BATTLE_TILE_W - ENGAGEMENT_CELL_DEADBAND / 2.0;
constexpr double PATH_WAYPOINT_REACHED_DISTANCE = BATTLE_TILE_W;
constexpr double MELEE_ATTACK_EFFECT_OFFSET = BATTLE_TILE_W * 2.0;
constexpr double MELEE_ATTACK_HIT_RADIUS = BATTLE_TILE_W * 2.0;
constexpr double MELEE_ATTACK_SAFETY_MARGIN = MELEE_ATTACK_HIT_RADIUS - ENGAGEMENT_CELL_ARRIVE_DISTANCE;
constexpr double MELEE_ATTACK_REACH = MELEE_ATTACK_EFFECT_OFFSET + MELEE_ATTACK_HIT_RADIUS - MELEE_ATTACK_SAFETY_MARGIN;
constexpr double MELEE_LOCAL_TARGET_RADIUS = MELEE_ATTACK_REACH + MELEE_ATTACK_EFFECT_OFFSET;
constexpr double DASH_ATTACK_ADVANCE_DISTANCE = MELEE_LOCAL_TARGET_RADIUS;
constexpr double DASH_ATTACK_MELEE_REACH = MELEE_ATTACK_REACH + DASH_ATTACK_ADVANCE_DISTANCE;
constexpr double RANGED_ATTACK_SAFETY_MARGIN = MELEE_ATTACK_HIT_RADIUS - ENGAGEMENT_CELL_DEADBAND;
constexpr double ROLE_MOVE_SPEED_DIVISOR = 22.0;
constexpr int BATTLE_COORD_COUNT = BATTLEMAP_COORD_COUNT;
constexpr int ROLE_STATUS_BAR_WIDTH = 48;
constexpr int ROLE_STATUS_BAR_HEIGHT = 6;
constexpr int ROLE_STATUS_BAR_Y = -120;
constexpr int ROLE_STATUS_BAR_STEP_Y = ROLE_STATUS_BAR_HEIGHT + ROLE_STATUS_BAR_HEIGHT / 3;
constexpr int ROLE_STATUS_BAR_MP_Y = ROLE_STATUS_BAR_Y + ROLE_STATUS_BAR_STEP_Y;
constexpr int ROLE_STATUS_BAR_FROZEN_Y = ROLE_STATUS_BAR_Y + ROLE_STATUS_BAR_STEP_Y * 2;
constexpr int DEFAULT_FORCED_RANGED_MIN_SELECT_DISTANCE = 6;

Pointf battlePos45To90(int x, int y)
{
    return {
        static_cast<float>(-y * BATTLE_TILE_W + x * BATTLE_TILE_W + BATTLE_COORD_COUNT * BATTLE_TILE_W),
        static_cast<float>(y * BATTLE_TILE_W + x * BATTLE_TILE_W),
        0.0f,
    };
}

Point battlePos90To45(double x, double y)
{
    x -= BATTLE_COORD_COUNT * BATTLE_TILE_W;
    return {
        static_cast<int>(std::round((x / BATTLE_TILE_W + y / BATTLE_TILE_W) / 2.0)),
        static_cast<int>(std::round((-x / BATTLE_TILE_W + y / BATTLE_TILE_W) / 2.0)),
        0,
    };
}

Pointf lerpPoint(const Pointf& from, const Pointf& to, float t)
{
    t = (std::max)(0.0f, (std::min)(t, 1.0f));
    return {
        from.x + (to.x - from.x) * t,
        from.y + (to.y - from.y) * t,
        from.z + (to.z - from.z) * t,
    };
}

float smoothStep(float t)
{
    t = (std::max)(0.0f, (std::min)(t, 1.0f));
    return t * t * (3.0f - 2.0f * t);
}

float cameraZoomBlend(int closeUp, int closeUpTotal)
{
    if (closeUp <= 0 || closeUpTotal <= 0)
    {
        return 0.0f;
    }

    if (CAMERA_ZOOM_EASE_FRAMES <= 0)
    {
        return 1.0f;
    }

    int easeFrames = std::max(1, std::min(CAMERA_ZOOM_EASE_FRAMES, closeUpTotal / 2));
    int elapsed = closeUpTotal - closeUp;
    if (elapsed < easeFrames)
    {
        return smoothStep(static_cast<float>(elapsed) / easeFrames);
    }
    if (closeUp < easeFrames)
    {
        return smoothStep(static_cast<float>(closeUp) / easeFrames);
    }
    return 1.0f;
}

int calcProjectileReach(int select_distance, int spawn_offset)
{
    return spawn_offset + PROJECTILE_BASE_TRAVEL + (select_distance - 1) * PROJECTILE_TRAVEL_PER_SD;
}

int effectiveProjectileSelectDistance(const Magic* magic, bool forcedRanged, int forcedRangedMinSelectDistance)
{
    if (!magic)
    {
        return 1;
    }

    int selectDistance = std::max(1, magic->SelectDistance);
    if (forcedRanged && magic->AttackAreaType == 0)
    {
        selectDistance = std::max(selectDistance, std::max(1, forcedRangedMinSelectDistance));
    }
    return selectDistance;
}

double battleSpeedToRefreshMultiplier(int battleSpeed)
{
    switch (battleSpeed)
    {
    case 0:
        return 0.5;
    case 2:
        return 2.0;
    default:
        return 1.0;
    }
}

float shakeJitter(int battleFrame, int roleId)
{
    uint32_t state = static_cast<uint32_t>(battleFrame) ^ (static_cast<uint32_t>(roleId) * 0x9e3779b9u);
    state ^= state >> 16;
    state *= 0x7feb352du;
    state ^= state >> 15;
    state *= 0x846ca68bu;
    state ^= state >> 16;
    return -2.5f + static_cast<float>(state & 0xffffu) * (5.0f / 65535.0f);
}

KysChess::RoleComboState makeSummonedCloneState(const KysChess::RoleComboState& sourceState, int cloneMaxHP)
{
    KysChess::RoleComboState cloneState;
    for (const auto& effect : sourceState.appliedEffects)
    {
        KysChess::ChessBattleEffects::applyEffect(cloneState, effect, effect.sourceComboId);
    }

    cloneState.cloneSummonCount = 0;
    cloneState.forcePullProtectRemaining = cloneState.forcePullProtectCharges;
    cloneState.forcePullExecuteRemaining = cloneState.forcePullExecuteCharges;
    cloneState.onSkillTeamHealPending = false;
    cloneState.isSummonedClone = true;

    if (cloneState.shieldPctMaxHP > 0 && cloneMaxHP > 0)
    {
        cloneState.shield = cloneMaxHP * cloneState.shieldPctMaxHP / 100;
    }
    if (cloneState.damageImmunityAfterFrames > 0)
    {
        cloneState.damageImmunityTimer = cloneState.damageImmunityAfterFrames;
    }
    if (cloneState.autoUltimateAfterFrames > 0)
    {
        cloneState.autoUltimateTimer = cloneState.autoUltimateAfterFrames;
    }
    cloneState.blockFirstHitsRemaining = cloneState.blockFirstHitsCount;

    return cloneState;
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

KysChess::Battle::BattlePresentationColor makePresentationColor(Color color)
{
    return { color.r, color.g, color.b, color.a };
}

Color poisonDamageTextColor()
{
    return { 0, 200, 0, 255 };
}

Color bleedDamageTextColor()
{
    return { 190, 120, 60, 255 };
}

int calcProjectileFrames(int select_distance, int spawn_offset)
{
    return (calcProjectileReach(select_distance, spawn_offset) - spawn_offset) / PROJECTILE_SPEED;
}

bool isSummonedCloneRole(const Role* r)
{
    if (!r)
    {
        return false;
    }
    auto& cs = KysChess::ChessCombo::getActiveStates();
    auto it = cs.find(r->ID);
    return it != cs.end() && it->second.isSummonedClone;
}

int getComboLookupId(const Role* r)
{
    if (!r)
    {
        return -1;
    }
    if (isSummonedCloneRole(r))
    {
        return -1;
    }
    return r->RealID >= 0 ? r->RealID : r->ID;
}

std::string formatStatusFrames(const char* label, int frames)
{
    if (frames <= 0)
    {
        return label;
    }
    return std::format("{}（{}幀）", label, frames);
}

std::string formatStatusPercent(const char* label, int pct)
{
    if (pct <= 0)
    {
        return label;
    }
    return std::format("{}（{}%）", label, pct);
}

std::string formatStatusPercentFrames(const char* label, int pct, int frames)
{
    if (pct > 0 && frames > 0)
    {
        return std::format("{}（{}%·{}幀）", label, pct, frames);
    }
    if (pct > 0)
    {
        return formatStatusPercent(label, pct);
    }
    return formatStatusFrames(label, frames);
}

std::string formatStackingEffectStatus(const char* label, int pctPerStack, int stacks)
{
    if (pctPerStack <= 0 || stacks <= 0)
    {
        return label;
    }

    return std::format("{} +{}%（{}層）", label, pctPerStack * stacks, stacks);
}

std::string formatStatusValue(const char* label, int value, const char* unit)
{
    if (value <= 0)
    {
        return label;
    }
    return std::format("{}（{}{}）", label, value, unit);
}

std::string formatStatusRange(const char* label, int current, int maxValue, const char* unit)
{
    if (current <= 0 || maxValue <= 0)
    {
        return label;
    }
    return std::format("{}（{}/{}{}）", label, current, maxValue, unit);
}

std::string formatCooldownIncreaseStatus(int pct, int before, int after)
{
    if (pct <= 0)
    {
        return "冷卻延長";
    }
    if (before > 0 && after > 0)
    {
        return std::format("冷卻延長（+{}%，{}→{}幀）", pct, before, after);
    }
    return std::format("冷卻延長（+{}%）", pct);
}

std::string formatExecuteStatus(int thresholdPct)
{
    if (thresholdPct <= 0)
    {
        return "觸發處決";
    }
    return std::format("觸發處決（斬殺線{}%）", thresholdPct);
}

double calcBlinkReach(const Magic* magic)
{
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
        return std::min(MAX_EFFECTIVE_BATTLE_REACH, static_cast<double>(calcProjectileReach(magic->SelectDistance, BATTLE_TILE_W * 2) - 10));
    }
    return std::max(BATTLE_TILE_W * 3.0, static_cast<double>(magic->SelectDistance * BATTLE_TILE_W));
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

bool isRangedStyleMagic(const Magic* magic, bool forceRanged)
{
    return magic
        && (magic->AttackAreaType == 1
            || magic->AttackAreaType == 2
            || magic->AttackAreaType == 3
            || isForcedRangedMagic(magic, forceRanged));
}

double effectiveBattleReach(const Magic* magic,
                            bool forceRanged,
                            int forcedRangedMinSelectDistance,
                            int projectileSpeedMultiplierPct)
{
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
        int selectDistance = effectiveProjectileSelectDistance(magic, isForcedRangedMagic(magic, forceRanged), forcedRangedMinSelectDistance);
        int projectileFrames = calcProjectileFrames(selectDistance, BATTLE_TILE_W * 2);
        double projectileReach = BATTLE_TILE_W * 2
            + projectileFrames * PROJECTILE_SPEED * projectileSpeedMultiplierPct / 100.0;
        return std::max(BATTLE_TILE_W * 2.0, projectileReach - RANGED_ATTACK_SAFETY_MARGIN);
    }
    return MELEE_ATTACK_REACH;
}

bool hasMPBlock(const Role* r)
{
    auto& cs = KysChess::ChessCombo::getActiveStates();
    auto it = cs.find(r->ID);
    return it != cs.end() && it->second.mpBlockTimer > 0;
}

bool hasScriptedImpact(const KysChess::Battle::BattleAttackEvent& event)
{
    return event.scriptedDamage > 0 || event.scriptedStunFrames > 0 || event.scriptedBleedStacks > 0;
}

Role* findOptionalRoleByBattleId(const std::vector<Role*>& roles, int unitId)
{
    if (unitId < 0)
    {
        return nullptr;
    }
    auto it = std::find_if(roles.begin(), roles.end(), [&](Role* role)
        {
            return role && role->ID == unitId;
    });
    return it != roles.end() ? *it : nullptr;
}

void changeRoleMP(Role* r, double delta)
{
    if (!r)
    {
        return;
    }
    if (delta > 0 && hasMPBlock(r))
    {
        return;
    }

    if (delta > 0)
    {
        auto& cs = KysChess::ChessCombo::getActiveStates();
        auto it = cs.find(r->ID);
        if (it != cs.end() && it->second.mpRecoveryBonusPct > 0)
        {
            delta *= (1.0 + it->second.mpRecoveryBonusPct / 100.0);
        }
    }

    r->MP += delta;
}

void setCoolDown(Role* r, int cd)
{
    r->CoolDown = cd;
    r->CoolDownMax = cd;
}

KysChess::Battle::BattleStatusUnitState makeBattleStatusUnit(Role* role, const KysChess::RoleComboState& state)
{
    KysChess::Battle::BattleStatusUnitState unit;
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

void writeBattleStatusUnit(Role* role, KysChess::RoleComboState& state, const KysChess::Battle::BattleStatusUnitState& unit)
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

KysChess::Battle::BattleDamageUnitState makeBattleDamageUnit(Role* role, const KysChess::RoleComboState* state)
{
    KysChess::Battle::BattleDamageUnitState unit;
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

void writeBattleDamageUnit(Role* role, KysChess::RoleComboState* state, const KysChess::Battle::BattleDamageUnitState& unit)
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

KysChess::Battle::BattleTeamEffectWorld makeBattleTeamEffectWorld(
    const std::vector<Role*>& roles,
    const std::map<int, KysChess::RoleComboState>& states)
{
    KysChess::Battle::BattleTeamEffectWorld world;
    for (auto role : roles)
    {
        assert(role);
        auto stateIt = states.find(role->ID);
        assert(stateIt != states.end());

        KysChess::Battle::BattleTeamEffectUnit unit;
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

const KysChess::Battle::BattleTeamEffectUnit& findBattleTeamEffectUnit(
    const KysChess::Battle::BattleTeamEffectWorld& world,
    int unitId)
{
    auto it = std::find_if(world.units.begin(), world.units.end(), [&](const KysChess::Battle::BattleTeamEffectUnit& unit)
        {
            return unit.id == unitId;
        });
    assert(it != world.units.end());
    return *it;
}

void writeBattleTeamEffectWorld(const KysChess::Battle::BattleTeamEffectWorld& world,
                                const std::vector<Role*>& roles,
                                std::map<int, KysChess::RoleComboState>& states)
{
    for (auto role : roles)
    {
        assert(role);
        const auto& unit = findBattleTeamEffectUnit(world, role->ID);
        role->HP = unit.hp;
        role->MP = unit.mp;
        role->CoolDown = unit.cooldown;
        auto stateIt = states.find(role->ID);
        assert(stateIt != states.end());
        stateIt->second.shield = unit.shield;
    }
}

KysChess::Battle::BattleDeathEffectWorld makeBattleDeathEffectWorld(
    const std::vector<Role*>& roles,
    const std::map<int, KysChess::RoleComboState>& states)
{
    KysChess::Battle::BattleDeathEffectWorld world;
    const auto& allCombos = KysChess::ChessCombo::getAllCombos();
    for (int comboId = 0; comboId < static_cast<int>(allCombos.size()); ++comboId)
    {
        if (!allCombos[comboId].isAntiCombo)
        {
            world.regularSynergyComboIds.insert(comboId);
        }
    }

    for (auto role : roles)
    {
        assert(role);
        auto stateIt = states.find(role->ID);
        assert(stateIt != states.end());

        KysChess::Battle::BattleDeathEffectUnit unit;
        unit.id = role->ID;
        unit.team = role->Team;
        unit.alive = role->Dead == 0;
        unit.hp = role->HP;
        unit.maxHp = role->MaxHP;
        unit.attack = role->Attack;
        unit.defence = role->Defence;
        unit.shield = stateIt->second.shield;
        unit.shieldPctMaxHp = stateIt->second.shieldPctMaxHP;
        unit.shieldOnAllyDeathTracker = stateIt->second.shieldOnAllyDeathTracker;
        unit.appliedEffects = stateIt->second.appliedEffects;

        int comboLookupId = getComboLookupId(role);
        if (comboLookupId >= 0)
        {
            unit.comboIds = KysChess::ChessCombo::getCombosForRole(comboLookupId);
        }
        world.units.push_back(unit);
    }
    return world;
}

void writeBattleDeathEffectTrackers(const KysChess::Battle::BattleDeathEffectWorld& world,
                                    const std::vector<Role*>& roles,
                                    std::map<int, KysChess::RoleComboState>& states)
{
    for (auto role : roles)
    {
        assert(role);
        auto unitIt = std::find_if(world.units.begin(), world.units.end(), [&](const KysChess::Battle::BattleDeathEffectUnit& unit)
            {
                return unit.id == role->ID;
            });
        assert(unitIt != world.units.end());

        auto stateIt = states.find(role->ID);
        assert(stateIt != states.end());
        stateIt->second.shieldOnAllyDeathTracker = unitIt->shieldOnAllyDeathTracker;
    }
}

KysChess::Battle::BattleProjectileTargetWorld makeBattleProjectileTargetWorld(const std::vector<Role*>& roles)
{
    KysChess::Battle::BattleProjectileTargetWorld world;
    for (auto role : roles)
    {
        assert(role);
        auto grid = battlePos90To45(role->Pos.x, role->Pos.y);
        KysChess::Battle::BattleProjectileTargetUnit unit;
        unit.id = role->ID;
        unit.team = role->Team;
        unit.alive = role->Dead == 0;
        unit.hp = role->HP;
        unit.maxHp = role->MaxHP;
        unit.defense = role->Defence;
        unit.invincible = role->Invincible;
        unit.x = role->Pos.x;
        unit.y = role->Pos.y;
        unit.gridX = grid.x;
        unit.gridY = grid.y;
        world.units.push_back(unit);
    }
    return world;
}

KysChess::Battle::BattleCooldownState makeBattleCooldownState(Role* role)
{
    KysChess::Battle::BattleCooldownState state;
    if (!role)
    {
        state.alive = false;
        return state;
    }
    state.alive = role->Dead == 0;
    state.cooldown = role->CoolDown;
    state.cooldownMax = role->CoolDownMax;
    state.haveAction = role->HaveAction;
    state.operationType = KysChess::Battle::battleOperationFromLegacy(role->OperationType);
    state.actType = role->ActType;
    return state;
}

void writeBattleCooldownState(Role* role, const KysChess::Battle::BattleCooldownState& state)
{
    if (role)
    {
        role->CoolDown = state.cooldown;
    }
}

KysChess::Battle::BattleDamageModifierState makeBattleDamageModifier(const KysChess::RoleComboState* state)
{
    return KysChess::Battle::makeBattleDamageModifierState(state);
}

}    // namespace

void BattleSceneHades::applyTempAttackBuff(Role* role,
    KysChess::RoleComboState& state,
    int attackBonus,
    int durationFrames,
    const std::string& reason)
{
    if (!role || attackBonus == 0 || durationFrames <= 0)
    {
        return;
    }

    role->Attack += attackBonus;
    state.tempAttackBuffs.push_back({ attackBonus, durationFrames });
    if (!reason.empty())
    {
        logBattleStatus(role, role, reason);
    }
}

Magic* BattleSceneHades::triggerAutoUltimate(Role* role, bool consumeMP)
{
    if (!role || role->Dead != 0)
    {
        return nullptr;
    }

    Magic* magic = selectMagic(role, std::greater<double>{ });
    if (!magic)
    {
        return nullptr;
    }

    queueCoreSkillAttackSpawn(role, magic, true);
    if (consumeMP)
    {
        changeRoleMP(role, -role->MaxMP);
    }
    return magic;
}

bool BattleSceneHades::roleForcesRangedMagic(Role* role) const
{
    if (!role)
    {
        return false;
    }
    auto& states = KysChess::ChessCombo::getActiveStates();
    auto it = states.find(role->ID);
    return it != states.end() && it->second.forceRangedAttack;
}

int BattleSceneHades::getForcedRangedMinSelectDistance(Role* role) const
{
    if (!role)
    {
        return DEFAULT_FORCED_RANGED_MIN_SELECT_DISTANCE;
    }

    auto& states = KysChess::ChessCombo::getActiveStates();
    auto it = states.find(role->ID);
    if (it == states.end() || it->second.forceRangedMinSelectDistance <= 0)
    {
        return DEFAULT_FORCED_RANGED_MIN_SELECT_DISTANCE;
    }
    return std::max(1, it->second.forceRangedMinSelectDistance);
}

int BattleSceneHades::getProjectileSpeedMultiplierPct(Role* role) const
{
    if (!role)
    {
        return 100;
    }
    auto& states = KysChess::ChessCombo::getActiveStates();
    auto it = states.find(role->ID);
    if (it == states.end())
    {
        return 100;
    }
    return std::max(100, it->second.projectileSpeedMultiplierPct);
}

// Apply freeze frames to a role, accounting for low-HP immunity and freeze shield
void applyFrozen(Role* r, int frames)
{
    if (!r || frames <= 0)
    {
        return;
    }
    // Low HP immunity: below 25% HP
    if (r->MaxHP > 0 && r->HP < r->MaxHP * 0.25)
    {
        return;
    }

    auto& cs = KysChess::ChessCombo::getMutableStates();
    auto it = cs.find(r->ID);

    int totalFreezeRes = 0;
    if (it != cs.end())
    {
        totalFreezeRes = it->second.freezeReductionPct;
        if (it->second.shield > 0)
        {
            totalFreezeRes += it->second.shieldFreezeResPct;
        }
    }
    totalFreezeRes = std::clamp(totalFreezeRes, 0, 100);
    if (totalFreezeRes > 0)
    {
        frames = (frames * (100 - totalFreezeRes)) / 100;
    }

    if (it != cs.end() && it->second.controlImmunityFrames > 0)
    {
        int absorbed = std::min(frames, it->second.controlImmunityFrames);
        it->second.controlImmunityFrames -= absorbed;
        frames -= absorbed;
    }

    if (frames > 0)
    {
        r->Frozen += frames;
        r->FrozenMax = r->Frozen;    // Reset max when frozen is applied
    }
}

int BattleSceneHades::getOperationType(int attackAreaType)
{
    return KysChess::Battle::toLegacyOperationType(
        KysChess::Battle::BattleCombatIntentPlanner().operationTypeForAttackArea(attackAreaType));
}

const char* BattleSceneHades::getOperationTypeName(int operationType)
{
    if (operationType == 0)
    {
        return "輕擊";
    }
    if (operationType == 1)
    {
        return "重擊";
    }
    if (operationType == 2)
    {
        return "遠程";
    }
    return "未知";
}

BattleSceneHades::BattleSceneHades(KysChess::ChessRoleSave& roleSave, KysChess::ChessProgress& progress, KysChess::ChessManager& chessManager) : roleSave_(roleSave), progress_(progress), chessManager_(chessManager)
{
    full_window_ = 1;
    COORD_COUNT = BATTLEMAP_COORD_COUNT;

    earth_layer_.resize(COORD_COUNT);
    building_layer_.resize(COORD_COUNT);

    // heads_.resize(1);
    // int i = 0;
    // for (auto& h : heads_)
    // {
    //     h = std::make_shared<Head>();
    //     h->setAlwaysLight(1);
    //     addChild(h, 10, 10 + (i++) * 80);
    //     h->setVisible(false);
    // }
    // heads_[0]->setVisible(true);
    // heads_[0]->setRole(Save::getInstance()->getRole(0));

    head_boss_.resize(6);
    for (auto& h : head_boss_)
    {
        h = std::make_shared<Head>();
        h->setStyle(1);
        h->setVisible(false);
        addChild(h);
    }
    makeSpecialMagicEffect();
}

BattleSceneHades::BattleSceneHades(int id, KysChess::ChessRoleSave& roleSave, KysChess::ChessProgress& progress, KysChess::ChessManager& chessManager) :
    BattleSceneHades(roleSave, progress, chessManager)
{
    setID(id);
}

BattleSceneHades::~BattleSceneHades()
{
}

bool BattleSceneHades::isManualCameraEnabled() const
{
    return SystemSettings::getInstance()->data().manualCamera;
}

void BattleSceneHades::handleManualCameraInput(const EngineEvent& e)
{
    if (!isManualCameraEnabled())
    {
        manual_camera_dragging_ = false;
        return;
    }

    auto* engine = Engine::getInstance();
    switch (e.type)
    {
    case EVENT_MOUSE_BUTTON_DOWN:
        if (e.button.button == BUTTON_LEFT)
        {
            int present_x = 0;
            int present_y = 0;
            int present_w = 0;
            int present_h = 0;
            engine->getPresentRect(present_x, present_y, present_w, present_h);
            manual_camera_dragging_ = present_w > 0
                && present_h > 0
                && e.button.x >= present_x
                && e.button.x < present_x + present_w
                && e.button.y >= present_y
                && e.button.y < present_y + present_h;
        }
        break;
    case EVENT_MOUSE_BUTTON_UP:
        if (e.button.button == BUTTON_LEFT)
        {
            manual_camera_dragging_ = false;
        }
        break;
    case EVENT_MOUSE_MOTION:
        if (manual_camera_dragging_)
        {
            int present_x = 0;
            int present_y = 0;
            int present_w = 0;
            int present_h = 0;
            engine->getPresentRect(present_x, present_y, present_w, present_h);
            if (present_w > 0 && present_h > 0)
            {
                double world_delta_x = static_cast<double>(e.motion.xrel) * (render_center_x_ * 2.0 / present_w);
                double world_delta_y = static_cast<double>(e.motion.yrel) * (render_center_y_ * 2.0 / present_h);
                pos_.x -= static_cast<float>(world_delta_x);
                pos_.y -= static_cast<float>(world_delta_y * 2.0);
                clampCameraCenter();
            }
        }
        break;
    }
}

void BattleSceneHades::updateAutoCamera()
{
    Pointf desired = camera_target_;
    bool haveDesired = false;

    if (close_up_ <= 0)
    {
        Pointf center{ 0, 0, 0 };
        int count = 0;
        for (auto r : battle_roles_)
        {
            if (r && r->Team == 0 && r->Dead == 0)
            {
                center += r->Pos;
                count++;
            }
        }

        if (count > 0)
        {
            center.x /= count;
            center.y /= count;
            center.z /= count;
            desired = center;
            haveDesired = true;
        }
    }
    else
    {
        haveDesired = true;
    }

    if (haveDesired)
    {
        camera_target_ = desired;
    }

    float lerp = close_up_ > 0 ? CAMERA_DEATH_FOCUS_LERP : CAMERA_AUTO_FOLLOW_LERP;
    pos_ = lerpPoint(pos_, camera_target_, lerp);
    clampCameraCenter();
}

void BattleSceneHades::focusCameraOn(const Pointf& focusPoint, int zoomFrames)
{
    if (zoomFrames <= 0)
    {
        return;
    }

    if (close_up_ <= 0)
    {
        camera_target_ = focusPoint;
    }
    else
    {
        camera_target_ = lerpPoint(camera_target_, focusPoint, CAMERA_DEATH_RETARGET_BLEND);
    }

    close_up_ = std::max(close_up_, zoomFrames);
    close_up_total_ = std::max(close_up_total_, close_up_);
}

void BattleSceneHades::clampCameraCenter()
{
    float scene_w = static_cast<float>(COORD_COUNT * TILE_W * 2);
    float scene_h = static_cast<float>(COORD_COUNT * TILE_H * 2);
    if (scene_w <= 0.0f || scene_h <= 0.0f)
    {
        return;
    }

    float min_x = static_cast<float>(render_center_x_);
    float max_x = scene_w - static_cast<float>(render_center_x_);
    if (max_x <= min_x)
    {
        pos_.x = scene_w * 0.5f;
    }
    else
    {
        pos_.x = (std::max)(min_x, (std::min)(pos_.x, max_x));
    }

    float center_y = pos_.y * 0.5f;
    float min_y = static_cast<float>(render_center_y_);
    float max_y = scene_h - static_cast<float>(render_center_y_);
    if (max_y <= min_y)
    {
        center_y = scene_h * 0.5f;
    }
    else
    {
        center_y = (std::max)(min_y, (std::min)(center_y, max_y));
    }
    pos_.y = center_y * 2.0f;
}

Color BattleSceneHades::calculateHurtFlashColor(const Role* r, const Color& base_color) const
{
    auto it = hurt_flash_timers_.find(r->ID);
    if (it == hurt_flash_timers_.end())
    {
        return base_color;
    }

    int timer = it->second;
    int phase = timer % HURT_FLASH_PERIOD;

    if (phase < 2)
    {
        return { 255, 100, 100, base_color.a };
    }
    return base_color;
}

KysChess::Battle::BattlePresentationSnapshot BattleSceneHades::makePresentationSnapshot() const
{
    KysChess::Battle::BattlePresentationSnapshot snapshot;
    snapshot.frame = battle_frame_;
    for (auto role : battle_roles_)
    {
        if (!role)
        {
            continue;
        }

        snapshot.units.push_back({
            role->ID,
            role->RealID,
            role->Name,
            role->Team,
            role->Dead == 0,
            role->HP,
            role->MaxHP,
            role->MP,
            GameUtil::MAX_MP,
            role->CoolDown,
            role->Invincible,
            role->Pos,
            role->Velocity,
        });
    }
    return snapshot;
}

void BattleSceneHades::beginPresentationFrame()
{
    presentation_recorder_.beginFrame(makePresentationSnapshot());
}

void BattleSceneHades::publishPresentationFrame()
{
    last_presentation_frame_ = presentation_recorder_.consumeFrame();
    presentation_player_.play(last_presentation_frame_, {
        &tracker_,
        &text_effects_,
        &attack_effects_,
        [this](int unitId) -> Role*
        {
            return findRoleByBattleId(battle_roles_, unitId);
        },
    });
}

void BattleSceneHades::addFloatingText(Role* role, const std::string& text, Color color, int size, int type)
{
    presentation_recorder_.recordPresentation({
        KysChess::Battle::BattlePresentationEventType::FloatingText,
        battle_frame_,
        -1,
        role ? role->ID : -1,
        0,
        0,
        -1,
        size,
        type,
        text,
        "",
        "",
        makePresentationColor(color),
        role ? role->Pos : Pointf{},
    });
}

void BattleSceneHades::addRoleEffect(Role* role, int eftId, int totalFrames)
{
    assert(role);

    presentation_recorder_.recordPresentation({
        KysChess::Battle::BattlePresentationEventType::RoleEffect,
        battle_frame_,
        -1,
        role->ID,
        0,
        totalFrames,
        eftId,
    });
}

void BattleSceneHades::addDamageNumber(Role* role, int damage, Color color, int baseSize)
{
    assert(role);
    assert(damage > 0);

    presentation_recorder_.recordPresentation({
        KysChess::Battle::BattlePresentationEventType::DamageNumber,
        battle_frame_,
        -1,
        role->ID,
        damage,
        0,
        -1,
        baseSize,
        0,
        "",
        "",
        "",
        makePresentationColor(color),
        role->Pos,
    });
}

void BattleSceneHades::logBattleDamage(Role* source, Role* target, int amount, const std::string& skillName, const std::string& detailText)
{
    presentation_recorder_.recordPresentation({
        KysChess::Battle::BattlePresentationEventType::DamageLog,
        battle_frame_,
        source ? source->ID : -1,
        target ? target->ID : -1,
        amount,
        0,
        -1,
        0,
        0,
        "",
        skillName,
        detailText,
    });
}

void BattleSceneHades::logBattleHeal(Role* source, Role* target, int amount, const std::string& reason)
{
    presentation_recorder_.recordPresentation({
        KysChess::Battle::BattlePresentationEventType::HealLog,
        battle_frame_,
        source ? source->ID : -1,
        target ? target->ID : -1,
        amount,
        0,
        -1,
        0,
        0,
        reason,
    });
}

void BattleSceneHades::logBattleStatus(Role* source, Role* target, const std::string& text)
{
    presentation_recorder_.recordPresentation({
        KysChess::Battle::BattlePresentationEventType::StatusLog,
        battle_frame_,
        source ? source->ID : -1,
        target ? target->ID : -1,
        0,
        0,
        -1,
        0,
        0,
        text,
    });
}

void BattleSceneHades::attachProjectileBouncePrime(KysChess::Battle::BattleAttackSpawnRequest& request)
{
    auto& attack = request.initial;
    assert(attack.attackerUnitId >= 0);

    auto& states = KysChess::ChessCombo::getActiveStates();
    auto stateIt = states.find(attack.attackerUnitId);
    if (stateIt == states.end())
    {
        return;
    }
    auto prime = KysChess::Battle::BattleComboTriggerSystem().collectProjectileBouncePrime(
        stateIt->second,
        {
            attack.attackerUnitId,
            static_cast<int>(rand_.rand() * 100),
            static_cast<int>(PROJECTILE_BOUNCE_RANGE),
        });
    if (prime.count <= 0)
    {
        return;
    }

    KysChess::Battle::tryApplyProjectileBouncePrime(request,
        {
            prime.count,
            prime.chancePct,
            prime.rollPct,
            prime.range,
        });
}

bool BattleSceneHades::attackCanHitInvincible(Role* role) const
{
    assert(role);
    auto& cs = KysChess::ChessCombo::getActiveStates();
    auto it = cs.find(role->ID);
    if (it == cs.end())
    {
        return false;
    }

    return KysChess::Battle::BattleComboTriggerSystem().hasExecuteCombo(
        it->second,
        role->ID);
}

void BattleSceneHades::queueCoreAttackSpawn(KysChess::Battle::BattleAttackSpawnRequest request)
{
    assert(request.initial.attackerUnitId >= 0);
    auto* attacker = findRoleByBattleId(battle_roles_, request.initial.attackerUnitId);
    request.initial.executeCanHitInvincible = attackCanHitInvincible(attacker);
    pending_core_attack_spawns_.push_back(std::move(request));
}

void BattleSceneHades::draw()
{
    constexpr int kCullMargin = 96;

    auto* engine = Engine::getInstance();
    int w = render_center_x_ * 2;
    int h = render_center_y_ * 2;

    //在这个模式下，使用的是直角坐标
    engine->setRenderTarget("scene");
    engine->fillColor({ 0, 0, 0, 255 }, 0, 0, w, h);

    //以下是计算出需要画的区域，先画到一个大图上，再转贴到窗口
    {
        auto p = pos90To45(pos_.x, pos_.y);
        man_x_ = p.x;
        man_y_ = p.y;
    }

    Rect rect0 = { int(pos_.x - render_center_x_ - x_), int(pos_.y / 2 - render_center_y_ - y_), w, h };
    Rect rect1 = { 0, 0, w, h };
    if (rect0.x < 0)
    {
        rect1.x = -rect0.x;
        rect0.x = 0;
        rect0.w = w - rect1.x;
    }
    if (rect0.y < 0)
    {
        rect1.y = -rect0.y;
        rect0.y = 0;
        rect0.h = h - rect1.y;
    }
    rect0.w = std::min(rect0.w, COORD_COUNT * TILE_W * 2 - rect0.x);
    rect0.h = std::min(rect0.h, COORD_COUNT * TILE_H * 2 - rect0.y);

    auto whole_scene = engine->getTexture("whole_scene");
    bool use_whole_scene = use_whole_scene_ && whole_scene != nullptr;
    float zoomBlend = use_whole_scene ? cameraZoomBlend(close_up_, close_up_total_) : 0.0f;
    if (zoomBlend > 0.0f)
    {
        float zoomScale = 1.0f + (CAMERA_DEATH_ZOOM_SCALE - 1.0f) * zoomBlend;
        int zoomedW = std::max(1, int(rect0.w * zoomScale + 0.5f));
        int zoomedH = std::max(1, int(rect0.h * zoomScale + 0.5f));
        rect0.x += (rect0.w - zoomedW) / 2;
        rect0.y += (rect0.h - zoomedH) / 2 - int(20.0f * zoomBlend);
        rect0.w = zoomedW;
        rect0.h = zoomedH;
        rect0.x = std::max(0, std::min(rect0.x, COORD_COUNT * TILE_W * 2 - rect0.w));
        rect0.y = std::max(0, std::min(rect0.y, COORD_COUNT * TILE_H * 2 - rect0.h));
    }

    Rect clear_rect = {
        rect0.x - kCullMargin,
        rect0.y - kCullMargin,
        rect0.w + kCullMargin * 2,
        rect0.h + kCullMargin * 2
    };
    clear_rect.x = std::max(0, clear_rect.x);
    clear_rect.y = std::max(0, clear_rect.y);
    clear_rect.w = std::min(clear_rect.w, COORD_COUNT * TILE_W * 2 - clear_rect.x);
    clear_rect.h = std::min(clear_rect.h, COORD_COUNT * TILE_H * 2 - clear_rect.y);

    if (use_whole_scene)
    {
        engine->setRenderTarget(whole_scene);
        if (clear_rect.w > 0 && clear_rect.h > 0)
        {
            engine->fillColor({ 0, 0, 0, 255 }, clear_rect.x, clear_rect.y, clear_rect.w, clear_rect.h);
        }
    }
    else
    {
        engine->setRenderTarget("scene");
    }

    auto is_visible_world = [&](double world_x, double world_y) -> bool
    {
        return world_x >= clear_rect.x && world_x <= clear_rect.x + clear_rect.w
            && world_y >= clear_rect.y && world_y <= clear_rect.y + clear_rect.h;
    };
    float viewScaleX = (rect0.w > 0) ? float(rect1.w) / rect0.w : 1.0f;
    float viewScaleY = (rect0.h > 0) ? float(rect1.h) / rect0.h : 1.0f;
    auto renderWorldX = [&](double world_x) -> int
    {
        if (use_whole_scene)
        {
            return int(world_x);
        }
        return int(rect1.x + (world_x - rect0.x) * viewScaleX);
    };
    auto renderWorldY = [&](double world_y) -> int
    {
        if (use_whole_scene)
        {
            return int(world_y);
        }
        return int(rect1.y + (world_y - rect0.y) * viewScaleY);
    };

    for (int sum = -view_sum_region_; sum <= view_sum_region_ + 15; sum++)
    {
        for (int i = -view_width_region_; i <= view_width_region_; i++)
        {
            int ix = man_x_ + i + (sum / 2);
            int iy = man_y_ - i + (sum - sum / 2);
            auto p = pos45To90(ix, iy);
            if (!isOutLine(ix, iy))
            {
                int num = earth_layer_.data(ix, iy) / 2;
                Color color = { 255, 255, 255, 255 };
                bool need_draw = true;
                if (need_draw && num > 0)
                {
                    TextureManager::getInstance()->renderTexture("smap", num, renderWorldX(p.x), renderWorldY(p.y / 2.0), TextureManager::RenderInfo{ color });
                }
            }
        }
    }

    auto earth_tex = Engine::getInstance()->getTexture("earth");
    if (earth_tex && use_whole_scene)
    {
        //此画法节省资源
        Color c = { 255, 255, 255, 255 };
        engine->setColor(earth_tex, c);
        Rect earth_src = rect0;
        Rect earth_dst = rect0;
        //注意这种画法，地面最上面会缺一块
        earth_src.y += TILE_H * 2;
        if (earth_src.x < 0)
        {
            earth_dst.x -= earth_src.x;
            earth_src.x = 0;
        }
        if (earth_src.y < 0)
        {
            earth_dst.y -= earth_src.y;
            earth_src.y = 0;
        }
        if (earth_src.x + earth_src.w > COORD_COUNT * TILE_W * 2)
        {
            earth_dst.w -= (earth_src.x + earth_src.w - COORD_COUNT * TILE_W * 2);
            earth_src.w = COORD_COUNT * TILE_W * 2 - earth_src.x;
        }
        if (earth_src.y + earth_src.h > COORD_COUNT * TILE_H * 2)
        {
            earth_dst.h -= (earth_src.y + earth_src.h - COORD_COUNT * TILE_H * 2);
            earth_src.h = COORD_COUNT * TILE_H * 2 - earth_src.y;
        }
        std::vector<Color> color_v(4, { 255, 255, 255, 255 });
        engine->renderTextureLight(earth_tex, &earth_src, &earth_dst, color_v, { 0.25f, 0.0f, 0.0f, 0.0f });
    }

    struct DrawInfo
    {
        TextureWarpper* tex = nullptr;
        Pointf p;
        Pointf sort_p;
        Color color{ 255, 255, 255, 255 };
        uint8_t alpha = 255;
        int rot = 0;
        int shadow = 0;
        uint8_t white = 0;
    };

    std::vector<DrawInfo> draw_infos;
    draw_infos.reserve(10000);

    for (int sum = -view_sum_region_; sum <= view_sum_region_ + 15; sum++)
    {
        for (int i = -view_width_region_; i <= view_width_region_; i++)
        {
            int ix = man_x_ + i + (sum / 2);
            int iy = man_y_ - i + (sum - sum / 2);
            auto p = pos45To90(ix, iy);
            if (!isOutLine(ix, iy))
            {
                int num = building_layer_.data(ix, iy) / 2;
                if (num > 0)
                {
                    // TODO: make this legacy only
                    // TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y/2);
                    DrawInfo info;
                    info.tex = TextureManager::getInstance()->getTexture("smap", num);
                    if (!info.tex)
                    {
                        continue;
                    }
                    info.p.x = float(p.x);
                    info.p.y = float(p.y);
                    info.shadow = 0;
                    draw_infos.emplace_back(std::move(info));
                }
            }
        }
    }
    for (auto r : battle_roles_)
    {
        if (!is_visible_world(r->Pos.x, r->Pos.y / 2.0))
        {
            continue;
        }
        //if (r->Dead) { continue; }
        DrawInfo info;
        auto path = std::format("fight/fight{:03}", r->HeadID);
        info.color = { 255, 255, 255, 255 };
        info.alpha = 255;
        info.white = 0;
        if (battle_cursor_->isRunning() && !acting_role_->isAuto())
        {
            info.color = { 128, 128, 128, 255 };
            if (inEffect(acting_role_, r))
            {
                info.color = { 255, 255, 255, 255 };
            }
        }
        info.p = r->Pos;
        if (result_ == -1 && r->Shake)
        {
            info.p.x += shakeJitter(battle_frame_, r->ID);
        }
        r->FaceTowards = realTowardsToFaceTowards(r->RealTowards);
        info.tex = TextureManager::getInstance()->getTexture(path, calRolePic(r, r->ActType, r->ActFrame));
        if (!info.tex)
        {
            continue;
        }
        //if (r->HurtFrame)
        //{
        //    if (r->HurtFrame % 2 == 1)
        //    {
        //        info.white = 128;
        //    }

        //}
        if (r->Dead)
        {
            //if (r->Frozen == 0)
            {
                if (r->FaceTowards >= 2)
                {
                    info.rot = 90;
                }
                else
                {
                    info.rot = 270;
                }
            }
        }
        if (r->Attention)
        {
            info.alpha = 255 - r->Attention * 4;
        }

        // 應用受击闪红
        info.color = calculateHurtFlashColor(r, info.color);

        info.shadow = 1;
        //TextureManager::getInstance()->renderTexture(path, pic, r->X1, r->Y1, color, alpha);
        draw_infos.emplace_back(std::move(info));
    }

    //effects
    for (auto& ae : attack_effects_)
    {
        //for (auto r : ae.Defender)
        {
            auto effect_pos = ae.FollowRole ? ae.FollowRole->Pos + ae.Pos : ae.Pos;
            if (!is_visible_world(effect_pos.x, effect_pos.y / 2.0))
            {
                continue;
            }
            DrawInfo info;
            int frame = 0;
            if (ae.TotalEffectFrame > 0)
            {
                frame = ae.Frame % ae.TotalEffectFrame;
            }
            info.tex = TextureManager::getInstance()->getTexture(ae.Path, frame);
            if (!info.tex)
            {
                continue;
            }
            info.p = effect_pos;
            info.sort_p = effect_pos;
            info.sort_p.y += 10000;    // force projectiles to render on top
            info.color = { 255, 255, 255, 255 };
            info.alpha = ae.FollowRole ? 255 : 192;
            info.shadow = ae.FollowRole ? 0 : 1;
            if (!ae.FollowRole && ae.renderTeam() == 0)
            {
                info.shadow = 2;
            }
            if (!ae.FollowRole)
            {
                info.alpha = 255 * (ae.TotalFrame * 1.25 - ae.Frame) / (ae.TotalFrame * 1.25);    //越来越透明
            }
            draw_infos.emplace_back(std::move(info));
            //TextureManager::getInstance()->renderTexture(ae.Path, ae.Frame % ae.TotalEffectFrame, ae.X1, ae.Y1 / 2, { 255, 255, 255, 255 }, 192);
        }
    }

    auto sort_building = [](DrawInfo& d1, DrawInfo& d2)
    {
        auto& p1 = d1.sort_p.y != 0 ? d1.sort_p : d1.p;
        auto& p2 = d2.sort_p.y != 0 ? d2.sort_p : d2.p;
        if (p1.y != p2.y)
        {
            return p1.y < p2.y;
        }
        else
        {
            return p1.x < p2.x;
        }
    };
    std::sort(draw_infos.begin(), draw_infos.end(), sort_building);

    //影子
    for (auto& d : draw_infos)
    {
        if (d.shadow)
        {
            d.tex->load();
            if (d.tex->getTexture())
            {
                double scalex = 1, scaley = 0.3;
                int yd = d.tex->dy * 0.7;
                if (d.rot)
                {
                    scalex = 0.3;
                    scaley = 1;
                    yd = d.tex->dy * 0.1;
                }
                TextureManager::getInstance()->renderTexture(d.tex, renderWorldX(d.p.x), renderWorldY(d.p.y / 2.0 + yd),
                    TextureManager::RenderInfo{ { 32, 32, 32, 255 }, uint8_t(d.alpha / 2), scalex, scaley, double(d.rot) });
                // if (d.shadow == 1)
                // {
                //     TextureManager::getInstance()->renderTexture(d.tex, d.p.x, d.p.y / 2 + yd, { 32, 32, 32, 255 }, d.alpha / 2, scalex, scaley, d.rot);
                // }
                // if (d.shadow == 2)
                // {
                //     TextureManager::getInstance()->renderTexture(d.tex, d.p.x, d.p.y / 2 + yd, { 128, 128, 128, 255 }, d.alpha / 2, scalex, scaley, d.rot, 128);
                // }
            }
        }
    }
    for (auto& d : draw_infos)
    {
        double scaley = 1;
        if (d.rot)
        {
            scaley = 0.5;
        }
        std::vector<Color> color_v;
        //color_v[0] = { 128, 128, 64, 255 };
        std::vector<float> brightness_v(4, 0);
        brightness_v[0] = 0.5;
        brightness_v[2] = 0;
        TextureManager::getInstance()->renderTexture(d.tex,
            renderWorldX(d.p.x),
            renderWorldY(d.p.y / 2.0 - d.p.z),
            TextureManager::RenderInfo{ d.color, d.alpha, scaley, 1, static_cast<double>(d.rot), d.white, color_v, {} });
    }

    for (auto r : battle_roles_)
    {
        if (is_visible_world(r->Pos.x, r->Pos.y / 2.0))
        {
            renderExtraRoleInfo(r, renderWorldX(r->Pos.x), renderWorldY(r->Pos.y / 2.0));
        }
    }

    if (swapSelected_ && is_visible_world(swapSelected_->Pos.x, swapSelected_->Pos.y / 2.0))
    {
        engine->fillColor(
            { 255, 255, 0, 80 },
            renderWorldX(swapSelected_->Pos.x - 15),
            renderWorldY(swapSelected_->Pos.y / 2.0 - 5),
            std::max(1, int(30 * viewScaleX)),
            std::max(1, int(10 * viewScaleY)));
    }

    if (use_whole_scene)
    {
        Color c = { 255, 255, 255, 255 };
        engine->setColor(whole_scene, c);
        engine->setRenderTarget("scene");
        engine->renderTexture(whole_scene, &rect0, &rect1, 0);
    }
    engine->renderTextureToMain("scene");

    // World→UI coordinate transform for deferred text rendering.
    // world coords live on the battle map; rect0→rect1 maps them to the scene texture (w x h).
    // The scene texture is then stretched to fill the main texture (ui_w x ui_h).
    int ui_w = engine->getUIWidth();
    int ui_h = engine->getUIHeight();
    float sceneToUiX = (w > 0) ? float(ui_w) / w : 1.0f;
    float sceneToUiY = (h > 0) ? float(ui_h) / h : 1.0f;
    auto worldToUiX = [&](double wx) -> int
    {
        return int((rect1.x + (wx - rect0.x) * viewScaleX) * sceneToUiX);
    };
    auto worldToUiY = [&](double wy) -> int
    {
        return int((rect1.y + (wy - rect0.y) * viewScaleY) * sceneToUiY);
    };
    float sizeScale = viewScaleX * sceneToUiX;

    // Text effects (damage numbers etc.) - deferred for crisp rendering
    for (auto& te : text_effects_)
    {
        int ux = worldToUiX(te.Pos.x);
        int uy = worldToUiY(te.Pos.y / 2.0);
        if (ux >= -80 && ux < ui_w + 80 && uy >= -80 && uy < ui_h + 80)
        {
            int usize = (std::max)(1, int(te.Size * sizeScale));
            Font::getInstance()->draw(te.Text, usize, ux, uy, te.color, te.color.a);
        }
    }

    // Role info text (frozen timer, swap coordinates)
    for (auto r : battle_roles_)
    {
        if (r == nullptr || r->Dead) { continue; }
        double wx = r->Pos.x;
        double wy = r->Pos.y / 2.0;
        int ux = worldToUiX(wx);
        int uy = worldToUiY(wy);
        if (ux < -80 || ux >= ui_w + 80 || uy < -80 || uy >= ui_h + 80) { continue; }
        if (r->Frozen > 0 && r->FrozenMax > 0)
        {
            Font::getInstance()->draw(std::to_string(r->Frozen),
                (std::max)(1, int(9 * sizeScale)),
                worldToUiX(wx - 5), worldToUiY(wy + ROLE_STATUS_BAR_FROZEN_Y - 1),
                { 255, 255, 255, 255 });
        }
        else if (r->CoolDown > 0 && r->CoolDownMax > 0)
        {
            Font::getInstance()->draw(std::to_string(r->CoolDown),
                (std::max)(1, int(9 * sizeScale)),
                worldToUiX(wx - 5), worldToUiY(wy + ROLE_STATUS_BAR_FROZEN_Y - 1),
                { 230, 195, 120, 240 });
        }
        if (positionSwapActive_ && r->Team == 0)
        {
            std::string coord = std::format("({},{})", r->X(), r->Y());
            Font::getInstance()->draw(coord,
                (std::max)(1, int(28 * sizeScale)),
                worldToUiX(wx - 5), worldToUiY(wy - 5),
                { 255, 255, 100, 255 });
        }
    }

    // Draw total frames elapsed on top left
    Font::getInstance()->draw(std::to_string(battle_frame_), 20, 10, 10, { 255, 255, 255, 255 }, 200);

    //if (result_ >= 0)
    //{
    //    Engine::getInstance()->fillColor({ 0, 0, 0, 128 }, 0, 0, -1, -1);
    //}
}

void BattleSceneHades::dealEvent(EngineEvent& e)
{
    handleManualCameraInput(e);

    int steps = getBattleStepsThisRender();
    for (int step = 0; step < steps && !exit_; ++step)
    {
        advanceBattleFrame();
    }
}

void BattleSceneHades::dealEvent2(EngineEvent& e)
{
}

int BattleSceneHades::getBattleStepsThisRender()
{
    return 1;
}

void BattleSceneHades::advanceBattleFrame()
{
    auto* engine = Engine::getInstance();
    if (shake_ > 0)
    {
        x_ = rand_.rand_int(3) - rand_.rand_int(3);
        y_ = rand_.rand_int(3) - rand_.rand_int(3);
        shake_--;
    }
    beginPresentationFrame();
    if (frozen_ > 0)
    {
        frozen_--;
        engine->gameControllerRumble(100, 100, 50);
        publishPresentationFrame();
        battle_frame_++;
        return;
    }
    decreaseToZero(close_up_);
    if (close_up_ == 0)
    {
        if (isManualCameraEnabled())
        {
            clampCameraCenter();
        }
        else
        {
            updateAutoCamera();
        }
    }

    backRun1();
    publishPresentationFrame();
    battle_frame_++;
}

void BattleSceneHades::onEntrance()
{
    previous_refresh_interval_ = RunNode::getRefreshInterval();
    RunNode::setRefreshInterval(previous_refresh_interval_ * battleSpeedToRefreshMultiplier(SystemSettings::getInstance()->data().battleSpeed));

    calViewRegion();

    //注意此时才能得到窗口的大小，用来设置头像的位置
    head_self_->setPosition(10, 10);
    int count = 0;
    for (auto& h : head_boss_)
    {
        h->setPosition(Engine::getInstance()->getUIWidth() / 2 - h->getWidth() / 2, Engine::getInstance()->getUIHeight() - 50 - (25 * count++));
    }
    addChild(Weather::getInstance());

    makeEarthTexture();    //注意高度稍微多了一点

    auto* engine = Engine::getInstance();
    int whole_scene_w = COORD_COUNT * TILE_W * 2;
    int whole_scene_h = COORD_COUNT * TILE_H * 2;
    int max_texture_size = engine->getMaxTextureSize();
    bool is_legacy = GameUtil::isLegacyBrowser();
    use_whole_scene_ = !is_legacy
        && (max_texture_size <= 0 || (whole_scene_w <= max_texture_size && whole_scene_h <= max_texture_size));
    if (use_whole_scene_)
    {
        engine->createRenderedTexture("whole_scene", whole_scene_w, whole_scene_h);
    }
    else
    {
        LOG("whole_scene disabled: legacy={}, required={}x{}, renderer max={}\n",
            is_legacy, whole_scene_w, whole_scene_h, max_texture_size);
    }

    //首先设置位置和阵营，其他的后面统一处理
    int battleUid = 10000;
    int cloneSummonCount = 0;
    std::vector<size_t> cloneSourceIndices;
    //敌方
    for (int i = 0; i < BATTLE_ENEMY_COUNT; i++)
    {
        auto r_ptr = roleSave_.getRole(info_->Enemy[i]);
        if (r_ptr)
        {
            enemies_obj_.push_back(*r_ptr);
            auto r = &enemies_obj_.back();
            r->ID = battleUid++;
            r->resetBattleInfo();
            r->RealID = r_ptr->ID;
            enemies_.push_back(r);
            LOG("Adding enemy battle ID {} with {} name {}\n", r->ID, r->RealID, r->Name);
            r->setPositionOnly(info_->EnemyX[i], info_->EnemyY[i]);
            r->Team = 1;
            readFightFrame(r);
            r->FaceTowards = rand_.rand_int(4);
            man_x_ = r->X();
            man_y_ = r->Y();
        }
    }

    //初始状态
    for (auto r : enemies_)
    {
        setRoleInitState(r);
    }
    pos_ = enemies_[0]->Pos;

    //敌人按能力从低到高，依次出场
    std::sort(enemies_.begin(), enemies_.end(), [](Role* l, Role* r)
        {
            return l->MaxHP + l->Attack < r->MaxHP + r->Attack;
        });

    for (int i = 0; i < head_boss_.size() && i < enemies_.size(); i++)
    {
        head_boss_[i]->setRole(enemies_[enemies_.size() - i - 1]);
    }
    if (!extended_teammates_.empty())
    {
        while (!enemies_.empty())
        {
            battle_roles_.push_back(enemies_.front());
            enemies_.pop_front();
        }
    }
    else
    {
        for (int i = 0; i < 6; i++)
        {
            if (!enemies_.empty())
            {
                battle_roles_.push_back(enemies_.front());
                enemies_.pop_front();
            }
        }
    }

    //判断是不是有自動战斗人物
    if (!extended_teammates_.empty())
    {
        for (const auto& teammate_info : extended_teammates_)
        {
            auto r_ptr = roleSave_.getRole(teammate_info.ID);
            if (r_ptr)
            {
                friends_obj_.push_back(*r_ptr);
                auto r = &friends_obj_.back();
                r->ID = battleUid++;
                r->resetBattleInfo();
                r->RealID = r_ptr->ID;
                friends_.push_back(r);
                r->Auto = 2;
            }
        }

        // Detect combos using RealID (original Save IDs, deduped by set)
        auto getTeammateFightsWon = [&](size_t index)
        {
            if (index >= extended_teammates_.size())
            {
                return 0;
            }

            int chessInstanceId = extended_teammates_[index].chessInstanceId;
            if (chessInstanceId < 0)
            {
                return 0;
            }

            auto chess = chessManager_.tryFindChessByInstanceId(KysChess::ChessInstanceID{ chessInstanceId });
            return chess ? chess->fightsWon : 0;
        };

        auto getAllyEquipment = [&](size_t index)
        {
            int weaponId = index < teammate_weapons_.size() ? teammate_weapons_[index] : -1;
            int armorId = index < teammate_armors_.size() ? teammate_armors_[index] : -1;
            if (weaponId >= 0 || armorId >= 0)
            {
                return std::pair{ weaponId, armorId };
            }
            if (index >= extended_teammates_.size())
            {
                return std::pair{ -1, -1 };
            }

            auto& teammate = extended_teammates_[index];
            if (teammate.weaponId >= 0 || teammate.armorId >= 0)
            {
                return std::pair{ teammate.weaponId, teammate.armorId };
            }

            KysChess::ChessInstanceID chessInstanceId{ teammate.chessInstanceId };
            auto chess = chessManager_.tryFindChessByInstanceId(chessInstanceId);
            if (!chess)
            {
                return std::pair{ -1, -1 };
            }

            auto& c = *chess;
            return std::pair{
                c.weaponInstance.itemId,
                c.armorInstance.itemId
            };
        };

        auto getEnemyEquipment = [&](size_t index)
        {
            int weaponId = index < enemy_weapons_.size() ? enemy_weapons_[index] : -1;
            int armorId = index < enemy_armors_.size() ? enemy_armors_[index] : -1;
            return std::pair{ weaponId, armorId };
        };

        auto allyChessVec = [&]()
        {
            std::vector<KysChess::Chess> v;
            for (size_t i = 0; i < friends_obj_.size(); i++)
            {
                auto orig = roleSave_.getRole(friends_obj_[i].RealID);
                if (orig)
                {
                    KysChess::Chess chess;
                    chess.role = orig;
                    chess.star = i < extended_teammates_.size() ? extended_teammates_[i].star : 1;
                    if (i < extended_teammates_.size())
                    {
                        chess.id = KysChess::ChessInstanceID{ extended_teammates_[i].chessInstanceId };
                    }
                    auto [weaponId, armorId] = getAllyEquipment(i);
                    chess.weaponInstance.itemId = weaponId;
                    chess.armorInstance.itemId = armorId;
                    v.push_back(chess);
                }
            }
            return KysChess::ChessEquipment::withActiveSynergies(std::move(v));
        }();
        auto enemyChessVec = [&]()
        {
            std::vector<KysChess::Chess> v;
            for (size_t i = 0; i < enemies_obj_.size(); i++)
            {
                auto orig = roleSave_.getRole(enemies_obj_[i].RealID);
                if (orig)
                {
                    KysChess::Chess chess;
                    chess.role = orig;
                    chess.star = i < enemy_stars_.size() ? enemy_stars_[i] : 0;
                    auto [weaponId, armorId] = getEnemyEquipment(i);
                    chess.weaponInstance.itemId = weaponId;
                    chess.armorInstance.itemId = armorId;
                    v.push_back(chess);
                }
            }
            return KysChess::ChessEquipment::withActiveSynergies(std::move(v));
        }();
        auto allyCombos = KysChess::ChessCombo::detectCombos(allyChessVec);
        auto enemyCombos = KysChess::ChessCombo::detectCombos(enemyChessVec);
        auto allyStates = KysChess::ChessCombo::buildComboStates(allyCombos);
        auto enemyStates = KysChess::ChessCombo::buildComboStates(enemyCombos);
        auto allyComboGlobalEffects = KysChess::ChessCombo::collectGlobalEffects(allyCombos);
        auto enemyComboGlobalEffects = KysChess::ChessCombo::collectGlobalEffects(enemyCombos);

        struct FightWinGrowthBonus
        {
            int hp = 0;
            int atk = 0;
            int def = 0;
        };

        auto getFightWinGrowthBonus = [](const std::map<int, KysChess::RoleComboState>& states, int realId)
        {
            auto it = states.find(realId);
            if (it == states.end())
            {
                return FightWinGrowthBonus{};
            }
            return FightWinGrowthBonus{
                it->second.fightWinGrowthHP,
                it->second.fightWinGrowthATK,
                it->second.fightWinGrowthDEF
            };
        };

        // Apply star + combo-aware per-win growth on battle copies
        for (size_t i = 0; i < friends_obj_.size() && i < extended_teammates_.size(); i++)
        {
            auto extraFightWinGrowth = getFightWinGrowthBonus(allyStates, friends_obj_[i].RealID);
            KysChess::BattleRoleManager::applyStarBonus(&friends_obj_[i], extended_teammates_[i].star, getTeammateFightsWon(i),
                extraFightWinGrowth.hp, extraFightWinGrowth.atk, extraFightWinGrowth.def);
        }
        for (size_t i = 0; i < enemies_obj_.size() && i < enemy_stars_.size(); i++)
        {
            KysChess::BattleRoleManager::applyStarBonus(&enemies_obj_[i], enemy_stars_[i]);
        }

        int allyTeamFlatShield = KysChess::ChessCombo::computeTeamFlatShieldBonus(allyStates);
        int enemyTeamFlatShield = KysChess::ChessCombo::computeTeamFlatShieldBonus(enemyStates);
        std::vector<KysChess::ComboEffect> allyGlobalEffects = allyComboGlobalEffects;

        // Collect neigong global effects and apply them per battle copy.
        // This avoids stacking the same effect multiple times when duplicate role IDs are selected.
        {
            auto& obtained = progress_.getObtainedNeigong();
            if (!obtained.empty())
            {
                auto& pool = KysChess::ChessNeigong::getPool();
                for (int mid : obtained)
                {
                    for (auto& ng : pool)
                    {
                        if (ng.magicId == mid)
                        {
                            allyGlobalEffects.insert(allyGlobalEffects.end(), ng.effects.begin(), ng.effects.end());
                            break;
                        }
                    }
                }
            }
        }

        // Apply combo stat buffs on copies and remap to battle IDs
        KysChess::ChessCombo::clearActiveStates();
        auto& cs = KysChess::ChessCombo::getMutableStates();
        auto applyStateToCopy = [&](Role& r, KysChess::RoleComboState& s)
        {
            r.MaxHP += s.flatHP;
            r.Attack += s.flatATK;
            r.Defence += s.flatDEF;
            r.Speed += s.flatSPD;
            if (s.pctHP != 0)
            {
                r.MaxHP = static_cast<int>(r.MaxHP * (1.0 + s.pctHP / 100.0));
            }
            if (s.pctATK != 0)
            {
                r.Attack = static_cast<int>(r.Attack * (1.0 + s.pctATK / 100.0));
            }
            if (s.pctDEF != 0)
            {
                r.Defence = static_cast<int>(r.Defence * (1.0 + s.pctDEF / 100.0));
            }
            if (s.pctSPD != 0)
            {
                r.Speed = static_cast<int>(r.Speed * (1.0 + s.pctSPD / 100.0));
            }
            r.HP = r.MaxHP;
            if (s.shieldPctMaxHP > 0)
            {
                s.shield = r.MaxHP * s.shieldPctMaxHP / 100;
                logBattleStatus(&r, nullptr,
                    formatStatusValue("獲取", s.shield, "護盾"));
            }
        };
        auto applyEquipmentEffects = [&](KysChess::RoleComboState& state, int roleId, int weaponId, int armorId)
        {
            auto applyById = [&](int itemId)
            {
                if (itemId < 0)
                {
                    return;
                }
                auto* eq = KysChess::ChessEquipment::getById(itemId);
                if (!eq)
                {
                    return;
                }
                for (auto& effect : eq->effects)
                {
                    KysChess::ChessBattleEffects::applyEffect(state, effect);
                }
            };
            applyById(weaponId);
            applyById(armorId);
            for (auto* synergy : KysChess::ChessEquipment::getSynergiesFor(roleId, weaponId, armorId))
            {
                for (auto& effect : synergy->effects)
                {
                    KysChess::ChessBattleEffects::applyEffect(state, effect);
                }
            }
        };
        auto initializeTimedEffects = [](KysChess::RoleComboState& state)
        {
            if (state.damageImmunityAfterFrames > 0)
            {
                state.damageImmunityTimer = state.damageImmunityAfterFrames;
            }
            if (state.autoUltimateAfterFrames > 0)
            {
                state.autoUltimateTimer = state.autoUltimateAfterFrames;
            }
        };
        auto applyOnCopies = [&](std::deque<Role>& objs,
                                 std::map<int, KysChess::RoleComboState>& states,
                                 const std::vector<KysChess::ComboEffect>& globalEffects,
                                 int teamFlatShield,
                                 auto equipmentLookup)
        {
            for (size_t index = 0; index < objs.size(); ++index)
            {
                auto& r = objs[index];
                KysChess::RoleComboState battleState;
                auto it = states.find(r.RealID);
                if (it != states.end())
                {
                    battleState = it->second;
                }

                for (auto& effect : globalEffects)
                {
                    KysChess::ChessBattleEffects::applyEffect(battleState, effect);
                }

                auto [weaponId, armorId] = equipmentLookup(index);
                applyEquipmentEffects(battleState, r.RealID, weaponId, armorId);
                applyStateToCopy(r, battleState);
                if (teamFlatShield > 0)
                {
                    battleState.shield += teamFlatShield;
                    logBattleStatus(&r, nullptr,
                        formatStatusValue("全隊獲取", teamFlatShield, "護盾"));
                }
                initializeTimedEffects(battleState);
                battleState.blockFirstHitsRemaining = battleState.blockFirstHitsCount;
                cs[r.ID] = battleState;
            }
        };
        applyOnCopies(friends_obj_, allyStates, allyGlobalEffects, allyTeamFlatShield, getAllyEquipment);
        applyOnCopies(enemies_obj_, enemyStates, enemyComboGlobalEffects, enemyTeamFlatShield, getEnemyEquipment);
        refreshEnemyTopDebuffs();

        for (auto& role : friends_obj_)
        {
            auto it = cs.find(role.ID);
            if (it != cs.end())
            {
                cloneSummonCount = std::max(cloneSummonCount, it->second.cloneSummonCount);
            }
        }

        if (cloneSummonCount > 0)
        {
            struct CloneSourceCandidate
            {
                size_t index = static_cast<size_t>(-1);
                int chessInstanceId = -1;
                int star = 0;
                int power = 0;
            };

            std::vector<CloneSourceCandidate> cloneCandidates;
            for (size_t i = 0; i < friends_obj_.size() && i < extended_teammates_.size(); ++i)
            {
                auto it = cs.find(friends_obj_[i].ID);
                if (it == cs.end() || it->second.cloneSummonCount <= 0)
                {
                    continue;
                }

                cloneCandidates.push_back({ i,
                    extended_teammates_[i].chessInstanceId,
                    extended_teammates_[i].star,
                    friends_obj_[i].MaxHP + friends_obj_[i].Attack + friends_obj_[i].Defence });
            }

            std::sort(cloneCandidates.begin(), cloneCandidates.end(), [](const auto& left, const auto& right)
                {
                    if (left.star != right.star)
                    {
                        return left.star > right.star;
                    }
                    if (left.power != right.power)
                    {
                        return left.power > right.power;
                    }
                    return left.index < right.index;
                });

            std::set<int> usedInstanceIds;
            std::vector<size_t> fallbackIndices;
            for (const auto& candidate : cloneCandidates)
            {
                if (candidate.chessInstanceId >= 0)
                {
                    if (!usedInstanceIds.insert(candidate.chessInstanceId).second)
                    {
                        fallbackIndices.push_back(candidate.index);
                        continue;
                    }
                }
                cloneSourceIndices.push_back(candidate.index);
            }
            cloneSourceIndices.insert(cloneSourceIndices.end(), fallbackIndices.begin(), fallbackIndices.end());
            if (cloneSummonCount < static_cast<int>(cloneSourceIndices.size()))
            {
                cloneSourceIndices.resize(cloneSummonCount);
            }
        }
    }
    else if (info_->AutoTeamMate[0] >= 0)
    {
        for (int i = 0; i < TEAMMATE_COUNT; i++)
        {
            auto r = roleSave_.getRole(info_->AutoTeamMate[i]);
            if (r)
            {
                friends_.push_back(r);
                r->Auto = 2;    //由AI控制
            }
        }
    }

    if (extended_teammates_.empty() && 1)    //准许队友出场
    {
        auto team_menu = std::make_shared<TeamMenu>();
        team_menu->setMode(1);
        team_menu->setForceMainRole(true);
        team_menu->run();

        for (auto r : team_menu->getRoles())
        {
            if (std::find(friends_.begin(), friends_.end(), r) == friends_.end())
            {
                friends_.push_back(r);
            }
        }
    }
    //队友
    role_ = nullptr;
    for (int i = 0; i < friends_.size(); i++)
    {
        auto r = friends_[i];
        if (r)
        {
            battle_roles_.push_back(r);
            // Use extended teammate positions if available, otherwise use BattleInfo positions
            if (!extended_teammates_.empty() && i < extended_teammates_.size())
            {
                r->setPositionOnly(extended_teammates_[i].X, extended_teammates_[i].Y);
            }
            else
            {
                r->setPositionOnly(info_->TeamMateX[i], info_->TeamMateY[i]);
            }
            r->Team = 0;
            setRoleInitState(r);
        }
    }

    if (!extended_teammates_.empty()
        && cloneSummonCount > 0
        && !cloneSourceIndices.empty()
        && !clone_spawn_positions_.empty())
    {
        auto isOccupied45 = [&](int x, int y)
        {
            for (auto role : battle_roles_)
            {
                if (role->Dead != 0)
                {
                    continue;
                }
                auto pos = pos90To45(role->Pos.x, role->Pos.y);
                if (pos.x == x && pos.y == y)
                {
                    return true;
                }
            }
            return false;
        };

        auto& cs = KysChess::ChessCombo::getMutableStates();
        int spawned = 0;
        for (auto [x, y] : clone_spawn_positions_)
        {
            if (spawned >= cloneSummonCount)
            {
                break;
            }
            if (!canWalk45(x, y) || isOccupied45(x, y))
            {
                continue;
            }

            size_t sourceIndex = cloneSourceIndices[spawned % cloneSourceIndices.size()];
            if (sourceIndex >= friends_obj_.size())
            {
                continue;
            }

            auto source = &friends_obj_[sourceIndex];
            auto sourceState = cs.find(source->ID);
            if (sourceState == cs.end())
            {
                continue;
            }

            friends_obj_.push_back(*source);
            auto clone = &friends_obj_.back();
            clone->ID = battleUid++;
            clone->Auto = 2;
            clone->Team = source->Team;
            clone->Dead = 0;
            clone->LastAttacker = nullptr;
            clone->Velocity = { 0, 0, 0 };
            clone->Acceleration = { 0, 0, 0 };
            clone->UsingMagic = nullptr;
            clone->UsingItem = nullptr;
            clone->HaveAction = 0;
            clone->ActFrame = 0;
            clone->CoolDown = 0;
            clone->Frozen = 0;
            clone->FrozenMax = 0;
            clone->Invincible = 0;
            clone->HurtFrame = 0;
            clone->Shake = 0;
            clone->FindingWay = 0;
            clone->OperationCount = 0;
            clone->setPositionOnly(x, y);
            setRoleInitState(clone);
            battle_roles_.push_back(clone);
            cs[clone->ID] = makeSummonedCloneState(sourceState->second, clone->MaxHP);
            logBattleStatus(source, clone, std::format("七截分身（落点 {}, {}）", x, y));
            spawned++;
        }
    }

    // Center camera on allies
    if (!friends_.empty())
    {
        int sx = 0, sy = 0;
        for (auto r : friends_)
        {
            sx += r->X();
            sy += r->Y();
        }
        pos_ = pos45To90(sx / friends_.size(), sy / friends_.size());
    }
    battle_frame_ = 0;
    half_speed_step_on_next_render_ = true;
    manual_camera_dragging_ = false;
    camera_target_ = pos_;
    close_up_total_ = 0;
    clampCameraCenter();

    // Pre-battle position swap
    if (!extended_teammates_.empty() && progress_.isPositionSwapEnabled())
    {
        auto prompt = std::make_shared<MenuText>(std::vector<std::string>{ "地圖佈陣", "列表佈陣", "直接開戰" });
        prompt->setFontSize(36);
        prompt->arrange(0, 0, 0, 45);
        prompt->runAtPosition(300, 220);
        int choice = prompt->getResult();
        if (choice == 0)
        {
            runPositionSwapLoop();
        }
        else if (choice == 1)
        {
            runListBasedSwap();
        }
    }

    // int i = 0;
    // for (auto r : friends_)
    // {
    //     if (r && r != heads_[0]->getRole())
    //     {
    //         auto head = std::make_shared<Head>();
    //         head->setRole(r);
    //         head->setAlwaysLight(true);
    //         addChild(head, Engine::getInstance()->getWindowWidth() - 280, 10 + 80 * i++);
    //     }
    // }

    Audio::getInstance()->playMusic(KysChess::getRandomBattleMusic());
    // Audio::getInstance()->playMusic(info_->Music);
}

void BattleSceneHades::onExit()
{
    if (previous_refresh_interval_ > 0.0)
    {
        RunNode::setRefreshInterval(previous_refresh_interval_);
        previous_refresh_interval_ = 0.0;
    }

    Engine::getInstance()->destroyTexture("whole_scene");
    use_whole_scene_ = false;

    // hurt_flash_timers_.clear();
    // Engine::getInstance()->hideBattleLogWindow();
}

void BattleSceneHades::calExpGot()
{
    BattleScene::calExpGot();

    if (result_ != 0)
    {
        return;
    }

    if (!count_fights_won_)
    {
        return;
    }

    std::set<int> rewardedInstanceIds;
    for (const auto& teammate : extended_teammates_)
    {
        if (teammate.chessInstanceId < 0)
        {
            continue;
        }
        if (!rewardedInstanceIds.insert(teammate.chessInstanceId).second)
        {
            continue;
        }

        chessManager_.incrementFightsWon(KysChess::ChessInstanceID{ teammate.chessInstanceId });
    }
}

class PositionSwapNode : public RunNode
{
    BattleSceneHades* battle_;

public:
    PositionSwapNode(BattleSceneHades* b) : battle_(b) { }

    void dealEvent(EngineEvent& e) override
    {
        // Tile-click swap
        if (e.type == EVENT_MOUSE_BUTTON_UP && e.button.button == BUTTON_LEFT)
        {
            auto p = battle_->getMousePosition(battle_->man_x_, battle_->man_y_);
            Role* clicked = nullptr;
            for (auto r : battle_->getBattleRoles())
            {
                if (r && r->Team == 0 && r->X() == p.x && r->Y() == p.y)
                {
                    clicked = r;
                    break;
                }
            }
            if (!clicked)
            {
                return;
            }
            if (!battle_->swapSelected_)
            {
                battle_->swapSelected_ = clicked;
            }
            else if (clicked != battle_->swapSelected_)
            {
                int ax = battle_->swapSelected_->X(), ay = battle_->swapSelected_->Y();
                int bx = clicked->X(), by = clicked->Y();
                battle_->swapSelected_->setPositionOnly(bx, by);
                clicked->setPositionOnly(ax, ay);
                auto pa = battle_->pos45To90(bx, by);
                auto pb = battle_->pos45To90(ax, ay);
                battle_->swapSelected_->Pos.x = pa.x;
                battle_->swapSelected_->Pos.y = pa.y;
                clicked->Pos.x = pb.x;
                clicked->Pos.y = pb.y;
                battle_->swapSelected_ = nullptr;
            }
        }
    }

    void onPressedCancel() override
    {
        auto menu = std::make_shared<MenuText>(std::vector<std::string>{ "佈陣完成", "繼續調整" });
        menu->setFontSize(36);
        menu->arrange(0, 0, 0, 45);
        menu->runAtPosition(400, 300);
        if (menu->getResult() == 0)
        {
            battle_->swapSelected_ = nullptr;
            exit_ = true;
        }
    }
};

void BattleSceneHades::runPositionSwapLoop()
{
    swapSelected_ = nullptr;
    auto node = std::make_shared<PositionSwapNode>(this);
    node->run();
}

void BattleSceneHades::runListBasedSwap()
{
    positionSwapActive_ = true;
    // Collect friendly roles
    std::vector<Role*> allies;
    for (auto r : battle_roles_)
    {
        if (r && r->Team == 0)
        {
            allies.push_back(r);
        }
    }
    if (allies.size() < 2)
    {
        return;
    }

    // Build menu items with proper alignment using getTextDrawSize
    // Name is left-aligned, coordinates are right-aligned
    auto buildNames = [&](int highlight = -1)
    {
        // First pass: find max name width and max coord width separately
        int maxNameLen = 0;
        int maxCoordLen = 0;
        for (auto r : allies)
        {
            std::string name = r->Name;
            std::string coord = std::format("({},{})", r->X(), r->Y());
            int nameLen = Font::getTextDrawSize(name);
            int coordLen = Font::getTextDrawSize(coord);
            if (nameLen > maxNameLen)
            {
                maxNameLen = nameLen;
            }
            if (coordLen > maxCoordLen)
            {
                maxCoordLen = coordLen;
            }
        }
        std::vector<std::string> names;
        std::vector<Color> colors;
        std::vector<Color> outlineColors;
        std::vector<bool> animateOutlines;
        for (int i = 0; i < (int)allies.size(); i++)
        {
            std::string name = allies[i]->Name;
            std::string coord = std::format("({},{})", allies[i]->X(), allies[i]->Y());
            int nameLen = Font::getTextDrawSize(name);
            int coordLen = Font::getTextDrawSize(coord);
            // Pad name to max, then pad coord prefix so coords right-align
            std::string s = name;
            int gap = (maxNameLen - nameLen) + (maxCoordLen - coordLen) + 2;
            for (int g = 0; g < gap; g++)
            {
                s += ' ';
            }
            s += coord;
            names.push_back(s);
            colors.push_back({ 255, 255, 255, 255 });
            if (i == highlight)
            {
                outlineColors.push_back({ 255, 255, 100, 255 });
                animateOutlines.push_back(true);
            }
            else
            {
                outlineColors.push_back({ 0, 0, 0, 0 });
                animateOutlines.push_back(false);
            }
        }
        return std::make_tuple(names, colors, outlineColors, animateOutlines);
    };

    while (true)
    {
        // Pick first role
        auto [names1, colors1, outlines1, anim1] = buildNames();
        auto menu1 = std::make_shared<MenuText>();
        menu1->setStrings(names1, colors1, outlines1, anim1);
        menu1->setFontSize(28);
        menu1->arrange(0, 0, 0, 36);
        menu1->runAtPosition(100, 100);
        int sel1 = menu1->getResult();
        if (sel1 < 0 || sel1 >= (int)allies.size())
        {
            // Cancelled first pick — ask to confirm or continue
            auto confirm = std::make_shared<MenuText>(std::vector<std::string>{ "佈陣完成", "繼續調整" });
            confirm->setFontSize(36);
            confirm->arrange(0, 0, 0, 45);
            confirm->runAtPosition(400, 300);
            if (confirm->getResult() == 0)
            {
                break;
            }
            continue;
        }

        // Pick second role — highlight the first selection
        auto [names2, colors2, outlines2, anim2] = buildNames(sel1);
        auto menu2 = std::make_shared<MenuText>();
        menu2->setStrings(names2, colors2, outlines2, anim2);
        menu2->setFontSize(28);
        menu2->arrange(0, 0, 0, 36);
        menu2->runAtPosition(100, 100);
        int sel2 = menu2->getResult();
        if (sel2 < 0 || sel2 >= (int)allies.size())
        {
            continue;    // cancelled second pick, retry
        }
        if (sel2 == sel1)
        {
            continue;    // same role, retry
        }

        // Perform swap
        auto* a = allies[sel1];
        auto* b = allies[sel2];
        int ax = a->X(), ay = a->Y();
        int bx = b->X(), by = b->Y();
        a->setPositionOnly(bx, by);
        b->setPositionOnly(ax, ay);
        auto pa = pos45To90(bx, by);
        auto pb = pos45To90(ax, ay);
        a->Pos.x = pa.x;
        a->Pos.y = pa.y;
        b->Pos.x = pb.x;
        b->Pos.y = pb.y;
    }
    swapSelected_ = nullptr;
    positionSwapActive_ = false;
}

void BattleSceneHades::backRun1()
{
    if (!advanceBattleFrameBeforeDamage())
    {
        return;
    }
    applyPendingPreResolvedDamageFrame();
    advanceBattleFrameAfterDamage();
}
bool BattleSceneHades::advanceBattleFrameBeforeDamage()
{

    // 更新受击闪红计时器
    for (auto it = hurt_flash_timers_.begin(); it != hurt_flash_timers_.end();)
    {
        it->second--;
        if (it->second <= 0)
        {
            it = hurt_flash_timers_.erase(it);
        }
        else
        {
            ++it;
        }
    }

    if (slow_ > 0)
    {
        if (battle_frame_ % CAMERA_SLOW_STEP_INTERVAL) { return false; }
        //x_ = rand_.rand_int(2) - rand_.rand_int(2);
        //y_ = rand_.rand_int(2) - rand_.rand_int(2);
        slow_--;
    }
    ultHitRoles_.clear();
    criticalHitRoles_.clear();

    {
        auto frameState = makeCoreFrameState(makeNoOpCoreWorld());
        populateCoreStatusState(frameState);
        KysChess::Battle::BattleFrameRunner().advanceFrame(frameState);
        applyCoreStatusState(frameState);
        handleCoreStatusEvents(frameState.status.events);
    }

    for (auto r : battle_roles_)
    {
        for (auto m : r->getLearnedMagics())
        {
            if (special_magic_effect_every_frame_.count(m->Name))
            {
                special_magic_effect_every_frame_[m->Name](r);
            }
        }
        decreaseToZero(r->Frozen);
        decreaseToZero(r->Shake);

        // Combo: per-frame effects
        {
            auto& cs = KysChess::ChessCombo::getMutableStates();
            auto it = cs.find(r->ID);
            if (it != cs.end())
            {
                for (const auto& event : KysChess::Battle::BattleComboTriggerSystem().advanceFrameRuntime(
                         it->second,
                         {
                             battle_frame_,
                             r->HP,
                             r->MaxHP,
                             r->Dead == 0,
                             isLastAliveInTeam(r),
                         }))
                {
                    applyComboFrameRuntimeEvent(r, it->second, event, cs);
                }
            }
        }

        // Defensive timers should expire in real battle ticks even when control
        // effects stop movement/actions. Otherwise periodic immunity can cool
        // down while the previous invincible window is frozen in place.
        decreaseToZero(r->HurtFrame);
        decreaseToZero(r->Attention);
        decreaseToZero(r->Invincible);

        auto runtimeFrameState = makeCoreFrameState(makeNoOpCoreWorld());
        runtimeFrameState.attacks.attacks.clear();
        runtimeFrameState.attacks.sharedHitGroupTargets.clear();
        auto& comboStatesForRuntime = KysChess::ChessCombo::getMutableStates();
        auto comboForRuntime = comboStatesForRuntime.find(r->ID);
        if (comboForRuntime != comboStatesForRuntime.end())
        {
            runtimeFrameState.combo.units.emplace(r->ID, comboForRuntime->second);
            runtimeFrameState.runtime.percentRolls.reserve(comboForRuntime->second.triggeredEffects.size() + 1);
            for (size_t i = 0; i <= comboForRuntime->second.triggeredEffects.size(); ++i)
            {
                runtimeFrameState.runtime.percentRolls.push_back(rand_.rand() * 100.0);
            }
        }
        KysChess::Battle::BattleFrameRuntimeUnitInput runtimeInput;
        runtimeInput.unitId = r->ID;
        runtimeInput.input = makeBattleFrameUnitRuntimeInput(r, battle_frame_, 3, 3);
        runtimeInput.hp = r->HP;
        runtimeInput.maxHp = r->MaxHP;
        runtimeInput.alive = r->Dead == 0;
        runtimeInput.lastAlive = isLastAliveInTeam(r);
        runtimeFrameState.runtime.units.push_back(runtimeInput);

        auto runtimeFrameResult = KysChess::Battle::BattleFrameRunner().advanceFrame(runtimeFrameState);
        assert(runtimeFrameState.runtime.committedResults.size() == 1);
        const auto& runtimeUnitResult = runtimeFrameState.runtime.committedResults.front();
        applyBattleFrameUnitRuntimeResult(r, runtimeUnitResult.result);
        if (comboForRuntime != comboStatesForRuntime.end())
        {
            auto committedCombo = runtimeFrameState.combo.units.find(r->ID);
            assert(committedCombo != runtimeFrameState.combo.units.end());
            comboForRuntime->second = committedCombo->second;
            for (const auto& event : runtimeUnitResult.comboEvents)
            {
                assert(event.type == KysChess::Battle::BattleComboFrameRuntimeEventType::PostSkillInvincibility);
                applyComboFrameRuntimeEvent(r, comboForRuntime->second, event, comboStatesForRuntime);
            }
        }
        for (const auto& command : runtimeFrameResult.commands)
        {
            const auto* teamHeal = std::get_if<KysChess::Battle::BattleTeamHealCommand>(&command);
            assert(teamHeal);
            LegacyTeamEffectCommitRequest request;
            request.type = LegacyTeamEffectCommitType::Heal;
            request.sourceUnitId = teamHeal->sourceUnitId;
            request.flatHeal = teamHeal->flatHeal;
            request.pctHeal = teamHeal->pctHeal;
            auto teamEvents = commitLegacyTeamEffect(request);
            playCommittedTeamEffectEvents(r, teamEvents, teamHeal->reason.c_str());
        }
        if (runtimeUnitResult.result.mpDelta != 0)
        {
            changeRoleMP(r, runtimeUnitResult.result.mpDelta);
        }

        if (r->Frozen > 0)
        {
            continue;
        }

        //更新速度，加速度，力学位置
        {
            auto& movementRuntime = movement_runtime_[r];
            int dashStartFrame = -1;
            int dashEndFrame = -1;
            if (r->OperationType == 3 && r->HaveAction)
            {
                dashStartFrame = calCast(r->ActType, r->OperationType, r);
                dashEndFrame = dashStartFrame + DASH_MOMENTUM_FRAMES;
                if (r->ActFrame > dashEndFrame)
                {
                    r->Velocity = { 0, 0, 0 };
                }
            }
            KysChess::Battle::BattleMovementPhysicsInput input;
            input.state.position = r->Pos;
            input.state.velocity = r->Velocity;
            input.state.acceleration = r->Acceleration;
            input.state.movementDashFrames = movementRuntime.movement_dash_frames;
            input.state.movementDashCooldown = movementRuntime.movement_dash_cooldown;
            input.state.movementDashSpreadFrames = movementRuntime.movement_dash_spread_frames;
            input.config.gravity = gravity_;
            input.config.friction = friction_;
            input.config.postDashSpreadFrames = POST_DASH_SPREAD_FRAMES;
            input.actionDashActive = dashStartFrame >= 0 && r->ActFrame >= dashStartFrame && r->ActFrame <= dashEndFrame;
            input.canMove = [this, r](Pointf position, int separationDistance)
            {
                return canWalk90(position, r, separationDistance);
            };

            auto state = KysChess::Battle::BattleMovementPhysicsSystem().advance(input);
            r->Pos = state.position;
            r->Velocity = state.velocity;
            r->Acceleration = state.acceleration;
            movementRuntime.movement_dash_frames = state.movementDashFrames;
            movementRuntime.movement_dash_cooldown = state.movementDashCooldown;
            movementRuntime.movement_dash_spread_frames = state.movementDashSpreadFrames;
        }
        //else
        //{
        //    r->Velocity = { 0, 0 };
        //    if (r->HP <= 0)
        //    {
        //        r->Dead = 1;
        //        //此處只為嚴格化，但與擊退部分可能衝突
        //    }
        //}
    }

    //if (current_frame_ % 2 == 0)
    {
        prepareCoreMovementDecisions();
        for (auto r : battle_roles_)
        {
            //有行动
            Action(r);
            //ai策略
            AI(r);
        }
    }

    //效果
    //if (current_frame_ % 2 == 0)
    {
        auto frameState = makeCoreFrameState(makeNoOpCoreWorld());
        frameState.pendingAttackSpawns = std::move(pending_core_attack_spawns_);
        pending_core_attack_spawns_.clear();
        for (size_t i = 0; i + 1 < frameState.attacks.attacks.size(); ++i)
        {
            const auto& attack1 = frameState.attacks.attacks[i];
            auto* attacker1 = findRoleByBattleId(battle_roles_, attack1.state.attackerUnitId);
            auto* skill1 = Save::getInstance()->getMagic(attack1.state.skillId);
            for (size_t j = i + 1; j < frameState.attacks.attacks.size(); ++j)
            {
                const auto& attack2 = frameState.attacks.attacks[j];
                auto* attacker2 = findRoleByBattleId(battle_roles_, attack2.state.attackerUnitId);
                if (attacker1->Team == attacker2->Team)
                {
                    continue;
                }
                auto* skill2 = Save::getInstance()->getMagic(attack2.state.skillId);
                if (skill1)
                {
                    frameState.projectileCancel.baseDamages.push_back({
                        attack1.id,
                        attack2.id,
                        resolveLegacyMagicBaseDamage(attacker1, attacker2, skill1),
                    });
                }
                if (skill2)
                {
                    frameState.projectileCancel.baseDamages.push_back({
                        attack2.id,
                        attack1.id,
                        resolveLegacyMagicBaseDamage(attacker2, attacker1, skill2),
                    });
                }
            }
        }
        auto frameResult = KysChess::Battle::BattleFrameRunner().advanceFrame(frameState);
        auto attackEvents = std::move(frameResult.attackEvents);
        for (const auto& command : frameState.projectileCancel.committedCommands)
        {
            applyBattleProjectileCancelDamage(
                findRoleByBattleId(battle_roles_, command.sourceUnitId),
                command.damage);
            applyBattleProjectileCancelDamage(
                findRoleByBattleId(battle_roles_, command.otherSourceUnitId),
                command.otherDamage);
        }
        shared_hit_group_targets_.clear();
        for (const auto& [groupId, targets] : frameState.attacks.sharedHitGroupTargets)
        {
            auto& output = shared_hit_group_targets_[groupId];
            output.insert(targets.begin(), targets.end());
        }
        for (const auto& event : frameResult.frame.presentationEvents)
        {
            presentation_recorder_.recordPresentation(event);
        }
        advanceVisualOnlyEffects(attack_effects_);

        for (const auto& event : attackEvents)
        {
            if (event.type == KysChess::Battle::BattleAttackEventType::Hit)
            {
                auto* r = findRoleByBattleId(battle_roles_, event.unitId);
                bool scriptedImpact = hasScriptedImpact(event);

                if (event.skillId >= 0)
                {
                    auto* skill = Save::getInstance()->getMagic(event.skillId);
                    assert(skill);
                    Audio::getInstance()->playESound(skill->EffectID);
                }
                if (scriptedImpact)
                {
                    r->Shake = 5;
                    auto* attacker = findOptionalRoleByBattleId(battle_roles_, event.sourceUnitId);
                    KysChess::Battle::BattleHitResolutionInput hitInput;
                    hitInput.attackEvent = event;
                    if (attacker)
                    {
                        hitInput.attacker = makeBattleHitUnitSnapshot(attacker);
                    }
                    else
                    {
                        hitInput.attacker.id = event.sourceUnitId;
                    }
                    hitInput.defender = makeBattleHitUnitSnapshot(r);
                    hitInput.sharedBleedMaxStacks = event.scriptedBleedStacks > 0 ? getSharedBleedMaxStacks(attacker) : 0;
                    applyResolvedBattleHit(KysChess::Battle::BattleHitResolver().resolve(hitInput));
                    continue;
                }

                auto* attacker = findRoleByBattleId(battle_roles_, event.sourceUnitId);

                // Combo: dodge check
                {
                    auto& cs = KysChess::ChessCombo::getMutableStates();
                    auto dit = cs.find(r->ID);
                    if (dit != cs.end())
                    {
                        auto dodge = KysChess::Battle::BattleComboTriggerSystem().resolveDodge(
                            dit->second,
                            attacker->ID,
                            rand_.rand() * 100.0);
                        if (dodge.dodged)
                        {
                            dit->second.dodgedLast = true;
                            addRoleEffect(r, KysChess::EFT_EVADE, ROLE_STATUS_EFT_FRAMES);
                            logBattleStatus(r, attacker, "閃避了來襲攻擊");
                            continue;
                        }
                    }
                }

                shake_ = event.ultimate ? 10 : 0;
                r->Frozen = 0;
                if (event.mainProjectile)
                {
                    applyFrozen(r, event.ultimate ? 10 : 5);
                }
                r->Shake = event.ultimate ? 10 : 5;
                if (event.ultimate) { ultHitRoles_.insert(r); }
                if (event.operationType != KysChess::Battle::BattleOperationType::None
                    || event.hiddenWeaponItemId >= 0)
                {
                    Engine::getInstance()->gameControllerRumble(100, 100, 50);
                    auto* usingMagic = event.skillId >= 0 ? Save::getInstance()->getMagic(event.skillId) : nullptr;
                    auto* usingHiddenWeapon = event.hiddenWeaponItemId >= 0 ? Save::getInstance()->getItem(event.hiddenWeaponItemId) : nullptr;
                    assert(usingHiddenWeapon || usingMagic);
                    assert(event.totalFrame > 0);

                    KysChess::Battle::BattleHitResolutionInput hitInput;
                    hitInput.attackEvent = event;
                    hitInput.attacker = makeBattleHitUnitSnapshot(attacker);
                    hitInput.defender = makeBattleHitUnitSnapshot(r);
                    hitInput.skill = makeBattleHitSkillSnapshot(
                        attacker,
                        r,
                        usingMagic,
                        usingMagic ? resolveLegacyMagicBaseDamage(attacker, r, usingMagic) : 0);
                    hitInput.item = makeBattleHitItemSnapshot(
                        usingHiddenWeapon,
                        usingHiddenWeapon ? calHiddenWeaponHurt(attacker, r, usingHiddenWeapon) : 0);
                    hitInput.pendingDefenderHpDamage = pendingPreResolvedHpDamage(r);
                    hitInput.randomDamageVariance = static_cast<int>(5 * (rand_.rand() - rand_.rand()));

                    auto& cs = KysChess::ChessCombo::getMutableStates();
                    auto attackerComboIt = cs.find(attacker->ID);
                    auto defenderComboIt = cs.find(r->ID);
                    if (attackerComboIt != cs.end())
                    {
                        hitInput.attackerCombo = attackerComboIt->second;
                    }
                    if (defenderComboIt != cs.end())
                    {
                        hitInput.defenderCombo = defenderComboIt->second;
                    }
                    for (int i = 0; i < 64; ++i)
                    {
                        hitInput.percentRolls.push_back(rand_.rand() * 100.0);
                    }

                    auto hitResolution = KysChess::Battle::BattleHitResolver().resolve(hitInput);
                    applyResolvedBattleHit(hitResolution);
                }
                //std::vector<std::string> = {};
            }
        }
        active_attack_world_ = std::move(frameState.attacks);
        active_attack_world_.attacks.erase(
            std::remove_if(
                active_attack_world_.attacks.begin(),
                active_attack_world_.attacks.end(),
                [](const auto& attack)
                {
                    return attack.frame >= attack.state.totalFrame;
                }),
            active_attack_world_.attacks.end());
        //删除播放完毕的
        for (auto it = attack_effects_.begin(); it != attack_effects_.end();)
        {
            if (it->Frame >= it->TotalFrame)
            {
                it = attack_effects_.erase(it);
            }
            else
            {
                it++;
            }
        }
    }


    return true;
}

void BattleSceneHades::applyPendingPreResolvedDamageFrame()
{
    //此處计算累积傷害
    auto isOccupied45 = [&](int x, int y, const Role* ignore)
    {
        for (auto role : battle_roles_)
        {
            if (role == ignore || role->Dead != 0)
            {
                continue;
            }
            auto pos = pos90To45(role->Pos.x, role->Pos.y);
            if (pos.x == x && pos.y == y)
            {
                return true;
            }
        }
        return false;
    };
    auto isUnattended = [&](Role* target, int team)
    {
        for (auto role : battle_roles_)
        {
            if (role->Dead != 0 || role->Team != team)
            {
                continue;
            }
            if (EuclidDis(role->Pos, target->Pos) <= TILE_W * 3)
            {
                return false;
            }
        }
        return true;
    };
    auto spawnBasicAttackProjectile = [&](Role* attacker, Role* target)
    {
        if (!attacker || !target || attacker->Dead != 0 || target->Dead != 0)
        {
            return;
        }

        Magic* basicMagic = Save::getInstance()->getMagic(1);
        if (!basicMagic)
        {
            return;
        }

        auto direction = target->Pos - attacker->Pos;
        if (direction.norm() <= 0.01)
        {
            return;
        }

        direction.normTo(1);
        attacker->RealTowards = direction;
        setFaceTowardsNearest(attacker);

        KysChess::Battle::BattleAttackSpawnRequest request;
        request.initial.attackerUnitId = attacker->ID;
        request.initial.skillId = basicMagic->ID;
        request.initial.preferredTargetUnitId = target->ID;
        request.initial.requirePreferredTarget = true;
        request.initial.track = true;
        request.initial.operationType = KysChess::Battle::BattleOperationType::Melee;
        request.initial.visualEffectId = 11;
        request.initial.position = attacker->Pos + MELEE_ATTACK_EFFECT_OFFSET * direction;
        request.initial.velocity = target->Pos - request.initial.position;
        if (request.initial.velocity.norm() <= 0.01)
        {
            request.initial.velocity = direction;
        }
        request.initial.velocity.normTo(PROJECTILE_SPEED);
        request.initial.totalFrame = std::max(20,
            static_cast<int>(std::ceil(EuclidDis(target->Pos, request.initial.position) / static_cast<double>(PROJECTILE_SPEED))) + 15);
        attachProjectileBouncePrime(request);
        queueCoreAttackSpawn(std::move(request));
        Audio::getInstance()->playESound(attacker->ActType);
    };
    auto tryForcePull = [&](Role* pulled, int pullerTeam, Role* reference, bool executeMode)
    {
        if (!pulled || !reference || pulled->Dead != 0)
        {
            return false;
        }

        auto& cs = KysChess::ChessCombo::getMutableStates();
        std::vector<Role*> candidates;
        for (auto role : battle_roles_)
        {
            if (role->Dead != 0 || role->Team != pullerTeam)
            {
                continue;
            }
            auto it = cs.find(role->ID);
            if (it == cs.end() || it->second.isSummonedClone)
            {
                continue;
            }
            if (executeMode)
            {
                if (!it->second.forcePullExecute || it->second.forcePullExecuteRemaining <= 0)
                {
                    continue;
                }
            }
            else
            {
                if (!it->second.forcePullProtect || it->second.forcePullProtectRemaining <= 0)
                {
                    continue;
                }
            }
            candidates.push_back(role);
        }

        std::sort(candidates.begin(), candidates.end(), [&](Role* lhs, Role* rhs)
            {
                return EuclidDis(lhs->Pos, reference->Pos) < EuclidDis(rhs->Pos, reference->Pos);
            });

        auto currentPos45 = pos90To45(pulled->Pos.x, pulled->Pos.y);
        auto commitPull = [&](Role* puller, const Point& cell)
        {
            int x = cell.x;
            int y = cell.y;
            auto pos = pos45To90(x, y);
            pulled->setPositionOnly(x, y);
            pulled->Pos.x = pos.x;
            pulled->Pos.y = pos.y;
            pulled->Pos.z = 0;
            pulled->Velocity = { 0, 0, 0 };
            pulled->Acceleration = { 0, 0, gravity_ };
            pulled->FindingWay = 0;
            setFaceTowardsNearest(pulled);
            setFaceTowardsNearest(puller);
        };

        if (executeMode)
        {
            for (auto puller : candidates)
            {
                auto center = pos90To45(puller->Pos.x, puller->Pos.y);
                std::vector<Point> cells;
                for (int dx = -1; dx <= 1; ++dx)
                {
                    for (int dy = -1; dy <= 1; ++dy)
                    {
                        if (dx == 0 && dy == 0)
                        {
                            continue;
                        }
                        int x = center.x + dx;
                        int y = center.y + dy;
                        if (!canWalk45(x, y) || isOccupied45(x, y, pulled))
                        {
                            continue;
                        }
                        if (x == currentPos45.x && y == currentPos45.y)
                        {
                            continue;
                        }
                        cells.push_back({ x, y });
                    }
                }
                if (cells.empty())
                {
                    continue;
                }

                std::sort(cells.begin(), cells.end(), [&](const Point& lhs, const Point& rhs)
                    {
                        return std::abs(lhs.x - currentPos45.x) + std::abs(lhs.y - currentPos45.y)
                            < std::abs(rhs.x - currentPos45.x) + std::abs(rhs.y - currentPos45.y);
                    });

                const auto& cell = cells.front();
                commitPull(puller, cell);

                auto& pullState = cs[puller->ID];
                pullState.forcePullExecuteRemaining = std::max(0, pullState.forcePullExecuteRemaining - 1);
                spawnBasicAttackProjectile(puller, pulled);
                logBattleStatus(puller, pulled, "處決挪移");

                addFloatingText(pulled, "挪移", { 160, 220, 255, 255 }, STATUS_TEXT_SIZE);
                return true;
            }

            return false;
        }

        Pointf allyCenter{ 0, 0, 0 };
        Pointf enemyCenter{ 0, 0, 0 };
        int allyCount = 0;
        int enemyCount = 0;
        for (auto role : battle_roles_)
        {
            if (role->Dead != 0 || role == pulled)
            {
                continue;
            }
            if (role->Team == pullerTeam)
            {
                allyCenter += role->Pos;
                allyCount++;
            }
            else
            {
                enemyCenter += role->Pos;
                enemyCount++;
            }
        }
        if (allyCount > 0)
        {
            allyCenter.x /= allyCount;
            allyCenter.y /= allyCount;
        }
        if (enemyCount > 0)
        {
            enemyCenter.x /= enemyCount;
            enemyCenter.y /= enemyCount;
        }

        Pointf backrowVec = allyCenter - enemyCenter;
        double backrowLen = std::sqrt(backrowVec.x * backrowVec.x + backrowVec.y * backrowVec.y);

        struct PullChoice
        {
            bool valid = false;
            Role* puller = nullptr;
            Point cell{ };
            int enemyThreat = std::numeric_limits<int>::max();
            int enemyMinDist = -1;
            int allySupport = -1;
            double backrowScore = std::numeric_limits<double>::lowest();
            int pullerDist = std::numeric_limits<int>::max();
        };

        auto betterChoice = [&](const PullChoice& lhs, const PullChoice& rhs)
        {
            if (!rhs.valid)
            {
                return lhs.valid;
            }
            if (!lhs.valid)
            {
                return false;
            }
            if (lhs.enemyThreat != rhs.enemyThreat)
            {
                return lhs.enemyThreat < rhs.enemyThreat;
            }
            if (lhs.enemyMinDist != rhs.enemyMinDist)
            {
                return lhs.enemyMinDist > rhs.enemyMinDist;
            }
            if (lhs.allySupport != rhs.allySupport)
            {
                return lhs.allySupport > rhs.allySupport;
            }
            if (std::abs(lhs.backrowScore - rhs.backrowScore) > 1e-6)
            {
                return lhs.backrowScore > rhs.backrowScore;
            }
            if (lhs.pullerDist != rhs.pullerDist)
            {
                return lhs.pullerDist < rhs.pullerDist;
            }
            if (lhs.cell.x != rhs.cell.x)
            {
                return lhs.cell.x < rhs.cell.x;
            }
            return lhs.cell.y < rhs.cell.y;
        };

        constexpr int PROTECT_SEARCH_RADIUS = 10;
        PullChoice bestChoice;
        for (auto puller : candidates)
        {
            auto center = pos90To45(puller->Pos.x, puller->Pos.y);
            PullChoice bestForPuller;
            bestForPuller.valid = false;
            bestForPuller.puller = puller;
            bestForPuller.pullerDist = calDistance(center.x, center.y, currentPos45.x, currentPos45.y);

            for (int dx = -PROTECT_SEARCH_RADIUS; dx <= PROTECT_SEARCH_RADIUS; ++dx)
            {
                for (int dy = -PROTECT_SEARCH_RADIUS; dy <= PROTECT_SEARCH_RADIUS; ++dy)
                {
                    if (std::abs(dx) + std::abs(dy) > PROTECT_SEARCH_RADIUS)
                    {
                        continue;
                    }
                    if (dx == 0 && dy == 0)
                    {
                        continue;
                    }
                    int x = center.x + dx;
                    int y = center.y + dy;
                    if (!canWalk45(x, y) || isOccupied45(x, y, pulled))
                    {
                        continue;
                    }
                    if (x == currentPos45.x && y == currentPos45.y)
                    {
                        continue;
                    }

                    PullChoice choice;
                    choice.valid = true;
                    choice.puller = puller;
                    choice.cell = { x, y };
                    choice.pullerDist = calDistance(x, y, center.x, center.y);

                    for (auto role : battle_roles_)
                    {
                        if (role->Dead != 0 || role == pulled)
                        {
                            continue;
                        }
                        auto rolePos45 = pos90To45(role->Pos.x, role->Pos.y);
                        int d = calDistance(x, y, rolePos45.x, rolePos45.y);
                        if (role->Team == pullerTeam)
                        {
                            if (d <= 4)
                            {
                                choice.allySupport++;
                            }
                        }
                        else
                        {
                            choice.enemyMinDist = std::min(choice.enemyMinDist, d);
                            if (d <= 4)
                            {
                                choice.enemyThreat++;
                            }
                        }
                    }
                    if (choice.enemyMinDist == std::numeric_limits<int>::max())
                    {
                        choice.enemyMinDist = BATTLEMAP_COORD_COUNT;
                    }
                    if (backrowLen > 0.0001)
                    {
                        auto candidatePos90 = pos45To90(x, y);
                        double vx = candidatePos90.x - allyCenter.x;
                        double vy = candidatePos90.y - allyCenter.y;
                        choice.backrowScore = (vx * backrowVec.x + vy * backrowVec.y) / backrowLen;
                    }

                    if (betterChoice(choice, bestForPuller))
                    {
                        bestForPuller = choice;
                    }
                }
            }

            if (betterChoice(bestForPuller, bestChoice))
            {
                bestChoice = bestForPuller;
            }
        }

        if (!bestChoice.valid)
        {
            return false;
        }

        commitPull(bestChoice.puller, bestChoice.cell);

        auto& pullState = cs[bestChoice.puller->ID];
        pullState.forcePullProtectRemaining = std::max(0, pullState.forcePullProtectRemaining - 1);
        int hpBefore = pulled->HP;
        int heal = std::max(1, pulled->MaxHP * 10 / 100);
        pulled->HP = std::min(pulled->MaxHP, pulled->HP + heal);
        pulled->Invincible += 10;
        if (pulled->HP > hpBefore)
        {
            addRoleEffect(pulled, KysChess::EFT_HEAL, ROLE_STATUS_EFT_FRAMES);
            logBattleHeal(bestChoice.puller, pulled, pulled->HP - hpBefore, "保護挪移");
        }
        logBattleStatus(bestChoice.puller, pulled, "保護挪移");
        logBattleStatus(bestChoice.puller, pulled, formatStatusFrames("短暫無敵", 10));

        addFloatingText(pulled, "挪移", { 160, 220, 255, 255 }, STATUS_TEXT_SIZE);
        return true;
    };
    std::vector<PendingPreResolvedHpDamage> pendingPreResolvedHpDamage;
    std::unordered_map<Role*, size_t> pendingDamageIndexByTarget;
    for (const auto& pending : pending_pre_resolved_hp_damage_)
    {
        assert(pending.target);
        assert(pending.damage > 0);

        auto it = pendingDamageIndexByTarget.find(pending.target);
        if (it == pendingDamageIndexByTarget.end())
        {
            pendingDamageIndexByTarget[pending.target] = pendingPreResolvedHpDamage.size();
            pendingPreResolvedHpDamage.push_back(pending);
            continue;
        }

        auto& aggregate = pendingPreResolvedHpDamage[it->second];
        aggregate.source = pending.source;
        aggregate.damage += pending.damage;
        aggregate.critical = aggregate.critical || pending.critical;
        aggregate.ultimate = aggregate.ultimate || pending.ultimate;
        aggregate.executed = aggregate.executed || pending.executed;
        if (!pending.skillName.empty())
        {
            aggregate.skillName = pending.skillName;
        }
        if (!pending.detailText.empty())
        {
            aggregate.detailText = pending.detailText;
        }
        if (pending.damageTextSize > 0)
        {
            aggregate.damageColor = pending.damageColor;
            aggregate.damageTextSize = pending.damageTextSize;
        }
    }
    pending_pre_resolved_hp_damage_.clear();

    auto& cs = KysChess::ChessCombo::getMutableStates();
    KysChess::Battle::BattleDamageApplicationInput damageApplicationInput;
    damageApplicationInput.frame = battle_frame_;
    damageApplicationInput.deathEffects = makeBattleDeathEffectWorld(battle_roles_, cs);
    damageApplicationInput.pendingAliveByTeam[1] = static_cast<int>(enemies_.size());
    for (auto role : battle_roles_)
    {
        assert(role);
        damageApplicationInput.units.push_back({ role->ID, role->Team, role->Dead == 0 });
    }

    for (const auto& pending : pendingPreResolvedHpDamage)
    {
        assert(pending.target);
        assert(pending.damage > 0);

        auto r = pending.target;
        r->LastAttacker = pending.source;
        auto sit = cs.find(r->ID);
        auto kit = r->LastAttacker ? cs.find(r->LastAttacker->ID) : cs.end();

        KysChess::Battle::BattleDamageTransactionInput damageInput;
        damageInput.request.attackerUnitId = r->LastAttacker ? r->LastAttacker->ID : -1;
        damageInput.request.defenderUnitId = r->ID;
        damageInput.request.baseDamage = pending.executed ? std::max(pending.damage, r->HP) : pending.damage;
        damageInput.request.preResolvedDamage = true;
        damageInput.attacker = makeBattleDamageUnit(r->LastAttacker, kit != cs.end() ? &kit->second : nullptr);
        damageInput.defender = makeBattleDamageUnit(r, sit != cs.end() ? &sit->second : nullptr);
        damageApplicationInput.pendingTransactions.push_back(std::move(damageInput));
        if (sit != cs.end())
        {
            auto effects = makeBattleDamageApplicationUnitEffects(sit->second);
            if (effects)
            {
                damageApplicationInput.unitEffects[r->ID] = *effects;
            }
        }
    }

    auto damageApplicationResult = KysChess::Battle::BattleDamageApplicationSystem().apply(damageApplicationInput);
    assert(damageApplicationResult.transactions.size() == pendingPreResolvedHpDamage.size());

    auto hasLifecycleEvent = [&](KysChess::Battle::BattleDamageLifecycleEventType type, int targetUnitId)
    {
        return std::any_of(
            damageApplicationResult.lifecycleEvents.begin(),
            damageApplicationResult.lifecycleEvents.end(),
            [&](const KysChess::Battle::BattleDamageLifecycleEvent& event)
            {
                return event.type == type && event.targetUnitId == targetUnitId;
            });
    };

    for (size_t i = 0; i < damageApplicationResult.transactions.size(); ++i)
    {
        const auto& pending = pendingPreResolvedHpDamage[i];
        auto r = pending.target;
        int hpBefore = r->HP;
        auto sit = cs.find(r->ID);
        auto kit = r->LastAttacker ? cs.find(r->LastAttacker->ID) : cs.end();
        const auto& damageTaken = damageApplicationResult.transactions[i];
        if (kit != cs.end())
        {
            writeBattleDamageUnit(r->LastAttacker, &kit->second, damageTaken.attacker);
        }
        writeBattleDamageUnit(r, sit != cs.end() ? &sit->second : nullptr, damageTaken.defender);

        int committedHpDamage = 0;
        for (const auto& event : damageTaken.events)
        {
            if (event.type == KysChess::Battle::BattleDamageEventType::DamageApplied)
            {
                committedHpDamage += event.value;
            }
        }

        if (committedHpDamage > 0)
        {
            if (pending.executed)
            {
                addFloatingText(r, "處決！", { 255, 136, 48, 255 }, ULT_DAMAGE_TEXT_SIZE);
            }
            else
            {
                bool emphasized = pending.ultimate || pending.critical;
                Color numberColor = pending.damageTextSize > 0
                    ? pending.damageColor
                    : damageTextColor(r, emphasized);
                int numberSize = pending.damageTextSize > 0
                    ? pending.damageTextSize
                    : (emphasized ? ULT_DAMAGE_TEXT_SIZE : NORMAL_DAMAGE_TEXT_SIZE);
                addDamageNumber(r, committedHpDamage, numberColor, numberSize);
            }
            logBattleDamage(r->LastAttacker, r, committedHpDamage, pending.skillName, pending.detailText);

            AttackEffect ae1;
            ae1.FollowRole = r;
            //ae1.EffectNumber = eft[rand_.rand() * eft.size()];
            ae1.setPath(std::format("eft/bld{:03}", int(rand_.rand() * 5)));
            ae1.TotalFrame = ae1.TotalEffectFrame;
            ae1.Frame = 0;
            attack_effects_.push_back(std::move(ae1));
            hurt_flash_timers_[r->ID] = HURT_FLASH_DURATION;
            double mpGain = static_cast<double>(committedHpDamage) / r->MaxHP * 75.0;
            changeRoleMP(r, mpGain);
        }

        if (damageTaken.hurtInvincGranted)
        {
            logBattleStatus(r, r, formatStatusFrames("受傷無敵", damageTaken.invincibilityGranted));
        }
        if (damageTaken.deathPrevented)
        {
            for (const auto& event : damageApplicationResult.presentationEvents)
            {
                if (event.targetUnitId == r->ID)
                {
                    presentation_recorder_.recordPresentation(event);
                }
            }
        }
        if (hasLifecycleEvent(KysChess::Battle::BattleDamageLifecycleEventType::UnitDied, r->ID))
        {
            //LOG("{} has been beat\n", r->Name);
            if (sit != cs.end())
            {
                sit->second.onSkillTeamHealPending = false;
            }
            if (hasLifecycleEvent(KysChess::Battle::BattleDamageLifecycleEventType::KillRecorded, r->ID))
            {
                tracker_.recordKill(r->LastAttacker, r, battle_frame_);
            }
            tracker_.recordDeath(r, battle_frame_);
            refreshEnemyTopDebuffs();

            if (r->LastAttacker && kit != cs.end())
            {
                if (damageTaken.attackerDelta.hpDelta > 0)
                {
                    logBattleHeal(r->LastAttacker, r->LastAttacker, damageTaken.attackerDelta.hpDelta,
                        std::format("擊殺回復 {}%", kit->second.killHealPct));
                }
                if (damageTaken.attackerDelta.invincibleDelta > 0)
                {
                    logBattleStatus(r->LastAttacker, r->LastAttacker,
                        formatStatusFrames("擊殺無敵", damageTaken.attackerDelta.invincibleDelta));
                }
                if (damageTaken.attackerDelta.attackDelta > 0)
                {
                    logBattleStatus(r->LastAttacker, r->LastAttacker,
                        std::format("嗜血（+{}攻）", damageTaken.attackerDelta.attackDelta));
                }
            }

            // Transfer 独行 to next strongest alive member
            KysChess::ChessCombo::transferAntiCombo(r->ID, battle_roles_);

            //r->Velocity = r->Pos - ae1.Attacker->Pos;
            r->Velocity.normTo(15);    //因為已經有擊退速度，可以直接利用
            r->Velocity.z = 12;
            //r->Velocity.normTo(std::min(committedHpDamage / 1.0, 15.0));
            r->Velocity.normTo(committedHpDamage / 2.0);
            r->Frozen = 0;
            applyFrozen(r, 5);
            x_ = rand_.rand_int(2) - rand_.rand_int(2);
            y_ = rand_.rand_int(2) - rand_.rand_int(2);
            if (!isManualCameraEnabled())
            {
                focusCameraOn(r->Pos, CAMERA_DEATH_ZOOM_FRAMES);
            }
            frozen_ = 5;
            shake_ = 10;
            slow_ = CAMERA_DEATH_SLOW_FRAMES;
            if (!isManualCameraEnabled())
            {
                close_up_ = std::max(close_up_, CAMERA_DEATH_ZOOM_FRAMES);
                close_up_total_ = std::max(close_up_total_, close_up_);
            }
        }

        if (r->Dead == 0)
        {
            if (r->MaxHP > 0
                && hpBefore * 4 > r->MaxHP
                && r->HP * 4 <= r->MaxHP
                && r->LastAttacker
                && r->LastAttacker->Dead == 0
                && r->LastAttacker->Team != r->Team)
            {
                tryForcePull(r, r->Team, r, false);
            }

            if (r->MaxHP > 0
                && hpBefore * 100 > r->MaxHP * 15
                && r->HP * 100 <= r->MaxHP * 15
                && isUnattended(r, 1 - r->Team))
            {
                tryForcePull(r, 1 - r->Team, r, true);
            }
        }
    }

    for (const auto& command : damageApplicationResult.commands)
    {
        applyBattleGameplayCommand(command);
    }
    writeBattleDeathEffectTrackers(damageApplicationResult.deathEffects, battle_roles_, cs);

    for (const auto& event : damageApplicationResult.lifecycleEvents)
    {
        if (event.type != KysChess::Battle::BattleDamageLifecycleEventType::BattleEnded)
        {
            continue;
        }
        if (result_ == -1)
        {
            if (!isManualCameraEnabled())
            {
                close_up_ = std::max(close_up_, CAMERA_BATTLE_END_ZOOM_FRAMES);
                close_up_total_ = std::max(close_up_total_, close_up_);
            }
            frozen_ = 60;
            slow_ = CAMERA_BATTLE_END_SLOW_FRAMES;
            shake_ = 60;
            result_ = event.value;
            tracker_.recordBattleEnd(battle_frame_, event.value);
        }
    }

}

void BattleSceneHades::advanceBattleFrameAfterDamage()
{
    for (auto r : battle_roles_)
    {
        r->HP = GameUtil::limit(r->HP, 0, r->MaxHP);
        r->MP = GameUtil::limit(r->MP, 0, GameUtil::MAX_MP);
        r->PhysicalPower = GameUtil::limit(r->PhysicalPower, 0, 100);
    }

    //处理文字
    {
        for (auto& te : text_effects_)
        {
            if (te.Type == 0) { te.Pos.y -= 2; }
            te.Frame++;
        }
        for (auto it = text_effects_.begin(); it != text_effects_.end();)
        {
            if (it->Frame >= 30)
            {
                it = text_effects_.erase(it);
            }
            else
            {
                it++;
            }
        }
    }

    {
        //人物出场
        while (!enemies_.empty())
        {
            battle_roles_.push_back(enemies_.front());
            enemies_.pop_front();
            // battle_roles_.back()->Attention = 30;
        }
        //亮血条
        if (enemies_.size() < head_boss_.size())
        {
            for (int i = 0; i < head_boss_.size(); i++)
            {
                if (i >= enemies_.size())
                {
                    head_boss_[i]->setVisible(true);
                }
            }
        }
        if (slow_ == 0 && (result_ == 0 || result_ == 1))
        {
            calExpGot();
            setExit(true);
        }
    }
}

void BattleSceneHades::Action(Role* r)
{
    if (r->HaveAction)
    {
        if (r->OperationType != 3)
        {
            r->Velocity = { 0, 0, 0 };
        }
        //音效和动画
        if (r->OperationType >= 0 && r->OperationType <= 3
            //&& r->ActFrame == r->FightFrame[r->ActType] - 3
            && r->ActFrame == calCast(r->ActType, r->OperationType, r))
        {
            //r->HaveAction = 0;
            r->PreActTimer = battle_frame_;
            for (auto m : r->getLearnedMagics())
            {
                if (special_magic_effect_attack_.count(m->Name))
                {
                    special_magic_effect_attack_[m->Name](r);
                }
            }
            Magic* magic = r->UsingMagic;

            auto pendingCast = pending_cast_results_.find(r);
            assert(pendingCast != pending_cast_results_.end());
            assert(magic);
            const auto& castResult = pendingCast->second;
            assert(castResult.decision.canCast);
            KysChess::Battle::BattleActionCommitInput actionInput;
            actionInput.unit = makeBattleActionCommitUnitSnapshot(r);
            actionInput.hasCast = true;
            actionInput.cast = castResult;
            actionInput.blinkRandomRoll = rand_.rand_int(std::numeric_limits<int>::max());
            actionInput.blinkReach = calcBlinkReach(magic);
            actionInput.blinkWeakTargetDefWeight = BLINK_WEAK_TARGET_DEF_WEIGHT;
            actionInput.strengthenedMeleeOperationCountThreshold = 2;
            for (auto role : battle_roles_)
            {
                if (role)
                {
                    actionInput.targets.push_back(makeBattleActionTargetSnapshot(role));
                }
            }

            auto& comboStates = KysChess::ChessCombo::getMutableStates();
            auto comboIt = comboStates.find(r->ID);
            if (comboIt != comboStates.end())
            {
                actionInput.combo = comboIt->second;
            }
            auto actionResult = KysChess::Battle::BattleActionCommitSystem().commit(actionInput);
            if (comboIt != comboStates.end())
            {
                comboIt->second = actionResult.combo;
            }

            for (const auto& command : actionResult.blinkCommands)
            {
                auto* target = findRoleByBattleId(battle_roles_, command.targetUnitId);
                logBattleStatus(r, target, command.selectedWeakest ? "閃擊追殺" : "閃擊突襲");
                auto targetPos45 = pos90To45(target->Pos.x, target->Pos.y);
                auto currentPos45 = pos90To45(r->Pos.x, r->Pos.y);
                int gridReach = std::max(1, static_cast<int>(command.reach / TILE_W) + 1);
                std::vector<Point> cells;
                for (int dx = -gridReach; dx <= gridReach; ++dx)
                {
                    for (int dy = -gridReach; dy <= gridReach; ++dy)
                    {
                        int x = targetPos45.x + dx;
                        int y = targetPos45.y + dy;
                        if (x == currentPos45.x && y == currentPos45.y)
                        {
                            continue;
                        }
                        if (!canWalk45(x, y))
                        {
                            continue;
                        }

                        auto pos = pos45To90(x, y);
                        if (EuclidDis(pos, target->Pos) > command.reach)
                        {
                            continue;
                        }

                        bool occupied = false;
                        for (auto role : battle_roles_)
                        {
                            if (role == r || role->Dead != 0)
                            {
                                continue;
                            }
                            auto rolePos45 = pos90To45(role->Pos.x, role->Pos.y);
                            if (rolePos45.x == x && rolePos45.y == y)
                            {
                                occupied = true;
                                break;
                            }
                        }
                        if (!occupied)
                        {
                            cells.push_back({ x, y });
                        }
                    }
                }

                if (!cells.empty())
                {
                    const auto& cell = cells[rand_.rand_int(static_cast<int>(cells.size()))];
                    int x = cell.x;
                    int y = cell.y;
                    auto pos = pos45To90(x, y);
                    r->setPositionOnly(x, y);
                    r->Pos.x = pos.x;
                    r->Pos.y = pos.y;
                    r->Pos.z = 0;
                    r->Velocity = { 0, 0, 0 };
                    r->Acceleration = { 0, 0, gravity_ };
                    r->FindingWay = 0;
                    Audio::getInstance()->playESound(BLINK_SOUND_EFFECT_ID);
                    setFaceTowardsNearest(r);
                    r->RealTowards = target->Pos - r->Pos;
                    if (r->RealTowards.norm() > 0.01)
                    {
                        r->RealTowards.normTo(1);
                    }
                }
            }

            Audio::getInstance()->playASound(magic->SoundID);
            for (const auto& event : actionResult.presentationEvents)
            {
                presentation_recorder_.recordPresentation(event);
            }
            auto spawnRequests = actionResult.attackSpawnRequests;
            for (auto& request : spawnRequests)
            {
                attachProjectileBouncePrime(request);
                queueCoreAttackSpawn(std::move(request));
            }
            r->OperationCount = actionResult.operationCount;
            applyBattleCastCommit(r, castResult);
            pending_cast_results_.erase(pendingCast);
            ultCasters_.erase(r);
        }

        if (r->UsingItem)
        {
            Item* item = r->UsingItem;
            KysChess::Battle::BattleActionCommitInput actionInput;
            actionInput.unit = makeBattleActionCommitUnitSnapshot(r);
            actionInput.hasItem = true;
            actionInput.item = makeBattleActionItemSnapshot(item);
            actionInput.strengthenedMeleeOperationCountThreshold = 2;
            actionInput.hiddenWeaponTotalFrame = 100;
            auto hiddenWeaponVelocity = r->RealTowards;
            if (auto r0 = findFarthestEnemy(r->Team, r->Pos))
            {
                hiddenWeaponVelocity = r0->Pos - r->Pos;
            }
            hiddenWeaponVelocity.normTo(10);
            actionInput.hiddenWeaponVelocity = hiddenWeaponVelocity;

            auto actionResult = KysChess::Battle::BattleActionCommitSystem().commit(actionInput);
            for (const auto& command : actionResult.itemUseCommands)
            {
                assert(command.itemId == item->ID);
                r->useItem(item);
            }
            auto spawnRequests = actionResult.attackSpawnRequests;
            for (auto& request : spawnRequests)
            {
                attachProjectileBouncePrime(request);
                queueCoreAttackSpawn(std::move(request));
            }
            for (const auto& delta : actionResult.itemCountDeltas)
            {
                Event::getInstance()->addItemWithoutHint(delta.itemId, delta.delta);
            }
            r->UsingItem = nullptr;
        }
        r->ActFrame++;
        if (r->CoolDown > 0 && r->ActType >= 0 && r->OperationType >= 0)
        {
            int recoveryFrames = r->OperationType == 3 ? DASH_MOMENTUM_FRAMES : ACTION_RECOVERY_FRAMES;
            if (r->ActFrame > calCast(r->ActType, r->OperationType, r) + recoveryFrames)
            {
                r->HaveAction = 0;
                r->OperationType = -1;
                r->ActType = -1;
            }
        }
    }
}

void BattleSceneHades::queueCoreSkillAttackSpawn(Role* r, Magic* magic, bool isUltimate, int operationType)
{
    if (!r || !magic)
    {
        return;
    }

    if (auto* target = findNearestEnemy(r->Team, r->Pos))
    {
        const bool forceRanged = roleForcesRangedMagic(r);
        const int forcedRangedMinSelectDistance = getForcedRangedMinSelectDistance(r);
        const int projectileSpeedMultiplierPct = getProjectileSpeedMultiplierPct(r);
        auto& comboStates = KysChess::ChessCombo::getMutableStates();
        auto comboIt = comboStates.find(r->ID);
        const bool dashAttackEnabled = comboIt != comboStates.end() && comboIt->second.dashAttack;

        int dashHitCount = 1;
        const double multiHitScore = (r->Speed + r->getActProperty(magic->MagicType)) / 180.0;
        if (rand_.rand() < multiHitScore)
        {
            dashHitCount++;
        }
        if (rand_.rand() < multiHitScore * 0.5)
        {
            dashHitCount++;
        }

        Pointf dashVelocity = target->Pos - r->Pos;
        if (dashVelocity.norm() > 0.01)
        {
            dashVelocity.normTo(MELEE_ATTACK_HIT_RADIUS / DASH_MOMENTUM_FRAMES);
        }

        BattleCastSkillAdapterInput skill;
        skill.magic = magic;
        skill.reach = std::min(
            effectiveBattleReach(magic, forceRanged, forcedRangedMinSelectDistance, projectileSpeedMultiplierPct),
            MAX_EFFECTIVE_BATTLE_REACH);
        skill.forceRanged = forceRanged;
        skill.rangedStyle = isRangedStyleMagic(magic, forceRanged);
        skill.projectileSpeedMultiplierPct = projectileSpeedMultiplierPct;
        skill.meleeSplashCount = isUltimate && magic->AttackAreaType == 0 ? 1 : 0;
        skill.extraProjectileCount = isUltimate ? getUltimateExtraProjectileCount(r) : 0;

        BattleCastAdapterInput castAdapterInput;
        castAdapterInput.unit = r;
        castAdapterInput.target = target;
        castAdapterInput.normalSkill = skill;
        castAdapterInput.ultimateSkill = skill;
        castAdapterInput.canStartAttack = true;
        castAdapterInput.dashAttackEnabled = dashAttackEnabled;
        castAdapterInput.dashVelocity = dashVelocity;
        castAdapterInput.dashHitCount = dashHitCount;
        castAdapterInput.emitDashFollowUpSkillAttack = dashAttackEnabled;
        castAdapterInput.dashFollowUpOperationType = getOperationType(magic->AttackAreaType);
        castAdapterInput.targetDistance = EuclidDis(r->Pos, target->Pos);
        castAdapterInput.meleeAttackReach = MELEE_ATTACK_REACH;
        castAdapterInput.dashAttackReach = DASH_ATTACK_MELEE_REACH;
        castAdapterInput.operationCount = r->OperationCount;
        castAdapterInput.cooldownReductionPct = comboIt != comboStates.end() ? comboIt->second.cdrPct : 0;

        const int resolvedOperationType = operationType >= 0 ? operationType : getOperationType(magic->AttackAreaType);
        auto castResult = KysChess::Battle::BattleCastPlanner().commitSelectedCast(
            makeBattleCastInput(castAdapterInput),
            isUltimate,
            KysChess::Battle::battleOperationFromLegacy(resolvedOperationType));

        KysChess::Battle::BattleActionCommitInput actionInput;
        actionInput.unit = makeBattleActionCommitUnitSnapshot(r);
        actionInput.hasCast = true;
        actionInput.cast = castResult;
        actionInput.strengthenedMeleeOperationCountThreshold = 2;
        if (comboIt != comboStates.end())
        {
            actionInput.combo = comboIt->second;
        }
        auto actionResult = KysChess::Battle::BattleActionCommitSystem().commit(actionInput);
        if (comboIt != comboStates.end())
        {
            comboIt->second = actionResult.combo;
        }

        Audio::getInstance()->playASound(magic->SoundID);
        for (const auto& event : actionResult.presentationEvents)
        {
            presentation_recorder_.recordPresentation(event);
        }
        auto spawnRequests = actionResult.attackSpawnRequests;
        for (auto& request : spawnRequests)
        {
            attachProjectileBouncePrime(request);
            queueCoreAttackSpawn(std::move(request));
        }
        r->OperationCount = actionResult.operationCount;
        return;
    }

    return;
}

std::vector<KysChess::Battle::BattleTeamEffectEvent> BattleSceneHades::commitLegacyTeamEffect(
    const LegacyTeamEffectCommitRequest& request)
{
    assert(request.sourceUnitId >= 0);

    auto& cs = KysChess::ChessCombo::getMutableStates();
    auto world = makeBattleTeamEffectWorld(battle_roles_, cs);
    std::vector<KysChess::Battle::BattleTeamEffectEvent> events;
    switch (request.type)
    {
    case LegacyTeamEffectCommitType::Heal:
        events = KysChess::Battle::BattleTeamEffectSystem().applyTeamHeal(
            world,
            request.sourceUnitId,
            request.flatHeal,
            request.pctHeal);
        break;
    case LegacyTeamEffectCommitType::MpRestore:
        events = KysChess::Battle::BattleTeamEffectSystem().applyTeamMp(
            world,
            request.sourceUnitId,
            request.amount);
        break;
    case LegacyTeamEffectCommitType::Shield:
        events = KysChess::Battle::BattleTeamEffectSystem().applyTeamShield(
            world,
            request.sourceUnitId,
            request.amount,
            request.refreshOnly);
        break;
    default:
        assert(false);
    }
    writeBattleTeamEffectWorld(world, battle_roles_, cs);
    return events;
}

void BattleSceneHades::playCommittedTeamEffectEvents(
    Role* source,
    const std::vector<KysChess::Battle::BattleTeamEffectEvent>& events,
    const char* reason)
{
    assert(source);
    assert(reason);

    for (const auto& event : events)
    {
        auto ally = findRoleByBattleId(battle_roles_, event.targetUnitId);
        switch (event.type)
        {
        case KysChess::Battle::BattleTeamEffectEventType::Heal:
            addRoleEffect(ally, KysChess::EFT_HEAL, ROLE_STATUS_EFT_FRAMES);
            logBattleHeal(source, ally, event.value, reason);
            break;
        case KysChess::Battle::BattleTeamEffectEventType::MpRestore:
            logBattleStatus(source, ally, std::format("{}+{}MP", reason, event.value));
            break;
        case KysChess::Battle::BattleTeamEffectEventType::ShieldGain:
            logBattleStatus(source, ally, formatStatusValue(reason, event.value, "護盾"));
            break;
        default:
            assert(false);
        }
    }
}

int BattleSceneHades::getSharedBleedMaxStacks(Role* source) const
{
    if (!source)
    {
        return 1;
    }

    auto& cs = KysChess::ChessCombo::getActiveStates();
    auto it = cs.find(source->ID);
    if (it == cs.end() || it->second.bleedChancePct <= 0)
    {
        return 1;
    }

    return std::max(1, it->second.bleedMaxStacks);
}

bool BattleSceneHades::isLastAliveInTeam(Role* role) const
{
    assert(role);
    for (auto ally : battle_roles_)
    {
        if (ally->Team == role->Team && ally != role && ally->Dead == 0)
        {
            return false;
        }
    }
    return true;
}

void BattleSceneHades::applyComboFrameRuntimeEvent(
    Role* role,
    KysChess::RoleComboState& state,
    const KysChess::Battle::BattleComboFrameRuntimeEvent& event,
    std::map<int, KysChess::RoleComboState>& comboStates)
{
    assert(role);

    switch (event.type)
    {
    case KysChess::Battle::BattleComboFrameRuntimeEventType::AutoUltimateReady:
        if (auto magic = triggerAutoUltimate(role, false))
        {
            addFloatingText(role, std::string(magic->Name), { 255, 215, 0, 255 }, EMPHASIS_TEXT_SIZE);
            logBattleStatus(role, nullptr, std::format("自動絕招·{}", std::string(magic->Name)));
        }
        break;
    case KysChess::Battle::BattleComboFrameRuntimeEventType::SelfHpRegen:
    {
        assert(event.value > 0);
        auto world = makeBattleTeamEffectWorld(battle_roles_, comboStates);
        auto events = KysChess::Battle::BattleTeamEffectSystem().applySelfHeal(world, role->ID, event.value);
        writeBattleTeamEffectWorld(world, battle_roles_, comboStates);
        for (const auto& teamEvent : events)
        {
            assert(teamEvent.type == KysChess::Battle::BattleTeamEffectEventType::Heal);
            logBattleHeal(role, role, teamEvent.value, "生命回復");
        }
        break;
    }
    case KysChess::Battle::BattleComboFrameRuntimeEventType::HealAura:
    {
        auto world = makeBattleTeamEffectWorld(battle_roles_, comboStates);
        auto events = KysChess::Battle::BattleTeamEffectSystem().applyHealAura(
            world,
            role->ID,
            event.value,
            event.value2,
            TILE_W * 6.0,
            event.durationFrames);
        writeBattleTeamEffectWorld(world, battle_roles_, comboStates);
        for (const auto& teamEvent : events)
        {
            if (teamEvent.type == KysChess::Battle::BattleTeamEffectEventType::Heal)
            {
                logBattleHeal(role, findRoleByBattleId(battle_roles_, teamEvent.targetUnitId), teamEvent.value, "治療光環");
            }
            else
            {
                assert(teamEvent.type == KysChess::Battle::BattleTeamEffectEventType::CooldownReduced);
            }
        }
        break;
    }
    case KysChess::Battle::BattleComboFrameRuntimeEventType::HealPercentSelf:
    {
        assert(event.value > 0);
        int hpBefore = role->HP;
        role->HP = std::min(role->MaxHP, role->HP + role->MaxHP * event.value / 100);
        if (role->HP > hpBefore)
        {
            addRoleEffect(role, KysChess::EFT_HEAL, ROLE_STATUS_EFT_FRAMES);
            logBattleHeal(role, role, role->HP - hpBefore, "爆發治療");
        }
        break;
    }
    case KysChess::Battle::BattleComboFrameRuntimeEventType::BroadcastTriggerTimer:
        assert(event.durationFrames > 0);
        for (auto ally : battle_roles_)
        {
            if (ally->Team == role->Team && ally != role && ally->Dead == 0)
            {
                auto allyState = comboStates.find(ally->ID);
                assert(allyState != comboStates.end());
                allyState->second.triggerTimers[event.trigger] = event.durationFrames;
            }
        }
        break;
    case KysChess::Battle::BattleComboFrameRuntimeEventType::PostSkillInvincibility:
    {
        assert(event.value > 0);
        KysChess::Battle::BattleEffectWorld world;
        world.units.push_back({
            role->ID,
            role->Team,
            role->Dead == 0,
            role->HP,
            role->MaxHP,
            role->MP,
            role->MaxMP,
            role->CoolDown,
            role->Invincible,
            state.shield,
        });
        KysChess::Battle::BattleEffectRegistry registry;
        KysChess::Battle::BattleEffectDispatcher dispatcher(registry);
        dispatcher.addEffect({
            role->ID,
            "技能後無敵",
            KysChess::Battle::BattleHook::SkillFinished,
            KysChess::Battle::BattleEffectTarget::Source,
            "技能後無敵",
            event.value,
        });
        auto commands = dispatcher.dispatch(
            world,
            { KysChess::Battle::BattleHook::SkillFinished, role->ID, -1 });
        role->Invincible = world.units.front().invincible;
        for (const auto& command : commands)
        {
            assert(command.type == KysChess::Battle::BattleEffectCommandType::AddInvincibility);
            logBattleStatus(role, role, formatStatusFrames("技能後無敵", command.value));
        }
        break;
    }
    default:
        assert(false);
        break;
    }
}

int BattleSceneHades::getUltimateExtraProjectileCount(Role* r)
{
    assert(r);
    auto& cs = KysChess::ChessCombo::getMutableStates();
    auto it = cs.find(r->ID);
    assert(it != cs.end());

    auto& s = it->second;
    return KysChess::Battle::BattleComboTriggerSystem().collectExtraProjectileCount(
        s,
        { KysChess::Battle::BattleComboTriggerHook::AfterSkillCast, r->ID, -1 },
        std::max(0, s.ultimateExtraProjectiles),
        []() { return 0.0; });
}

void BattleSceneHades::spawnUltimateExtraProjectiles(const KysChess::Battle::BattleAttackEvent& prototype, int extraCount)
{
    assert(extraCount > 0);
    assert(prototype.ultimate);
    assert(prototype.sourceUnitId >= 0);

    spawnExtraProjectiles(prototype, extraCount, "絕招追加彈", nullptr);
}

void BattleSceneHades::spawnHitExtraProjectiles(const KysChess::Battle::BattleAttackEvent& prototype, int extraCount, Role* target)
{
    assert(extraCount > 0);
    assert(prototype.sourceUnitId >= 0);

    spawnExtraProjectiles(prototype, extraCount, "命中追加彈", target);
}

void BattleSceneHades::spawnExtraProjectiles(
    const KysChess::Battle::BattleAttackEvent& prototype,
    int extraCount,
    const char* logLabel,
    Role* target)
{
    assert(extraCount > 0);
    assert(prototype.sourceUnitId >= 0);
    assert(logLabel);

    auto* attacker = findRoleByBattleId(battle_roles_, prototype.sourceUnitId);
    auto* usingMagic = prototype.skillId >= 0 ? Save::getInstance()->getMagic(prototype.skillId) : nullptr;
    if (usingMagic)
    {
        logBattleStatus(attacker, target,
            std::format("{}·{}（{}發）", logLabel, usingMagic->Name, extraCount));
    }
    else
    {
        logBattleStatus(attacker, target,
            std::format("{}（{}發）", logLabel, extraCount));
    }

    spawnTrackingProjectileSpread(prototype, extraCount);
}

void BattleSceneHades::spawnNearbyTrackingProjectiles(const KysChess::Battle::BattleAttackEvent& prototype,
    Role* centerTarget,
    int rangePixels,
    int damagePct)
{
    assert(prototype.sourceUnitId >= 0);
    assert(centerTarget);
    assert(rangePixels > 0);

    auto* attacker = findRoleByBattleId(battle_roles_, prototype.sourceUnitId);
    auto targetIds = KysChess::Battle::BattleProjectileTargetingSystem().selectNearbyTargets(
        makeBattleProjectileTargetWorld(battle_roles_),
        attacker->ID,
        centerTarget->ID,
        rangePixels);
    if (targetIds.empty())
    {
        return;
    }

    auto* usingMagic = prototype.skillId >= 0 ? Save::getInstance()->getMagic(prototype.skillId) : nullptr;
    if (usingMagic)
    {
        logBattleStatus(attacker, centerTarget,
            std::format("範圍追蹤彈·{}（{}發）", usingMagic->Name, targetIds.size()));
    }
    else
    {
        logBattleStatus(attacker, centerTarget,
            std::format("範圍追蹤彈（{}發）", targetIds.size()));
    }

    double projectileSpeed = PROJECTILE_SPEED * getProjectileSpeedMultiplierPct(attacker) / 100.0;
    double damageScale = std::max(1, damagePct) / 100.0;
    for (int targetId : targetIds)
    {
        auto target = findRoleByBattleId(battle_roles_, targetId);
        KysChess::Battle::BattleAttackSpawnRequest request;
        request.initial.attackerUnitId = attacker->ID;
        request.initial.skillId = prototype.skillId;
        request.initial.preferredTargetUnitId = target->ID;
        request.initial.requirePreferredTarget = true;
        request.initial.track = true;
        request.initial.operationType = KysChess::Battle::BattleOperationType::RangedProjectile;
        request.initial.visualEffectId = prototype.visualEffectId;
        request.initial.ultimate = prototype.ultimate;
        request.initial.ignoreProjectileCancel = prototype.skillId < 0;
        request.initial.hiddenWeaponItemId = prototype.hiddenWeaponItemId;
        request.initial.scriptedDamage = prototype.scriptedDamage;
        request.initial.scriptedStunFrames = prototype.scriptedStunFrames;
        request.initial.scriptedBleedStacks = prototype.scriptedBleedStacks;
        request.initial.sharedHitGroupId = prototype.sharedHitGroupId;
        request.initial.strengthMultiplier = prototype.strengthMultiplier * static_cast<float>(damageScale);
        request.initial.suppressNearbyTrackingProjectileProc = true;
        request.initial.mainProjectile = false;
        request.initial.position = prototype.position;
        request.initial.velocity = target->Pos - request.initial.position;
        if (request.initial.velocity.norm() <= 0.01)
        {
            request.initial.velocity = attacker->RealTowards;
        }
        if (request.initial.velocity.norm() <= 0.01)
        {
            request.initial.velocity = { 1, 0, 0 };
        }
        request.initial.velocity.normTo(projectileSpeed);
        request.initial.totalFrame = std::max(20,
            static_cast<int>(std::ceil(EuclidDis(target->Pos, request.initial.position) / std::max(1.0, projectileSpeed))) + 18);
        attachProjectileBouncePrime(request);
        queueCoreAttackSpawn(std::move(request));
    }
}

void BattleSceneHades::spawnSpiralBleedProjectiles(Role* attacker, int bleedStacks, int projectileCount)
{
    if (!attacker || bleedStacks <= 0 || projectileCount <= 0)
    {
        return;
    }

    Pointf forward = attacker->RealTowards.norm() > 0.01 ? attacker->RealTowards : Pointf{1, 0, 0};
    forward.normTo(1);
    float baseAngle = static_cast<float>(forward.getAngle());
    float angularVelocity = 0.42f;
    float radiusGrowth = static_cast<float>(PROJECTILE_SPEED * getProjectileSpeedMultiplierPct(attacker) / 100.0 * 0.9);
    int totalFrames = 35;
    int sharedHitGroupId = next_shared_hit_group_id_++;
    shared_hit_group_targets_[sharedHitGroupId].clear();

    for (int i = 0; i < projectileCount; ++i)
    {
        KysChess::Battle::BattleAttackSpawnRequest request;
        request.initial.attackerUnitId = attacker->ID;
        request.initial.operationType = KysChess::Battle::BattleOperationType::RangedProjectile;
        request.initial.visualEffectId = 48;
        request.initial.position = attacker->Pos;
        request.initial.totalFrame = totalFrames;
        request.initial.scriptedBleedStacks = bleedStacks;
        request.initial.sharedHitGroupId = sharedHitGroupId;
        request.initial.ignoreProjectileCancel = true;
        request.initial.through = true;
        request.spiralMotion = true;
        request.spiralCenter = attacker->Pos;
        request.spiralRadius = 0.0f;
        request.spiralRadiusGrowth = radiusGrowth;
        request.spiralAngle = baseAngle + static_cast<float>(2.0 * M_PI * i / projectileCount);
        request.spiralAngularVelocity = angularVelocity;
        queueCoreAttackSpawn(std::move(request));
    }
}

void BattleSceneHades::refreshEnemyTopDebuffs()
{
    auto& cs = KysChess::ChessCombo::getMutableStates();

    int liveAllies = 0;
    int topTargets = 0;
    int perMemberValue = 0;
    for (const auto& ally : friends_obj_)
    {
        if (ally.Dead != 0)
        {
            continue;
        }

        auto sit = cs.find(ally.ID);
        if (sit == cs.end() || sit->second.enemyTopDebuffCount <= 0)
        {
            continue;
        }

        liveAllies++;
        topTargets = std::max(topTargets, sit->second.enemyTopDebuffCount);
        perMemberValue = std::max(perMemberValue, sit->second.enemyTopDebuffValue);
    }

    if (liveAllies <= 0 || topTargets <= 0 || perMemberValue <= 0)
    {
        topTargets = std::max(topTargets, 0);
    }

    std::vector<size_t> enemyOrder;
    enemyOrder.reserve(enemies_obj_.size());
    for (size_t i = 0; i < enemies_obj_.size(); ++i)
    {
        if (enemies_obj_[i].Dead == 0)
        {
            enemyOrder.push_back(i);
        }
    }

    auto enemySortScore = [&](size_t index) -> long long
    {
        int tier = index < enemies_obj_.size() ? enemies_obj_[index].Cost : 0;
        int star = index < enemy_stars_.size() ? enemy_stars_[index] : 0;
        if (tier <= 0)
        {
            return 0;
        }

        long long score = 1;
        for (int i = 0; i < star; ++i)
        {
            score *= tier;
        }
        return score;
    };

    std::stable_sort(enemyOrder.begin(), enemyOrder.end(), [&](size_t l, size_t r)
        {
            long long lScore = enemySortScore(l);
            long long rScore = enemySortScore(r);
            if (lScore != rScore)
            {
                return lScore > rScore;
            }

            int lHP = l < enemies_obj_.size() ? enemies_obj_[l].MaxHP : 0;
            int rHP = r < enemies_obj_.size() ? enemies_obj_[r].MaxHP : 0;
            return lHP > rHP;
        });

    int assignedTargets = 0;
    for (size_t index : enemyOrder)
    {
        auto& enemy = enemies_obj_[index];
        auto& state = cs[enemy.ID];
        int desired = 0;
        if (assignedTargets < topTargets && liveAllies > 0 && perMemberValue > 0)
        {
            desired = perMemberValue * liveAllies;
            ++assignedTargets;
        }

        int delta = desired - state.enemyTopDebuffApplied;
        if (delta != 0)
        {
            int change = delta > 0 ? delta : -delta;
            enemy.Attack = std::max(0, enemy.Attack - delta);
            enemy.Defence = std::max(0, enemy.Defence - delta);
            state.enemyTopDebuffApplied = desired;
            logBattleStatus(nullptr,
                &enemy,
                std::format("陰险：前{}名攻防{}{}（{}名存活）",
                    topTargets,
                    delta > 0 ? "-" : "+",
                    change,
                    liveAllies));
        }
    }
}

void BattleSceneHades::spawnAreaImpactProjectiles(Role* attacker,
    Role* origin,
    int areaSize,
    int eftId,
    int damage,
    int stunFrames,
    Role* trackedTarget,
    int maxTargets)
{
    assert(origin);
    assert(damage > 0);

    Role* source = attacker ? attacker : origin;
    auto spawnImpactProjectile = [&](Role* target, bool guaranteedTarget = false)
    {
        if (!target || target == origin || target->Dead != 0 || target->Team == origin->Team)
        {
            return;
        }

        auto direction = target->Pos - origin->Pos;
        if (direction.norm() <= 0.01)
        {
            direction = { 1, 0, 0 };
        }
        direction.normTo(1);

        auto spawnOffset = direction;
        spawnOffset.normTo(TILE_W * 1.5);
        KysChess::Battle::BattleAttackSpawnRequest request;
        request.initial.attackerUnitId = source->ID;
        request.initial.preferredTargetUnitId = target->ID;
        request.initial.scriptedDamage = damage;
        request.initial.scriptedStunFrames = stunFrames;
        request.initial.track = true;
        request.initial.operationType = KysChess::Battle::BattleOperationType::RangedProjectile;
        request.initial.ignoreProjectileCancel = true;
        request.initial.visualEffectId = eftId;
        request.initial.position = origin->Pos + spawnOffset;
        request.initial.velocity = target->Pos - request.initial.position;
        if (request.initial.velocity.norm() <= 0.01)
        {
            request.initial.velocity = direction;
        }
        request.initial.velocity.normTo(PROJECTILE_SPEED);
        int travelFrames = static_cast<int>(std::ceil(EuclidDis(target->Pos, request.initial.position) / static_cast<double>(PROJECTILE_SPEED)));
        request.initial.totalFrame = std::max(15, travelFrames + 15);
        queueCoreAttackSpawn(std::move(request));
    };

    auto targetIds = KysChess::Battle::BattleProjectileTargetingSystem().selectAreaImpactTargets(
        makeBattleProjectileTargetWorld(battle_roles_),
        origin->ID,
        areaSize,
        maxTargets,
        trackedTarget ? trackedTarget->ID : -1);

    for (int targetId : targetIds)
    {
        spawnImpactProjectile(findRoleByBattleId(battle_roles_, targetId));
    }
}

void BattleSceneHades::spawnTrackingProjectileSpread(const KysChess::Battle::BattleAttackEvent& prototype,
    int projectileCount,
    int initialFrame,
    int frameStep,
    int randomFrameRange)
{
    assert(projectileCount > 0);
    assert(prototype.sourceUnitId >= 0);

    auto* attacker = findRoleByBattleId(battle_roles_, prototype.sourceUnitId);
    auto direction = prototype.velocity;
    double speed = direction.norm();
    if (speed <= 0.01)
    {
        speed = (prototype.operationType == KysChess::Battle::BattleOperationType::RangedProjectile) ? PROJECTILE_SPEED : 3.0;
    }

    if (direction.norm() <= 0.01)
    {
        direction = attacker->RealTowards;
        if (direction.norm() <= 0.01)
        {
            if (auto target = findNearestEnemy(attacker->Team, prototype.position))
            {
                direction = target->Pos - prototype.position;
            }
        }
    }
    assert(direction.norm() > 0.01);

    direction.normTo(1);
    double baseAngle = direction.getAngle();
    double spreadRadians = ((prototype.track
                                || prototype.operationType == KysChess::Battle::BattleOperationType::TrackingProjectile
                                || prototype.operationType == KysChess::Battle::BattleOperationType::RangedProjectile)
                                ? 12.0
                                : 20.0)
        / 180.0 * M_PI;
    Role* primaryTarget = findNearestEnemy(attacker->Team, prototype.position);
    double maxTravel = speed * std::max(1, prototype.totalFrame - prototype.frame) + TILE_W * 2;
    std::vector<Role*> alternateTargets;
    for (auto enemy : battle_roles_)
    {
        if (!enemy || enemy->Dead != 0 || enemy->Team == attacker->Team || enemy == primaryTarget)
        {
            continue;
        }
        if (EuclidDis(enemy->Pos, prototype.position) > maxTravel)
        {
            continue;
        }
        alternateTargets.push_back(enemy);
    }
    std::sort(alternateTargets.begin(), alternateTargets.end(), [&](Role* lhs, Role* rhs)
        {
            auto lhsDir = lhs->Pos - prototype.position;
            auto rhsDir = rhs->Pos - prototype.position;
            double lhsAngle = lhsDir.norm() > 0.01 ? lhsDir.getAngle() : baseAngle;
            double rhsAngle = rhsDir.norm() > 0.01 ? rhsDir.getAngle() : baseAngle;
            double lhsDelta = std::abs(std::atan2(std::sin(lhsAngle - baseAngle), std::cos(lhsAngle - baseAngle)));
            double rhsDelta = std::abs(std::atan2(std::sin(rhsAngle - baseAngle), std::cos(rhsAngle - baseAngle)));
            if (lhsDelta != rhsDelta)
            {
                return lhsDelta < rhsDelta;
            }
            return EuclidDis(lhs->Pos, prototype.position) < EuclidDis(rhs->Pos, prototype.position);
        });

    for (int i = 0; i < projectileCount; ++i)
    {
        Role* assignedTarget = i < static_cast<int>(alternateTargets.size()) ? alternateTargets[i] : primaryTarget;
        double angle = baseAngle;
        int preferredTargetId = -1;
        if (assignedTarget)
        {
            auto targetDir = assignedTarget->Pos - prototype.position;
            if (targetDir.norm() > 0.01)
            {
                angle = targetDir.getAngle();
            }
            preferredTargetId = assignedTarget->ID;
        }
        else if (projectileCount > 1)
        {
            double offset = (i - (projectileCount - 1) / 2.0) * spreadRadians;
            angle += offset;
        }

        Pointf velocity = { static_cast<float>(std::cos(angle)), static_cast<float>(std::sin(angle)) };
        velocity.normTo(speed);
        int requestFrame = initialFrame + i * frameStep;
        if (randomFrameRange > 0)
        {
            requestFrame = initialFrame + rand_.rand_int(randomFrameRange);
        }
        bool requestTracks = prototype.track
            || prototype.operationType == KysChess::Battle::BattleOperationType::Melee
            || prototype.operationType == KysChess::Battle::BattleOperationType::Dash;
        int totalFrame = prototype.totalFrame;
        if (prototype.operationType == KysChess::Battle::BattleOperationType::Melee
            || prototype.operationType == KysChess::Battle::BattleOperationType::Dash)
        {
            totalFrame = std::max(totalFrame, 60);
        }

        KysChess::Battle::BattleAttackSpawnRequest request;
        request.initial.attackerUnitId = attacker->ID;
        request.initial.skillId = prototype.skillId;
        request.initial.preferredTargetUnitId = preferredTargetId;
        request.initial.requirePreferredTarget = false;
        request.initial.totalFrame = totalFrame;
        request.initial.track = requestTracks;
        request.initial.through = prototype.through;
        request.initial.ultimate = prototype.ultimate;
        request.initial.ignoreProjectileCancel = prototype.skillId < 0;
        request.initial.sharedHitGroupId = prototype.sharedHitGroupId;
        request.initial.visualEffectId = prototype.visualEffectId;
        request.initial.operationType = prototype.operationType;
        request.initial.hiddenWeaponItemId = prototype.hiddenWeaponItemId;
        request.initial.scriptedDamage = prototype.scriptedDamage;
        request.initial.scriptedStunFrames = prototype.scriptedStunFrames;
        request.initial.scriptedBleedStacks = prototype.scriptedBleedStacks;
        request.initial.strengthMultiplier = prototype.strengthMultiplier;
        request.initial.suppressNearbyTrackingProjectileProc = prototype.suppressNearbyTrackingProjectileProc;
        request.initial.mainProjectile = false;
        request.initial.position = prototype.position;
        request.initial.velocity = velocity;
        request.initialFrame = requestFrame;
        queueCoreAttackSpawn(std::move(request));
    }
}

void BattleSceneHades::AI(Role* r)
{
    if (r->Dead != 0 || r->HaveAction)
    {
        return;
    }

    bool canStartAttack = r->CoolDown == 0;
    bool isUltimate = r->MP >= r->MaxMP;
    Magic* equippedMagic = r->UsingMagic;
    Magic* normalMagic = equippedMagic ? equippedMagic : selectMagic(r, std::less<double>{ });
    Magic* ultimateMagic = equippedMagic ? equippedMagic : selectMagic(r, std::greater<double>{ });

    auto& movementRuntime = movement_runtime_[r];
    Role* r0 = findNearestEnemy(r->Team, r->Pos);
    if (!r0)
    {
        r->Velocity = { 0, 0, 0 };
        r->FindingWay = 0;
        r->OperationType = -1;
        return;
    }

    double enemyDistance = EuclidDis(r->Pos, r0->Pos);
    bool dashAttackEnabled = false;
    int cooldownReductionPct = 0;
    {
        auto& dcs = KysChess::ChessCombo::getMutableStates();
        auto dit = dcs.find(r->ID);
        if (dit != dcs.end())
        {
            dashAttackEnabled = dit->second.dashAttack;
            cooldownReductionPct = dit->second.cdrPct;
        }
    }

    const int forcedRangedMinSelectDistance = getForcedRangedMinSelectDistance(r);
    const int projectileSpeedMultiplierPct = getProjectileSpeedMultiplierPct(r);
    auto makeSkillInput = [&](Magic* magic, bool ultimate)
    {
        BattleCastSkillAdapterInput skill;
        if (!magic)
        {
            return skill;
        }
        const bool forceRanged = roleForcesRangedMagic(r);
        skill.magic = magic;
        skill.reach = std::min(
            effectiveBattleReach(magic, forceRanged, forcedRangedMinSelectDistance, projectileSpeedMultiplierPct),
            MAX_EFFECTIVE_BATTLE_REACH);
        skill.forceRanged = forceRanged;
        skill.rangedStyle = isRangedStyleMagic(magic, forceRanged);
        skill.projectileSpeedMultiplierPct = projectileSpeedMultiplierPct;
        skill.meleeSplashCount = ultimate && magic->AttackAreaType == 0 ? 1 : 0;
        skill.extraProjectileCount = ultimate ? getUltimateExtraProjectileCount(r) : 0;
        return skill;
    };

    Magic* selectedMagic = isUltimate && ultimateMagic ? ultimateMagic : normalMagic;
    int dashHitCount = 1;
    if (selectedMagic)
    {
        const double multiHitScore = (r->Speed + r->getActProperty(selectedMagic->MagicType)) / 180.0;
        if (rand_.rand() < multiHitScore)
        {
            dashHitCount++;
        }
        if (rand_.rand() < multiHitScore * 0.5)
        {
            dashHitCount++;
        }
    }

    Pointf dashVelocity = r0->Pos - r->Pos;
    if (dashVelocity.norm() > 0.01)
    {
        dashVelocity.normTo(MELEE_ATTACK_HIT_RADIUS / DASH_MOMENTUM_FRAMES);
    }

    BattleCastAdapterInput castAdapterInput;
    castAdapterInput.unit = r;
    castAdapterInput.target = r0;
    castAdapterInput.normalSkill = makeSkillInput(normalMagic, false);
    castAdapterInput.ultimateSkill = makeSkillInput(ultimateMagic, true);
    castAdapterInput.canStartAttack = canStartAttack;
    castAdapterInput.movementDashActive = movementRuntime.movement_dash_frames > 0;
    castAdapterInput.dashAttackEnabled = dashAttackEnabled;
    castAdapterInput.dashVelocity = dashVelocity;
    castAdapterInput.dashHitCount = dashHitCount;
    castAdapterInput.emitDashFollowUpSkillAttack = dashAttackEnabled && selectedMagic;
    castAdapterInput.dashFollowUpOperationType = selectedMagic ? getOperationType(selectedMagic->AttackAreaType) : -1;
    castAdapterInput.targetDistance = enemyDistance;
    castAdapterInput.meleeAttackReach = MELEE_ATTACK_REACH;
    castAdapterInput.dashAttackReach = DASH_ATTACK_MELEE_REACH;
    castAdapterInput.operationCount = r->OperationCount;
    castAdapterInput.cooldownReductionPct = cooldownReductionPct;

    auto castResult = KysChess::Battle::BattleCastPlanner().plan(makeBattleCastInput(castAdapterInput));
    if (castResult.decision.equipSkill)
    {
        r->UsingMagic = castResult.decision.ultimate ? ultimateMagic : normalMagic;
    }

    if (movementRuntime.movement_dash_frames > 0)
    {
        return;
    }

    auto faceTargetAndHold = [&]()
    {
        auto holdDirection = r0->Pos - r->Pos;
        if (holdDirection.norm() > 0.01)
        {
            holdDirection.normTo(1);
            r->RealTowards = holdDirection;
        }
        r->Velocity = { 0, 0, 0 };
        r->FindingWay = 0;
        r->OperationType = -1;
    };

    auto coreDecisionIt = core_movement_decisions_.find(r);
    if (coreDecisionIt != core_movement_decisions_.end())
    {
        const auto& coreDecision = coreDecisionIt->second;
        bool coreWantsMove = coreDecision.action == KysChess::Battle::MovementAction::Move
            || coreDecision.action == KysChess::Battle::MovementAction::Dash;
        auto coreVelocity = coreDecision.velocity;
        if (coreWantsMove && coreVelocity.norm() > 0.01)
        {
            r->OperationType = -1;
            r->FindingWay = 0;
            r->Velocity = coreDecision.velocity;
            r->RealTowards = coreDecision.velocity;
            r->RealTowards.normTo(1);
            if (coreDecision.action == KysChess::Battle::MovementAction::Dash
                && movementRuntime.movement_dash_frames <= 0)
            {
                // 只在新滑步開始時重設計時；持續中的滑步由物理更新逐格扣除。
                movementRuntime.movement_dash_frames = DASH_MOMENTUM_FRAMES;
                movementRuntime.movement_dash_cooldown = MOVEMENT_DASH_COOLDOWN_FRAMES;
                movementRuntime.movement_dash_spread_frames = 0;
            }
            return;
        }
    }

    r->RealTowards = r0->Pos - r->Pos;
    if (r->RealTowards.norm() > 0.01)
    {
        r->RealTowards.normTo(1);
    }

    if (castResult.decision.canCast)
    {
        r->UsingMagic = castResult.decision.ultimate ? ultimateMagic : normalMagic;
        assert(r->UsingMagic);
        applyBattleCastStart(r, castResult, r->UsingMagic->MagicType);
        pending_cast_results_[r] = std::move(castResult);
        movementRuntime.movement_dash_spread_frames = 0;
    }

    if (!r->HaveAction)
    {
        faceTargetAndHold();
    }
}

void BattleSceneHades::onPressedCancel()
{
}

void BattleSceneHades::renderExtraRoleInfo(Role* r, double x, double y)
{
    if (r == nullptr || r->Dead)
    {
        return;
    }

    auto& comboStates = KysChess::ChessCombo::getActiveStates();
    const KysChess::RoleComboState* comboState = nullptr;
    if (auto it = comboStates.find(r->ID); it != comboStates.end())
    {
        comboState = &it->second;
    }

    // 画个血条
    Color outline_color = { 0, 0, 0, 128 };
    const int barLeft = int(std::round(x - ROLE_STATUS_BAR_WIDTH / 2.0));

    auto renderOutline = [&](int bar_x, int bar_y, int width, int height, Color color, int alpha)
    {
        Rect top = { bar_x - 1, bar_y - 1, width + 2, 1 };
        Rect bottom = { bar_x - 1, bar_y + height, width + 2, 1 };
        Rect left = { bar_x - 1, bar_y, 1, height };
        Rect right = { bar_x + width, bar_y, 1, height };
        Engine::getInstance()->renderSquareTexture(&top, color, alpha);
        Engine::getInstance()->renderSquareTexture(&bottom, color, alpha);
        Engine::getInstance()->renderSquareTexture(&left, color, alpha);
        Engine::getInstance()->renderSquareTexture(&right, color, alpha);
    };

    auto renderBar = [&](int bar_y, int cur, int max, Color background_color, Color shadow_color)
    {
        if (max <= 0)
        {
            return;
        }

        double perc = std::clamp(static_cast<double>(cur) / max, 0.0, 1.0);
        double alpha = 1;

        // Draw full background bar (max HP/MP) - shadow
        Rect r_bg = { barLeft, bar_y, ROLE_STATUS_BAR_WIDTH, ROLE_STATUS_BAR_HEIGHT };
        Engine::getInstance()->renderSquareTexture(&r_bg, shadow_color, 128 * alpha);

        // Draw current HP/MP bar on top (percentage of max)
        Rect r1 = { barLeft, bar_y, int(perc * ROLE_STATUS_BAR_WIDTH), ROLE_STATUS_BAR_HEIGHT };
        Engine::getInstance()->renderSquareTexture(&r1, background_color, 192 * alpha);

        // Draw outline
        renderOutline(barLeft, bar_y, ROLE_STATUS_BAR_WIDTH, ROLE_STATUS_BAR_HEIGHT, outline_color, 128 * alpha);
    };

    Color background_color = { 0, 255, 0, 128 };    // 我方绿色
    Color shadow_color = { 64, 64, 64, 128 };       // 背景阴影
    if (r->Team == 1)
    {
        // 敌方红色
        background_color = { 255, 0, 0, 128 };
    }

    renderBar(y + ROLE_STATUS_BAR_Y, r->HP, r->MaxHP, background_color, shadow_color);

    if (r->MaxHP > 0 && r->HP > 0)
    {
        int bar_y = y + ROLE_STATUS_BAR_Y;
        int hpFillWidth = std::clamp(static_cast<int>(std::round(ROLE_STATUS_BAR_WIDTH * (static_cast<double>(r->HP) / r->MaxHP))), 0, ROLE_STATUS_BAR_WIDTH);

        if (comboState && comboState->shield > 0)
        {
            int shieldWidth = std::clamp(static_cast<int>(std::round(ROLE_STATUS_BAR_WIDTH * (static_cast<double>(comboState->shield) / r->MaxHP))), 1, ROLE_STATUS_BAR_WIDTH);
            int visibleShieldWidth = std::min(shieldWidth, hpFillWidth);
            Rect shieldRect = { barLeft, bar_y, visibleShieldWidth, ROLE_STATUS_BAR_HEIGHT };
            // int shieldAlpha = std::clamp(90 + comboState->shield * 120 / std::max(1, r->MaxHP), 90, 180);
            Engine::getInstance()->renderSquareTexture(&shieldRect, { 250, 200, 0, 255 }, 255);
        }

        bool hasDamageProtection = r->Invincible > 0 || (comboState && comboState->blockFirstHitsRemaining > 0);
        if (hasDamageProtection)
        {
            Color protectionColor = comboState && comboState->blockFirstHitsRemaining > 0 ? Color{ 255, 220, 110, 255 } : Color{ 255, 170, 95, 255 };
            renderOutline(barLeft, bar_y, ROLE_STATUS_BAR_WIDTH, ROLE_STATUS_BAR_HEIGHT, protectionColor, 220);
        }
    }

    Color mp_color = { 0, 0, 255, 128 };
    Color mp_shadow_color = { 64, 64, 64, 128 };
    renderBar(y + ROLE_STATUS_BAR_MP_Y, r->MP, r->MaxMP, mp_color, mp_shadow_color);

    // Frozen / cooldown bar – frozen takes priority
    if (r->Frozen > 0 && r->FrozenMax > 0)
    {
        Color frozen_color = { 200, 220, 255, 192 };
        int bar_y = y + ROLE_STATUS_BAR_FROZEN_Y;
        double perc = static_cast<double>(r->Frozen) / r->FrozenMax;

        // Draw outline (same color as bar)
        renderOutline(barLeft, bar_y, ROLE_STATUS_BAR_WIDTH, ROLE_STATUS_BAR_HEIGHT, frozen_color, 128);

        // Draw current frozen bar
        Rect r_frozen = { barLeft, bar_y, int(perc * ROLE_STATUS_BAR_WIDTH), ROLE_STATUS_BAR_HEIGHT };
        Engine::getInstance()->renderSquareTexture(&r_frozen, frozen_color, 192);
    }
    else if (r->CoolDown > 0 && r->CoolDownMax > 0)
    {
        Color cd_color = { 255, 210, 140, 160 };
        int bar_y = y + ROLE_STATUS_BAR_FROZEN_Y;
        double perc = static_cast<double>(r->CoolDown) / r->CoolDownMax;

        renderOutline(barLeft, bar_y, ROLE_STATUS_BAR_WIDTH, ROLE_STATUS_BAR_HEIGHT, cd_color, 100);

        Rect r_cd = { barLeft, bar_y, int(perc * ROLE_STATUS_BAR_WIDTH), ROLE_STATUS_BAR_HEIGHT };
        Engine::getInstance()->renderSquareTexture(&r_cd, cd_color, 160);
    }
}

int BattleSceneHades::checkResult()
{
    int team0 = getTeamMateCount(0);
    int team1 = enemies_.size() + getTeamMateCount(1);
    if (team0 > 0 && team1 == 0)
    {
        return 0;
    }
    if (team0 == 0 && team1 >= 0)
    {
        return 1;
    }
    return -1;
}

void BattleSceneHades::setRoleInitState(Role* r)
{
    BattleScene::setRoleInitState(r);
    if (r->Team == 0)
    {
        r->HP = r->MaxHP;
        r->MP = 0;
        r->PhysicalPower = (std::max)(r->PhysicalPower, 90);
    }
    else
    {
        r->HP = r->MaxHP;
        r->MP = 0;
        r->PhysicalPower = (std::max)(r->PhysicalPower, 90);
    }

    auto p = pos45To90(r->X(), r->Y());

    r->Pos.x = p.x;
    r->Pos.y = p.y;
    if (r->FaceTowards == Towards_RightDown)
    {
        r->RealTowards = { 1, 1 };
    }
    if (r->FaceTowards == Towards_RightUp)
    {
        r->RealTowards = { 1, -1 };
    }
    if (r->FaceTowards == Towards_LeftDown)
    {
        r->RealTowards = { -1, 1 };
    }
    if (r->FaceTowards == Towards_LeftUp)
    {
        r->RealTowards = { -1, -1 };
    }
    r->Acceleration = { 0, 0, gravity_ };
}

Role* BattleSceneHades::findNearestEnemy(int team, Pointf p)
{
    double dis = 4096;
    Role* r0 = nullptr;
    for (auto r1 : battle_roles_)
    {
        if (r1->Dead == 0 && team != r1->Team)
        {
            auto dis1 = EuclidDis(p, r1->Pos);
            if (dis1 < dis)
            {
                dis = dis1;
                r0 = r1;
            }
        }
    }
    return r0;
}

Role* BattleSceneHades::findFarthestEnemy(int team, Pointf p)
{
    double dis = 0;
    Role* r0 = nullptr;
    for (auto r1 : battle_roles_)
    {
        if (r1->Dead == 0 && team != r1->Team)
        {
            auto dis1 = EuclidDis(p, r1->Pos);
            if (dis1 > dis)
            {
                dis = dis1;
                r0 = r1;
            }
        }
    }
    return r0;
}

//前搖
int BattleSceneHades::calCast(int act_type, int operation_type, Role* r)
{
    int v[4] = { 25, 30, 20, 25 };
    if (operation_type >= 0 && operation_type <= 3)
    {
        return v[operation_type];
    }
    return 0;
}

//冷卻减去前搖就是後搖
//需注意攻擊判定可能仍然存在，嚴格來說攻擊判定存在的時間加上前搖應小於冷卻
int BattleSceneHades::calCoolDown(int act_type, int operation_type, Role* r)
{
    int i = r->getActProperty(act_type);
    int v[4] = { 105 - i / 2, 185 - i, 115 - i / 2, 45 };
    int min_v[4] = { 60, 70, 70, 45 };
    if (operation_type >= 0 && operation_type <= 3)
    {
        int c = std::max(min_v[operation_type], v[operation_type]);
        int spd = std::min(150, r->Speed);
        c = c * (1.0 - 0.5 * spd / 150.0);
        c = std::max(calCast(act_type, operation_type, r) + 2, c);
        // CDR from combos/neigong — clamp so cooldown never drops below cast time
        auto& cs = KysChess::ChessCombo::getMutableStates();
        auto it = cs.find(r->ID);
        if (it != cs.end() && it->second.cdrPct > 0)
        {
            c = static_cast<int>(c * (1.0 - it->second.cdrPct / 100.0));
            c = std::max(calCast(act_type, operation_type, r) + 2, c);
        }
        // std::print("{} cast time {}\n", r->Name, c);
        return c;
    }
    else
    {
        return 0;
    }
}

void BattleSceneHades::applyResolvedBattleHit(const KysChess::Battle::BattleHitResolutionResult& hitResolution)
{
    auto& cs = KysChess::ChessCombo::getMutableStates();
    auto attackerComboIt = cs.find(hitResolution.attackerUnitId);
    auto defenderComboIt = cs.find(hitResolution.defenderUnitId);
    if (attackerComboIt != cs.end())
    {
        attackerComboIt->second = hitResolution.attackerCombo;
    }
    if (defenderComboIt != cs.end())
    {
        defenderComboIt->second = hitResolution.defenderCombo;
    }
    for (const auto& command : hitResolution.commands)
    {
        applyBattleGameplayCommand(command);
    }
    for (const auto& presentationEvent : hitResolution.presentationEvents)
    {
        presentation_recorder_.recordPresentation(presentationEvent);
    }
}

void BattleSceneHades::applyBattleGameplayCommand(const KysChess::Battle::BattleGameplayCommand& command)
{
        if (const auto* hpDamage = std::get_if<KysChess::Battle::BattleHpDamageCommand>(&command))
        {
            auto source = hpDamage->sourceUnitId >= 0 ? findRoleByBattleId(battle_roles_, hpDamage->sourceUnitId) : nullptr;
            auto target = findRoleByBattleId(battle_roles_, hpDamage->targetUnitId);
            if (hpDamage->critical)
            {
                criticalHitRoles_.insert(target);
            }
            queuePreResolvedHpDamage(source,
                                     target,
                                     hpDamage->damage,
                                     hpDamage->critical,
                                     hpDamage->ultimate,
                                     hpDamage->executed,
                                     hpDamage->skillName,
                                     hpDamage->detailText);
        }
        else if (const auto* sideEffect = std::get_if<KysChess::Battle::BattleAcceptedHitSideEffectCommand>(&command))
        {
            auto source = sideEffect->sourceUnitId >= 0 ? findRoleByBattleId(battle_roles_, sideEffect->sourceUnitId) : nullptr;
            auto target = findRoleByBattleId(battle_roles_, sideEffect->targetUnitId);
            applyAcceptedHitSideEffectTransaction(source, target, sideEffect->damage);
        }
        else if (const auto* mpDamage = std::get_if<KysChess::Battle::BattleMpDamageCommand>(&command))
        {
            auto source = mpDamage->sourceUnitId >= 0 ? findRoleByBattleId(battle_roles_, mpDamage->sourceUnitId) : nullptr;
            auto target = findRoleByBattleId(battle_roles_, mpDamage->targetUnitId);
            auto result = applyAcceptedHitSideEffectTransaction(source, target, mpDamage->damage);
            for (const auto& event : result.events)
            {
                if (event.type == KysChess::Battle::BattleDamageEventType::MpDamageApplied)
                {
                    addDamageNumber(target, event.value, { 160, 32, 240, 255 });
                }
            }
        }
        else if (const auto* knockback = std::get_if<KysChess::Battle::BattleKnockbackCommand>(&command))
        {
            auto target = findRoleByBattleId(battle_roles_, knockback->targetUnitId);
            target->Velocity += knockback->velocityDelta;
            if (knockback->velocityCap > 0.0 && target->Velocity.norm() > knockback->velocityCap)
            {
                target->Velocity.normTo(static_cast<float>(knockback->velocityCap));
            }
            if (knockback->grantHurtFrame) { target->HurtFrame = 1; }
        }
        else if (const auto* teamHeal = std::get_if<KysChess::Battle::BattleTeamHealCommand>(&command))
        {
            LegacyTeamEffectCommitRequest request;
            request.type = LegacyTeamEffectCommitType::Heal;
            request.sourceUnitId = teamHeal->sourceUnitId;
            request.flatHeal = teamHeal->flatHeal;
            request.pctHeal = teamHeal->pctHeal;
            auto teamEvents = commitLegacyTeamEffect(request);
            playCommittedTeamEffectEvents(
                findRoleByBattleId(battle_roles_, teamHeal->sourceUnitId),
                teamEvents,
                teamHeal->reason.c_str());
        }
        else if (const auto* teamMp = std::get_if<KysChess::Battle::BattleTeamMpRestoreCommand>(&command))
        {
            LegacyTeamEffectCommitRequest request;
            request.type = LegacyTeamEffectCommitType::MpRestore;
            request.sourceUnitId = teamMp->sourceUnitId;
            request.amount = teamMp->amount;
            auto teamEvents = commitLegacyTeamEffect(request);
            playCommittedTeamEffectEvents(
                findRoleByBattleId(battle_roles_, teamMp->sourceUnitId),
                teamEvents,
                teamMp->reason.c_str());
        }
        else if (const auto* teamShield = std::get_if<KysChess::Battle::BattleTeamShieldCommand>(&command))
        {
            LegacyTeamEffectCommitRequest request;
            request.type = LegacyTeamEffectCommitType::Shield;
            request.sourceUnitId = teamShield->sourceUnitId;
            request.amount = teamShield->amount;
            request.refreshOnly = teamShield->refreshOnly;
            auto teamEvents = commitLegacyTeamEffect(request);
            playCommittedTeamEffectEvents(
                findRoleByBattleId(battle_roles_, teamShield->sourceUnitId),
                teamEvents,
                teamShield->reason.c_str());
        }
        else if (const auto* currentHp = std::get_if<KysChess::Battle::BattleCurrentHpBlastCommand>(&command))
        {
            auto source = findRoleByBattleId(battle_roles_, currentHp->sourceUnitId);
            for (auto enemy : battle_roles_)
            {
                if (!enemy || enemy->Team == source->Team || enemy->Dead != 0)
                {
                    continue;
                }
                int damage = std::max(1, enemy->HP * currentHp->damagePct / 100);
                queuePreResolvedHpDamage(source, enemy, damage, false, false, false, "", currentHp->reason);
            }
        }
        else if (const auto* spiral = std::get_if<KysChess::Battle::BattleSpiralBleedProjectileCommand>(&command))
        {
            spawnSpiralBleedProjectiles(
                findRoleByBattleId(battle_roles_, spiral->sourceUnitId),
                spiral->bleedStacks,
                spiral->projectileCount);
        }
        else if (const auto* nearby = std::get_if<KysChess::Battle::BattleNearbyTrackingProjectilesCommand>(&command))
        {
            spawnNearbyTrackingProjectiles(
                nearby->prototype,
                findRoleByBattleId(battle_roles_, nearby->centerTargetUnitId),
                nearby->rangePixels,
                nearby->damagePct);
        }
        else if (const auto* extra = std::get_if<KysChess::Battle::BattleHitExtraProjectilesCommand>(&command))
        {
            spawnHitExtraProjectiles(
                extra->prototype,
                extra->extraCount,
                findRoleByBattleId(battle_roles_, extra->targetUnitId));
        }
        else if (const auto* shieldExplosion = std::get_if<KysChess::Battle::BattleShieldExplosionCommand>(&command))
        {
            auto source = findRoleByBattleId(battle_roles_, shieldExplosion->sourceUnitId);
            spawnAreaImpactProjectiles(
                source,
                source,
                shieldExplosion->areaSize,
                shieldExplosion->effectId,
                shieldExplosion->damage);
            logBattleStatus(source, nullptr, formatStatusValue(shieldExplosion->reason.c_str(), shieldExplosion->damage, "傷害"));
        }
        else if (const auto* mpRestore = std::get_if<KysChess::Battle::BattleMpRestoreCommand>(&command))
        {
            auto role = findRoleByBattleId(battle_roles_, mpRestore->unitId);
            int restored = std::min(mpRestore->amount, std::max(0, role->MaxMP - role->MP));
            if (restored > 0)
            {
                role->MP += restored;
                logBattleStatus(role, role, mpRestore->reason);
            }
        }
        else if (const auto* autoUltimate = std::get_if<KysChess::Battle::BattleAutoUltimateCommand>(&command))
        {
            triggerAutoUltimate(findRoleByBattleId(battle_roles_, autoUltimate->unitId), autoUltimate->consumeMp);
        }
        else if (const auto* tempAttack = std::get_if<KysChess::Battle::BattleTempAttackBuffCommand>(&command))
        {
            auto role = findRoleByBattleId(battle_roles_, tempAttack->unitId);
            if (tempAttack->permanent)
            {
                role->Attack += tempAttack->attackBonus;
                role->Defence += tempAttack->defenceBonus;
                if (!tempAttack->reason.empty())
                {
                    logBattleStatus(role, role, std::format("{}（攻防+{}）", tempAttack->reason, tempAttack->attackBonus));
                }
            }
            else
            {
                auto& state = KysChess::ChessCombo::getMutableStates()[role->ID];
                applyTempAttackBuff(role, state, tempAttack->attackBonus, tempAttack->durationFrames, tempAttack->reason);
            }
        }
        else if (const auto* deathAoe = std::get_if<KysChess::Battle::BattleDeathAoeProjectileCommand>(&command))
        {
            auto source = findRoleByBattleId(battle_roles_, deathAoe->sourceUnitId);
            auto trackedTarget = deathAoe->trackedTargetUnitId >= 0
                ? findRoleByBattleId(battle_roles_, deathAoe->trackedTargetUnitId)
                : nullptr;
            logBattleStatus(source, nullptr,
                formatStatusPercentFrames("殉爆", deathAoe->damagePct, deathAoe->stunFrames));
            spawnAreaImpactProjectiles(source,
                                       source,
                                       7,
                                       KysChess::EFT_DEATH_BLAST,
                                       deathAoe->damage,
                                       deathAoe->stunFrames,
                                       trackedTarget,
                                       deathAoe->maxTargets);
        }
        else if (const auto* heal = std::get_if<KysChess::Battle::BattleUnitHealCommand>(&command))
        {
            auto source = heal->sourceUnitId >= 0 ? findRoleByBattleId(battle_roles_, heal->sourceUnitId) : nullptr;
            auto target = findRoleByBattleId(battle_roles_, heal->targetUnitId);
            int before = target->HP;
            target->HP = std::min(target->MaxHP, target->HP + heal->amount);
            if (target->HP > before)
            {
                addRoleEffect(target, KysChess::EFT_HEAL, ROLE_STATUS_EFT_FRAMES);
                logBattleHeal(source, target, target->HP - before, heal->reason);
            }
        }
        else if (const auto* shield = std::get_if<KysChess::Battle::BattleUnitShieldCommand>(&command))
        {
            auto source = shield->sourceUnitId >= 0 ? findRoleByBattleId(battle_roles_, shield->sourceUnitId) : nullptr;
            auto target = findRoleByBattleId(battle_roles_, shield->targetUnitId);
            auto& state = KysChess::ChessCombo::getMutableStates()[target->ID];
            state.shield += shield->amount;
            logBattleStatus(source, target, formatStatusValue(shield->reason.c_str(), shield->amount, "護盾"));
        }
        else if (const auto* lastAttacker = std::get_if<KysChess::Battle::BattleLastAttackerCommand>(&command))
        {
            auto target = findRoleByBattleId(battle_roles_, lastAttacker->targetUnitId);
            target->LastAttacker = findRoleByBattleId(battle_roles_, lastAttacker->attackerUnitId);
        }
        else
        {
            assert(false);
        }
}

int BattleSceneHades::calRolePic(Role* r, int style, int frame)
{
    if (style < 0 || style >= 5)
    {
        style = resolveFightStyle(r, -1);
        if (style >= 0)
        {
            return r->FightFrame[style] * r->FaceTowards;
        }
    }
    else
    {
        style = resolveFightStyle(r, style);
    }
    if (style < 0)
    {
        return r->FaceTowards;
    }
    int total = 0;
    for (int i = 0; i < 5; i++)
    {
        if (i == style)
        {
            //停留在最后一帧
            if (frame < r->FightFrame[style] - 2)
            {
                return total + r->FightFrame[style] * r->FaceTowards + frame;
            }
            else
            {
                return total + r->FightFrame[style] * r->FaceTowards + r->FightFrame[style] - 2;
            }
        }
        total += r->FightFrame[i] * 4;
    }
    return r->FaceTowards;
}

void BattleSceneHades::makeSpecialMagicEffect()
{
}

void BattleSceneHades::queuePreResolvedHpDamage(Role* source,
                                                Role* target,
                                                int damage,
                                                bool critical,
                                                bool ultimate,
                                                bool executed,
                                                const std::string& skillName,
                                                const std::string& detailText,
                                                Color damageColor,
                                                int damageTextSize)
{
    assert(target);
    assert(damage > 0);

    pending_pre_resolved_hp_damage_.push_back({
        source,
        target,
        damage,
        critical,
        ultimate,
        executed,
        skillName,
        detailText,
        damageColor,
        damageTextSize,
    });
}

int BattleSceneHades::pendingPreResolvedHpDamage(Role* target) const
{
    assert(target);

    int damage = 0;
    for (const auto& pending : pending_pre_resolved_hp_damage_)
    {
        if (pending.target == target)
        {
            damage += pending.damage;
        }
    }
    return damage;
}

KysChess::Battle::BattleDamageTransactionResult BattleSceneHades::applyAcceptedHitSideEffectTransaction(
    Role* source,
    Role* target,
    KysChess::Battle::BattleDamageRequest request)
{
    assert(target);

    auto& states = KysChess::ChessCombo::getMutableStates();
    auto sourceStateIt = source ? states.find(source->ID) : states.end();
    auto& targetState = states[target->ID];
    request.attackerUnitId = source ? source->ID : -1;
    request.defenderUnitId = target->ID;
    request.acceptedHit = true;

    KysChess::Battle::BattleDamageTransactionInput input;
    input.request = request;
    input.attacker = makeBattleDamageUnit(source, sourceStateIt != states.end() ? &sourceStateIt->second : nullptr);
    input.defender = makeBattleDamageUnit(target, &targetState);
    input.defenderStatus = makeBattleStatusUnit(target, targetState);
    input.defenderCooldown = makeBattleCooldownState(target);

    auto frameState = makeCoreFrameState(makeNoOpCoreWorld());
    frameState.damage.pendingTransactions.push_back(std::move(input));
    KysChess::Battle::BattleFrameRunner().advanceFrame(frameState);
    assert(frameState.damage.committedTransactions.size() == 1);
    auto result = std::move(frameState.damage.committedTransactions.front());
    if (source)
    {
        writeBattleDamageUnit(
            source,
            sourceStateIt != states.end() ? &sourceStateIt->second : nullptr,
            result.attacker);
    }
    writeBattleDamageUnit(target, &targetState, result.defender);
    writeBattleStatusUnit(target, targetState, result.defenderStatus);
    writeBattleCooldownState(target, result.defenderCooldown);
    return result;
}

int BattleSceneHades::resolveLegacyMagicBaseDamage(Role* attacker, Role* defender, Magic* magic)
{
    assert(attacker);
    assert(defender);
    assert(magic);

    double defence = defender->Defence;
    // Armor penetration: reduce effective defence
    auto& cs = KysChess::ChessCombo::getMutableStates();
    auto it = cs.find(attacker->ID);
    if (it != cs.end())
    {
        defence = KysChess::Battle::BattleComboTriggerSystem().resolveArmorPenetratedDefense(
            it->second,
            { attacker->ID, defender->ID, defence },
            [&]() { return rand_.rand() * 100.0; }).defense;
    }

    return KysChess::Battle::BattleDamageSystem().resolveMagicBaseDamage({
        attacker->Attack,
        attacker->getMagicPower(magic),
        defence,
        rand_.rand_int(10) - rand_.rand_int(10),
    });
}

KysChess::Battle::BattleMovementConfig BattleSceneHades::makeCoreMovementConfig() const
{
    KysChess::Battle::BattleMovementGeometry geometry;
    geometry.tileWidth = BATTLE_TILE_W;
    geometry.reservationHorizonFrames = PATH_REFRESH_FRAMES;
    geometry.dashFrames = DASH_MOMENTUM_FRAMES;
    geometry.dashCooldownFrames = MOVEMENT_DASH_COOLDOWN_FRAMES;
    geometry.slotSwitchCooldownFrames = PATH_REFRESH_FRAMES;
    geometry.maxRangedReach = MAX_EFFECTIVE_BATTLE_REACH;
    geometry.meleeAttackEffectOffset = MELEE_ATTACK_EFFECT_OFFSET;
    geometry.meleeAttackHitRadius = MELEE_ATTACK_HIT_RADIUS;
    return KysChess::Battle::BattleGeometry(geometry).movementConfig();
}

KysChess::Battle::BattleUnitState BattleSceneHades::makeCoreMovementUnit(Role* role, const MovementRuntime* movementRuntime)
{
    bool isUltimate = role->MP >= role->MaxMP;
    Magic* plannedMagic = role->UsingMagic;
    if (!plannedMagic)
    {
        plannedMagic = isUltimate ? selectMagic(role, std::greater<double>{ }) : selectMagic(role, std::less<double>{ });
    }

    bool forceRanged = roleForcesRangedMagic(role);
    const auto& cs = KysChess::ChessCombo::getActiveStates();
    auto comboIt = cs.find(role->ID);
    bool dashAttackEnabled = comboIt != cs.end() && comboIt->second.dashAttack;
    int forcedRangedMinSelectDistance = getForcedRangedMinSelectDistance(role);
    int projectileSpeedMultiplierPct = getProjectileSpeedMultiplierPct(role);

    KysChess::Battle::BattleUnitState unit;
    unit.id = role->ID;
    unit.realRoleId = role->RealID;
    unit.name = role->Name;
    unit.team = role->Team;
    unit.alive = role->Dead == 0;
    unit.position = role->Pos;
    unit.velocity = role->Velocity;
    unit.speed = role->Speed / ROLE_MOVE_SPEED_DIVISOR;
    unit.star = role->Star;
    unit.reach = plannedMagic
        ? std::min(
            effectiveBattleReach(plannedMagic, forceRanged, forcedRangedMinSelectDistance, projectileSpeedMultiplierPct),
            MAX_EFFECTIVE_BATTLE_REACH)
        : MELEE_ATTACK_REACH;
    unit.style = isRangedStyleMagic(plannedMagic, forceRanged)
        ? KysChess::Battle::CombatStyle::Ranged
        : KysChess::Battle::CombatStyle::Melee;
    unit.taXue = dashAttackEnabled;
    unit.dashAttack = dashAttackEnabled;
    unit.canAttack = role->CoolDown == 0;
    if (movementRuntime)
    {
        unit.assignedSlot = movementRuntime->core_assigned_slot;
        unit.slotSwitchCooldownRemaining = movementRuntime->core_slot_switch_cooldown;
        unit.dashFramesRemaining = movementRuntime->movement_dash_frames;
        unit.dashCooldownRemaining = movementRuntime->movement_dash_cooldown;
    }
    return unit;
}

void BattleSceneHades::applyCoreMovementSnapshot(const KysChess::Battle::BattleTickResult& result, const std::map<int, Role*>& rolesByBattleId)
{
    for (const auto& unit : result.snapshot.units)
    {
        auto it = rolesByBattleId.find(unit.id);
        if (it == rolesByBattleId.end())
        {
            continue;
        }
        auto& movementRuntime = movement_runtime_[it->second];
        movementRuntime.core_assigned_slot = unit.assignedSlot;
        movementRuntime.core_slot_switch_cooldown = unit.slotSwitchCooldownRemaining;
    }
}

KysChess::Battle::BattleFrameState BattleSceneHades::makeCoreFrameState(
    KysChess::Battle::BattleWorldState world)
{
    KysChess::Battle::BattleFrameState frameState;
    frameState.world = std::move(world);
    frameState.attacks = makeBattleAttackWorld(
        battle_roles_,
        active_attack_world_,
        shared_hit_group_targets_);
    return frameState;
}

void BattleSceneHades::populateCoreStatusState(KysChess::Battle::BattleFrameState& frameState)
{
    auto& comboStates = KysChess::ChessCombo::getMutableStates();
    for (auto role : battle_roles_)
    {
        assert(role);
        auto stateIt = comboStates.find(role->ID);
        if (stateIt != comboStates.end() && role->Dead == 0)
        {
            frameState.status.units.push_back(makeBattleStatusUnit(role, stateIt->second));
        }
    }
}

void BattleSceneHades::applyCoreStatusState(const KysChess::Battle::BattleFrameState& frameState)
{
    auto& comboStates = KysChess::ChessCombo::getMutableStates();
    for (const auto& unit : frameState.status.units)
    {
        auto role = findRoleByBattleId(battle_roles_, unit.id);
        assert(role);
        auto stateIt = comboStates.find(unit.id);
        assert(stateIt != comboStates.end());
        writeBattleStatusUnit(role, stateIt->second, unit);
    }
}

void BattleSceneHades::handleCoreStatusEvents(const std::vector<KysChess::Battle::BattleStatusEvent>& events)
{
    for (const auto& event : events)
    {
        auto target = findRoleByBattleId(battle_roles_, event.unitId);
        auto source = findOptionalRoleByBattleId(battle_roles_, event.sourceUnitId);
        switch (event.type)
        {
        case KysChess::Battle::BattleStatusEventType::PoisonDamage:
            queuePreResolvedHpDamage(source,
                                     target,
                                     event.value,
                                     false,
                                     false,
                                     false,
                                     "",
                                     "中毒",
                                     poisonDamageTextColor(),
                                     NORMAL_DAMAGE_TEXT_SIZE);
            break;
        case KysChess::Battle::BattleStatusEventType::BleedDamage:
            queuePreResolvedHpDamage(source,
                                     target,
                                     event.value,
                                     false,
                                     false,
                                     false,
                                     "",
                                     "流血",
                                     bleedDamageTextColor(),
                                     NORMAL_DAMAGE_TEXT_SIZE);
            break;
        case KysChess::Battle::BattleStatusEventType::InvincibilityGranted:
            logBattleStatus(target, target, formatStatusFrames("週期免傷", event.value));
            break;
        case KysChess::Battle::BattleStatusEventType::TempAttackExpired:
            break;
        }
    }
}

KysChess::Battle::BattleWorldState BattleSceneHades::makeNoOpCoreWorld() const
{
    KysChess::Battle::BattleWorldState world;
    world.frame = battle_frame_;
    world.config = makeCoreMovementConfig();
    world.canStandAt = [](Pointf) { return true; };
    return world;
}

void BattleSceneHades::prepareCoreMovementDecisions()
{
    if (core_movement_frame_ == battle_frame_)
    {
        return;
    }
    core_movement_frame_ = battle_frame_;
    core_movement_decisions_.clear();

    KysChess::Battle::BattleWorldState world;
    world.frame = battle_frame_;
    world.config = makeCoreMovementConfig();
    world.canStandAt = [this](Pointf p)
    {
        auto p45 = pos90To45(p.x, p.y);
        return !isOutLine(p45.x, p45.y) && canWalk45(p45.x, p45.y);
    };

    std::map<int, Role*> rolesByBattleId;
    for (auto* role : battle_roles_)
    {
        if (!role || role->Dead != 0)
        {
            continue;
        }

        auto movementIt = movement_runtime_.find(role);
        world.units.push_back(makeCoreMovementUnit(role, movementIt != movement_runtime_.end() ? &movementIt->second : nullptr));
        rolesByBattleId[role->ID] = role;
    }

    auto frameState = makeCoreFrameState(std::move(world));
    auto result = KysChess::Battle::BattleFrameRunner().advanceFrame(frameState).movement;
    applyCoreMovementSnapshot(result, rolesByBattleId);
    for (const auto& [battleId, decision] : result.decisions)
    {
        auto it = rolesByBattleId.find(battleId);
        if (it != rolesByBattleId.end())
        {
            core_movement_decisions_[it->second] = decision;
        }
    }
}
