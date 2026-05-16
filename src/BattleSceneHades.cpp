#include "BattleSceneHades.h"
#include "Audio.h"
#include "BattleSceneBattleAdapter.h"
#include "BattleScenePresentationConstants.h"
#include "BattleSceneSetupBuilder.h"
#include "BattleStarStats.h"
#include "battle/BattleAttackSystem.h"
#include "battle/BattleCombatIntent.h"
#include "battle/BattleFind.h"
#include "battle/BattleOperation.h"
#include "battle/BattleStatusSystem.h"
#include "ChessBalance.h"
#include "ChessCombo.h"
#include "ChessEquipment.h"
#include "ChessNeigong.h"
#include "ChessUiCommon.h"
#include "Engine.h"
#include "Event.h"
#include "Font.h"
#include "GameUtil.h"
#include "MainScene.h"
#include "Save.h"
#include "SystemSettings.h"
#include "TextureManager.h"
#include "Weather.h"
#include "strfunc.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <format>
#include <numeric>
#include <set>
#include <utility>

namespace
{
constexpr int HURT_FLASH_DURATION = 15;
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
constexpr int BATTLE_COORD_COUNT = BATTLEMAP_COORD_COUNT;
constexpr int ROLE_STATUS_BAR_WIDTH = 48;
constexpr int ROLE_STATUS_BAR_HEIGHT = 6;
constexpr int ROLE_STATUS_BAR_Y = -120;
constexpr int ROLE_STATUS_BAR_STEP_Y = ROLE_STATUS_BAR_HEIGHT + ROLE_STATUS_BAR_HEIGHT / 3;
constexpr int ROLE_STATUS_BAR_MP_Y = ROLE_STATUS_BAR_Y + ROLE_STATUS_BAR_STEP_Y;
constexpr int ROLE_STATUS_BAR_FROZEN_Y = ROLE_STATUS_BAR_Y + ROLE_STATUS_BAR_STEP_Y * 2;

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

std::array<int, 5> readBattleFightFramesForHeadId(int headId)
{
    std::array<int, 5> fightFrames{};
    const auto textGroup = std::format("fight/fight{:03}", headId);
    const auto frameText = TextureManager::getInstance()->getTextureGroup(textGroup)->getFileContent("fightframe.txt");
    std::vector<int> frames;
    strfunc::findNumbers(frameText, &frames);
    for (int index = 0; index < static_cast<int>(frames.size()) / 2; ++index)
    {
        fightFrames[frames[index * 2]] = frames[index * 2 + 1];
    }
    return fightFrames;
}

}    // namespace

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
    battle_map_.initialize(COORD_COUNT);
}

BattleSceneHades::BattleSceneHades(int id, KysChess::ChessRoleSave& roleSave, KysChess::ChessProgress& progress, KysChess::ChessManager& chessManager) :
    BattleSceneHades(roleSave, progress, chessManager)
{
    setID(id);
}

BattleSceneHades::~BattleSceneHades()
{
}

