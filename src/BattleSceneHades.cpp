#include "BattleSceneHades.h"
#include "Audio.h"
#include "BattleScenePresentationConstants.h"
#include "BattleSceneSetupBuilder.h"
#include "BattleStarStats.h"
#include "battle/BattleAttackSystem.h"
#include "battle/BattleCombatIntent.h"
#include "battle/BattleInitialization.h"
#include "battle/BattleOperation.h"
#include "battle/BattleStatusSystem.h"
#include "ChessBalance.h"
#include "ChessCombo.h"
#include "ChessEquipment.h"
#include "ChessNeigong.h"
#include "ChessUiCommon.h"
#include "Engine.h"
#include "Event.h"
#include "Find.h"
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
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <numeric>
#include <set>
#include <utility>

namespace
{
constexpr float CAMERA_DEATH_ZOOM_SCALE = 0.5f;        // 0.5 == original half-screen zoom; 1.0 disables zoom
constexpr int CAMERA_ZOOM_EASE_FRAMES = 0;             // 0 restores the old hard snap zoom; higher = softer zoom edges
constexpr int CAMERA_SLOW_STEP_INTERVAL = 4;           // lower = less slow-motion, higher = slower motion
constexpr double CORPSE_SHADOW_Y_OFFSET_RATIO = 0.1;
constexpr double CORPSE_BODY_SCALE_X = 0.5;
constexpr double CORPSE_BODY_SCALE_Y = 1.0;
constexpr int BATTLE_COORD_COUNT = BATTLEMAP_COORD_COUNT;
constexpr int ROLE_STATUS_BAR_WIDTH = 48;
constexpr int ROLE_STATUS_BAR_HEIGHT = 6;
constexpr int ROLE_STATUS_BAR_Y = -120;
constexpr int ROLE_STATUS_BAR_STEP_Y = ROLE_STATUS_BAR_HEIGHT + ROLE_STATUS_BAR_HEIGHT / 3;
constexpr int ROLE_STATUS_BAR_MP_Y = ROLE_STATUS_BAR_Y + ROLE_STATUS_BAR_STEP_Y;
constexpr int ROLE_STATUS_BAR_FROZEN_Y = ROLE_STATUS_BAR_Y + ROLE_STATUS_BAR_STEP_Y * 2;

struct RuntimeFrozenStatus
{
    int frames{};
    int maxFrames{};
};

RuntimeFrozenStatus runtimeFrozenStatusForUnit(
    const KysChess::Battle::BattleRuntimeSession* session,
    int unitId)
{
    if (session == nullptr)
    {
        return {};
    }
    const auto& record = session->runtime().units.require(unitId);
    return {
        record.status.effects.frozenTimer,
        record.status.effects.frozenMaxTimer,
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

double renderGroundY(const Pointf& position)
{
    return position.y / 2.0 - position.z;
}

double corpseShadowY(const Pointf& position, const TextureWarpper& tex)
{
    return renderGroundY(position) + tex.dy * CORPSE_SHADOW_Y_OFFSET_RATIO;
}

Point corpseShadowRenderPosition(
    const TextureWarpper& tex,
    const Point& bodyRenderPosition,
    double bodyScaleX,
    double bodyScaleY,
    double shadowScaleX,
    double shadowScaleY)
{
    const double bodyCenterX = bodyRenderPosition.x - tex.dx + tex.w * bodyScaleX / 2.0;
    const double bodyCenterY = bodyRenderPosition.y - tex.dy + tex.h * bodyScaleY / 2.0;
    return {
        static_cast<int>(bodyCenterX - tex.w * shadowScaleX / 2.0 + tex.dx),
        static_cast<int>(bodyCenterY - tex.h * shadowScaleY / 2.0 + tex.dy),
    };
}

Point rotatedAnchorRenderPosition(const TextureWarpper& tex, int anchorX, int anchorY, double zoomX, double zoomY, double angle)
{
    if (angle == 0.0)
    {
        return { anchorX, anchorY };
    }

    const double width = tex.w * zoomX;
    const double height = tex.h * zoomY;
    const double centerX = width / 2.0;
    const double centerY = height / 2.0;
    const double localAnchorX = tex.dx - centerX;
    const double localAnchorY = tex.dy - centerY;
    const double radians = angle * M_PI / 180.0;
    const double rotatedAnchorX = localAnchorX * std::cos(radians) - localAnchorY * std::sin(radians);
    const double rotatedAnchorY = localAnchorX * std::sin(radians) + localAnchorY * std::cos(radians);

    return {
        static_cast<int>(anchorX + tex.dx - centerX - rotatedAnchorX),
        static_cast<int>(anchorY + tex.dy - centerY - rotatedAnchorY),
    };
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

struct BattleSceneRuntimeFrameEffects
{
    void playEffectSound(int effectSoundId)
    {
        Audio::getInstance()->playESound(effectSoundId);
    }

    void playAttackSound(int attackSoundId)
    {
        Audio::getInstance()->playASound(attackSoundId);
    }

    void rumble(int lowFrequency, int highFrequency, int durationMs)
    {
        Engine::getInstance()->gameControllerRumble(lowFrequency, highFrequency, durationMs);
    }

    int textDrawSize(const std::string& text)
    {
        return Font::getTextDrawSize(text);
    }

    int effectFrameCount(const std::string& path)
    {
        return TextureManager::getInstance()->getTextureGroupCount(path);
    }
};

}    // namespace

int BattleSceneHades::getOperationType(int attackAreaType)
{
    return KysChess::Battle::toPresentationOperationKind(
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

BattleSceneHades::BattleSceneHades(KysChess::ChessRoleSave& roleSave, KysChess::ChessProgress& progress, KysChess::ChessManager& chessManager) :
    roleSave_(roleSave),
    progress_(progress),
    chessManager_(chessManager),
    frame_applier_({
        battle_report_,
        scene_units_,
        attack_effects_,
        text_effects_,
        hurt_flash_timers_,
        rand_,
        pos_,
        result_,
        frozen_,
        slow_,
        shake_,
        x_,
        y_,
        camera_,
        frame_applier_camera_bounds_,
        manual_camera_enabled_,
    })
{
    full_window_ = 1;
    COORD_COUNT = BATTLEMAP_COORD_COUNT;
    battle_map_.initialize(COORD_COUNT);
    updateFrameApplierContext();
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

BattleSceneCameraBounds BattleSceneHades::makeCameraBounds() const
{
    return {
        static_cast<float>(COORD_COUNT * TILE_W * 2),
        static_cast<float>(COORD_COUNT * TILE_H * 2),
        static_cast<float>(render_center_x_),
        static_cast<float>(render_center_y_),
    };
}

Color BattleSceneHades::calculateHurtFlashColor(int unitId, const Color& baseColor) const
{
    auto it = hurt_flash_timers_.find(unitId);
    const int timer = it != hurt_flash_timers_.end() ? it->second : 0;
    return BattleSceneRenderMath::hurtFlashColor(timer, baseColor);
}

void BattleSceneHades::updateFrameApplierContext()
{
    frame_applier_camera_bounds_ = makeCameraBounds();
    manual_camera_enabled_ = isManualCameraEnabled();
}

void BattleSceneHades::initializeBattleRuntime(
    KysChess::BattleSceneSetupBuilder::BattleSceneSetupBuildResult setupBuild)
{
    KysChess::ChessCombo::clearActiveStates();
    auto creationInput = makeBattleRuntimeSessionCreationInput(std::move(setupBuild));
    auto creation = KysChess::Battle::BattleRuntimeSession::createInitialized(std::move(creationInput));
    auto initialization = std::move(creation.initialization);
    battle_session_.emplace(std::move(creation.session));
    KysChess::ChessCombo::getMutableStates() = initialization.comboStates;
    scene_units_.initialize(*battle_session_);
    if (!initialization.logEvents.empty() || !initialization.visualEvents.empty())
    {
        KysChess::Battle::BattlePresentationFrame frame;
        frame.visualEvents = std::move(initialization.visualEvents);
        frame.logEvents = std::move(initialization.logEvents);
        BattleSceneRuntimeFrameEffects effects;
        updateFrameApplierContext();
        frame_applier_.apply(frame, effects);
    }
}

void BattleSceneHades::setBattleRuntimeRandomSeed(unsigned int seed)
{
    battle_random_seed_ = seed;
}

KysChess::Battle::BattleRuntimeSessionCreationInput BattleSceneHades::makeBattleRuntimeSessionCreationInput(
    KysChess::BattleSceneSetupBuilder::BattleSceneSetupBuildResult setupBuild)
{
    auto input = std::move(setupBuild.sessionInput);
    input.rules = KysChess::Battle::makeHadesBattleRuntimeRules(TILE_W, BATTLE_COORD_COUNT);
    input.randomSeed = battle_random_seed_;
    input.rules.movementPhysicsConfig.gravity = gravity_;
    input.rules.movementPhysicsConfig.friction = friction_;
    input.comboStates = KysChess::ChessCombo::getMutableStates();
    input.battleFrame = battle_frame_;

    auto* basicMagic = Save::getInstance()->getMagic(1);
    assert(basicMagic);
    input.rescueCounterAttackSkillId = basicMagic->ID;

    input.terrainCells.reserve(BATTLE_COORD_COUNT * BATTLE_COORD_COUNT);
    input.rescueCells.reserve(BATTLE_COORD_COUNT * BATTLE_COORD_COUNT);
    input.rules.movementCollisionWorld.walkableByCell.assign(BATTLE_COORD_COUNT * BATTLE_COORD_COUNT, 0);
    for (int x = 0; x < BATTLE_COORD_COUNT; ++x)
    {
        for (int y = 0; y < BATTLE_COORD_COUNT; ++y)
        {
            const auto position = battle_map_.pos45To90(x, y);
            const bool walkable = battle_map_.canWalk45(x, y);
            input.terrainCells.push_back({ position, walkable });
            input.rescueCells.push_back({
                x,
                y,
                walkable,
                false,
                -1,
                position,
            });
            const auto movementCellIndex = static_cast<std::size_t>(y * BATTLE_COORD_COUNT + x);
            input.rules.movementCollisionWorld.walkableByCell[movementCellIndex] = walkable ? 1 : 0;
        }
    }

    auto& obtained = progress_.getObtainedNeigong();
    input.setup.obtainedNeigongMagicIds.assign(obtained.begin(), obtained.end());
    for (const auto& [x, y] : clone_spawn_positions_)
    {
        bool occupied = false;
        for (const auto& unit : input.units)
        {
            if (unit.alive && unit.gridX == x && unit.gridY == y)
            {
                occupied = true;
                break;
            }
        }
        input.setup.cloneCells.push_back({ x, y, true, occupied });
    }
    return input;
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
    float zoomBlend = use_whole_scene ? cameraZoomBlend(camera_.closeUpFrames(), camera_.closeUpTotalFrames()) : 0.0f;
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
        bool corpse = false;
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
                    // TODO: Browser fallback path only
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
    for (const auto& unit : scene_units_.runtimeUnits())
    {
        //if (r->Dead) { continue; }
        DrawInfo info;
        const auto& presentation = scene_units_.requirePresentation(unit.id);
        auto path = std::format("fight/fight{:03}", unit.headId);
        info.color = { 255, 255, 255, 255 };
        info.alpha = 255;
        info.white = 0;
        info.p = unit.motion.position;
        if (result_ == -1 && presentation.shake)
        {
            info.p.x += shakeJitter(battle_frame_, unit.id);
        }
        const int renderActType = unit.alive ? unit.animation.actType : -1;
        const int renderActFrame = unit.alive ? unit.animation.actFrame : 0;
        info.tex = TextureManager::getInstance()->getTexture(
            path,
            BattleSceneRenderMath::calRenderUnitPic(
                unit.fightFrames,
                unit.motion.facing,
            renderActType,
            renderActFrame));
        if (!info.tex)
        {
            continue;
        }
        if (!unit.alive)
        {
            info.corpse = true;
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
        if (presentation.attention)
        {
            info.alpha = 255 - presentation.attention * 4;
        }

        // 應用受击闪红
        info.color = calculateHurtFlashColor(unit.id, info.color);

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
                effect_pos = scene_units_.requireRuntimeUnit(ae.FollowUnitId).motion.position + ae.Pos;
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
                double shadowY = d.p.y / 2.0 + yd;
                if (d.corpse)
                {
                    shadowY = corpseShadowY(d.p, *d.tex);
                }
                Point shadowRenderPosition = rotatedAnchorRenderPosition(
                    *d.tex,
                    renderWorldX(d.p.x),
                    renderWorldY(shadowY),
                    scalex,
                    scaley,
                    static_cast<double>(d.rot));
                if (d.corpse)
                {
                    const Point bodyRenderPosition = rotatedAnchorRenderPosition(
                        *d.tex,
                        renderWorldX(d.p.x),
                        renderWorldY(renderGroundY(d.p)),
                        CORPSE_BODY_SCALE_X,
                        CORPSE_BODY_SCALE_Y,
                        static_cast<double>(d.rot));
                    shadowRenderPosition = corpseShadowRenderPosition(
                        *d.tex,
                        bodyRenderPosition,
                        CORPSE_BODY_SCALE_X,
                        CORPSE_BODY_SCALE_Y,
                        scalex,
                        scaley);
                    shadowRenderPosition.y += static_cast<int>(d.tex->dy * CORPSE_SHADOW_Y_OFFSET_RATIO);
                }
                TextureManager::getInstance()->renderTexture(d.tex, shadowRenderPosition.x, shadowRenderPosition.y,
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
            scaley = CORPSE_BODY_SCALE_X;
        }
        std::vector<Color> color_v;
        //color_v[0] = { 128, 128, 64, 255 };
        std::vector<float> brightness_v(4, 0);
        brightness_v[0] = 0.5;
        brightness_v[2] = 0;
        const double scalex = scaley;
        const double zoomY = d.rot ? CORPSE_BODY_SCALE_Y : 1.0;
        const Point renderPosition = rotatedAnchorRenderPosition(
            *d.tex,
            renderWorldX(d.p.x),
            renderWorldY(renderGroundY(d.p)),
            scalex,
            zoomY,
            static_cast<double>(d.rot));
        TextureManager::getInstance()->renderTexture(d.tex,
            renderPosition.x,
            renderPosition.y,
            TextureManager::RenderInfo{ d.color, d.alpha, scalex, zoomY, static_cast<double>(d.rot), d.white, color_v, {} });
    }

    for (const auto& unit : scene_units_.runtimeUnits())
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
        const auto& selectedPosition = scene_units_.requireRuntimeUnit(swapSelectedUnitId_).motion.position;
        if (is_visible_world(selectedPosition.x, selectedPosition.y / 2.0))
        {
            engine->fillColor(
                { 255, 255, 0, 80 },
                renderWorldX(selectedPosition.x - 15),
                renderWorldY(selectedPosition.y / 2.0 - 5),
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
    for (const auto& unit : scene_units_.runtimeUnits())
    {
        if (!unit.alive) { continue; }
        double wx = unit.motion.position.x;
        double wy = unit.motion.position.y / 2.0;
        int ux = worldToUiX(wx);
        int uy = worldToUiY(wy);
        if (ux < -80 || ux >= ui_w + 80 || uy < -80 || uy >= ui_h + 80) { continue; }
        const auto frozen = runtimeFrozenStatusForUnit(battle_session_ ? &*battle_session_ : nullptr, unit.id);
        if (frozen.frames > 0 && frozen.maxFrames > 0)
        {
            Font::getInstance()->draw(std::to_string(frozen.frames),
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
        if (positionSwapActive_ && unit.team == 0)
        {
            std::string coord = std::format("({},{})", unit.grid.x, unit.grid.y);
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
    if (auto manualCenter = camera_.handleManualInput(e, pos_, makeCameraBounds()))
    {
        pos_ = *manualCenter;
    }

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
    if (frozen_ > 0)
    {
        frozen_--;
        engine->gameControllerRumble(100, 100, 50);
        battle_frame_++;
        return;
    }
    camera_.decreaseCloseUp();
    if (!camera_.closeUpActive())
    {
        if (isManualCameraEnabled())
        {
            pos_ = BattleSceneCamera::clampCenter(pos_, makeCameraBounds());
        }
        else
        {
            pos_ = camera_.updateAuto(pos_, scene_units_, makeCameraBounds());
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
    bool browserNeedsFallback = GameUtil::isLegacyBrowser();
    use_whole_scene_ = !browserNeedsFallback
        && (max_texture_size <= 0 || (whole_scene_w <= max_texture_size && whole_scene_h <= max_texture_size));
    if (use_whole_scene_)
    {
        engine->createRenderedTexture("whole_scene", whole_scene_w, whole_scene_h);
    }
    else
    {
        LOG("whole_scene disabled: browser_fallback={}, required={}x{}, renderer max={}\n",
            browserNeedsFallback, whole_scene_w, whole_scene_h, max_texture_size);
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
    camera_.reset(pos_);
    pos_ = BattleSceneCamera::clampCenter(pos_, makeCameraBounds());

    initializeBattleRuntime(std::move(setupBuild));
    runPreBattlePositionSwapIfEnabled();

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
                const auto& unit = battle_->scene_units_.requireRuntimeUnit(unitId);
                if (unit.grid.x == p.x && unit.grid.y == p.y)
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
                assert(battle_->battle_session_.has_value());
                battle_->battle_session_->swapSetupUnitPositions(
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
    positionSwapActive_ = true;
    auto node = std::make_shared<PositionSwapNode>(this);
    node->run();
    positionSwapActive_ = false;
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
            const auto& unit = scene_units_.requireRuntimeUnit(unitId);
            std::string name = unit.name;
            std::string coord = std::format("({},{})", unit.grid.x, unit.grid.y);
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
            const auto& unit = scene_units_.requireRuntimeUnit(allies[i]);
            std::string name = unit.name;
            std::string coord = std::format("({},{})", unit.grid.x, unit.grid.y);
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
        assert(battle_session_.has_value());
        battle_session_->swapSetupUnitPositions(
            allies[sel1],
            allies[sel2]);
    }
    swapSelectedUnitId_ = -1;
    positionSwapActive_ = false;
}

void BattleSceneHades::backRun1()
{
    if (!shouldAdvanceBattleSimulation())
    {
        return;
    }

    advanceBattlePresentationEffects(attack_effects_, true);
    auto frame = battle_session_->runFrame();
    BattleSceneRuntimeFrameEffects effects;
    updateFrameApplierContext();
    frame_applier_.apply(frame, effects);
    advanceScenePresentationFrame();
    finishBattleIfReady();
}

bool BattleSceneHades::shouldAdvanceBattleSimulation()
{
    ageHurtFlashTimers();

    if (slow_ > 0)
    {
        if (battle_frame_ % CAMERA_SLOW_STEP_INTERVAL)
        {
            return false;
        }
        //x_ = rand_.rand_int(2) - rand_.rand_int(2);
        //y_ = rand_.rand_int(2) - rand_.rand_int(2);
        slow_--;
    }
    return true;
}

void BattleSceneHades::ageHurtFlashTimers()
{
    // 更新受擊閃紅計時器
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
}

void BattleSceneHades::advanceScenePresentationFrame()
{
    scene_units_.decreaseTransientPresentationState();

    // 處理文字
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
}

void BattleSceneHades::finishBattleIfReady()
{
    if (slow_ == 0 && (result_ == 0 || result_ == 1))
    {
        calExpGot();
        setExit(true);
    }
}

void BattleSceneHades::onPressedCancel()
{
}

void BattleSceneHades::renderExtraRoleInfo(
    const KysChess::Battle::BattleRuntimeUnit& unit,
    double x,
    double y)
{
    if (!unit.alive)
    {
        return;
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
    if (unit.team == 1)
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

        if (unit.shield > 0)
        {
            int shieldWidth = std::clamp(
            static_cast<int>(std::round(ROLE_STATUS_BAR_WIDTH * (static_cast<double>(unit.shield) / unit.vitals.maxHp))),
                1,
                ROLE_STATUS_BAR_WIDTH);
            int visibleShieldWidth = std::min(shieldWidth, hpFillWidth);
            Rect shieldRect = { barLeft, bar_y, visibleShieldWidth, ROLE_STATUS_BAR_HEIGHT };
            Engine::getInstance()->renderSquareTexture(&shieldRect, { 250, 200, 0, 255 }, 255);
        }

        const int firstHitBlocks = [&]()
        {
            if (!battle_session_)
            {
                return 0;
            }
            return battle_session_->runtime().units.require(unit.id).damage.blockFirstHitsRemaining;
        }();
        bool hasDamageProtection = unit.invincible > 0 || firstHitBlocks > 0;
        if (hasDamageProtection)
        {
            Color protectionColor = firstHitBlocks > 0 ? Color{ 255, 220, 110, 255 } : Color{ 255, 170, 95, 255 };
            renderOutline(barLeft, bar_y, ROLE_STATUS_BAR_WIDTH, ROLE_STATUS_BAR_HEIGHT, protectionColor, 220);
        }
    }

    Color mp_color = { 0, 0, 255, 128 };
    Color mp_shadow_color = { 64, 64, 64, 128 };
    renderBar(y + ROLE_STATUS_BAR_MP_Y, unit.vitals.mp, unit.vitals.maxMp, mp_color, mp_shadow_color);

    // Frozen / cooldown bar – frozen takes priority
    const auto frozen = runtimeFrozenStatusForUnit(battle_session_ ? &*battle_session_ : nullptr, unit.id);
    if (frozen.frames > 0 && frozen.maxFrames > 0)
    {
        Color frozen_color = { 200, 220, 255, 192 };
        int bar_y = y + ROLE_STATUS_BAR_FROZEN_Y;
        double perc = static_cast<double>(frozen.frames) / frozen.maxFrames;

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
    return result_;
}
