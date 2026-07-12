#include "BattleSceneHades.h"
#include "Audio.h"
#include "BattleLogPresenter.h"
#include "BattleSetupFactory.h"
#include "BattleScenePauseControl.h"
#include "BattleScenePresentationConstants.h"
#include "PaperCameraControlMath.h"
#include "battle/BattleAttackSystem.h"
#include "battle/BattleCombatIntent.h"
#include "battle/BattleInitialization.h"
#include "battle/BattleOperation.h"
#include "battle/BattleStatusSystem.h"
#include "ChessGameSession.h"
#include "ChessGuiBattleFlow.h"
#include "ChessUiCommon.h"
#include "Engine.h"
#include "Event.h"
#include "Find.h"
#include "Font.h"
#include "GameUtil.h"
#include "MainScene.h"
#include "Menu.h"
#include "SystemSettings.h"
#include "TextureManager.h"
#include "Weather.h"
#include "filefunc.h"
#include "strfunc.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <numeric>
#include <set>
#include <tuple>
#include <unordered_map>
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
constexpr int BATTLE_CONTROL_PLAY_TEXTURE_ID = 333;
constexpr int BATTLE_CONTROL_PAUSE_TEXTURE_ID = 334;
constexpr int BATTLE_CONTROL_LOG_TEXTURE_ID = 335;
constexpr int BATTLE_CONTROL_SPEED_TEXTURE_ID = 336;
constexpr double BATTLE_FRAME_PROFILE_SLOW_MS = 4.0;
constexpr float PAPER_CAMERA_FOV = 60.0f;
constexpr float PAPER_CAMERA_CLASSIC_VIEW_ANGLE = -0.17453292520f;
constexpr float PAPER_FREE_CAMERA_ROTATE_SPEED = 0.035f;
constexpr float PAPER_FREE_CAMERA_HEIGHT_SPEED = 6.0f;
constexpr float PAPER_FREE_CAMERA_PAN_SPEED = 10.0f;
constexpr float PAPER_CAMERA_MIN_DISTANCE = 220.0f;
constexpr float PAPER_CAMERA_MAX_DISTANCE = 1200.0f;
constexpr float PAPER_CAMERA_ZOOM_STEP = 24.0f;
constexpr float PAPER_CAMERA_MIN_HEIGHT = 80.0f;
constexpr float PAPER_CAMERA_MAX_HEIGHT = 520.0f;
constexpr int PAPER_DEATH_SHAKE_FRAMES = 60;
constexpr float PAPER_AUTO_CENTER_DEAD_ZONE = 18.0f;
constexpr float PAPER_AUTO_CENTER_MAX_STEP = 16.0f;
constexpr float PAPER_GROUND_MESH_CENTER_RATIO = 0.64f;
constexpr int PAPER_GROUND_MESH_TARGET_DIVISIONS = 160;
constexpr int PAPER_GROUND_MESH_EXTENSION_DIVISIONS_PER_SIDE = 10;
constexpr int PAPER_GROUND_MESH_ORIGINAL_EDGE_DIVISIONS_PER_SIDE = 18;
constexpr float PAPER_GROUND_EDGE_FADE_TILE_COUNT = 16.0f;
constexpr float PAPER_GROUND_HORIZON_FADE_START_RATIO = 0.72f;
constexpr float PAPER_GROUND_HORIZON_FADE_END_RATIO = 1.35f;
constexpr float PAPER_GROUND_RADIAL_FADE_START_RATIO = 0.34f;
constexpr float PAPER_GROUND_RADIAL_FADE_END_RATIO = 0.48f;
constexpr uint8_t PAPER_GROUND_EDGE_MIN_ALPHA = 0;
constexpr uint8_t PAPER_GROUND_HORIZON_MIN_ALPHA = 0;
bool isPaperWallTile(int num)
{
    return (num >= 701 && num <= 1139)
        || (num >= 1410 && num <= 1436)
        || (num >= 1505 && num <= 1621)
        || (num >= 1816 && num <= 1849)
        || (num >= 2116 && num <= 2144)
        || (num >= 2184 && num <= 2285);
}

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

Pointf normalizeOr(Pointf value, Pointf fallback)
{
    if (value.norm() == 0)
    {
        value = fallback;
    }
    if (value.norm() != 0)
    {
        value.normTo(1);
    }
    return value;
}

int realTowardsToPaperFaceTowards(const Pointf& dir, const Pointf& viewDir, const Pointf& paperRight, int currentFaceTowards)
{
    Pointf n = dir;
    n.z = 0;
    if (n.norm() == 0)
    {
        return currentFaceTowards;
    }
    n.normTo(1);
    const float right = n.x * paperRight.x + n.y * paperRight.y;
    const float forward = n.x * viewDir.x + n.y * viewDir.y;
    constexpr float rightEpsilon = 0.2f;
    if (std::abs(right) < rightEpsilon)
    {
        const bool keepRight = currentFaceTowards == Towards_RightUp || currentFaceTowards == Towards_RightDown;
        return keepRight ? (forward > 0 ? Towards_RightUp : Towards_RightDown)
                         : (forward > 0 ? Towards_LeftUp : Towards_LeftDown);
    }
    if (right > 0 && forward > 0) { return Towards_RightUp; }
    if (right > 0 && forward <= 0) { return Towards_RightDown; }
    if (right < 0 && forward >= 0) { return Towards_LeftUp; }
    if (right < 0 && forward < 0) { return Towards_LeftDown; }
    return currentFaceTowards;
}

bool pointsInCameraFront(Camera& camera, const std::vector<Pointf>& points)
{
    for (const auto& point : points)
    {
        if (camera.getDepth(point) <= camera.getNearPlane())
        {
            return false;
        }
    }
    return true;
}

void renderProjectedQuad(Camera& camera, Texture* texture, const std::vector<Pointf>& world, const std::vector<FPoint>& source)
{
    if (!texture || world.size() != 4 || !pointsInCameraFront(camera, world))
    {
        return;
    }
    auto projected = camera.getProj(world);
    std::vector<FPoint> destination;
    destination.reserve(projected.size());
    for (const auto& point : projected)
    {
        destination.push_back({ static_cast<float>(point.x), static_cast<float>(point.y) });
    }
    Engine::getInstance()->renderTexture(texture, nullptr, destination, source);
}

void renderProjectedPlaneMesh(Camera& camera, Texture* texture, const std::vector<Pointf>& world,
    const std::vector<FPoint>& source, int columns, int rows)
{
    if (!texture || world.size() != 4 || source.size() != 4 || !pointsInCameraFront(camera, world))
    {
        return;
    }

    columns = std::clamp(columns, 1, 32);
    rows = std::clamp(rows, 1, 32);
    std::vector<FPoint> destination;
    std::vector<FPoint> meshSource;
    std::vector<int> indices;
    destination.reserve((columns + 1) * (rows + 1));
    meshSource.reserve((columns + 1) * (rows + 1));
    indices.reserve(columns * rows * 6);

    for (int row = 0; row <= rows; ++row)
    {
        const float v = static_cast<float>(row) / rows;
        for (int column = 0; column <= columns; ++column)
        {
            const float u = static_cast<float>(column) / columns;
            const Pointf top = world[0] * (1.0f - u) + world[1] * u;
            const Pointf bottom = world[3] * (1.0f - u) + world[2] * u;
            const auto projected = camera.getProj({ top * (1.0f - v) + bottom * v }).front();
            destination.push_back({ projected.x, projected.y });

            const FPoint sourceTop = {
                source[0].x + (source[1].x - source[0].x) * u,
                source[0].y + (source[1].y - source[0].y) * u,
            };
            const FPoint sourceBottom = {
                source[3].x + (source[2].x - source[3].x) * u,
                source[3].y + (source[2].y - source[3].y) * u,
            };
            meshSource.push_back({
                sourceTop.x + (sourceBottom.x - sourceTop.x) * v,
                sourceTop.y + (sourceBottom.y - sourceTop.y) * v,
            });
        }
    }

    for (int row = 0; row < rows; ++row)
    {
        for (int column = 0; column < columns; ++column)
        {
            const int topLeft = row * (columns + 1) + column;
            const int topRight = topLeft + 1;
            const int bottomLeft = topLeft + columns + 1;
            const int bottomRight = bottomLeft + 1;
            indices.insert(indices.end(), { topLeft, topRight, bottomRight, bottomRight, bottomLeft, topLeft });
        }
    }
    Engine::getInstance()->renderTextureMesh(texture, destination, meshSource, {}, indices);
}

Texture* ensurePaperCursorFloorTexture(Engine& engine)
{
    bool needDrawTexture = true;
    if (auto texture = engine.getTexture("paper-cursor-floor"))
    {
        int width = 0;
        int height = 0;
        Engine::getTextureSize(texture, width, height);
        needDrawTexture = width != 1 || height != 1;
    }

    engine.createRenderedTexture("paper-cursor-floor", 1, 1);
    auto texture = engine.getTexture("paper-cursor-floor");
    if (texture && needDrawTexture)
    {
        auto previousTarget = engine.getRenderTarget();
        engine.setRenderTarget(texture);
        engine.fillColor({ 255, 255, 255, 255 }, 0, 0, 1, 1, BLENDMODE_NONE);
        engine.setRenderTarget(previousTarget);
    }
    return texture;
}