void BattleSceneHades::setID(int id)
{
    battle_id_ = id;
    info_ = BattleMap::getInstance()->getBattleInfo(id);
    battle_map_.loadBattlefield(info_->BattleFieldID);
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
        for (const auto& unit : scene_units_.units())
        {
            if (unit.identity.team == 0 && unit.alive)
            {
                center += unit.motion.position;
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

Color BattleSceneHades::calculateHurtFlashColor(int unitId, const Color& baseColor) const
{
    auto it = hurt_flash_timers_.find(unitId);
    const int timer = it != hurt_flash_timers_.end() ? it->second : 0;
    return BattleSceneRenderMath::hurtFlashColor(timer, baseColor);
}

void BattleSceneHades::beginPresentationFrame()
{
    presentation_recorder_.beginFrame(battle_frame_);
}

void BattleSceneHades::publishPresentationFrame()
{
    last_presentation_frame_ = presentation_recorder_.consumeFrame();
    report_player_.playLogs(last_presentation_frame_.logEvents, {
        &battle_report_,
        &scene_units_,
    });
    presentation_player_.play(last_presentation_frame_, {
        &text_effects_,
        &attack_effects_,
        &scene_units_,
    });
}

void BattleSceneHades::initializeBattleRuntime(
    KysChess::BattleSceneSetupBuilder::BattleSceneSetupBuildResult setupBuild)
{
    KysChess::ChessCombo::clearActiveStates();
    auto buildContext = makeBattleRuntimeBuildContext(std::move(setupBuild));
    auto created = KysChess::BattleSceneBattleAdapter::createInitializedBattleRuntimeSession(
        buildContext);
    battle_session_.emplace(std::move(created.session));
    KysChess::ChessCombo::getMutableStates() = created.sceneInitialization.comboStates;
    scene_units_.initialize(std::move(created.sceneInitialization.units));
    if (!created.sceneInitialization.logEvents.empty() || !created.sceneInitialization.visualEvents.empty())
    {
        beginPresentationFrame();
        for (const auto& event : created.sceneInitialization.visualEvents)
        {
            presentation_recorder_.recordVisual(event);
        }
        for (const auto& event : created.sceneInitialization.logEvents)
        {
            presentation_recorder_.recordLog(event);
        }
        publishPresentationFrame();
    }
}

void BattleSceneHades::setBattleRuntimeRandomSeed(unsigned int seed)
{
    battle_random_seed_ = seed;
}

KysChess::BattleSceneBattleAdapter::BattleRuntimeBuildContext BattleSceneHades::makeBattleRuntimeBuildContext(
    KysChess::BattleSceneSetupBuilder::BattleSceneSetupBuildResult setupBuild)
{
    KysChess::BattleSceneBattleAdapter::BattleRuntimeBuildContext context;
    context.rules = KysChess::Battle::makeHadesBattleRuntimeRules(TILE_W, BATTLE_COORD_COUNT);
    context.randomSeed = battle_random_seed_;
    context.rules.movementPhysicsConfig.gravity = gravity_;
    context.rules.movementPhysicsConfig.friction = friction_;
    context.input.runtimeSetupSeed.units = std::move(setupBuild.initializationUnits);
    context.input.runtimeSetupSeed.allyRoster = std::move(setupBuild.allyRoster);
    context.input.runtimeSetupSeed.enemyRoster = std::move(setupBuild.enemyRoster);
    context.input.runtimeSetupSeed.cloneSources = std::move(setupBuild.cloneSources);
    context.input.actionPlanSeeds = std::move(setupBuild.actionPlanSeeds);
    context.input.units = std::move(setupBuild.units);
    context.input.comboStates = &KysChess::ChessCombo::getMutableStates();
    context.input.battleFrame = battle_frame_;

    auto* basicMagic = Save::getInstance()->getMagic(1);
    assert(basicMagic);
    context.input.rescueCounterAttackSkillId = basicMagic->ID;

    context.input.terrainCells.reserve(BATTLE_COORD_COUNT * BATTLE_COORD_COUNT);
    context.input.rescueCells.reserve(BATTLE_COORD_COUNT * BATTLE_COORD_COUNT);
    context.rules.movementCollisionWorld.walkableByCell.assign(BATTLE_COORD_COUNT * BATTLE_COORD_COUNT, 0);
    for (int x = 0; x < BATTLE_COORD_COUNT; ++x)
    {
        for (int y = 0; y < BATTLE_COORD_COUNT; ++y)
        {
            const auto position = battle_map_.pos45To90(x, y);
            const bool walkable = battle_map_.canWalk45(x, y);
            context.input.terrainCells.push_back({ position, walkable });
            context.input.rescueCells.push_back({
                x,
                y,
                walkable,
                false,
                -1,
                position,
            });
            const auto movementCellIndex = static_cast<std::size_t>(y * BATTLE_COORD_COUNT + x);
            context.rules.movementCollisionWorld.walkableByCell[movementCellIndex] = walkable ? 1 : 0;
        }
    }

    auto& obtained = progress_.getObtainedNeigong();
    context.input.runtimeSetupSeed.obtainedNeigongMagicIds.assign(obtained.begin(), obtained.end());
    for (const auto& [x, y] : clone_spawn_positions_)
    {
        bool occupied = false;
        for (const auto& unit : context.input.units)
        {
            if (unit.alive && unit.gridX == x && unit.gridY == y)
            {
                occupied = true;
                break;
            }
        }
        context.input.runtimeSetupSeed.cloneCells.push_back({ x, y, true, occupied });
    }
    return context;
}

void BattleSceneHades::runPreBattlePositionSwapIfEnabled()
{
    if (progress_.isPositionSwapEnabled())
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
}

void BattleSceneHades::commitFinalSetupPlacementToRuntime()
{
    assert(battle_session_.has_value());
    battle_session_->commitSetupPlacement(scene_units_.makeSetupPlacementInput());
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
        auto p = battle_map_.pos90To45(pos_.x, pos_.y);
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
            auto p = battle_map_.pos45To90(ix, iy);
            if (!battle_map_.isOutLine(ix, iy))
            {
                int num = battle_map_.earthLayer().data(ix, iy) / 2;
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
            auto p = battle_map_.pos45To90(ix, iy);
            if (!battle_map_.isOutLine(ix, iy))
            {
                int num = battle_map_.buildingLayer().data(ix, iy) / 2;
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
    for (const auto& unit : scene_units_.units())
    {
        if (!unit.active)
        {
            continue;
        }
        if (!is_visible_world(unit.motion.position.x, unit.motion.position.y / 2.0))
        {
            continue;
        }
        //if (r->Dead) { continue; }
        DrawInfo info;
        auto path = std::format("fight/fight{:03}", unit.identity.headId);
        info.color = { 255, 255, 255, 255 };
        info.alpha = 255;
        info.white = 0;
        info.p = unit.motion.position;
        if (result_ == -1 && unit.shake)
        {
            info.p.x += shakeJitter(battle_frame_, unit.unitId);
        }
        info.tex = TextureManager::getInstance()->getTexture(
            path,
            BattleSceneRenderMath::calRenderUnitPic(
                unit.fightFrames,
                unit.motion.facing,
                unit.animation.actType,
                unit.animation.actFrame));
        if (!info.tex)
        {
            continue;
        }
        if (!unit.alive)
        {
            //if (r->Frozen == 0)
            {
                if (realTowardsToFaceTowards(unit.motion.facing) >= 2)
                {
                    info.rot = 90;
                }
                else
                {
                    info.rot = 270;
                }
            }
        }
        if (unit.attention)
        {
            info.alpha = 255 - unit.attention * 4;
        }

        // 應用受击闪红
        info.color = calculateHurtFlashColor(unit.unitId, info.color);

        info.shadow = 1;
        //TextureManager::getInstance()->renderTexture(path, pic, r->X1, r->Y1, color, alpha);
        draw_infos.emplace_back(std::move(info));
    }

    //effects
    for (auto& ae : attack_effects_)
    {
        {
            Pointf effect_pos = ae.Pos;
            if (ae.FollowUnitId >= 0)
            {
                effect_pos = scene_units_.requireUnit(ae.FollowUnitId).motion.position + ae.Pos;
            }
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
            const bool followsUnit = ae.FollowUnitId >= 0;
            info.alpha = followsUnit ? 255 : 192;
            info.shadow = followsUnit ? 0 : 1;
            if (!followsUnit && ae.renderTeam() == 0)
            {
                info.shadow = 2;
            }
            if (!followsUnit)
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

    for (const auto& unit : scene_units_.units())
    {
        if (is_visible_world(unit.motion.position.x, unit.motion.position.y / 2.0))
        {
            renderExtraRoleInfo(
                unit,
                renderWorldX(unit.motion.position.x),
                renderWorldY(unit.motion.position.y / 2.0));
        }
    }

    if (swapSelectedUnitId_ >= 0)
    {
        const auto& selectedUnit = scene_units_.requireUnit(swapSelectedUnitId_);
        if (is_visible_world(selectedUnit.motion.position.x, selectedUnit.motion.position.y / 2.0))
        {
            engine->fillColor(
                { 255, 255, 0, 80 },
                renderWorldX(selectedUnit.motion.position.x - 15),
                renderWorldY(selectedUnit.motion.position.y / 2.0 - 5),
                std::max(1, int(30 * viewScaleX)),
                std::max(1, int(10 * viewScaleY)));
        }
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
    for (const auto& unit : scene_units_.units())
    {
        if (!unit.alive) { continue; }
        double wx = unit.motion.position.x;
        double wy = unit.motion.position.y / 2.0;
        int ux = worldToUiX(wx);
        int uy = worldToUiY(wy);
        if (ux < -80 || ux >= ui_w + 80 || uy < -80 || uy >= ui_h + 80) { continue; }
        if (unit.frozen > 0 && unit.frozenMax > 0)
        {
            Font::getInstance()->draw(std::to_string(unit.frozen),
                (std::max)(1, int(9 * sizeScale)),
                worldToUiX(wx - 5), worldToUiY(wy + ROLE_STATUS_BAR_FROZEN_Y - 1),
                { 255, 255, 255, 255 });
        }
        else if (unit.animation.cooldown > 0 && unit.animation.cooldownMax > 0)
        {
            Font::getInstance()->draw(std::to_string(unit.animation.cooldown),
                (std::max)(1, int(9 * sizeScale)),
                worldToUiX(wx - 5), worldToUiY(wy + ROLE_STATUS_BAR_FROZEN_Y - 1),
                { 230, 195, 120, 240 });
        }
        if (positionSwapActive_ && unit.identity.team == 0)
        {
            std::string coord = std::format("({},{})", unit.gridX, unit.gridY);
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
    switch (SystemSettings::getInstance()->data().battleSpeed)
    {
    case 0:
        half_speed_step_on_next_render_ = true;
        return 2;
    case 2:
    {
        const int steps = half_speed_step_on_next_render_ ? 1 : 0;
        half_speed_step_on_next_render_ = !half_speed_step_on_next_render_;
        return steps;
    }
    default:
        half_speed_step_on_next_render_ = true;
        return 1;
    }
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
    BattleSceneRenderMath::decreaseToZero(close_up_);
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

    calViewRegion();

    addChild(Weather::getInstance());

    battle_map_.makeEarthTexture(render_center_x_, render_center_y_);

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

    assert(!extended_teammates_.empty());

    using KysChess::BattleSceneSetupBuilder::BattleSceneSetupUnitRequest;

    std::vector<BattleSceneSetupUnitRequest> enemyRequests;
    std::vector<BattleSceneSetupUnitRequest> allyRequests;

    auto getAllyEquipment = [&](size_t index)
    {
        int weaponId = index < teammate_weapons_.size() ? teammate_weapons_[index] : -1;
        int armorId = index < teammate_armors_.size() ? teammate_armors_[index] : -1;
        if (weaponId >= 0 || armorId >= 0)
        {
            return std::pair{ weaponId, armorId };
        }

        assert(index < extended_teammates_.size());
        const auto& teammate = extended_teammates_[index];
        if (teammate.weaponId >= 0 || teammate.armorId >= 0)
        {
            return std::pair{ teammate.weaponId, teammate.armorId };
        }

        auto chess = chessManager_.tryFindChessByInstanceId(KysChess::ChessInstanceID{ teammate.chessInstanceId });
        if (!chess)
        {
            return std::pair{ -1, -1 };
        }
        return std::pair{ chess->weaponInstance.itemId, chess->armorInstance.itemId };
    };
    auto getEnemyEquipment = [&](size_t index)
    {
        return std::pair{
            index < enemy_weapons_.size() ? enemy_weapons_[index] : -1,
            index < enemy_armors_.size() ? enemy_armors_[index] : -1,
        };
    };
    auto getTeammateFightsWon = [&](size_t index)
    {
        assert(index < extended_teammates_.size());
        const int chessInstanceId = extended_teammates_[index].chessInstanceId;
        if (chessInstanceId < 0)
        {
            return 0;
        }

        auto chess = chessManager_.tryFindChessByInstanceId(KysChess::ChessInstanceID{ chessInstanceId });
        return chess ? chess->fightsWon : 0;
    };

    for (int i = 0; i < BATTLE_ENEMY_COUNT; i++)
    {
        auto* source = roleSave_.getRole(info_->Enemy[i]);
        if (!source)
        {
            continue;
        }

        const auto [weaponId, armorId] = getEnemyEquipment(enemyRequests.size());
        BattleSceneSetupUnitRequest request;
        request.source = source;
        request.team = 1;
        request.gridX = info_->EnemyX[i];
        request.gridY = info_->EnemyY[i];
        request.star = KysChess::normalizeBattleStar(i < static_cast<int>(enemy_stars_.size()) ? enemy_stars_[i] : 0);
        request.faceTowardsFallback = source->FaceTowards != Towards_None
            ? source->FaceTowards
            : Towards_RightDown;
        request.weaponId = weaponId;
        request.armorId = armorId;
        request.sourceOrder = static_cast<int>(enemyRequests.size());
        enemyRequests.push_back(std::move(request));
        LOG("Adding enemy role {} name {}\n", source->ID, source->Name);
    }
    assert(!enemyRequests.empty());

    allyRequests.reserve(extended_teammates_.size());
    for (size_t index = 0; index < extended_teammates_.size(); ++index)
    {
        const auto& teammate = extended_teammates_[index];
        auto* source = roleSave_.getRole(teammate.ID);
        assert(source);

        const auto [weaponId, armorId] = getAllyEquipment(index);
        BattleSceneSetupUnitRequest request;
        request.source = source;
        request.team = 0;
        request.gridX = teammate.X;
        request.gridY = teammate.Y;
        request.star = KysChess::normalizeBattleStar(teammate.star);
        request.faceTowardsFallback = source->FaceTowards;
        request.weaponId = weaponId;
        request.armorId = armorId;
        request.chessInstanceId = teammate.chessInstanceId;
        request.fightsWon = getTeammateFightsWon(index);
        request.sourceOrder = static_cast<int>(index);
        allyRequests.push_back(std::move(request));
    }
    assert(!allyRequests.empty());

    std::vector<std::size_t> enemyIndexes(enemyRequests.size());
    std::iota(enemyIndexes.begin(), enemyIndexes.end(), 0);
    std::sort(enemyIndexes.begin(), enemyIndexes.end(), [&](std::size_t lhs, std::size_t rhs)
        {
            const auto* left = enemyRequests[lhs].source;
            const auto* right = enemyRequests[rhs].source;
            assert(left);
            assert(right);
            return left->MaxHP + left->Attack < right->MaxHP + right->Attack;
        });
    int nextUnitId = 0;
    for (const auto index : enemyIndexes)
    {
        enemyRequests[index].unitId = nextUnitId++;
    }
    for (auto& request : allyRequests)
    {
        request.unitId = nextUnitId++;
    }

    auto applySharedSetupCallbacks = [&](BattleSceneSetupUnitRequest& request)
    {
        assert(request.source);
        request.gravity = gravity_;
        request.position = battle_map_.pos45To90(request.gridX, request.gridY);
        request.fightFrames = readBattleFightFramesForHeadId(request.source->HeadID);
        for (int index = 0; index < ROLE_MAGIC_COUNT; ++index)
        {
            request.magicSlots[index] = Save::getInstance()->getMagic(request.source->MagicID[index]);
        }
    };

    std::vector<BattleSceneSetupUnitRequest> setupRequests;
    setupRequests.reserve(enemyRequests.size() + allyRequests.size());
    setupRequests.insert(setupRequests.end(), enemyRequests.begin(), enemyRequests.end());
    setupRequests.insert(setupRequests.end(), allyRequests.begin(), allyRequests.end());
    for (auto& request : setupRequests)
    {
        applySharedSetupCallbacks(request);
    }
    auto setupBuild = KysChess::BattleSceneSetupBuilder::buildSetupUnits(setupRequests);

    int sx = 0;
    int sy = 0;
    for (const auto& ally : allyRequests)
    {
        sx += ally.gridX;
        sy += ally.gridY;
    }
    pos_ = battle_map_.pos45To90(
        sx / static_cast<int>(allyRequests.size()),
        sy / static_cast<int>(allyRequests.size()));
    auto cameraCell = battle_map_.pos90To45(pos_.x, pos_.y);
    man_x_ = cameraCell.x;
    man_y_ = cameraCell.y;

    battle_frame_ = 0;
    half_speed_step_on_next_render_ = true;
    manual_camera_dragging_ = false;
    camera_target_ = pos_;
    close_up_total_ = 0;
    clampCameraCenter();

    initializeBattleRuntime(std::move(setupBuild));
    runPreBattlePositionSwapIfEnabled();
    commitFinalSetupPlacementToRuntime();

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
    // Engine::getInstance()->hideBattleLogOverlay();
}

void BattleSceneHades::calExpGot()
{
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

BattlePostBattleSummary BattleSceneHades::makePostBattleSummary() const
{
    return scene_units_.makePostBattleSummary(battle_report_.report(), result_);
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
            int clickedUnitId = -1;
            for (int unitId : battle_->scene_units_.allyUnitIds())
            {
                const auto& unit = battle_->scene_units_.requireUnit(unitId);
                if (unit.identity.team == 0 && unit.gridX == p.x && unit.gridY == p.y)
                {
                    clickedUnitId = unitId;
                    break;
                }
            }
            if (clickedUnitId < 0)
            {
                return;
            }
            if (battle_->swapSelectedUnitId_ < 0)
            {
                battle_->swapSelectedUnitId_ = clickedUnitId;
            }
            else if (clickedUnitId != battle_->swapSelectedUnitId_)
            {
                battle_->scene_units_.swapSetupUnitPositions(
                    battle_->swapSelectedUnitId_,
                    clickedUnitId);
                battle_->swapSelectedUnitId_ = -1;
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
            battle_->swapSelectedUnitId_ = -1;
            exit_ = true;
        }
    }
};

void BattleSceneHades::runPositionSwapLoop()
{
    swapSelectedUnitId_ = -1;
    auto node = std::make_shared<PositionSwapNode>(this);
    node->run();
}

void BattleSceneHades::runListBasedSwap()
{
    positionSwapActive_ = true;
    auto allies = scene_units_.allyUnitIds();
    if (allies.size() < 2)
    {
        positionSwapActive_ = false;
        return;
    }

    // Build menu items with proper alignment using getTextDrawSize
    // Name is left-aligned, coordinates are right-aligned
    auto buildNames = [&](int highlight = -1)
    {
        // First pass: find max name width and max coord width separately
        int maxNameLen = 0;
        int maxCoordLen = 0;
        for (int unitId : allies)
        {
            const auto& unit = scene_units_.requireUnit(unitId);
            std::string name = unit.identity.name;
            std::string coord = std::format("({},{})", unit.gridX, unit.gridY);
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
            const auto& unit = scene_units_.requireUnit(allies[i]);
            std::string name = unit.identity.name;
            std::string coord = std::format("({},{})", unit.gridX, unit.gridY);
            int nameLen = Font::getTextDrawSize(name);
            int coordLen = Font::getTextDrawSize(coord);
            // Pad name to max, then pad coord prefix so coords right-align
            int gap = (maxNameLen - nameLen) + (maxCoordLen - coordLen) + 2;
            std::string s = std::format("{}{}{}", name, std::string(gap, ' '), coord);
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
        scene_units_.swapSetupUnitPositions(
            allies[sel1],
            allies[sel2]);
    }
    swapSelectedUnitId_ = -1;
    positionSwapActive_ = false;
}

void BattleSceneHades::backRun1()
{
    auto input = buildBattleFrameInput();
    SceneBattleFrameResult result;
    if (input.shouldAdvance)
    {
        result.advanced = true;
        advanceBattlePresentationEffects(attack_effects_, true);
        auto frameResult = battle_session_->runFrame();
        applyCoreFrameResult(frameResult);
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
    return input;
}

void BattleSceneHades::applyCoreFrameResult(
    const KysChess::Battle::BattleFrameResult& frameResult)
{
    auto& comboStates = KysChess::ChessCombo::getMutableStates();
    BattleSceneFrameStateApplyContext context;
    context.units = &scene_units_;
    context.comboStates = &comboStates;
    context.hurtFlashTimers = &hurt_flash_timers_;
    context.random = &rand_;
    context.pos45To90 = [this](int x, int y)
    {
        return battle_map_.pos45To90(x, y);
    };
    context.transferAntiCombo = [this](int unitId)
    {
        KysChess::ChessCombo::transferAntiCombo(unitId, scene_units_.makeComboBattleUnitRefs());
    };
    context.gravity = gravity_;
    context.manualCameraEnabled = isManualCameraEnabled();
    context.hurtFlashDuration = HURT_FLASH_DURATION;
    context.blinkSoundEffectId = BLINK_SOUND_EFFECT_ID;
    context.deathZoomFrames = CAMERA_DEATH_ZOOM_FRAMES;
    context.battleEndZoomFrames = CAMERA_BATTLE_END_ZOOM_FRAMES;
    context.deathSlowFrames = CAMERA_DEATH_SLOW_FRAMES;
    context.battleEndSlowFrames = CAMERA_BATTLE_END_SLOW_FRAMES;
    applySceneFrameStateResult(frame_state_applier_.apply(frameResult, result_, context));

    report_player_.playProjectileCancelDamageCommands(frameResult.projectileCancelDamageCommands, {
        &battle_report_,
        &scene_units_,
    });
    for (const auto& event : frameResult.frame.visualEvents)
    {
        presentation_recorder_.recordVisual(event);
    }
    for (const auto& event : frameResult.frame.logEvents)
    {
        presentation_recorder_.recordLog(event);
    }

    impact_player_.play(frameResult, {
        &scene_units_,
        &shake_,
        [](int effectSoundId)
        {
            Audio::getInstance()->playESound(effectSoundId);
        },
        [](int lowFrequency, int highFrequency, int durationMs)
        {
            Engine::getInstance()->gameControllerRumble(lowFrequency, highFrequency, durationMs);
        },
    });
}

void BattleSceneHades::applySceneFrameStateResult(const BattleSceneFrameStateApplyResult& result)
{
    for (const auto& command : result.bloodEffects)
    {
        BattleAttackEffect effect;
        effect.FollowUnitId = command.followUnitId;
        effect.setPath(command.path);
        effect.TotalFrame = effect.TotalEffectFrame;
        effect.Frame = 0;
        effect.VisualOnly = 1;
        attack_effects_.push_back(std::move(effect));
    }
    for (int soundId : result.effectSoundIds)
    {
        Audio::getInstance()->playESound(soundId);
    }
    for (int soundId : result.attackSoundIds)
    {
        Audio::getInstance()->playASound(soundId);
    }
    for (const auto& rumble : result.rumbles)
    {
        Engine::getInstance()->gameControllerRumble(
            rumble.lowFrequency,
            rumble.highFrequency,
            rumble.durationMs);
    }
    if (result.unitDied)
    {
        x_ = result.jitterX;
        y_ = result.jitterY;
    }
    if (result.cameraFocus)
    {
        focusCameraOn(*result.cameraFocus, result.closeUpFrames);
    }
    else if (result.closeUpFrames > 0)
    {
        close_up_ = std::max(close_up_, result.closeUpFrames);
        close_up_total_ = std::max(close_up_total_, close_up_);
    }
    if (result.frozenFrames > 0)
    {
        frozen_ = result.frozenFrames;
    }
    if (result.slowFrames > 0)
    {
        slow_ = result.slowFrames;
    }
    if (result.sceneShake > 0)
    {
        shake_ = result.sceneShake;
    }
    if (result.battleEnded)
    {
        result_ = result.battleResult;
    }
}

void BattleSceneHades::applyLegacyBattleFrameResult(const SceneBattleFrameResult& result)
{
    if (!result.advanced)
    {
        return;
    }
    for (auto& unit : scene_units_.units())
    {
        if (!unit.active)
        {
            continue;
        }
        unit.vitals.hp = GameUtil::limit(unit.vitals.hp, 0, unit.vitals.maxHp);
        unit.vitals.mp = GameUtil::limit(unit.vitals.mp, 0, unit.vitals.maxMp);
        BattleSceneRenderMath::decreaseToZero(unit.shake);
        BattleSceneRenderMath::decreaseToZero(unit.attention);
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

    if (slow_ == 0 && (result_ == 0 || result_ == 1))
    {
        calExpGot();
        setExit(true);
    }
}

void BattleSceneHades::playCorePresentationFrame()
{
    publishPresentationFrame();
}

void BattleSceneHades::onPressedCancel()
{
}

void BattleSceneHades::renderExtraRoleInfo(const BattleSceneUnit& unit, double x, double y)
{
    if (!unit.alive)
    {
        return;
    }

    const auto& comboState = unit.combo;

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
    if (unit.identity.team == 1)
    {
        // 敌方红色
        background_color = { 255, 0, 0, 128 };
    }

    renderBar(y + ROLE_STATUS_BAR_Y, unit.vitals.hp, unit.vitals.maxHp, background_color, shadow_color);

    if (unit.vitals.maxHp > 0 && unit.vitals.hp > 0)
    {
        int bar_y = y + ROLE_STATUS_BAR_Y;
        int hpFillWidth = std::clamp(
            static_cast<int>(std::round(ROLE_STATUS_BAR_WIDTH * (static_cast<double>(unit.vitals.hp) / unit.vitals.maxHp))),
            0,
            ROLE_STATUS_BAR_WIDTH);

        if (comboState.shield > 0)
        {
            int shieldWidth = std::clamp(
                static_cast<int>(std::round(ROLE_STATUS_BAR_WIDTH * (static_cast<double>(comboState.shield) / unit.vitals.maxHp))),
                1,
                ROLE_STATUS_BAR_WIDTH);
            int visibleShieldWidth = std::min(shieldWidth, hpFillWidth);
            Rect shieldRect = { barLeft, bar_y, visibleShieldWidth, ROLE_STATUS_BAR_HEIGHT };
            // int shieldAlpha = std::clamp(90 + comboState->shield * 120 / std::max(1, r->MaxHP), 90, 180);
            Engine::getInstance()->renderSquareTexture(&shieldRect, { 250, 200, 0, 255 }, 255);
        }

        bool hasDamageProtection = unit.invincible > 0 || comboState.blockFirstHitsRemaining > 0;
        if (hasDamageProtection)
        {
            Color protectionColor = comboState.blockFirstHitsRemaining > 0 ? Color{ 255, 220, 110, 255 } : Color{ 255, 170, 95, 255 };
            renderOutline(barLeft, bar_y, ROLE_STATUS_BAR_WIDTH, ROLE_STATUS_BAR_HEIGHT, protectionColor, 220);
        }
    }

    Color mp_color = { 0, 0, 255, 128 };
    Color mp_shadow_color = { 64, 64, 64, 128 };
    renderBar(y + ROLE_STATUS_BAR_MP_Y, unit.vitals.mp, unit.vitals.maxMp, mp_color, mp_shadow_color);

    // Frozen / cooldown bar – frozen takes priority
    if (unit.frozen > 0 && unit.frozenMax > 0)
    {
        Color frozen_color = { 200, 220, 255, 192 };
        int bar_y = y + ROLE_STATUS_BAR_FROZEN_Y;
        double perc = static_cast<double>(unit.frozen) / unit.frozenMax;

        // Draw outline (same color as bar)
        renderOutline(barLeft, bar_y, ROLE_STATUS_BAR_WIDTH, ROLE_STATUS_BAR_HEIGHT, frozen_color, 128);

        // Draw current frozen bar
        Rect r_frozen = { barLeft, bar_y, int(perc * ROLE_STATUS_BAR_WIDTH), ROLE_STATUS_BAR_HEIGHT };
        Engine::getInstance()->renderSquareTexture(&r_frozen, frozen_color, 192);
    }
    else if (unit.animation.cooldown > 0 && unit.animation.cooldownMax > 0)
    {
        Color cd_color = { 255, 210, 140, 160 };
        int bar_y = y + ROLE_STATUS_BAR_FROZEN_Y;
        double perc = static_cast<double>(unit.animation.cooldown) / unit.animation.cooldownMax;

        renderOutline(barLeft, bar_y, ROLE_STATUS_BAR_WIDTH, ROLE_STATUS_BAR_HEIGHT, cd_color, 100);

        Rect r_cd = { barLeft, bar_y, int(perc * ROLE_STATUS_BAR_WIDTH), ROLE_STATUS_BAR_HEIGHT };
        Engine::getInstance()->renderSquareTexture(&r_cd, cd_color, 160);
    }
}

int BattleSceneHades::checkResult()
{
    if (result_ != -1)
    {
        return result_;
    }

    const int team0 = scene_units_.aliveUnitsOnTeam(0);
    const int team1 = scene_units_.aliveUnitsOnTeam(1);
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
