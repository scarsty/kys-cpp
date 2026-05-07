#include "BattleSceneHades.h"
#include "Audio.h"
#include "BattleRoleManager.h"
#include "BattleSceneBattleAdapter.h"
#include "BattleScenePresentationConstants.h"
#include "battle/BattleAttackSystem.h"
#include "battle/BattleCombatIntent.h"
#include "battle/BattleStatusSystem.h"
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

namespace
{
using KysChess::BattleSceneBattleAdapter::findRoleByBattleId;
using KysChess::BattleSceneBattleAdapter::applyBattleActionFrameResults;
using KysChess::BattleSceneBattleAdapter::applyBattleFrameUnitRuntimeResult;
using KysChess::BattleSceneBattleAdapter::applyBattleProjectileCancelDamage;
using KysChess::BattleSceneBattleAdapter::BattleActionFrameAdapterContext;
using KysChess::BattleSceneBattleAdapter::BattleFrameHitAdapterInput;
using KysChess::BattleSceneBattleAdapter::BattleMovementPhysicsFrameAdapterContext;
using KysChess::BattleSceneBattleAdapter::appendBattleFrameHitInput;
using KysChess::BattleSceneBattleAdapter::applyBattleLifecycleEvents;
using KysChess::BattleSceneBattleAdapter::commitBattleSelectedSkillAction;
using KysChess::BattleSceneBattleAdapter::configureBattleAttackWorld;
using KysChess::BattleSceneBattleAdapter::makeBattleDamageUnit;
using KysChess::BattleSceneBattleAdapter::makeBattleDamagePresentationStyle;
using KysChess::BattleSceneBattleAdapter::makeBattlePresentationColor;
using KysChess::BattleSceneBattleAdapter::makeBattleRuntimeUnit;
using KysChess::BattleSceneBattleAdapter::makeBattleStatusUnit;
using KysChess::BattleSceneBattleAdapter::applyBattleMovementPhysicsFrameResults;
using KysChess::BattleSceneBattleAdapter::effectiveBattleReach;
using KysChess::BattleSceneBattleAdapter::forcedRangedMinSelectDistance;
using KysChess::BattleSceneBattleAdapter::isBattleRangedStyleMagic;
using KysChess::BattleSceneBattleAdapter::populateBattleActionFrame;
using KysChess::BattleSceneBattleAdapter::projectileSpeedMultiplierPct;
using KysChess::BattleSceneBattleAdapter::resolveBattleMagicBaseDamage;
using KysChess::BattleSceneBattleAdapter::roleForcesRangedMagic;
using KysChess::BattleSceneBattleAdapter::selectHigherPowerMagic;
using KysChess::BattleSceneBattleAdapter::selectLowerPowerMagic;
using KysChess::BattleSceneBattleAdapter::writeBattleDamageUnit;
using KysChess::BattleSceneBattleAdapter::writeBattleStatusUnit;

constexpr int BATTLE_TILE_W = Scene::TILE_W;
constexpr int PROJECTILE_SPEED = BATTLE_TILE_W / 3;
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
constexpr double ROLE_MOVE_SPEED_DIVISOR = 22.0;
constexpr int BATTLE_COORD_COUNT = BATTLEMAP_COORD_COUNT;
constexpr int ROLE_STATUS_BAR_WIDTH = 48;
constexpr int ROLE_STATUS_BAR_HEIGHT = 6;
constexpr int ROLE_STATUS_BAR_Y = -120;
constexpr int ROLE_STATUS_BAR_STEP_Y = ROLE_STATUS_BAR_HEIGHT + ROLE_STATUS_BAR_HEIGHT / 3;
constexpr int ROLE_STATUS_BAR_MP_Y = ROLE_STATUS_BAR_Y + ROLE_STATUS_BAR_STEP_Y;
constexpr int ROLE_STATUS_BAR_FROZEN_Y = ROLE_STATUS_BAR_Y + ROLE_STATUS_BAR_STEP_Y * 2;

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

Color poisonDamageTextColor()
{
    return { 0, 200, 0, 255 };
}

Color bleedDamageTextColor()
{
    return { 190, 120, 60, 255 };
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

Role* requireFrameRole(
    const KysChess::BattleSceneBattleAdapter::BattleFrameApplyContext& bundle,
    int unitId)
{
    auto it = bundle.rolesByBattleId.find(unitId);
    assert(it != bundle.rolesByBattleId.end());
    assert(it->second);
    return it->second;
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

KysChess::Battle::BattleDeathEffectStore makeBattleDeathEffectStore(
    const std::vector<Role*>& roles,
    const std::map<int, KysChess::RoleComboState>& states)
{
    KysChess::Battle::BattleDeathEffectStore store;
    const auto& allCombos = KysChess::ChessCombo::getAllCombos();
    for (int comboId = 0; comboId < static_cast<int>(allCombos.size()); ++comboId)
    {
        if (!allCombos[comboId].isAntiCombo)
        {
            store.regularSynergyComboIds.insert(comboId);
        }
    }

    for (auto role : roles)
    {
        assert(role);
        auto stateIt = states.find(role->ID);
        assert(stateIt != states.end());

        KysChess::Battle::BattleDeathEffectExtras extras;
        extras.id = role->ID;
        extras.shieldPctMaxHp = stateIt->second.shieldPctMaxHP;
        extras.shieldOnAllyDeathTracker = stateIt->second.shieldOnAllyDeathTracker;
        extras.appliedEffects = stateIt->second.appliedEffects;

        int comboLookupId = getComboLookupId(role);
        if (comboLookupId >= 0)
        {
            extras.comboIds = KysChess::ChessCombo::getCombosForRole(comboLookupId);
        }
        store.units.push_back(std::move(extras));
    }
    return store;
}

void writeBattleDeathEffectTrackers(const KysChess::Battle::BattleDeathEffectStore& store,
                                    const std::vector<Role*>& roles,
                                    std::map<int, KysChess::RoleComboState>& states)
{
    for (auto role : roles)
    {
        assert(role);
        auto extrasIt = std::find_if(store.units.begin(), store.units.end(), [&](const KysChess::Battle::BattleDeathEffectExtras& extras)
            {
                return extras.id == role->ID;
            });
        assert(extrasIt != store.units.end());

        auto stateIt = states.find(role->ID);
        assert(stateIt != states.end());
        stateIt->second.shieldOnAllyDeathTracker = extrasIt->shieldOnAllyDeathTracker;
    }
}

}    // namespace

Magic* BattleSceneHades::commitAutoUltimate(Role* role, bool consumeMP)
{
    if (!role || role->Dead != 0)
    {
        return nullptr;
    }

    Magic* magic = selectHigherPowerMagic(role);
    if (!magic)
    {
        return nullptr;
    }

    auto selectedAction = commitBattleSelectedSkillAction(
        role,
        magic,
        true,
        -1,
        makeBattleActionFrameAdapterContext());
    for (int soundId : selectedAction.applyResult.attackSoundIds)
    {
        Audio::getInstance()->playASound(soundId);
    }
    for (const auto& event : selectedAction.applyResult.visualEvents)
    {
        presentation_recorder_.recordVisual(event);
    }
    for (const auto& event : selectedAction.applyResult.logEvents)
    {
        presentation_recorder_.recordLog(event);
    }
    for (auto& request : selectedAction.applyResult.attackSpawnRequests)
    {
        queueCoreAttackSpawn(std::move(request));
    }
    if (consumeMP)
    {
        changeRoleMP(role, -role->MaxMP);
    }
    return selectedAction.magic;
}

// Apply freeze frames to a role, accounting for low-HP immunity and freeze shield
void applyFrozen(Role* r, int frames, std::map<int, KysChess::RoleComboState>& comboStates)
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

    auto it = comboStates.find(r->ID);

    int totalFreezeRes = 0;
    if (it != comboStates.end())
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

    if (it != comboStates.end() && it->second.controlImmunityFrames > 0)
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

void BattleSceneHades::recordFloatingTextPresentation(Role* role, const std::string& text, Color color, int size, int type)
{
    presentation_recorder_.recordVisual({
        KysChess::Battle::BattleVisualEventType::FloatingText,
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
        makeBattlePresentationColor(color),
        role ? role->Pos : Pointf{},
    });
}

void BattleSceneHades::recordRoleEffectPresentation(Role* role, int eftId, int totalFrames)
{
    assert(role);

    presentation_recorder_.recordVisual({
        KysChess::Battle::BattleVisualEventType::RoleEffect,
        battle_frame_,
        -1,
        role->ID,
        0,
        totalFrames,
        eftId,
    });
}

void BattleSceneHades::recordDamageNumberPresentation(Role* role, int damage, Color color, int baseSize)
{
    assert(role);
    assert(damage > 0);

    presentation_recorder_.recordVisual({
        KysChess::Battle::BattleVisualEventType::DamageNumber,
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
        makeBattlePresentationColor(color),
        role->Pos,
    });
}

void BattleSceneHades::recordBattleStatusLog(Role* source, Role* target, const std::string& text)
{
    presentation_recorder_.recordLog({
        KysChess::Battle::BattleLogEventType::Status,
        battle_frame_,
        source ? source->ID : -1,
        target ? target->ID : -1,
        0,
        text,
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

    return KysChess::Battle::frameComboHasExecute(
        it->second,
        role->ID);
}

void BattleSceneHades::queueCoreAttackSpawn(KysChess::Battle::BattleAttackSpawnRequest request)
{
    assert(request.initial.attackerUnitId >= 0);
    auto* attacker = findRoleByBattleId(battle_roles_, request.initial.attackerUnitId);
    if (request.initial.skillId >= 0 && request.initial.skillName.empty())
    {
        auto* magic = Save::getInstance()->getMagic(request.initial.skillId);
        assert(magic);
        request.initial.skillName = magic->Name;
        request.initial.skillHurtType = magic->HurtType;
        request.initial.skillMagicType = magic->MagicType;
        request.initial.skillEffectId = magic->EffectID;
        request.initial.skillAttackerActProperty = attacker->getActProperty(magic->MagicType);
        request.initial.skillMagicPower = attacker->getMagicPower(magic);
    }
    if (request.initial.hiddenWeaponItemId >= 0 && request.initial.hiddenWeaponItemName.empty())
    {
        auto* item = Save::getInstance()->getItem(request.initial.hiddenWeaponItemId);
        assert(item);
        request.initial.hiddenWeaponItemName = item->Name;
        request.initial.hiddenWeaponEffectId = item->HiddenWeaponEffectID;
    }
    request.initial.executeCanHitInvincible = attackCanHitInvincible(attacker);
    assert(battle_session_.has_value());
    battle_session_->enqueueAttackSpawn(std::move(request));
}

void BattleSceneHades::initializeBattleRuntimeSession()
{
    KysChess::Battle::BattleRuntimeInit init;
    battle_session_.emplace(std::move(init));
    initializeBattleRuntimeStaticState();
    battleRuntime().world.config = makeCoreMovementConfig();
    battleRuntime().units.gridTransform = { TILE_W, BATTLE_COORD_COUNT };
    auto& comboStates = KysChess::ChessCombo::getMutableStates();
    battleRuntime().combo.units = comboStates;
    battleRuntime().combo.events.clear();
    battleRuntime().units.units.clear();
    battleRuntime().units.units.reserve(battle_roles_.size());
    battleRuntime().status.units.clear();
    battleRuntime().status.units.reserve(battle_roles_.size());
    for (auto* role : battle_roles_)
    {
        assert(role);
        auto stateIt = comboStates.find(role->ID);
        battleRuntime().units.units.push_back(makeBattleRuntimeUnit(
            role,
            stateIt != comboStates.end() ? &stateIt->second : nullptr,
            battleRuntime().units.gridTransform));
        if (stateIt != comboStates.end())
        {
            battleRuntime().status.units.push_back(KysChess::Battle::makeBattleStatusRuntimeUnit(
                makeBattleStatusUnit(role, stateIt->second)));
        }
    }
    initializeCoreDamageState();
    configureBattleAttackWorld(battleRuntime().attacks);
    battleRuntime().teamEffects.healAuraRadius = TILE_W * 6.0;
    battleRuntime().deathEffects.store = makeBattleDeathEffectStore(
        battle_roles_,
        comboStates);
    auto* basicMagic = Save::getInstance()->getMagic(1);
    assert(basicMagic);
    battleRuntime().rescue.executeUnattendedRadius = TILE_W * 3.0;
    battleRuntime().rescue.counterAttack.skillId = basicMagic->ID;
    battleRuntime().rescue.counterAttack.visualEffectId = 11;
    battleRuntime().rescue.counterAttack.projectileSpeed = PROJECTILE_SPEED;
    battleRuntime().rescue.counterAttack.meleeAttackEffectOffset = MELEE_ATTACK_EFFECT_OFFSET;
    battleRuntime().rescue.counterAttack.minimumTotalFrames = 20;
    battleRuntime().rescue.counterAttack.totalFramePadding = 15;
    battleRuntime().movementPhysics.config.gravity = gravity_;
    battleRuntime().movementPhysics.config.friction = friction_;
    battleRuntime().movementPhysics.config.postDashSpreadFrames = POST_DASH_SPREAD_FRAMES;
    const auto castConfig = KysChess::BattleSceneBattleAdapter::makeBattleCastConfig();
    battleRuntime().movementPhysics.actionCastFrames.assign(
        castConfig.castFrames.begin(),
        castConfig.castFrames.end());
    battleRuntime().movementPhysics.dashMomentumFrames = DASH_MOMENTUM_FRAMES;
    battleRuntime().movementPhysics.collision.tileWidth = TILE_W;
    battleRuntime().movementPhysics.collision.coordCount = BATTLE_COORD_COUNT;
    battleRuntime().movementPhysics.collision.defaultSeparationDistance = TILE_W * 1.5;
    battleRuntime().projectileFollowUps.projectileSpeed = PROJECTILE_SPEED;
    battleRuntime().projectileFollowUps.minimumProjectileFrames = 20;
    battleRuntime().projectileFollowUps.nearbyProjectileFramePadding = 18;
    battleRuntime().projectileFollowUps.areaProjectileFramePadding = 15;
    battleRuntime().projectileFollowUps.areaSpawnDistance = TILE_W * 1.5;
    battleRuntime().projectileFollowUps.nextSharedHitGroupId = next_shared_hit_group_id_;
}

KysChess::Battle::BattleRuntimeState& BattleSceneHades::battleRuntime()
{
    assert(battle_session_.has_value());
    return battle_session_->runtimeForTests();
}

const KysChess::Battle::BattleRuntimeState& BattleSceneHades::battleRuntime() const
{
    assert(battle_session_.has_value());
    return battle_session_->runtime();
}

BattleActionFrameAdapterContext BattleSceneHades::makeBattleActionFrameAdapterContext()
{
    BattleActionFrameAdapterContext context;
    context.roles = &battle_roles_;
    context.units = &battleRuntime().units;
    context.random = &rand_;
    for (auto* role : battle_roles_)
    {
        assert(role);
        context.unitCells.emplace(role->ID, pos90To45(role->Pos.x, role->Pos.y));
    }
    context.movementRuntime = &battleRuntime().movementRuntime;
    context.pendingCastResults = &battleRuntime().pendingCastResults;
    context.comboStates = &battleRuntime().combo.units;
    context.movementDecisions = &battleRuntime().movementDecisions;
    context.ultimateCasters = &battleRuntime().ultimateCasters;
    context.config.maxEffectiveBattleReach = MAX_EFFECTIVE_BATTLE_REACH;
    context.config.meleeAttackHitRadius = MELEE_ATTACK_HIT_RADIUS;
    context.config.meleeAttackReach = MELEE_ATTACK_REACH;
    context.config.dashAttackMeleeReach = DASH_ATTACK_MELEE_REACH;
    context.config.blinkWeakTargetDefWeight = BLINK_WEAK_TARGET_DEF_WEIGHT;
    context.config.dashMomentumFrames = DASH_MOMENTUM_FRAMES;
    context.config.movementDashCooldownFrames = MOVEMENT_DASH_COOLDOWN_FRAMES;
    context.config.actionRecoveryFrames = ACTION_RECOVERY_FRAMES;
    context.config.hiddenWeaponTotalFrame = 100;
    context.config.battleFrame = battle_frame_;
    context.config.gravity = gravity_;
    context.config.projectileBounceRange = static_cast<int>(PROJECTILE_BOUNCE_RANGE);
    context.config.coordCount = BATTLE_COORD_COUNT;
    return context;
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
                recordBattleStatusLog(&r, nullptr,
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
                    recordBattleStatusLog(&r, nullptr,
                        formatStatusValue("全隊獲取", teamFlatShield, "護盾"));
                }
                initializeTimedEffects(battleState);
                battleState.blockFirstHitsRemaining = battleState.blockFirstHitsCount;
                cs[r.ID] = battleState;
            }
        };
        applyOnCopies(friends_obj_, allyStates, allyGlobalEffects, allyTeamFlatShield, getAllyEquipment);
        applyOnCopies(enemies_obj_, enemyStates, enemyComboGlobalEffects, enemyTeamFlatShield, getEnemyEquipment);
        updateEnemyTopDebuffState();

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
            cs[clone->ID] = KysChess::ChessBattleEffects::makeSummonedCloneState(sourceState->second, clone->MaxHP);
            recordBattleStatusLog(source, clone, std::format("七截分身（落點 {}, {}）", x, y));
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

    initializeBattleRuntimeSession();

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
    auto input = buildBattleFrameInput();
    SceneBattleFrameResult result;
    if (input.shouldAdvance)
    {
        result.advanced = true;
        auto actionContext = makeBattleActionFrameAdapterContext();
        BattleMovementPhysicsFrameAdapterContext movementPhysicsContext;
        auto& scratch = battle_session_->beginFrameScratch();
        auto bundle = buildCoreRuntimeContext(actionContext, movementPhysicsContext, scratch);
        auto frameResult = battle_session_->runFrame();
        applyCoreFrameResult(bundle, frameResult, actionContext, movementPhysicsContext);
    }
    applyLegacyBattleFrameResult(result);
    playCorePresentationFrame();
}

BattleSceneHades::SceneBattleFrameInput BattleSceneHades::buildBattleFrameInput()
{
    SceneBattleFrameInput input;

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
        if (battle_frame_ % CAMERA_SLOW_STEP_INTERVAL)
        {
            input.shouldAdvance = false;
            return input;
        }
        //x_ = rand_.rand_int(2) - rand_.rand_int(2);
        //y_ = rand_.rand_int(2) - rand_.rand_int(2);
        slow_--;
    }
    ultHitRoles_.clear();
    criticalHitRoles_.clear();
    return input;
}

KysChess::BattleSceneBattleAdapter::BattleFrameApplyContext BattleSceneHades::buildCoreRuntimeContext(
    BattleActionFrameAdapterContext& actionContext,
    BattleMovementPhysicsFrameAdapterContext& movementPhysicsContext,
    KysChess::Battle::BattleFrameScratch& scratch)
{
    auto& comboStates = battleRuntime().combo.units;
    initializeBattleRuntimeStaticState();

    KysChess::BattleSceneBattleAdapter::BattleFrameApplyContext bundle;
    for (auto* role : battle_roles_)
    {
        assert(role);
        bundle.rolesByBattleId.emplace(role->ID, role);
    }
    populateCoreMovementWorld();
    battleRuntime().projectileFollowUps.nextSharedHitGroupId = next_shared_hit_group_id_;
    KysChess::Battle::clearBattleDamageFrameScratch(battleRuntime());
    battleRuntime().status.events.clear();
    battleRuntime().combo.events.clear();
    battleRuntime().deathEffects.events.clear();
    battleRuntime().teamEffects.committedEvents.clear();
    battleRuntime().effects.committedCommands.clear();

    for (auto role : battle_roles_)
    {
        assert(role);
        decreaseToZero(role->Shake);
        decreaseToZero(role->HurtFrame);
        decreaseToZero(role->Attention);
        decreaseToZero(role->Invincible);

        auto comboIt = comboStates.find(role->ID);
        if (comboIt != comboStates.end())
        {
            scratch.runtime.percentRolls.reserve(
                scratch.runtime.percentRolls.size() + comboIt->second.triggeredEffects.size() + 1);
            for (size_t i = 0; i <= comboIt->second.triggeredEffects.size(); ++i)
            {
                scratch.runtime.percentRolls.push_back(rand_.rand() * 100.0);
            }
        }

    }

    movementPhysicsContext.roles = &battle_roles_;

    battleRuntime().movementDecisions.clear();
    core_movement_frame_ = battle_frame_;
    populateBattleActionFrame(scratch, actionContext);
    for (int unitId : actionContext.movementDashStartUnitIds)
    {
        auto* role = requireFrameRole(bundle, unitId);
        auto& movementRuntime = battleRuntime().movementRuntime[role->ID];
        if (movementRuntime.movementDashFrames <= 0)
        {
            movementRuntime.movementDashFrames = DASH_MOMENTUM_FRAMES;
            movementRuntime.movementDashCooldown = MOVEMENT_DASH_COOLDOWN_FRAMES;
            movementRuntime.movementDashSpreadFrames = 0;
        }
    }

    for (size_t i = 0; i + 1 < battleRuntime().attacks.attacks.size(); ++i)
    {
        const auto& attack1 = battleRuntime().attacks.attacks[i];
        auto* attacker1 = requireFrameRole(bundle, attack1.state.attackerUnitId);
        auto* skill1 = Save::getInstance()->getMagic(attack1.state.skillId);
        for (size_t j = i + 1; j < battleRuntime().attacks.attacks.size(); ++j)
        {
            const auto& attack2 = battleRuntime().attacks.attacks[j];
            auto* attacker2 = requireFrameRole(bundle, attack2.state.attackerUnitId);
            if (attacker1->Team == attacker2->Team)
            {
                continue;
            }
            auto* skill2 = Save::getInstance()->getMagic(attack2.state.skillId);
            if (skill1)
            {
                scratch.projectileCancelBaseDamages.push_back({
                    attack1.id,
                    attack2.id,
                    calculateHitMagicBaseDamage(attacker1, attacker2, skill1),
                });
            }
            if (skill2)
            {
                scratch.projectileCancelBaseDamages.push_back({
                    attack2.id,
                    attack1.id,
                    calculateHitMagicBaseDamage(attacker2, attacker1, skill2),
                });
            }
        }
    }
    populateCoreHitState(bundle, scratch);
    return bundle;
}

void BattleSceneHades::applyCoreFrameResult(
    KysChess::BattleSceneBattleAdapter::BattleFrameApplyContext& bundle,
    const KysChess::Battle::BattleFrameResult& frameResult,
    const BattleActionFrameAdapterContext& actionContext,
    const BattleMovementPhysicsFrameAdapterContext& movementPhysicsContext)
{
    applyCoreStatusState(bundle);
    for (const auto& runtimeUnitResult : frameResult.runtimeResults)
    {
        auto* role = requireFrameRole(bundle, runtimeUnitResult.unitId);
        applyBattleFrameUnitRuntimeResult(role, runtimeUnitResult.result);
        for (const auto& event : runtimeUnitResult.comboEvents)
        {
            if (event.type == KysChess::Battle::BattleComboFrameRuntimeEventType::AutoUltimateReady)
            {
                playAutoUltimateReady(role);
            }
        }
        if (runtimeUnitResult.result.mpDelta != 0)
        {
            changeRoleMP(role, runtimeUnitResult.result.mpDelta);
        }
    }

    applyCoreDamageTransactions(bundle, frameResult.hitResults);
    applyCoreTeamEffectState(bundle);
    for (const auto& command : battleRuntime().effects.committedCommands)
    {
        auto* target = requireFrameRole(bundle, command.targetUnitId);
        const auto& unit = battleRuntime().units.requireUnit(command.targetUnitId);
        switch (command.type)
        {
        case KysChess::Battle::BattleEffectCommandType::AddInvincibility:
            target->Invincible = unit.invincible;
            break;
        default:
            assert(false);
            break;
        }
    }
    applyCoreFrameApplications(bundle, frameResult.applications);
    applyBattleMovementPhysicsFrameResults(frameResult.movementPhysicsResults, movementPhysicsContext);
    battleRuntime().movementDecisions.clear();
    for (const auto& [battleId, decision] : frameResult.movement.decisions)
    {
        auto it = bundle.rolesByBattleId.find(battleId);
        if (it != bundle.rolesByBattleId.end())
        {
            battleRuntime().movementDecisions[battleId] = decision;
        }
    }

    auto actionApply = applyBattleActionFrameResults(frameResult.actionResults, actionContext);
    for (int soundId : actionApply.attackSoundIds)
    {
        Audio::getInstance()->playASound(soundId);
    }
    for (int i = 0; i < actionApply.blinkSoundCount; ++i)
    {
        Audio::getInstance()->playESound(BLINK_SOUND_EFFECT_ID);
    }
    for (const auto& event : actionApply.visualEvents)
    {
        presentation_recorder_.recordVisual(event);
    }
    for (const auto& event : actionApply.logEvents)
    {
        presentation_recorder_.recordLog(event);
    }
    for (int unitId : actionApply.clearMovementDashSpreadUnitIds)
    {
        battleRuntime().movementRuntime[unitId].movementDashSpreadFrames = 0;
    }
    for (int unitId : actionApply.faceTowardsNearestUnitIds)
    {
        setFaceTowardsNearest(requireFrameRole(bundle, unitId));
    }
    for (const auto& command : frameResult.projectileCancelDamageCommands)
    {
        applyBattleProjectileCancelDamage(
            requireFrameRole(bundle, command.sourceUnitId),
            command.damage);
        applyBattleProjectileCancelDamage(
            requireFrameRole(bundle, command.otherSourceUnitId),
            command.otherDamage);
    }
    for (const auto& event : frameResult.frame.visualEvents)
    {
        presentation_recorder_.recordVisual(event);
    }
    for (const auto& event : frameResult.frame.logEvents)
    {
        presentation_recorder_.recordLog(event);
    }
    advanceVisualOnlyEffects(attack_effects_);

    for (const auto& event : frameResult.attackEvents)
    {
        if (event.type == KysChess::Battle::BattleAttackEventType::Hit)
        {
            auto* role = requireFrameRole(bundle, event.unitId);
            bool scriptedImpact = hasScriptedImpact(event);

            if (event.skillId >= 0)
            {
                auto* skill = Save::getInstance()->getMagic(event.skillId);
                assert(skill);
                Audio::getInstance()->playESound(skill->EffectID);
            }
            if (scriptedImpact)
            {
                role->Shake = 5;
                continue;
            }

            shake_ = event.ultimate ? 10 : 0;
            role->Frozen = 0;
            if (event.mainProjectile)
            {
                applyFrozen(role, event.ultimate ? 10 : 5, battleRuntime().combo.units);
            }
            role->Shake = event.ultimate ? 10 : 5;
            if (event.ultimate)
            {
                ultHitRoles_.insert(role);
            }
            if (event.operationType != KysChess::Battle::BattleOperationType::None
                || event.hiddenWeaponItemId >= 0)
            {
                Engine::getInstance()->gameControllerRumble(100, 100, 50);
                auto* usingMagic = event.skillId >= 0 ? Save::getInstance()->getMagic(event.skillId) : nullptr;
                auto* usingHiddenWeapon = event.hiddenWeaponItemId >= 0 ? Save::getInstance()->getItem(event.hiddenWeaponItemId) : nullptr;
                assert(usingHiddenWeapon || usingMagic);
                assert(event.totalFrame > 0);
            }
        }
    }
    next_shared_hit_group_id_ = battleRuntime().projectileFollowUps.nextSharedHitGroupId;
    battleRuntime().attacks.attacks.erase(
        std::remove_if(
            battleRuntime().attacks.attacks.begin(),
            battleRuntime().attacks.attacks.end(),
            [](const auto& attack)
            {
                return attack.frame >= attack.state.totalFrame;
            }),
        battleRuntime().attacks.attacks.end());
    for (auto it = attack_effects_.begin(); it != attack_effects_.end();)
    {
        if (it->Frame >= it->TotalFrame)
        {
            it = attack_effects_.erase(it);
        }
        else
        {
            ++it;
        }
    }
    KysChess::ChessCombo::getMutableStates() = battleRuntime().combo.units;
}

void BattleSceneHades::applyLegacyBattleFrameResult(const SceneBattleFrameResult& result)
{
    if (!result.advanced)
    {
        return;
    }
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

void BattleSceneHades::playCorePresentationFrame()
{
    publishPresentationFrame();
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

void BattleSceneHades::playAutoUltimateReady(Role* role)
{
    assert(role);
    if (auto magic = commitAutoUltimate(role, false))
    {
        recordFloatingTextPresentation(role, std::string(magic->Name), { 255, 215, 0, 255 }, EMPHASIS_TEXT_SIZE);
        recordBattleStatusLog(role, nullptr, std::format("自動絕招·{}", std::string(magic->Name)));
    }
}

void BattleSceneHades::updateEnemyTopDebuffState()
{
    auto& cs = battle_session_.has_value()
        ? battleRuntime().combo.units
        : KysChess::ChessCombo::getMutableStates();

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
            recordBattleStatusLog(nullptr,
                &enemy,
                std::format("陰險：前{}名攻防{}{}（{}名存活）",
                    topTargets,
                    delta > 0 ? "-" : "+",
                    change,
                    liveAllies));
        }
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

void BattleSceneHades::setRoleInitState(Role* role)
{
    BattleScene::setRoleInitState(role);
    if (role->Team == 0)
    {
        role->HP = role->MaxHP;
        role->MP = 0;
        role->PhysicalPower = (std::max)(role->PhysicalPower, 90);
    }
    else
    {
        role->HP = role->MaxHP;
        role->MP = 0;
        role->PhysicalPower = (std::max)(role->PhysicalPower, 90);
    }

    auto p = pos45To90(role->X(), role->Y());

    role->Pos.x = p.x;
    role->Pos.y = p.y;
    if (role->FaceTowards == Towards_RightDown)
    {
        role->RealTowards = { 1, 1 };
    }
    if (role->FaceTowards == Towards_RightUp)
    {
        role->RealTowards = { 1, -1 };
    }
    if (role->FaceTowards == Towards_LeftDown)
    {
        role->RealTowards = { -1, 1 };
    }
    if (role->FaceTowards == Towards_LeftUp)
    {
        role->RealTowards = { -1, -1 };
    }
    role->Acceleration = { 0, 0, gravity_ };
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

void BattleSceneHades::populateCoreHitState(
    KysChess::BattleSceneBattleAdapter::BattleFrameApplyContext& bundle,
    KysChess::Battle::BattleFrameScratch& scratch)
{
    auto& frameState = battleRuntime();

    for (const auto& attack : frameState.attacks.attacks)
    {
        appendCoreHitInputsForAttack(bundle, scratch, attack.id, attack.state);
    }

    int nextAttackId = frameState.attacks.nextAttackId;
    for (const auto& spawn : frameState.pendingAttackSpawns)
    {
        appendCoreHitInputsForAttack(bundle, scratch, nextAttackId, spawn.initial);
        ++nextAttackId;
    }
}

void BattleSceneHades::appendCoreHitInputsForAttack(
    KysChess::BattleSceneBattleAdapter::BattleFrameApplyContext& bundle,
    KysChess::Battle::BattleFrameScratch& scratch,
    int attackId,
    const KysChess::Battle::BattleAttackState& attackState)
{
    auto* attacker = requireFrameRole(bundle, attackState.attackerUnitId);
    auto* magic = attackState.skillId >= 0 ? Save::getInstance()->getMagic(attackState.skillId) : nullptr;
    auto* hiddenWeapon = attackState.hiddenWeaponItemId >= 0
        ? Save::getInstance()->getItem(attackState.hiddenWeaponItemId)
        : nullptr;

    for (auto* defender : battle_roles_)
    {
        if (defender->Dead != 0 || defender->Team == attacker->Team)
        {
            continue;
        }

        std::vector<double> percentRolls;
        percentRolls.reserve(65);
        auto& comboStates = battleRuntime().combo.units;
        if (comboStates.find(defender->ID) != comboStates.end())
        {
            percentRolls.push_back(rand_.rand() * 100.0);
        }
        for (int i = 0; i < 64; ++i)
        {
            percentRolls.push_back(rand_.rand() * 100.0);
        }

        appendBattleFrameHitInput(scratch, BattleFrameHitAdapterInput{
            attackId,
            attacker,
            defender,
            magic,
            hiddenWeapon,
            magic ? calculateHitMagicBaseDamage(attacker, defender, magic) : 0,
            hiddenWeapon ? calHiddenWeaponHurt(attacker, defender, hiddenWeapon) : 0,
            attackState.scriptedBleedStacks > 0 ? getSharedBleedMaxStacks(attacker) : 1,
            static_cast<int>(5 * (rand_.rand() - rand_.rand())),
            std::move(percentRolls),
            0,
        });
    }
}

void BattleSceneHades::initializeBattleRuntimeStaticState()
{
    if (!battleRuntime().world.terrainCells.empty())
    {
        return;
    }

    battleRuntime().world.terrainCells.reserve(BATTLE_COORD_COUNT * BATTLE_COORD_COUNT);
    battleRuntime().rescue.positionsByCell.clear();
    battleRuntime().rescue.cells.clear();
    battleRuntime().movementPhysics.collision.cells.clear();

    for (int x = 0; x < BATTLE_COORD_COUNT; ++x)
    {
        for (int y = 0; y < BATTLE_COORD_COUNT; ++y)
        {
            const auto position = pos45To90(x, y);
            const bool walkable = !isOutLine(x, y) && canWalk45(x, y);
            battleRuntime().world.terrainCells.push_back({ position, walkable });
            battleRuntime().rescue.positionsByCell.emplace(std::pair{ x, y }, position);
            battleRuntime().rescue.cells.push_back({
                x,
                y,
                walkable,
                false,
                -1,
            });
            battleRuntime().movementPhysics.collision.cells.push_back({
                x,
                y,
                walkable,
            });
        }
    }
}

int BattleSceneHades::calculateHitMagicBaseDamage(Role* attacker, Role* defender, Magic* magic)
{
    assert(attacker);
    assert(defender);
    assert(magic);

    double defence = defender->Defence;
    // 破甲：降低有效防禦
    auto& cs = battleRuntime().combo.units;
    auto it = cs.find(attacker->ID);
    if (it != cs.end())
    {
        defence = KysChess::Battle::resolveFrameArmorPenetratedDefense(
            it->second,
            attacker->ID,
            defender->ID,
            defence,
            rand_.rand() * 100.0);
    }

    return resolveBattleMagicBaseDamage({
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

KysChess::Battle::BattleUnitState BattleSceneHades::makeCoreMovementUnit(
    Role* role)
{
    bool isUltimate = role->MP >= role->MaxMP;
    Magic* plannedMagic = role->UsingMagic;
    if (!plannedMagic)
    {
        plannedMagic = isUltimate ? selectHigherPowerMagic(role) : selectLowerPowerMagic(role);
    }

    auto& cs = battleRuntime().combo.units;
    auto comboIt = cs.find(role->ID);
    bool forceRanged = roleForcesRangedMagic(cs, role->ID);
    bool dashAttackEnabled = comboIt != cs.end() && comboIt->second.dashAttack;
    int forcedRangedDistance = forcedRangedMinSelectDistance(cs, role->ID);
    int speedMultiplierPct = projectileSpeedMultiplierPct(cs, role->ID);

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
            effectiveBattleReach(plannedMagic, forceRanged, forcedRangedDistance, speedMultiplierPct),
            MAX_EFFECTIVE_BATTLE_REACH)
        : MELEE_ATTACK_REACH;
    unit.style = isBattleRangedStyleMagic(plannedMagic, forceRanged)
        ? KysChess::Battle::CombatStyle::Ranged
        : KysChess::Battle::CombatStyle::Melee;
    unit.taXue = dashAttackEnabled;
    unit.dashAttack = dashAttackEnabled;
    unit.canAttack = role->CoolDown == 0;
    auto existingMovement = std::find_if(
        battleRuntime().world.units.begin(),
        battleRuntime().world.units.end(),
        [role](const KysChess::Battle::BattleUnitState& existing)
        {
            return existing.id == role->ID;
        });
    if (existingMovement != battleRuntime().world.units.end())
    {
        unit.assignedSlot = existingMovement->assignedSlot;
        unit.slotSwitchCooldownRemaining = existingMovement->slotSwitchCooldownRemaining;
    }
    auto movementIt = battleRuntime().movementRuntime.find(role->ID);
    if (movementIt != battleRuntime().movementRuntime.end())
    {
        unit.dashFramesRemaining = movementIt->second.movementDashFrames;
        unit.dashCooldownRemaining = movementIt->second.movementDashCooldown;
    }
    return unit;
}

void BattleSceneHades::applyCoreStatusState(
    const KysChess::BattleSceneBattleAdapter::BattleFrameApplyContext& bundle)
{
    auto& frameState = battleRuntime();
    auto& comboStates = frameState.combo.units;
    for (const auto& status : frameState.status.units)
    {
        auto role = requireFrameRole(bundle, status.id);
        assert(role);
        auto stateIt = comboStates.find(status.id);
        assert(stateIt != comboStates.end());
        const auto& runtimeUnit = frameState.units.requireUnit(status.id);
        const auto unit = KysChess::Battle::makeBattleStatusUnitState(status, runtimeUnit);
        writeBattleStatusUnit(role, stateIt->second, unit);
    }
}

void BattleSceneHades::initializeCoreDamageState()
{
    auto& frameState = battleRuntime();
    auto& comboStates = frameState.combo.units;
    frameState.damage = {};
    frameState.damage.aggregatePendingTransactionsByDefender = true;
    frameState.result.pendingAliveByTeam = { { 1, static_cast<int>(enemies_.size()) } };
    for (auto role : battle_roles_)
    {
        assert(role);
        auto stateIt = comboStates.find(role->ID);
        frameState.damage.unitExtras.push_back(KysChess::Battle::makeBattleDamageRuntimeUnit(
            makeBattleDamageUnit(
                role,
                stateIt != comboStates.end() ? &stateIt->second : nullptr)));
        frameState.damage.presentationStylesByDefender.emplace(role->ID, makeBattleDamagePresentationStyle(role));
        if (stateIt != comboStates.end() && stateIt->second.deathAOEPct > 0)
        {
            frameState.damage.unitEffects.emplace(
                role->ID,
                KysChess::Battle::BattleDamageApplicationUnitEffects{
                    stateIt->second.deathAOEPct,
                    stateIt->second.deathAOEStunFrames,
                    stateIt->second.deathAOEMaxTargets,
                });
        }
    }
}

void BattleSceneHades::applyCoreDamageTransactions(
    const KysChess::BattleSceneBattleAdapter::BattleFrameApplyContext& bundle,
    const std::vector<KysChess::Battle::BattleHitResolutionResult>& hitResults)
{
    auto& frameState = battleRuntime();
    for (const auto& hitResolution : hitResults)
    {
        if (hitResolution.critical)
        {
            criticalHitRoles_.insert(requireFrameRole(bundle, hitResolution.defenderUnitId));
        }
    }

    auto& cs = frameState.combo.units;
    std::set<int> diedUnitIds;
    std::vector<KysChess::Battle::BattleGameplayEvent> lifecycleEvents;
    for (const auto& event : frameState.damage.lifecycleEvents)
    {
        switch (event.type)
        {
        case KysChess::Battle::BattleDamageLifecycleEventType::UnitDied:
            diedUnitIds.insert(event.targetUnitId);
            lifecycleEvents.push_back({
                KysChess::Battle::BattleGameplayEventType::UnitDied,
                battle_frame_,
                event.sourceUnitId,
                event.targetUnitId,
                event.value,
            });
            break;
        case KysChess::Battle::BattleDamageLifecycleEventType::BattleEnded:
            lifecycleEvents.push_back({
                KysChess::Battle::BattleGameplayEventType::BattleEnded,
                battle_frame_,
                -1,
                -1,
                event.value,
            });
            break;
        case KysChess::Battle::BattleDamageLifecycleEventType::KillRecorded:
            break;
        }
    }

    for (const auto& damageTaken : frameState.damage.committedTransactions)
    {
        auto r = requireFrameRole(bundle, damageTaken.defender.id);
        Role* attacker = damageTaken.attacker.id >= 0
            ? requireFrameRole(bundle, damageTaken.attacker.id)
            : nullptr;
        r->LastAttacker = attacker;
        auto sit = cs.find(r->ID);
        auto kit = attacker ? cs.find(attacker->ID) : cs.end();
        if (attacker && kit != cs.end())
        {
            writeBattleDamageUnit(attacker, &kit->second, damageTaken.attacker);
        }
        writeBattleDamageUnit(r, sit != cs.end() ? &sit->second : nullptr, damageTaken.defender);
        if (sit != cs.end())
        {
            writeBattleStatusUnit(r, sit->second, damageTaken.defenderStatus);
        }
        r->CoolDown = damageTaken.defenderCooldown.cooldown;

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
            AttackEffect ae1;
            ae1.FollowRole = r;
            ae1.setPath(std::format("eft/bld{:03}", int(rand_.rand() * 5)));
            ae1.TotalFrame = ae1.TotalEffectFrame;
            ae1.Frame = 0;
            attack_effects_.push_back(std::move(ae1));
            hurt_flash_timers_[r->ID] = HURT_FLASH_DURATION;
            double mpGain = static_cast<double>(committedHpDamage) / r->MaxHP * 75.0;
            changeRoleMP(r, mpGain);
        }

        if (diedUnitIds.find(r->ID) != diedUnitIds.end())
        {
            if (sit != cs.end())
            {
                sit->second.onSkillTeamHealPending = false;
            }

            KysChess::ChessCombo::transferAntiCombo(r->ID, battle_roles_);

            r->Velocity.normTo(15);
            r->Velocity.z = 12;
            r->Velocity.normTo(committedHpDamage / 2.0);
            r->Frozen = 0;
            applyFrozen(r, 5, cs);
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
    }

    for (const auto& rescue : frameState.rescue.committedResults)
    {
        assert(rescue.teleport);
        auto* pulled = requireFrameRole(bundle, rescue.teleport->unitId);
        auto* puller = requireFrameRole(bundle, rescue.teleport->pullerUnitId);
        const auto& destination = rescue.teleport->destinationCell;
        auto pos = pos45To90(destination.x, destination.y);
        pulled->setPositionOnly(destination.x, destination.y);
        pulled->Pos.x = pos.x;
        pulled->Pos.y = pos.y;
        pulled->Pos.z = 0;
        pulled->Velocity = { 0, 0, 0 };
        pulled->Acceleration = { 0, 0, gravity_ };
        pulled->FindingWay = 0;
        setFaceTowardsNearest(pulled);
        setFaceTowardsNearest(puller);

        if (rescue.heal.amount > 0)
        {
            assert(rescue.heal.targetUnitId == pulled->ID);
            pulled->HP = std::min(pulled->MaxHP, pulled->HP + rescue.heal.amount);
            recordRoleEffectPresentation(pulled, KysChess::EFT_HEAL, ROLE_STATUS_EFT_FRAMES);
        }
        if (rescue.invincibility.frames > 0)
        {
            assert(rescue.invincibility.targetUnitId == pulled->ID);
            pulled->Invincible += rescue.invincibility.frames;
        }
    }

    writeBattleDeathEffectTrackers(frameState.deathEffects.store, battle_roles_, cs);

    auto lifecycleApply = applyBattleLifecycleEvents(
        {
            &tracker_,
            &battle_roles_,
            result_,
        },
        lifecycleEvents);
    if (lifecycleApply.unitDied)
    {
        updateEnemyTopDebuffState();
    }
    if (lifecycleApply.battleEnded)
    {
        if (!isManualCameraEnabled())
        {
            close_up_ = std::max(close_up_, CAMERA_BATTLE_END_ZOOM_FRAMES);
            close_up_total_ = std::max(close_up_total_, close_up_);
        }
        frozen_ = 60;
        slow_ = CAMERA_BATTLE_END_SLOW_FRAMES;
        shake_ = 60;
        result_ = lifecycleApply.battleResult;
    }
}

void BattleSceneHades::applyCoreTeamEffectState(
    const KysChess::BattleSceneBattleAdapter::BattleFrameApplyContext& bundle)
{
    auto& frameState = battleRuntime();
    auto& comboStates = frameState.combo.units;
    for (const auto& event : frameState.teamEffects.committedEvents)
    {
        auto* target = requireFrameRole(bundle, event.targetUnitId);
        const auto& unit = frameState.units.requireUnit(event.targetUnitId);
        switch (event.type)
        {
        case KysChess::Battle::BattleTeamEffectEventType::Heal:
            target->HP = unit.hp;
            break;
        case KysChess::Battle::BattleTeamEffectEventType::MpRestore:
            target->MP = unit.mp;
            break;
        case KysChess::Battle::BattleTeamEffectEventType::ShieldGain:
            comboStates[target->ID].shield = unit.shield;
            break;
        case KysChess::Battle::BattleTeamEffectEventType::CooldownReduced:
            target->CoolDown = unit.cooldown;
            break;
        }
    }
}

void BattleSceneHades::applyCoreFrameApplications(
    const KysChess::BattleSceneBattleAdapter::BattleFrameApplyContext& bundle,
    const KysChess::Battle::BattleFrameApplications& applications)
{
    for (const auto& knockback : applications.knockbacks)
    {
        auto* target = requireFrameRole(bundle, knockback.targetUnitId);
        target->Velocity += knockback.velocityDelta;
        if (knockback.velocityCap > 0.0 && target->Velocity.norm() > knockback.velocityCap)
        {
            target->Velocity.normTo(static_cast<float>(knockback.velocityCap));
        }
        if (knockback.grantHurtFrame)
        {
            target->HurtFrame = 1;
        }
    }
    for (const auto& restore : applications.mpRestores)
    {
        auto* target = requireFrameRole(bundle, restore.unitId);
        target->MP = std::min(GameUtil::MAX_MP, target->MP + restore.amount);
    }
    for (const auto& heal : applications.unitHeals)
    {
        auto* target = requireFrameRole(bundle, heal.targetUnitId);
        target->HP = std::min(target->MaxHP, target->HP + heal.amount);
        recordRoleEffectPresentation(target, KysChess::EFT_HEAL, ROLE_STATUS_EFT_FRAMES);
    }
    for (const auto& buff : applications.tempAttackBuffs)
    {
        auto* target = requireFrameRole(bundle, buff.unitId);
        target->Attack += buff.attackBonus;
        target->Defence += buff.defenceBonus;
    }
    for (const auto& lastAttacker : applications.lastAttackers)
    {
        auto* target = requireFrameRole(bundle, lastAttacker.targetUnitId);
        target->LastAttacker = requireFrameRole(bundle, lastAttacker.attackerUnitId);
    }
    for (const auto& request : applications.autoUltimateRequests)
    {
        commitAutoUltimate(requireFrameRole(bundle, request.unitId), request.consumeMp);
    }
    for (const auto& rumble : applications.rumbles)
    {
        Engine::getInstance()->gameControllerRumble(
            rumble.lowFrequency,
            rumble.highFrequency,
            rumble.durationMs);
    }
}

void BattleSceneHades::populateCoreMovementWorld()
{
    auto& world = battleRuntime().world;
    world.frame = battle_frame_;
    world.units.clear();

    for (auto* role : battle_roles_)
    {
        assert(role);
        if (role->Dead != 0)
        {
            continue;
        }

        world.units.push_back(makeCoreMovementUnit(role));
    }
}