float smoothStep(float edge0, float edge1, float value)
{
    if (edge1 <= edge0)
    {
        return value >= edge1 ? 1.0f : 0.0f;
    }
    const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
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

BattleSceneHades::BattleSceneHades(KysChess::ChessGameSession& session) :
    session_transition_source_(&session),
    frame_applier_({
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
    assert(session.state().preparedBattle);
    assert(session.state().preparedBattle->chosenMapId >= 0);
    setID(session.state().preparedBattle->chosenMapId);
    updateFrameApplierContext();
}

const KysChess::Battle::BattleRuntimeSession* BattleSceneHades::activeRuntimeSession() const
{
    if (formation_preview_runtime_)
    {
        return &*formation_preview_runtime_;
    }
    return session_transition_source_->presentationBattleRuntime();
}
BattleSceneHades::~BattleSceneHades()
{
}

void BattleSceneHades::setID(int id)
{
    battle_id_ = id;
    info_ = BattleMap::getInstance()->getBattleInfo(id);
    assert(info_);
    const auto& content = session_transition_source_->content();
    const auto map = content.battleMaps().find(id);
    assert(map != content.battleMaps().end());
    assert(info_->BattleFieldID == map->second.battlefieldId);
    const auto battlefield = content.battlefields().find(map->second.battlefieldId);
    assert(battlefield != content.battlefields().end());
    battle_map_.loadBattlefield(battlefield->second);
}

bool BattleSceneHades::isManualCameraEnabled() const
{
    return battle_paused_
        || SystemSettings::getInstance()->data().manualCamera
        || active_paper_battle_view_;
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

Pointf BattleSceneHades::clampPaperCameraCenter(Pointf center) const
{
    const float groundExtent = static_cast<float>(COORD_COUNT * TILE_W * 2);
    return BattleSceneRenderMath::clampPaperCameraCenter(center, groundExtent, groundExtent);
}

Pointf BattleSceneHades::clampCameraCenterForActiveView(Pointf center) const
{
    return active_paper_battle_view_
        ? clampPaperCameraCenter(center)
        : BattleSceneCamera::clampCenter(center, makeCameraBounds());
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

void BattleSceneHades::initializeSceneRuntime(
    const KysChess::Battle::BattleRuntimeSession& runtime)
{
    initializeScenePresentationUnits(scene_units_.initialize(runtime));

    const auto cameraCenter = KysChess::chessGuiInitialBattleCameraCenter(
        runtime.runtimeUnits(),
        [this](int x, int y) { return battle_map_.pos45To90(x, y); });
    pos_ = cameraCenter.world;
    const auto cameraCell = battle_map_.pos90To45(pos_.x, pos_.y);
    man_x_ = cameraCell.x;
    man_y_ = cameraCell.y;
    battle_frame_ = 0;
    half_speed_step_on_next_render_ = true;
    battle_paused_ = false;
    camera_.reset(pos_);
    pos_ = clampCameraCenterForActiveView(pos_);
}

void BattleSceneHades::initializeScenePresentationUnits(const std::vector<int>& unitIds)
{
    for (const int unitId : unitIds)
    {
        const auto& unit = scene_units_.requireRuntimeUnit(unitId);
        scene_units_.setFightFrames(unitId, readBattleFightFramesForHeadId(unit.headId));
    }
}

void BattleSceneHades::initializeFormationPreviewRuntime()
{
    assert(session_transition_source_);
    assert(session_transition_source_->state().phase == KysChess::ChessSessionPhase::BattlePreparation);
    assert(session_transition_source_->state().preparedBattle);
    auto input = KysChess::BattleSetupFactory::build(
        *session_transition_source_->state().preparedBattle,
        session_transition_source_->content(),
        session_transition_source_->state().obtainedNeigongIds,
        session_transition_source_->state().options.battleFrameLimit);
    auto creation = KysChess::Battle::BattleRuntimeSession::createInitialized(std::move(input));
    formation_preview_runtime_.emplace(std::move(creation.session));
    initializeSceneRuntime(*formation_preview_runtime_);
}

void BattleSceneHades::initializeSessionBattleRuntime()
{
    assert(session_transition_source_);
    const auto* runtime = session_transition_source_->pendingBattleRuntime();
    const auto* initialization = session_transition_source_->pendingBattleInitialization();
    assert(runtime);
    assert(initialization);
    initializeSceneRuntime(*runtime);
    battle_report_.consumeInitialization(*initialization, *runtime);

    KysChess::Battle::BattlePresentationFrame frame;
    frame.visualEvents = initialization->visualEvents;
    frame.logEvents = initialization->logEvents;
    if (!frame.logEvents.empty() || !frame.visualEvents.empty())
    {
        BattleSceneRuntimeFrameEffects effects;
        updateFrameApplierContext();
        frame_applier_.apply(frame, effects);
    }

}

void BattleSceneHades::runPreBattlePositionSwapIfEnabled()
{
    if (!session_transition_source_->state().options.positionSwapEnabled)
    {
        return;
    }

    auto prompt = std::make_shared<MenuText>(
        std::vector<std::string>{"地圖佈陣", "列表佈陣", "直接開戰"});
    prompt->setFontSize(36);
    prompt->arrange(0, 0, 0, 45);
    prompt->runAtPosition(300, 220);
    if (prompt->getResult() == 0)
    {
        runPositionSwapLoop();
    }
    else if (prompt->getResult() == 1)
    {
        runListBasedSwap();
    }
}

void BattleSceneHades::draw()
{
    if (active_paper_battle_view_)
    {
        drawPaperView();
        return;
    }
    drawClassicView();
}

std::optional<float> BattleSceneHades::defaultPaperCameraAngleFromRuntimeUnits() const
{
    // Keep a stable near-top-down direction so switching from 2D does not rotate to the enemy formation.
    return PAPER_CAMERA_CLASSIC_VIEW_ANGLE;
}

std::optional<Pointf> BattleSceneHades::defaultPaperCameraCenterFromRuntimeUnits() const
{
    if (!activeRuntimeSession())
    {
        return std::nullopt;
    }

    double bestDistanceSquared = -1.0;
    Pointf selectedAlly;
    Pointf selectedEnemy;
    for (const auto& ally : scene_units_.runtimeUnits())
    {
        if (!ally.alive || ally.team != 0)
        {
            continue;
        }
        for (const auto& enemy : scene_units_.runtimeUnits())
        {
            if (!enemy.alive || enemy.team != 1)
            {
                continue;
            }
            const Pointf delta = enemy.motion.position - ally.motion.position;
            const double distanceSquared = delta.x * delta.x + delta.y * delta.y;
            if (bestDistanceSquared < 0.0 || distanceSquared < bestDistanceSquared)
            {
                bestDistanceSquared = distanceSquared;
                selectedAlly = ally.motion.position;
                selectedEnemy = enemy.motion.position;
            }
        }
    }

    if (bestDistanceSquared < 0.0)
    {
        return std::nullopt;
    }
    Pointf center = (selectedAlly + selectedEnemy) * 0.5;
    center.z = 0.0f;
    return clampPaperCameraCenter(center);
}

void BattleSceneHades::updatePaperCameraAutoCenter(bool snap)
{
    auto center = defaultPaperCameraCenterFromRuntimeUnits();
    if (!center)
    {
        return;
    }
    if (snap)
    {
        pos_ = *center;
        return;
    }

    Pointf delta = *center - pos_;
    delta.z = 0.0f;
    const float distance = delta.norm();
    if (distance <= PAPER_AUTO_CENTER_DEAD_ZONE)
    {
        return;
    }
    const float step = std::min(PAPER_AUTO_CENTER_MAX_STEP, distance - PAPER_AUTO_CENTER_DEAD_ZONE);
    delta.normTo(step);
    pos_ = clampPaperCameraCenter(pos_ + delta);
}

void BattleSceneHades::switchBattleViewMode(bool paperView)
{
    paper_camera_touch_.reset();
    battle_control_capture_.reset();
    invalidatePointerOwnership();
    active_paper_battle_view_ = paperView;
    pos_ = clampCameraCenterForActiveView(pos_);
    x_ = 0;
    y_ = 0;
    if (paperView)
    {
        if (auto angle = defaultPaperCameraAngleFromRuntimeUnits())
        {
            paper_camera_angle_ = *angle;
        }
        // The classic view already initialized pos_; preserve it as the 3D entry center.
        paper_camera_auto_center_ = false;
        paper_camera_distance_ = std::clamp(paper_camera_distance_, PAPER_CAMERA_MIN_DISTANCE, PAPER_CAMERA_MAX_DISTANCE);
        paper_camera_height_ = std::clamp(paper_camera_height_, PAPER_CAMERA_MIN_HEIGHT, PAPER_CAMERA_MAX_HEIGHT);
    }
    else
    {
        camera_.reset(pos_);
    }
    updateFrameApplierContext();
}

void BattleSceneHades::drawClassicView()
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
    const bool cursorFloorActive = battle_cursor_->isRunning() && !acting_role_->isAuto();
    const bool cursorFloorActionMode = cursorFloorActive && battle_cursor_->getMode() == BattleCursor::Action;

    auto earth_tex = Engine::getInstance()->getTexture("earth_base");
    if (earth_tex)
    {
        Color c = { 255, 255, 255, 255 };
        engine->setColor(earth_tex, c);
        int earthTextureW = COORD_COUNT * TILE_W * 2;
        int earthTextureH = COORD_COUNT * TILE_H * 2;
        Engine::getTextureSize(earth_tex, earthTextureW, earthTextureH);
        const int earthOffsetX = std::max(0, (earthTextureW - COORD_COUNT * TILE_W * 2) / 2);
        const int earthOffsetY = std::max(0, (earthTextureH - COORD_COUNT * TILE_H * 2) / 2);
        Rect earth_src = rect0;
        Rect earth_dst = use_whole_scene ? rect0 : rect1;
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
        earth_src.x += earthOffsetX;
        earth_src.y += earthOffsetY;
        if (earth_src.w > 0 && earth_src.h > 0 && earth_dst.w > 0 && earth_dst.h > 0)
        {
            if (use_whole_scene)
            {
                std::vector<Color> color_v(4, { 255, 255, 255, 255 });
                engine->renderTextureLight(earth_tex, &earth_src, &earth_dst, color_v, { 0.25f, 0.0f, 0.0f, 0.0f });
            }
            else
            {
                engine->renderTexture(earth_tex, &earth_src, &earth_dst);
            }
        }
    }

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
                const bool selectable = canSelect(ix, iy);
                const auto floorDraw = BattleSceneRenderMath::battleCursorFloorDraw(
                    cursorFloorActive,
                    cursorFloorActionMode,
                    selectable,
                    haveEffect(ix, iy),
                    ix == select_x_ && iy == select_y_,
                    earth_tex != nullptr);
                if (floorDraw.shouldDraw && num > 0)
                {
                    TextureManager::getInstance()->renderTexture("smap", num, renderWorldX(p.x), renderWorldY(p.y / 2.0), TextureManager::RenderInfo{ floorDraw.color });
                }
            }
        }
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
                presentation.fightFrames,
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
                renderWorldY(unit.motion.position.y / 2.0) + ROLE_STATUS_BAR_Y);
        }
    }

    if (swapSelectedUnitId_ >= 0)
    {
        const auto& selectedPosition = scene_units_.requireRuntimeUnit(swapSelectedUnitId_).motion.position;
        if (is_visible_world(selectedPosition.x, selectedPosition.y / 2.0))
        {
            engine->fillColor(
                {255, 255, 0, 80},
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
        const auto frozen = runtimeFrozenStatusForUnit(activeRuntimeSession(), unit.id);
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
            Font::getInstance()->draw(
                std::format("({},{})", unit.grid.x, unit.grid.y),
                (std::max)(1, int(28 * sizeScale)),
                worldToUiX(wx - 5),
                worldToUiY(wy - 5),
                {255, 255, 100, 255});
        }
    }

    // Draw total frames elapsed on top left
    Font::getInstance()->draw(std::to_string(battle_frame_), 20, 10, 10, { 255, 255, 255, 255 }, 200);
    drawBattleControls();

    //if (result_ >= 0)
    //{
    //    Engine::getInstance()->fillColor({ 0, 0, 0, 128 }, 0, 0, -1, -1);
    //}
}

void BattleSceneHades::drawPaperView()
{
    auto* engine = Engine::getInstance();
    const int sceneW = render_center_x_ * 2;
    const int sceneH = render_center_y_ * 2;

    if (paper_camera_auto_center_)
    {
        updatePaperCameraAutoCenter(false);
    }

    engine->setRenderTarget("scene");
    engine->fillColor({ 72, 94, 118, 255 }, 0, 0, sceneW, sceneH);
    engine->fillColor({ 42, 48, 46, 255 }, 0, sceneH * 2 / 3, sceneW, sceneH / 3);

    paper_camera_.setViewport(static_cast<float>(sceneW), static_cast<float>(sceneH));
    paper_camera_.setFov(PAPER_CAMERA_FOV);
    paper_camera_.setNearPlane(8.0f);
    paper_camera_.center = { pos_.x, pos_.y, 0.0f };
    paper_camera_distance_ = std::clamp(paper_camera_distance_, PAPER_CAMERA_MIN_DISTANCE, PAPER_CAMERA_MAX_DISTANCE);
    paper_camera_height_ = std::clamp(paper_camera_height_, PAPER_CAMERA_MIN_HEIGHT, PAPER_CAMERA_MAX_HEIGHT);
    paper_camera_.pos = {
        pos_.x + std::cos(paper_camera_angle_) * paper_camera_distance_,
        pos_.y + std::sin(paper_camera_angle_) * paper_camera_distance_,
        paper_camera_height_
    };

    if (shake_ > 0)
    {
        Pointf viewDir = paper_camera_.center - paper_camera_.pos;
        viewDir.z = 0;
        viewDir = normalizeOr(viewDir, { 0, 1, 0 });
        const Pointf paperRight = normalizeOr({ viewDir.y, -viewDir.x, 0 }, { 1, 0, 0 });
        const Pointf shakeOffset = paperRight * static_cast<float>(x_ * 2) + viewDir * static_cast<float>(y_ * 2);
        paper_camera_.center += shakeOffset;
        paper_camera_.pos += shakeOffset;
    }

    paper_sky_.render(engine, sceneW, sceneH, paper_camera_.pos, paper_camera_.center);

    Pointf viewDir = paper_camera_.center - paper_camera_.pos;
    viewDir.z = 0;
    viewDir = normalizeOr(viewDir, { 0, 1, 0 });
    const Pointf paperRight = normalizeOr({ viewDir.y, -viewDir.x, 0 }, { 1, 0, 0 });

    auto earthTexture = engine->getTexture("earth");
    if (earthTexture)
    {
        Engine::setColor(earthTexture, { 255, 255, 255, 255 });

        int earthW = 0;
        int earthH = 0;
        Engine::getTextureSize(earthTexture, earthW, earthH);
        const float originalW = static_cast<float>(BATTLE_COORD_COUNT * TILE_W * 2);
        const float originalH = static_cast<float>(BATTLE_COORD_COUNT * TILE_W * 2);
        const float textureW = static_cast<float>(std::max(1, earthW));
        const float textureH = static_cast<float>(std::max(1, earthH));
        const float groundSourceOffsetX = std::max(0.0f, (textureW - originalW) * 0.5f);
        const float groundSourceOffsetY = std::max(0.0f, (textureH - originalH) * 0.5f);
        const float minX = -groundSourceOffsetX;
        const float minY = -groundSourceOffsetY;
        const float maxX = textureW - groundSourceOffsetX;
        const float maxY = textureH - groundSourceOffsetY;

        auto makeGroundMeshLines = [](float minValue, float maxValue, float originalSize)
        {
            std::vector<float> lines;
            lines.reserve(PAPER_GROUND_MESH_TARGET_DIVISIONS + PAPER_GROUND_MESH_EXTENSION_DIVISIONS_PER_SIDE * 2 + 8);
            auto addLine = [&](float value)
            {
                lines.push_back(std::clamp(value, minValue, maxValue));
            };
            auto addSegment = [&](float start, float end, int divisions)
            {
                if (start > maxValue || end < minValue)
                {
                    return;
                }
                start = std::clamp(start, minValue, maxValue);
                end = std::clamp(end, minValue, maxValue);
                if (end < start)
                {
                    return;
                }
                divisions = std::max(1, divisions);
                for (int i = 0; i <= divisions; ++i)
                {
                    const float factor = static_cast<float>(i) / divisions;
                    addLine(start + (end - start) * factor);
                }
            };

            const float centerMargin = originalSize * (1.0f - PAPER_GROUND_MESH_CENTER_RATIO) * 0.5f;
            const float originalMin = 0.0f;
            const float originalMax = originalSize;
            const float centerMin = originalMin + centerMargin;
            const float centerMax = originalMax - centerMargin;
            const int centerDivisions = std::max(16,
                PAPER_GROUND_MESH_TARGET_DIVISIONS
                - PAPER_GROUND_MESH_EXTENSION_DIVISIONS_PER_SIDE * 2
                - PAPER_GROUND_MESH_ORIGINAL_EDGE_DIVISIONS_PER_SIDE * 2);

            addSegment(minValue, originalMin, PAPER_GROUND_MESH_EXTENSION_DIVISIONS_PER_SIDE);
            addSegment(originalMin, centerMin, PAPER_GROUND_MESH_ORIGINAL_EDGE_DIVISIONS_PER_SIDE);
            addSegment(centerMin, centerMax, centerDivisions);
            addSegment(centerMax, originalMax, PAPER_GROUND_MESH_ORIGINAL_EDGE_DIVISIONS_PER_SIDE);
            addSegment(originalMax, maxValue, PAPER_GROUND_MESH_EXTENSION_DIVISIONS_PER_SIDE);

            std::sort(lines.begin(), lines.end());
            std::vector<float> uniqueLines;
            uniqueLines.reserve(lines.size());
            for (float value : lines)
            {
                if (uniqueLines.empty() || std::abs(value - uniqueLines.back()) > 0.25f)
                {
                    uniqueLines.push_back(value);
                }
            }
            if (uniqueLines.size() < 2)
            {
                uniqueLines = { minValue, maxValue };
            }
            return uniqueLines;
        };

        const auto meshXLines = makeGroundMeshLines(minX, maxX, originalW);
        const auto meshYLines = makeGroundMeshLines(minY, maxY, originalH);
        const int meshX = static_cast<int>(meshXLines.size()) - 1;
        const int meshY = static_cast<int>(meshYLines.size()) - 1;
        const float groundEdgeFadeDistance = static_cast<float>(TILE_W) * PAPER_GROUND_EDGE_FADE_TILE_COUNT;
        const float horizonFadeStart = std::max(groundEdgeFadeDistance, originalW * PAPER_GROUND_HORIZON_FADE_START_RATIO);
        const float horizonFadeEnd = std::max(horizonFadeStart + groundEdgeFadeDistance, originalW * PAPER_GROUND_HORIZON_FADE_END_RATIO);
        const float groundCenterX = textureW * 0.5f;
        const float groundCenterY = textureH * 0.5f;
        const float groundRadialBase = std::min(textureW, textureH);
        const float groundRadialFadeStart = groundRadialBase * PAPER_GROUND_RADIAL_FADE_START_RATIO;
        const float groundRadialFadeEnd = groundRadialBase * PAPER_GROUND_RADIAL_FADE_END_RATIO;

        std::vector<Pointf> world;
        std::vector<FPoint> source;
        std::vector<Color> colors;
        world.reserve(meshXLines.size() * meshYLines.size());
        source.reserve(meshXLines.size() * meshYLines.size());
        colors.reserve(meshXLines.size() * meshYLines.size());

        for (float sourceY : meshYLines)
        {
            for (float sourceX : meshXLines)
            {
                const float clampedSourceX = std::clamp(sourceX + groundSourceOffsetX, 0.0f, textureW);
                const float clampedSourceY = std::clamp(sourceY + groundSourceOffsetY, 0.0f, textureH);
                const float distanceToEdge = std::min(
                    std::min(clampedSourceX, textureW - clampedSourceX),
                    std::min(clampedSourceY, textureH - clampedSourceY));
                const float edgeFade = smoothStep(0.0f, groundEdgeFadeDistance, distanceToEdge);
                float alpha = PAPER_GROUND_EDGE_MIN_ALPHA + (255.0f - PAPER_GROUND_EDGE_MIN_ALPHA) * edgeFade;
                const float centerDx = clampedSourceX - groundCenterX;
                const float centerDy = clampedSourceY - groundCenterY;
                const float centerDistance = std::sqrt(centerDx * centerDx + centerDy * centerDy);
                const float radialFade = 1.0f - smoothStep(groundRadialFadeStart, groundRadialFadeEnd, centerDistance);
                alpha = std::min(alpha, 255.0f * radialFade);

                world.push_back({ sourceX, sourceY, 0 });
                source.push_back({ clampedSourceX, clampedSourceY });
                colors.push_back({ 255, 255, 255, static_cast<uint8_t>(std::clamp(alpha, 0.0f, 255.0f)) });
            }
        }

        const auto projected = paper_camera_.getProj(world);
        std::vector<FPoint> destination;
        std::vector<float> depths;
        destination.reserve(projected.size());
        depths.reserve(world.size());
        for (size_t i = 0; i < world.size(); ++i)
        {
            const float depth = paper_camera_.getDepth(world[i]);
            const float horizonFade = smoothStep(horizonFadeStart, horizonFadeEnd, depth);
            const float horizonAlpha = PAPER_GROUND_HORIZON_MIN_ALPHA
                + (255.0f - PAPER_GROUND_HORIZON_MIN_ALPHA) * (1.0f - horizonFade);
            colors[i].a = std::min(colors[i].a, static_cast<uint8_t>(std::clamp(horizonAlpha, 0.0f, 255.0f)));
            destination.push_back({ static_cast<float>(projected[i].x), static_cast<float>(projected[i].y) });
            depths.push_back(depth);
        }

        std::vector<int> indices;
        indices.reserve(meshX * meshY * 6);
        auto indexOf = [&](int ix, int iy)
        {
            return iy * (meshX + 1) + ix;
        };
        for (int iy = 0; iy < meshY; ++iy)
        {
            for (int ix = 0; ix < meshX; ++ix)
            {
                const int i0 = indexOf(ix, iy);
                const int i1 = indexOf(ix + 1, iy);
                const int i2 = indexOf(ix + 1, iy + 1);
                const int i3 = indexOf(ix, iy + 1);
                if (depths[i0] <= paper_camera_.getNearPlane()
                    || depths[i1] <= paper_camera_.getNearPlane()
                    || depths[i2] <= paper_camera_.getNearPlane()
                    || depths[i3] <= paper_camera_.getNearPlane())
                {
                    continue;
                }
                indices.insert(indices.end(), { i0, i1, i2, i2, i3, i0 });
            }
        }

        engine->renderTextureMesh(earthTexture, destination, source, colors, indices);
    }

    const bool cursorFloorActive = battle_cursor_->isRunning() && !acting_role_->isAuto();
    if (cursorFloorActive && canSelect(select_x_, select_y_))
    {
        const auto floorDraw = BattleSceneRenderMath::battleCursorFloorDraw(
            cursorFloorActive,
            battle_cursor_->getMode() == BattleCursor::Action,
            true,
            haveEffect(select_x_, select_y_),
            true,
            true);
        auto cursorFloorTexture = ensurePaperCursorFloorTexture(*engine);
        if (cursorFloorTexture && floorDraw.shouldDraw)
        {
            auto color = floorDraw.color;
            color.a = 145;
            Engine::setColor(cursorFloorTexture, color);

            const auto p00 = battle_map_.pos45To90(select_x_, select_y_);
            const auto p10 = battle_map_.pos45To90(select_x_ + 1, select_y_);
            const auto p11 = battle_map_.pos45To90(select_x_ + 1, select_y_ + 1);
            const auto p01 = battle_map_.pos45To90(select_x_, select_y_ + 1);
            const auto quad = BattleSceneRenderMath::paperFloorQuad(p00, p10, p11, p01, 1.5f);
            const std::vector<Pointf> world(quad.begin(), quad.end());
            const std::vector<FPoint> source = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
            renderProjectedQuad(paper_camera_, cursorFloorTexture, world, source);
        }
    }

    struct PaperSprite
    {
        Texture* texture = nullptr;
        TextureWarpper* tex = nullptr;
        Pointf anchor;
        Pointf sortAnchor;
        std::vector<Pointf> world;
        std::vector<FPoint> source;
        Color color{ 255, 255, 255, 255 };
        uint8_t alpha = 255;
        int rot = 0;
        int turn = 1;
        bool subdivideVertical = false;
        bool faceCamera = false;
        bool usePerspectiveMesh = false;
        bool debugRoleTexture = false;
        int debugHeadId = -1;
    };

    std::vector<PaperSprite> sprites;
    sprites.reserve(scene_units_.runtimeUnits().size() + attack_effects_.size() + 512);
    std::vector<std::pair<FPoint, std::string>> roleTextureDebugLabels;
    roleTextureDebugLabels.reserve(scene_units_.runtimeUnits().size());
    std::unordered_map<int, float> roleInfoAnchorZByUnitId;
    roleInfoAnchorZByUnitId.reserve(scene_units_.runtimeUnits().size());

    const auto cameraCell = battle_map_.pos90To45(pos_.x, pos_.y);
    const int buildingMargin = 8;
    auto paperDepth = [&](const Pointf& point)
    {
        return paper_camera_.getDepth(point);
    };
    auto isPaperWallAt = [&](int x, int y)
    {
        return !battle_map_.isOutLine(x, y)
            && isPaperWallTile(battle_map_.buildingLayer().data(x, y) / 2);
    };

    bool needDrawWallTexture = true;
    if (auto oldWallTexture = engine->getTexture("paper-wall-edge"))
    {
        int oldW = 0;
        int oldH = 0;
        Engine::getTextureSize(oldWallTexture, oldW, oldH);
        needDrawWallTexture = oldW != 16 || oldH != 16;
    }
    engine->createRenderedTexture("paper-wall-edge", 16, 16);
    auto wallTexture = engine->getTexture("paper-wall-edge");
    if (needDrawWallTexture)
    {
        auto previousTarget = engine->getRenderTarget();
        engine->setRenderTarget(wallTexture);
        engine->fillColor({ 70, 62, 50, 235 }, 0, 0, 16, 16, BLENDMODE_NONE);
        engine->fillColor({ 94, 84, 66, 235 }, 0, 1, 16, 1, BLENDMODE_NONE);
        engine->fillColor({ 47, 42, 35, 235 }, 0, 4, 16, 1, BLENDMODE_NONE);
        engine->fillColor({ 88, 78, 61, 235 }, 0, 8, 16, 1, BLENDMODE_NONE);
        engine->fillColor({ 42, 37, 31, 235 }, 0, 12, 16, 1, BLENDMODE_NONE);
        engine->fillColor({ 104, 92, 72, 210 }, 0, 15, 16, 1, BLENDMODE_NONE);
        engine->setRenderTarget(previousTarget);
    }

    std::unordered_map<int, TextureWarpper*> paperWallTextureCache;
    auto getPaperWallTexture = [&](int tile) -> TextureWarpper*
    {
        auto it = paperWallTextureCache.find(tile);
        if (it != paperWallTextureCache.end())
        {
            return it->second;
        }

        TextureWarpper* tex = nullptr;
        const auto filename = GameUtil::PATH() + "resource/paper-wall-texture/" + std::to_string(tile) + ".png";
        if (filefunc::fileExist(filename))
        {
            tex = TextureManager::getInstance()->getTexture("paper-wall-texture", tile);
            if (tex)
            {
                tex->load();
                if (!tex->getTexture())
                {
                    tex = nullptr;
                }
            }
        }
        paperWallTextureCache[tile] = tex;
        return tex;
    };

    auto addWallEdge = [&](const Pointf& a, const Pointf& b, int tile)
    {
        constexpr float wallHeight = 80.0f;
        const std::vector<Pointf> world = {
            { a.x, a.y, wallHeight },
            { b.x, b.y, wallHeight },
            { b.x, b.y, 0.0f },
            { a.x, a.y, 0.0f },
        };
        PaperSprite sprite;
        if (auto tex = getPaperWallTexture(tile))
        {
            sprite.texture = tex->getTexture();
            sprite.source = {
                { 0, 0 },
                { static_cast<float>(tex->w), 0 },
                { static_cast<float>(tex->w), static_cast<float>(tex->h) },
                { 0, static_cast<float>(tex->h) },
            };
        }
        else
        {
            sprite.texture = wallTexture;
            sprite.source = { { 0, 0 }, { 16, 0 }, { 16, 16 }, { 0, 16 } };
        }
        sprite.world = world;
        sprite.sortAnchor = { (a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, wallHeight * 0.5f };
        sprite.usePerspectiveMesh = true;
        sprites.emplace_back(std::move(sprite));
    };

    for (int sum = -view_sum_region_ - buildingMargin; sum <= view_sum_region_ + 15 + buildingMargin; ++sum)
    {
        for (int i = -view_width_region_ - buildingMargin; i <= view_width_region_ + buildingMargin; ++i)
        {
            const int ix = cameraCell.x + i + (sum / 2);
            const int iy = cameraCell.y - i + (sum - sum / 2);
            if (battle_map_.isOutLine(ix, iy))
            {
                continue;
            }
            const int tile = battle_map_.buildingLayer().data(ix, iy) / 2;
            if (tile <= 0)
            {
                continue;
            }
            if (isPaperWallTile(tile))
            {
                auto p00 = battle_map_.pos45To90(ix, iy);
                auto p10 = battle_map_.pos45To90(ix + 1, iy);
                auto p01 = battle_map_.pos45To90(ix, iy + 1);
                auto p11 = battle_map_.pos45To90(ix + 1, iy + 1);
                if (!isPaperWallAt(ix, iy - 1))
                {
                    addWallEdge(p00, p10, tile);
                }
                if (!isPaperWallAt(ix + 1, iy))
                {
                    addWallEdge(p10, p11, tile);
                }
                if (!isPaperWallAt(ix, iy + 1))
                {
                    addWallEdge(p11, p01, tile);
                }
                if (!isPaperWallAt(ix - 1, iy))
                {
                    addWallEdge(p01, p00, tile);
                }
                continue;
            }
            auto tex = TextureManager::getInstance()->getTexture("smap", tile);
            if (!tex)
            {
                continue;
            }
            auto anchor = battle_map_.pos45To90(ix, iy);
            PaperSprite sprite;
            sprite.tex = tex;
            sprite.anchor = anchor;
            sprite.sortAnchor = anchor + Pointf{ 0, 18, 0 };
            sprite.usePerspectiveMesh = true;
            sprites.push_back(std::move(sprite));
        }
    }

    for (const auto& unit : scene_units_.runtimeUnits())
    {
        const auto& presentation = scene_units_.requirePresentation(unit.id);
        const int faceTowards = realTowardsToPaperFaceTowards(
            unit.motion.facing,
            viewDir,
            paperRight,
            realTowardsToFaceTowards(unit.motion.facing));
        const int renderActType = unit.alive ? unit.animation.actType : -1;
        const int renderActFrame = unit.alive ? unit.animation.actFrame : 0;
        auto tex = TextureManager::getInstance()->getTexture(
            std::format("fight/fight{:03}", unit.headId),
            BattleSceneRenderMath::calRenderUnitPic(
                presentation.fightFrames,
                BattleSceneRenderMath::realTowardsFromFaceTowards(faceTowards),
                renderActType,
                renderActFrame));
        if (!tex)
        {
            continue;
        }
        tex->load();
        if (!tex->getTexture())
        {
            continue;
        }

        PaperSprite sprite;
        sprite.tex = tex;
        sprite.anchor = unit.motion.position;
        sprite.sortAnchor = sprite.anchor;
        sprite.debugRoleTexture = true;
        sprite.debugHeadId = unit.headId;
        sprite.alpha = presentation.attention ? 255 - presentation.attention * 4 : 255;
        sprite.color = calculateHurtFlashColor(unit.id, sprite.color);
        roleInfoAnchorZByUnitId[unit.id] = BattleSceneRenderMath::paperRoleInfoAnchorZ(tex->dy);
        if (result_ == -1 && presentation.shake)
        {
            sprite.anchor.x += shakeJitter(battle_frame_, unit.id);
        }
        if (!unit.alive)
        {
            sprite.rot = faceTowards >= 2 ? 90 : 270;
            sprite.turn = 0;
        }
        sprites.push_back(std::move(sprite));
    }

    for (const auto& effect : attack_effects_)
    {
        const int frame = effect.TotalEffectFrame > 0 ? effect.Frame % effect.TotalEffectFrame : 0;
        auto tex = TextureManager::getInstance()->getTexture(effect.Path, frame);
        if (!tex)
        {
            continue;
        }
        Pointf effectPosition = effect.Pos;
        if (effect.FollowUnitId >= 0)
        {
            effectPosition = scene_units_.requireRuntimeUnit(effect.FollowUnitId).motion.position + effect.Pos;
        }

        PaperSprite sprite;
        sprite.tex = tex;
        sprite.anchor = effectPosition;
        sprite.sortAnchor = effectPosition + Pointf{ 0, 220, 0 };
        sprite.faceCamera = true;
        if (effect.FollowUnitId >= 0)
        {
            sprite.alpha = 255;
        }
        else
        {
            sprite.alpha = static_cast<uint8_t>(std::clamp(
                255.0 * (effect.TotalFrame * 1.25 - effect.Frame) / (effect.TotalFrame * 1.25),
                0.0,
                255.0));
        }
        sprites.push_back(std::move(sprite));
    }

    std::sort(sprites.begin(), sprites.end(), [&](const PaperSprite& lhs, const PaperSprite& rhs)
        {
            const float leftDepth = paperDepth(lhs.sortAnchor);
            const float rightDepth = paperDepth(rhs.sortAnchor);
            if (leftDepth != rightDepth)
            {
                return leftDepth > rightDepth;
            }
            return lhs.turn < rhs.turn;
        });

    auto renderPaperTexture = [&](const PaperSprite& sprite)
    {
        if (sprite.texture && sprite.world.size() == 4 && sprite.source.size() == 4)
        {
            if (pointsInCameraFront(paper_camera_, sprite.world))
            {
                if (sprite.usePerspectiveMesh)
                {
                    renderProjectedPlaneMesh(paper_camera_, sprite.texture, sprite.world, sprite.source, 4, 4);
                }
                else
                {
                    renderProjectedQuad(paper_camera_, sprite.texture, sprite.world, sprite.source);
                }
            }
            return;
        }
        if (!sprite.tex)
        {
            return;
        }
        sprite.tex->load();
        auto* texture = sprite.tex->getTexture();
        if (!texture)
        {
            return;
        }
        const float left = -static_cast<float>(sprite.tex->dx);
        const float right = static_cast<float>(sprite.tex->w - sprite.tex->dx);
        const float top = static_cast<float>(sprite.tex->dy);
        const float bottom = static_cast<float>(sprite.tex->dy - sprite.tex->h);
        const Pointf cameraForward = normalizeOr(paper_camera_.center - paper_camera_.pos, { 0, 1, 0 });
        const Pointf cameraUp = normalizeOr(
            {
                paperRight.y * cameraForward.z - paperRight.z * cameraForward.y,
                paperRight.z * cameraForward.x - paperRight.x * cameraForward.z,
                paperRight.x * cameraForward.y - paperRight.y * cameraForward.x,
            },
            { 0, 0, 1 });
        auto localPoint = [&](float x, float z)
        {
            if (sprite.faceCamera)
            {
                return sprite.anchor + paperRight * x + cameraUp * z;
            }
            if (sprite.rot == 90)
            {
                return sprite.anchor + paperRight * -z + Pointf{ 0, 0, x };
            }
            if (sprite.rot == 270)
            {
                return sprite.anchor + paperRight * z + Pointf{ 0, 0, -x };
            }
            return sprite.anchor + paperRight * x + Pointf{ 0, 0, z };
        };
        const std::vector<Pointf> world = {
            localPoint(left, top),
            localPoint(right, top),
            localPoint(right, bottom),
            localPoint(left, bottom),
        };
        const std::vector<FPoint> source = {
            { 0, 0 },
            { static_cast<float>(sprite.tex->w), 0 },
            { static_cast<float>(sprite.tex->w), static_cast<float>(sprite.tex->h) },
            { 0, static_cast<float>(sprite.tex->h) },
        };
        Color color = sprite.color;
        color.a = sprite.alpha;
        Engine::setColor(texture, color);
        if (sprite.debugRoleTexture || sprite.usePerspectiveMesh)
        {
            constexpr int maxCellSize = 32;
            const int columnCount = std::clamp((sprite.tex->w + maxCellSize - 1) / maxCellSize, 1, 32);
            const int rowCount = std::clamp((sprite.tex->h + maxCellSize - 1) / maxCellSize, 1, 32);
            std::vector<FPoint> destination;
            std::vector<FPoint> meshSource;
            std::vector<int> indices;
            destination.reserve((columnCount + 1) * (rowCount + 1));
            meshSource.reserve((columnCount + 1) * (rowCount + 1));
            indices.reserve(columnCount * rowCount * 6);

            for (int row = 0; row <= rowCount; ++row)
            {
                const float v = static_cast<float>(row) / rowCount;
                const float z = top + (bottom - top) * v;
                for (int column = 0; column <= columnCount; ++column)
                {
                    const float u = static_cast<float>(column) / columnCount;
                    const float x = left + (right - left) * u;
                    const auto projected = paper_camera_.getProj({ localPoint(x, z) }).front();
                    destination.push_back({ projected.x, projected.y });
                    meshSource.push_back({ static_cast<float>(sprite.tex->w) * u, static_cast<float>(sprite.tex->h) * v });
                }
            }

            for (int row = 0; row < rowCount; ++row)
            {
                for (int column = 0; column < columnCount; ++column)
                {
                    const int topLeft = row * (columnCount + 1) + column;
                    const int topRight = topLeft + 1;
                    const int bottomLeft = topLeft + columnCount + 1;
                    const int bottomRight = bottomLeft + 1;
                    indices.insert(indices.end(), { topLeft, topRight, bottomRight, bottomRight, bottomLeft, topLeft });
                }
            }
            engine->renderTextureMesh(texture, destination, meshSource, {}, indices);
        }
        else if (sprite.subdivideVertical && sprite.tex->h > 64)
        {
            const int segmentCount = std::clamp((sprite.tex->h + 63) / 64, 2, 16);
            for (int segment = 0; segment < segmentCount; ++segment)
            {
                const float t0 = static_cast<float>(segment) / segmentCount;
                const float t1 = static_cast<float>(segment + 1) / segmentCount;
                const float z0 = top + (bottom - top) * t0;
                const float z1 = top + (bottom - top) * t1;
                const std::vector<Pointf> segmentWorld = {
                    localPoint(left, z0),
                    localPoint(right, z0),
                    localPoint(right, z1),
                    localPoint(left, z1),
                };
                const std::vector<FPoint> segmentSource = {
                    { 0, static_cast<float>(sprite.tex->h) * t0 },
                    { static_cast<float>(sprite.tex->w), static_cast<float>(sprite.tex->h) * t0 },
                    { static_cast<float>(sprite.tex->w), static_cast<float>(sprite.tex->h) * t1 },
                    { 0, static_cast<float>(sprite.tex->h) * t1 },
                };
                renderProjectedQuad(paper_camera_, texture, segmentWorld, segmentSource);
            }
        }
        else
        {
            renderProjectedQuad(paper_camera_, texture, world, source);
        }

        /*
        // Temporarily disabled texture-placement diagnostics. Uncomment this block and the label loop below
        // to inspect the projected canvas, its anchor, and the parsed frame offset again.
        if (sprite.debugRoleTexture && pointsInCameraFront(paper_camera_, world))
        {
            constexpr int gridDivisions = 4;
            for (int grid = 1; grid < gridDivisions; ++grid)
            {
                const float t = static_cast<float>(grid) / gridDivisions;
                const auto gridTop = paper_camera_.getProj({ localPoint(left + (right - left) * t, top) }).front();
                const auto gridBottom = paper_camera_.getProj({ localPoint(left + (right - left) * t, bottom) }).front();
                const auto gridLeft = paper_camera_.getProj({ localPoint(left, top + (bottom - top) * t) }).front();
                const auto gridRight = paper_camera_.getProj({ localPoint(right, top + (bottom - top) * t) }).front();
                engine->drawLine({ 255, 255, 255, 80 }, { gridTop.x, gridTop.y }, { gridBottom.x, gridBottom.y });
                engine->drawLine({ 255, 255, 255, 80 }, { gridLeft.x, gridLeft.y }, { gridRight.x, gridRight.y });
            }
            const auto projected = paper_camera_.getProj(world);
            for (int index = 0; index < 4; ++index)
            {
                const auto& from = projected[index];
                const auto& to = projected[(index + 1) % 4];
                engine->drawLine({ 255, 255, 255, 180 }, { from.x, from.y }, { to.x, to.y });
            }

            if (paper_camera_.getDepth(sprite.anchor) > paper_camera_.getNearPlane())
            {
                const auto anchor = paper_camera_.getProj({ sprite.anchor }).front();
                constexpr float crossRadius = 7.0f;
                engine->drawLine({ 255, 64, 64, 255 },
                    { anchor.x - crossRadius, anchor.y }, { anchor.x + crossRadius, anchor.y });
                engine->drawLine({ 255, 64, 64, 255 },
                    { anchor.x, anchor.y - crossRadius }, { anchor.x, anchor.y + crossRadius });
                roleTextureDebugLabels.emplace_back(
                    FPoint{ anchor.x + 10.0f, anchor.y + 10.0f },
                    std::format("H{} {}x{} d{},{}", sprite.debugHeadId, sprite.tex->w, sprite.tex->h,
                        sprite.tex->dx, sprite.tex->dy));
            }
        }
        */
    };

    for (const auto& sprite : sprites)
    {
        renderPaperTexture(sprite);
    }

    for (const auto& unit : scene_units_.runtimeUnits())
    {
        if (!unit.alive)
        {
            continue;
        }
        const auto anchorZ = roleInfoAnchorZByUnitId.find(unit.id);
        if (anchorZ == roleInfoAnchorZByUnitId.end())
        {
            continue;
        }
        Pointf barWorld = unit.motion.position;
        barWorld.z += anchorZ->second;
        if (paper_camera_.getDepth(barWorld) <= paper_camera_.getNearPlane())
        {
            continue;
        }
        const auto projected = paper_camera_.getProj({ barWorld });
        if (!projected.empty())
        {
            renderExtraRoleInfo(unit, projected.front().x, projected.front().y);
        }
    }

    engine->renderTextureToMain("scene");

    /* Temporarily disabled companion labels for the texture-placement diagnostics above.
    for (const auto& [position, label] : roleTextureDebugLabels)
    {
        Font::getInstance()->draw(label, 12, position.x, position.y, { 255, 240, 255, 255 }, 255);
    }
    */

    for (const auto& text : text_effects_)
    {
        assert(text.PaperAnchor.has_value());
        Pointf textWorld = *text.PaperAnchor;
        if (text.PaperFollowUnitId >= 0)
        {
            textWorld = scene_units_.requireRuntimeUnit(text.PaperFollowUnitId).motion.position;
        }
        double screenYOffset = 0.0;
        if (text.Type == 0)
        {
            screenYOffset = -static_cast<double>(text.Frame);
        }
        textWorld.z += BattleSceneRenderMath::paperFloatingTextAnchorZ();
        if (paper_camera_.getDepth(textWorld) <= paper_camera_.getNearPlane())
        {
            continue;
        }
        const auto projected = paper_camera_.getProj({ textWorld });
        if (!projected.empty())
        {
            Font::getInstance()->draw(text.Text, text.Size,
                projected.front().x + text.PaperScreenOffsetX,
                projected.front().y + screenYOffset,
                text.color,
                255);
        }
    }

    Font::getInstance()->draw(std::to_string(battle_frame_), 20, 10, 10, { 255, 255, 255, 255 }, 200);
    drawBattleControls();
}

void BattleSceneHades::dealEvent(EngineEvent& e)
{
    if (e.type == EVENT_KEY_UP && e.key.key == K_O && active_paper_battle_view_)
    {
        togglePaperCameraMode();
        return;
    }
    if (active_paper_battle_view_)
    {
        auto engine = Engine::getInstance();
        if (engine->checkKeyPress(K_Z))
        {
            paper_camera_distance_ += PAPER_CAMERA_ZOOM_STEP;
        }
        if (engine->checkKeyPress(K_X))
        {
            paper_camera_distance_ -= PAPER_CAMERA_ZOOM_STEP;
        }
        paper_camera_distance_ = std::clamp(paper_camera_distance_, PAPER_CAMERA_MIN_DISTANCE, PAPER_CAMERA_MAX_DISTANCE);

        float cameraRotate = 0.0f;
        float cameraHeightDelta = 0.0f;
        Pointf cameraPan;
        if (engine->checkKeyPress(K_LEFT))
        {
            cameraRotate -= 1.0f;
        }
        if (engine->checkKeyPress(K_RIGHT))
        {
            cameraRotate += 1.0f;
        }
        if (engine->checkKeyPress(K_UP))
        {
            cameraHeightDelta += 1.0f;
        }
        if (engine->checkKeyPress(K_DOWN))
        {
            cameraHeightDelta -= 1.0f;
        }
        cameraPan = paperKeyboardPanVector(
            paper_camera_angle_,
            engine->checkKeyPress(K_W),
            engine->checkKeyPress(K_S),
            engine->checkKeyPress(K_A),
            engine->checkKeyPress(K_D));
        if (cameraPan.norm() != 0)
        {
            paper_camera_auto_center_ = false;
            cameraPan.normTo(PAPER_FREE_CAMERA_PAN_SPEED);
            pos_ = clampPaperCameraCenter(pos_ + cameraPan);
        }
        paper_camera_angle_ += cameraRotate * PAPER_FREE_CAMERA_ROTATE_SPEED;
        paper_camera_height_ = std::clamp(
            paper_camera_height_ + cameraHeightDelta * PAPER_FREE_CAMERA_HEIGHT_SPEED,
            PAPER_CAMERA_MIN_HEIGHT,
            PAPER_CAMERA_MAX_HEIGHT);
    }
    if (battle_paused_ || Engine::getInstance()->isBattleLogOverlayOpen())
    {
        return;
    }

    int steps = getBattleStepsThisRender();
    for (int step = 0; step < steps && !exit_; ++step)
    {
        advanceBattleFrame();
    }
}

RunNode::PointerResult BattleSceneHades::onPointerEvent(const PointerEvent& event)
{
    const PointerIdentity pointer{event.source, event.pointerId, event.button};
    int uiW = 0;
    int uiH = 0;
    Engine::getInstance()->getUISize(uiW, uiH);
    const auto controls = makeBattleControlLayout(uiW, battle_paused_, active_paper_battle_view_);

    if (event.phase == PointerPhase::ButtonDown && event.button == SDL_BUTTON_LEFT)
    {
        if (battle_control_capture_.onDown(pointer, event.uiPosition, controls) != BattleControlId::None)
        {
            return PointerResult::Captured;
        }
    }
    if (battle_control_capture_.owns(pointer))
    {
        if (event.phase == PointerPhase::ButtonUp)
        {
            activateBattleControl(battle_control_capture_.onUp(pointer, event.uiPosition, controls));
        }
        else if (event.phase == PointerPhase::Cancel)
        {
            battle_control_capture_.cancel(pointer);
        }
        return PointerResult::Handled;
    }

    if (!active_paper_battle_view_)
    {
        if (auto center = camera_.handleManualInput(event, pos_, makeCameraBounds(), isManualCameraEnabled()))
        {
            pos_ = *center;
        }
        if (event.phase == PointerPhase::ButtonDown && camera_.manualDragging())
        {
            return PointerResult::Captured;
        }
        if (event.phase == PointerPhase::Motion && camera_.manualDragging())
        {
            return PointerResult::Handled;
        }
        if (event.phase == PointerPhase::ButtonUp || event.phase == PointerPhase::Cancel)
        {
            return PointerResult::Handled;
        }
    }
    return RunNode::onPointerEvent(event);
}

TouchPolicy BattleSceneHades::touchPolicy() const
{
    return active_paper_battle_view_ ? TouchPolicy::DirectTouch : TouchPolicy::PrimaryPointer;
}

void BattleSceneHades::onTouchSample(const TouchSample& sample)
{
    if (!active_paper_battle_view_ || Engine::getInstance()->isBattleLogOverlayOpen())
    {
        paper_camera_touch_.reset();
        return;
    }

    int uiW = 0;
    int uiH = 0;
    Engine::getInstance()->getUISize(uiW, uiH);
    const auto controls = makeBattleControlLayout(uiW, battle_paused_, active_paper_battle_view_);
    for (const auto& action : paper_camera_touch_.process(sample, controls))
    {
        switch (action.type)
        {
        case PaperCameraTouchActionType::ControlActivated:
            activateBattleControl(action.control);
            break;
        case PaperCameraTouchActionType::PanActivated:
            paper_camera_auto_center_ = false;
            break;
        case PaperCameraTouchActionType::Pan:
            pos_ = clampPaperCameraCenter(pos_ + paperTouchPanDelta(
                paper_camera_angle_,
                paper_camera_distance_,
                paper_camera_height_,
                PAPER_CAMERA_FOV,
                static_cast<float>(uiH),
                action.delta.x,
                action.delta.y));
            break;
        case PaperCameraTouchActionType::Pair:
            if (action.previousSpan > 0.001f && action.currentSpan > 0.001f)
            {
                paper_camera_distance_ = std::clamp(
                    paperTouchPinchDistance(paper_camera_distance_, action.previousSpan, action.currentSpan),
                    PAPER_CAMERA_MIN_DISTANCE,
                    PAPER_CAMERA_MAX_DISTANCE);
            }
            paper_camera_angle_ += paperTouchRotationDelta(action.delta.x, static_cast<float>(uiW));
            paper_camera_height_ = std::clamp(
                paper_camera_height_ + paperTouchHeightDelta(
                    action.delta.y,
                    static_cast<float>(uiH),
                    PAPER_CAMERA_MIN_HEIGHT,
                    PAPER_CAMERA_MAX_HEIGHT),
                PAPER_CAMERA_MIN_HEIGHT,
                PAPER_CAMERA_MAX_HEIGHT);
            break;
        }
    }
}

void BattleSceneHades::onPointerInputReset()
{
    paper_camera_touch_.reset();
    battle_control_capture_.reset();
}

void BattleSceneHades::dealEvent2(EngineEvent& e)
{
}

bool BattleSceneHades::canToggleBattlePause() const
{
    return battleSceneCanTogglePause(camera_.closeUpActive(), result_);
}

void BattleSceneHades::toggleBattlePause()
{
    setBattlePaused(!battle_paused_);
}

void BattleSceneHades::setBattlePaused(bool paused)
{
    if (paused && !canToggleBattlePause())
    {
        return;
    }
    if (battle_paused_ != paused)
    {
        battle_paused_ = paused;
        ++battle_control_layout_revision_;
    }
    updateFrameApplierContext();
}

void BattleSceneHades::showInBattleLog()
{
    auto model = BattleLogPresenter().present(makePostBattleSummary(), battle_report_.report());
    if (result_ < 0)
    {
        model.title = "戰鬥中日誌";
        model.resultText = "戰鬥中";
        model.totalFrames = battle_frame_;
    }
    Engine::getInstance()->showBattleLogOverlay(model, false);
    paper_camera_touch_.reset();
    battle_control_capture_.reset();
    invalidatePointerOwnership();
}

void BattleSceneHades::cycleBattleSpeed()
{
    auto* settings = SystemSettings::getInstance();
    auto updated = settings->snapshot();
    updated.battleSpeed = nextBattleSpeedSetting(updated.battleSpeed);
    settings->update(updated);
    half_speed_step_on_next_render_ = true;
}

void BattleSceneHades::togglePaperBattleView()
{
    const bool nextPaperView = !active_paper_battle_view_;
    auto* settings = SystemSettings::getInstance();
    auto updated = settings->snapshot();
    if (updated.paperBattleView != nextPaperView)
    {
        updated.paperBattleView = nextPaperView;
        settings->update(updated);
    }
    switchBattleViewMode(nextPaperView);
}

void BattleSceneHades::togglePaperCameraMode()
{
    paper_camera_auto_center_ = !paper_camera_auto_center_;
    if (paper_camera_auto_center_)
    {
        updatePaperCameraAutoCenter(true);
    }
}

void BattleSceneHades::activateBattleControl(BattleControlId control)
{
    switch (control)
    {
    case BattleControlId::Pause:
        toggleBattlePause();
        break;
    case BattleControlId::Speed:
        cycleBattleSpeed();
        break;
    case BattleControlId::PaperView:
        togglePaperBattleView();
        break;
    case BattleControlId::CameraMode:
        togglePaperCameraMode();
        break;
    case BattleControlId::Log:
        showInBattleLog();
        break;
    case BattleControlId::None:
        break;
    }
}

void BattleSceneHades::drawBattleControls()
{
    int uiW = 0;
    int uiH = 0;
    Engine::getInstance()->getUISize(uiW, uiH);
    const auto layout = makeBattleControlLayout(uiW, battle_paused_, active_paper_battle_view_);
    const bool pauseEnabled = canToggleBattlePause();
    const auto* settings = SystemSettings::getInstance();
    const std::string speedText(battleSpeedDisplayText(settings->data().battleSpeed));
    const std::string paperViewText(battlePaperViewDisplayText(active_paper_battle_view_));
    const std::string paperCameraModeText(battlePaperCameraModeDisplayText(paper_camera_auto_center_));

    auto* engine = Engine::getInstance();
    auto drawIconButton = [&](const Rect& rect, int textureId, bool enabled)
    {
        const uint8_t alpha = enabled ? 255 : 96;
        engine->fillRoundedRect({ 0, 0, 0, 95 }, rect.x + 5, rect.y + 5, rect.w - 10, rect.h - 10, 14);
        TextureManager::getInstance()->renderTexture(
            "title",
            textureId,
            rect.x,
            rect.y,
            TextureManager::RenderInfo{ { 255, 255, 255, 255 }, alpha },
            rect.w,
            rect.h);
        if (!enabled)
        {
            engine->fillRoundedRect({ 0, 0, 0, 80 }, rect.x + 4, rect.y + 4, rect.w - 8, rect.h - 8, 12);
        }
    };

    auto drawTextButton = [&](const Rect& rect, int textureId, const std::string& text)
    {
        engine->fillRoundedRect({ 0, 0, 0, 95 }, rect.x + 5, rect.y + 5, rect.w - 10, rect.h - 10, 14);
        if (textureId >= 0)
        {
            TextureManager::getInstance()->renderTexture(
                "title",
                textureId,
                rect.x,
                rect.y,
                TextureManager::RenderInfo{ { 255, 255, 255, 255 }, 255 },
                rect.w,
                rect.h);
        }
        const int fontSize = 20;
        const int textW = Font::getTextDrawSize(text) * fontSize / 2;
        const int textX = rect.x + (rect.w - textW) / 2;
        const int textY = rect.y + (rect.h - fontSize) / 2 + 2;
        Font::getInstance()->draw(text, fontSize, textX + 1, textY + 1, { 24, 18, 10, 255 }, 180);
        Font::getInstance()->draw(text, fontSize, textX, textY, { 245, 222, 128, 255 }, 255);
    };

    drawTextButton(*layout.rect(BattleControlId::Speed), BATTLE_CONTROL_SPEED_TEXTURE_ID, speedText);
    drawTextButton(*layout.rect(BattleControlId::PaperView), BATTLE_CONTROL_SPEED_TEXTURE_ID, paperViewText);
    if (active_paper_battle_view_)
    {
        drawTextButton(*layout.rect(BattleControlId::CameraMode), BATTLE_CONTROL_SPEED_TEXTURE_ID, paperCameraModeText);
    }
    if (battle_paused_)
    {
        drawIconButton(*layout.rect(BattleControlId::Log), BATTLE_CONTROL_LOG_TEXTURE_ID, true);
    }
    drawIconButton(
        *layout.rect(BattleControlId::Pause),
        battle_paused_ ? BATTLE_CONTROL_PLAY_TEXTURE_ID : BATTLE_CONTROL_PAUSE_TEXTURE_ID,
        pauseEnabled || battle_paused_);
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
            pos_ = clampCameraCenterForActiveView(pos_);
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
    assert(session_transition_source_);
    previous_refresh_interval_ = RunNode::getRefreshInterval();
    battle_paused_ = false;
    active_paper_battle_view_ = battleSceneInitialPaperView(
        SystemSettings::getInstance()->data().paperBattleView,
        session_transition_source_->state().options.positionSwapEnabled);
    paper_camera_auto_center_ = true;
    Engine::getInstance()->hideBattleLogOverlay();
    paper_sky_.reset();
    paper_sky_.generateClouds();

    calViewRegion();
    addChild(Weather::getInstance());
    battle_map_.makeEarthTexture(render_center_x_, render_center_y_);

    auto* engine = Engine::getInstance();
    const int wholeSceneWidth = COORD_COUNT * TILE_W * 2;
    const int wholeSceneHeight = COORD_COUNT * TILE_H * 2;
    const int maximumTextureSize = engine->getMaxTextureSize();
    const bool browserNeedsFallback = GameUtil::isLegacyBrowser();
    use_whole_scene_ = BattleSceneRenderMath::shouldUseWholeSceneTexture(
        browserNeedsFallback,
        maximumTextureSize,
        wholeSceneWidth,
        wholeSceneHeight);
    if (use_whole_scene_)
    {
        engine->createRenderedTexture("whole_scene", wholeSceneWidth, wholeSceneHeight);
    }
    else
    {
        LOG("whole_scene disabled: browser_fallback={}, required={}x{}, renderer max={}\n",
            browserNeedsFallback,
            wholeSceneWidth,
            wholeSceneHeight,
            maximumTextureSize);
    }

    if (session_transition_source_->state().phase == KysChess::ChessSessionPhase::BattlePreparation)
    {
        initializeFormationPreviewRuntime();
        runPreBattlePositionSwapIfEnabled();
        if (RunNode::runOwnerExitRequested())
        {
            setExit(true);
            return;
        }

        KysChess::ChessAction start;
        start.type = KysChess::ChessActionType::StartBattle;
        const auto started = session_transition_source_->beginAction(start);
        assert(started.accepted && started.transitionPending);
    }
    initializeSessionBattleRuntime();
    formation_preview_runtime_.reset();
    if (SystemSettings::getInstance()->data().paperBattleView && !active_paper_battle_view_)
    {
        switchBattleViewMode(true);
    }
    else if (auto angle = defaultPaperCameraAngleFromRuntimeUnits())
    {
        paper_camera_angle_ = *angle;
    }
    if (active_paper_battle_view_)
    {
        // Preserve the classic-view center selected during runtime initialization.
        paper_camera_auto_center_ = false;
    }
    const auto& prepared = *session_transition_source_->lastBattlePrepared();
    if (prepared.kind == KysChess::PreparedChessBattleKind::Standalone)
    {
        const auto map = session_transition_source_->content().battleMaps().find(battle_id_);
        assert(map != session_transition_source_->content().battleMaps().end());
        Audio::getInstance()->playMusic(map->second.musicId);
    }
    else
    {
        Audio::getInstance()->playMusic(KysChess::getRandomBattleMusic());
    }
}
void BattleSceneHades::onExit()
{
    if (previous_refresh_interval_ > 0.0)
    {
        RunNode::setRefreshInterval(previous_refresh_interval_);
        previous_refresh_interval_ = 0.0;
    }

    Engine::getInstance()->destroyTexture("whole_scene");
    Engine::getInstance()->hideBattleLogOverlay();
    use_whole_scene_ = false;

    // hurt_flash_timers_.clear();
    // Engine::getInstance()->hideBattleLogOverlay();
}

void BattleSceneHades::calExpGot()
{
}

BattlePostBattleSummary BattleSceneHades::makePostBattleSummary() const
{
    return scene_units_.makePostBattleSummary(battle_report_.report(), result_);
}

class PositionSwapNode : public RunNode
{
public:
    explicit PositionSwapNode(BattleSceneHades* battle) : battle_(battle) {}

    void dealEvent(EngineEvent&) override
    {
    }

    PointerResult onPointerEvent(const PointerEvent& event) override
    {
        if (event.button != SDL_BUTTON_LEFT)
        {
            return RunNode::onPointerEvent(event);
        }
        if (event.phase == PointerPhase::ButtonDown)
        {
            return event.insidePresent ? PointerResult::Captured : PointerResult::Ignored;
        }
        if (event.phase != PointerPhase::ButtonUp)
        {
            return PointerResult::Handled;
        }

        const auto position = battle_->getMousePosition(
            event.uiPosition.x,
            event.uiPosition.y,
            battle_->man_x_,
            battle_->man_y_);
        int clickedUnitId = -1;
        for (const int unitId : battle_->scene_units_.allyUnitIds())
        {
            const auto& unit = battle_->scene_units_.requireRuntimeUnit(unitId);
            if (unit.grid.x == position.x && unit.grid.y == position.y)
            {
                clickedUnitId = unitId;
                break;
            }
        }
        if (clickedUnitId < 0)
        {
            return PointerResult::Handled;
        }
        if (battle_->swapSelectedUnitId_ < 0)
        {
            battle_->swapSelectedUnitId_ = clickedUnitId;
        }
        else if (clickedUnitId != battle_->swapSelectedUnitId_)
        {
            KysChess::ChessAction action;
            action.type = KysChess::ChessActionType::SwapPositions;
            action.chessInstanceId = battle_->swapSelectedUnitId_;
            action.targetChessInstanceId = clickedUnitId;
            const auto result = battle_->session_transition_source_->beginAction(action);
            assert(result.accepted);
            battle_->formation_preview_runtime_->swapSetupUnitPositions(
                battle_->swapSelectedUnitId_,
                clickedUnitId);
            battle_->swapSelectedUnitId_ = -1;
        }
        return PointerResult::Handled;
    }

    void onPressedCancel() override
    {
        auto menu = std::make_shared<MenuText>(
            std::vector<std::string>{"佈陣完成", "繼續調整"});
        menu->setFontSize(36);
        menu->arrange(0, 0, 0, 45);
        menu->runAtPosition(400, 300);
        if (menu->getResult() == 0)
        {
            battle_->swapSelectedUnitId_ = -1;
            setExit(true);
        }
    }

private:
    BattleSceneHades* battle_{};
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
    const auto allies = scene_units_.allyUnitIds();
    if (allies.size() < 2)
    {
        positionSwapActive_ = false;
        return;
    }

    const auto buildNames = [&](int highlight = -1) {
        int maximumNameWidth = 0;
        int maximumCoordinateWidth = 0;
        for (const int unitId : allies)
        {
            const auto& unit = scene_units_.requireRuntimeUnit(unitId);
            maximumNameWidth = std::max(maximumNameWidth, Font::getTextDrawSize(unit.name));
            maximumCoordinateWidth = std::max(
                maximumCoordinateWidth,
                Font::getTextDrawSize(std::format("({},{})", unit.grid.x, unit.grid.y)));
        }

        std::vector<std::string> names;
        std::vector<Color> colors;
        std::vector<Color> outlineColors;
        std::vector<bool> animateOutlines;
        for (int index = 0; index < static_cast<int>(allies.size()); ++index)
        {
            const auto& unit = scene_units_.requireRuntimeUnit(allies[index]);
            const auto coordinate = std::format("({},{})", unit.grid.x, unit.grid.y);
            const int gap = maximumNameWidth - Font::getTextDrawSize(unit.name)
                + maximumCoordinateWidth - Font::getTextDrawSize(coordinate)
                + 2;
            names.push_back(std::format("{}{}{}", unit.name, std::string(gap, ' '), coordinate));
            colors.push_back({255, 255, 255, 255});
            outlineColors.push_back(index == highlight ? Color{255, 255, 100, 255} : Color{});
            animateOutlines.push_back(index == highlight);
        }
        return std::make_tuple(names, colors, outlineColors, animateOutlines);
    };

    for (;;)
    {
        auto [firstNames, firstColors, firstOutlines, firstAnimations] = buildNames();
        auto firstMenu = std::make_shared<MenuText>();
        firstMenu->setStrings(firstNames, firstColors, firstOutlines, firstAnimations);
        firstMenu->setFontSize(28);
        firstMenu->arrange(0, 0, 0, 36);
        firstMenu->runAtPosition(100, 100);
        const int first = firstMenu->getResult();
        if (first < 0)
        {
            auto confirm = std::make_shared<MenuText>(
                std::vector<std::string>{"佈陣完成", "繼續調整"});
            confirm->setFontSize(36);
            confirm->arrange(0, 0, 0, 45);
            confirm->runAtPosition(400, 300);
            if (confirm->getResult() == 0)
            {
                break;
            }
            continue;
        }

        auto [secondNames, secondColors, secondOutlines, secondAnimations] = buildNames(first);
        auto secondMenu = std::make_shared<MenuText>();
        secondMenu->setStrings(secondNames, secondColors, secondOutlines, secondAnimations);
        secondMenu->setFontSize(28);
        secondMenu->arrange(0, 0, 0, 36);
        secondMenu->runAtPosition(100, 100);
        const int second = secondMenu->getResult();
        if (second < 0 || second == first)
        {
            continue;
        }

        KysChess::ChessAction action;
        action.type = KysChess::ChessActionType::SwapPositions;
        action.chessInstanceId = allies[first];
        action.targetChessInstanceId = allies[second];
        const auto result = session_transition_source_->beginAction(action);
        assert(result.accepted);
        formation_preview_runtime_->swapSetupUnitPositions(allies[first], allies[second]);
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
    KysChess::Battle::BattlePresentationFrame frame;
    auto advance = session_transition_source_->advanceAutomatic(1);
    assert(advance.frames.size() <= 1);
    initializeScenePresentationUnits(scene_units_.synchronizeRuntimeUnits());
    if (!advance.frames.empty())
    {
        frame = std::move(advance.frames.front());
    }
    const auto* runtime = activeRuntimeSession();
    assert(runtime);
    battle_report_.consumeFrame(frame, *runtime);
    BattleSceneRuntimeFrameEffects effects;
    updateFrameApplierContext();
    frame_applier_.apply(frame, effects);
    if (active_paper_battle_view_)
    {
        for (const auto& event : frame.gameplayEvents)
        {
            if (event.type == KysChess::Battle::BattleGameplayEventType::UnitDied)
            {
                shake_ = std::max(shake_, PAPER_DEATH_SHAKE_FRAMES);
                break;
            }
        }
    }
    KysChess::captureChessGuiBattleCompletion(
        completed_action_result_,
        advance.completedAction,
        session_transition_source_->state().lastBattleOutcome,
        result_);
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
        completed_scene_run_ = true;
        setExit(true);
    }
}

void BattleSceneHades::onPressedCancel()
{
}

void BattleSceneHades::renderExtraRoleInfo(
    const KysChess::Battle::BattleRuntimeUnit& unit,
    double x,
    double statusBarY)
{
    if (!unit.alive)
    {
        return;
    }

    // 畫個血條
    Color outline_color = { 0, 0, 0, 128 };
    const int barLeft = int(std::round(x - ROLE_STATUS_BAR_WIDTH / 2.0));
    const int hpBarY = static_cast<int>(std::round(statusBarY));
    const int mpBarY = hpBarY + (ROLE_STATUS_BAR_MP_Y - ROLE_STATUS_BAR_Y);
    const int extraBarY = hpBarY + (ROLE_STATUS_BAR_FROZEN_Y - ROLE_STATUS_BAR_Y);

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

    Color background_color = { 0, 255, 0, 128 };    // 我方綠色
    Color shadow_color = { 64, 64, 64, 128 };       // 背景陰影
    if (unit.team == 1)
    {
        // 敵方紅色
        background_color = { 255, 0, 0, 128 };
    }

    renderBar(hpBarY, unit.vitals.hp, unit.vitals.maxHp, background_color, shadow_color);

    if (unit.vitals.maxHp > 0 && unit.vitals.hp > 0)
    {
        int bar_y = hpBarY;
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
            const auto* runtime = activeRuntimeSession();
            if (!runtime)
            {
                return 0;
            }
            return runtime->runtime().units.require(unit.id).damage.blockFirstHitsRemaining;
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
    renderBar(mpBarY, unit.vitals.mp, unit.vitals.maxMp, mp_color, mp_shadow_color);

    // Frozen / cooldown bar – frozen takes priority
    const auto frozen = runtimeFrozenStatusForUnit(activeRuntimeSession(), unit.id);
    if (frozen.frames > 0 && frozen.maxFrames > 0)
    {
        Color frozen_color = { 200, 220, 255, 192 };
        int bar_y = extraBarY;
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
        int bar_y = extraBarY;
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
