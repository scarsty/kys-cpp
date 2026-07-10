#include "BattleSceneHades.h"
#include "Audio.h"
#include "BattleLogPresenter.h"
#include "BattleScenePauseControl.h"
#include "BattleScenePresentationConstants.h"
#include "BattleSceneSetupBuilder.h"
#include "BattleStarStats.h"
#include "battle/BattleAttackSystem.h"
#include "battle/BattleCombatIntent.h"
#include "battle/BattleInitialization.h"
#include "battle/BattleOperation.h"
#include "battle/BattleStatusSystem.h"
#include "ChessBalance.h"
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
constexpr int BATTLE_CONTROL_BUTTON_SIZE = 64;
constexpr int BATTLE_CONTROL_BUTTON_GAP = 8;
constexpr int BATTLE_CONTROL_BUTTON_MARGIN = 12;
constexpr int BATTLE_CONTROL_PLAY_TEXTURE_ID = 333;
constexpr int BATTLE_CONTROL_PAUSE_TEXTURE_ID = 334;
constexpr int BATTLE_CONTROL_LOG_TEXTURE_ID = 335;
constexpr int BATTLE_CONTROL_SPEED_TEXTURE_ID = 336;
constexpr double BATTLE_FRAME_PROFILE_SLOW_MS = 4.0;
constexpr float PAPER_CAMERA_FOV = 60.0f;
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

struct BattleControlLayout
{
    Rect speed{};
    Rect paperView{};
    Rect cameraMode{};
    Rect log{};
    Rect pause{};
};

bool pointInRect(const Point& point, const Rect& rect)
{
    return point.x >= rect.x && point.x < rect.x + rect.w
        && point.y >= rect.y && point.y < rect.y + rect.h;
}

Point uiPointFromWindowPoint(int x, int y)
{
    int presentX = 0;
    int presentY = 0;
    int presentW = 0;
    int presentH = 0;
    int uiW = 0;
    int uiH = 0;
    auto* engine = Engine::getInstance();
    engine->getPresentRect(presentX, presentY, presentW, presentH);
    engine->getUISize(uiW, uiH);
    return BattleSceneRenderMath::windowPointToUiPoint(
        { x, y },
        { presentX, presentY, presentW, presentH },
        uiW,
        uiH);
}

BattleControlLayout battleControlLayout(int uiWidth, bool logVisible, bool cameraModeVisible)
{
    BattleControlLayout layout;
    int x = uiWidth - BATTLE_CONTROL_BUTTON_MARGIN - BATTLE_CONTROL_BUTTON_SIZE;
    auto takeButton = [&]()
    {
        Rect rect = { x, BATTLE_CONTROL_BUTTON_MARGIN, BATTLE_CONTROL_BUTTON_SIZE, BATTLE_CONTROL_BUTTON_SIZE };
        x -= BATTLE_CONTROL_BUTTON_SIZE + BATTLE_CONTROL_BUTTON_GAP;
        return rect;
    };

    layout.pause = takeButton();
    if (logVisible)
    {
        layout.log = takeButton();
    }
    if (cameraModeVisible)
    {
        layout.cameraMode = takeButton();
    }
    layout.paperView = takeButton();
    layout.speed = takeButton();
    return layout;
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

void BattleSceneHades::initializeBattleRuntime(
    KysChess::BattleSceneSetupBuilder::BattleSceneSetupBuildResult setupBuild)
{
    auto creationInput = makeBattleRuntimeSessionCreationInput(std::move(setupBuild));
    auto creation = KysChess::Battle::BattleRuntimeSession::createInitialized(std::move(creationInput));
    auto initialization = std::move(creation.initialization);
    battle_session_.emplace(std::move(creation.session));
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
    input.battleFrame = battle_frame_;
    input.profiling.enabled = SystemSettings::getInstance()->data().debugLatencyLog;
    input.profiling.slowFrameThresholdMs = BATTLE_FRAME_PROFILE_SLOW_MS;

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
    auto appendCloneSpawnCells = [&](const std::vector<std::pair<int, int>>& positions, int team)
    {
        for (const auto& [x, y] : positions)
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
            input.setup.cloneCells.push_back({ x, y, true, occupied, team });
        }
    };
    appendCloneSpawnCells(clone_spawn_positions_, 0);
    appendCloneSpawnCells(enemy_clone_spawn_positions_, 1);
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
    if (active_paper_battle_view_)
    {
        drawPaperView();
        return;
    }
    drawClassicView();
}

std::optional<float> BattleSceneHades::defaultPaperCameraAngleFromRuntimeUnits() const
{
    if (!battle_session_)
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

    Pointf direction = selectedEnemy - selectedAlly;
    direction.z = 0.0f;
    if (direction.norm() == 0)
    {
        return std::nullopt;
    }
    return static_cast<float>(std::atan2(-direction.y, -direction.x));
}

std::optional<Pointf> BattleSceneHades::defaultPaperCameraCenterFromRuntimeUnits() const
{
    if (!battle_session_)
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
    active_paper_battle_view_ = paperView;
    pos_ = clampCameraCenterForActiveView(pos_);
    x_ = 0;
    y_ = 0;
    if (paperView)
    {
        paper_camera_auto_center_ = true;
        if (auto angle = defaultPaperCameraAngleFromRuntimeUnits())
        {
            paper_camera_angle_ = *angle;
        }
        updatePaperCameraAutoCenter(true);
        paper_camera_auto_center_ = battlePaperCameraAutoCenterAfterEntry();
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
                renderWorldY(unit.motion.position.y / 2.0) + ROLE_STATUS_BAR_Y);
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
    };

    std::vector<PaperSprite> sprites;
    sprites.reserve(scene_units_.runtimeUnits().size() + attack_effects_.size() + 512);
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
                unit.fightFrames,
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
        sprite.subdivideVertical = true;
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
                renderProjectedQuad(paper_camera_, sprite.texture, sprite.world, sprite.source);
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
        auto localPoint = [&](float x, float z)
        {
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
        if (sprite.subdivideVertical && sprite.tex->h > 64)
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
    const bool battleControlHandled = handleBattleControlEvent(e);
    if (!battleControlHandled && e.type == EVENT_KEY_UP && e.key.key == K_O && active_paper_battle_view_)
    {
        togglePaperCameraMode();
        return;
    }
    if (!battleControlHandled && active_paper_battle_view_)
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
        Pointf forward = { -std::cos(paper_camera_angle_), -std::sin(paper_camera_angle_), 0.0f };
        forward = normalizeOr(forward, { 0, 1, 0 });
        const Pointf right = normalizeOr({ -forward.y, forward.x, 0.0f }, { 1, 0, 0 });
        if (engine->checkKeyPress(K_W))
        {
            cameraPan += forward;
        }
        if (engine->checkKeyPress(K_S))
        {
            cameraPan += -forward;
        }
        if (engine->checkKeyPress(K_A))
        {
            cameraPan += -right;
        }
        if (engine->checkKeyPress(K_D))
        {
            cameraPan += right;
        }
        if (cameraPan.norm() != 0)
        {
            if (!paper_camera_auto_center_)
            {
                cameraPan.normTo(PAPER_FREE_CAMERA_PAN_SPEED);
                pos_ = clampPaperCameraCenter(pos_ + cameraPan);
            }
        }
        paper_camera_angle_ += cameraRotate * PAPER_FREE_CAMERA_ROTATE_SPEED;
        paper_camera_height_ = std::clamp(
            paper_camera_height_ + cameraHeightDelta * PAPER_FREE_CAMERA_HEIGHT_SPEED,
            PAPER_CAMERA_MIN_HEIGHT,
            PAPER_CAMERA_MAX_HEIGHT);
    }
    else if (!battleControlHandled)
    {
        if (auto manualCenter = camera_.handleManualInput(e, pos_, makeCameraBounds(), isManualCameraEnabled()))
        {
            pos_ = *manualCenter;
        }
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
    battle_paused_ = paused;
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

bool BattleSceneHades::handleBattleControlEvent(EngineEvent& e)
{
    if ((e.type != EVENT_MOUSE_BUTTON_DOWN && e.type != EVENT_MOUSE_BUTTON_UP)
        || e.button.button != BUTTON_LEFT)
    {
        return false;
    }

    int uiW = 0;
    int uiH = 0;
    Engine::getInstance()->getUISize(uiW, uiH);
    const auto layout = battleControlLayout(uiW, battle_paused_, active_paper_battle_view_);
    const Point uiPoint = uiPointFromWindowPoint(e.button.x, e.button.y);

    auto hitsVisibleControl = [&]()
    {
        return pointInRect(uiPoint, layout.pause)
            || pointInRect(uiPoint, layout.speed)
            || pointInRect(uiPoint, layout.paperView)
            || (active_paper_battle_view_ && pointInRect(uiPoint, layout.cameraMode))
            || (battle_paused_ && pointInRect(uiPoint, layout.log));
    };

    if (!hitsVisibleControl())
    {
        return false;
    }

    if (e.type == EVENT_MOUSE_BUTTON_DOWN)
    {
        return true;
    }

    if (pointInRect(uiPoint, layout.pause))
    {
        toggleBattlePause();
        return true;
    }
    if (pointInRect(uiPoint, layout.speed))
    {
        cycleBattleSpeed();
        return true;
    }
    if (pointInRect(uiPoint, layout.paperView))
    {
        togglePaperBattleView();
        return true;
    }
    if (active_paper_battle_view_ && pointInRect(uiPoint, layout.cameraMode))
    {
        togglePaperCameraMode();
        return true;
    }
    if (battle_paused_ && pointInRect(uiPoint, layout.log))
    {
        showInBattleLog();
        return true;
    }
    return true;
}

void BattleSceneHades::drawBattleControls()
{
    int uiW = 0;
    int uiH = 0;
    Engine::getInstance()->getUISize(uiW, uiH);
    const auto layout = battleControlLayout(uiW, battle_paused_, active_paper_battle_view_);
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

    drawTextButton(layout.speed, BATTLE_CONTROL_SPEED_TEXTURE_ID, speedText);
    drawTextButton(layout.paperView, BATTLE_CONTROL_SPEED_TEXTURE_ID, paperViewText);
    if (active_paper_battle_view_)
    {
        drawTextButton(layout.cameraMode, BATTLE_CONTROL_SPEED_TEXTURE_ID, paperCameraModeText);
    }
    if (battle_paused_)
    {
        drawIconButton(layout.log, BATTLE_CONTROL_LOG_TEXTURE_ID, true);
    }
    drawIconButton(
        layout.pause,
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
    previous_refresh_interval_ = RunNode::getRefreshInterval();
    battle_paused_ = false;
    active_paper_battle_view_ = battleSceneInitialPaperView(
        SystemSettings::getInstance()->data().paperBattleView,
        progress_.isPositionSwapEnabled());
    paper_camera_auto_center_ = true;
    Engine::getInstance()->hideBattleLogOverlay();
    paper_sky_.reset();
    paper_sky_.generateClouds();

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
    battle_paused_ = false;
    camera_.reset(pos_);
    pos_ = clampCameraCenterForActiveView(pos_);

    initializeBattleRuntime(std::move(setupBuild));
    runPreBattlePositionSwapIfEnabled();

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
        updatePaperCameraAutoCenter(true);
        paper_camera_auto_center_ = battlePaperCameraAutoCenterAfterEntry();
    }

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
    Engine::getInstance()->hideBattleLogOverlay();
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
    renderBar(mpBarY, unit.vitals.mp, unit.vitals.maxMp, mp_color, mp_shadow_color);

    // Frozen / cooldown bar – frozen takes priority
    const auto frozen = runtimeFrozenStatusForUnit(battle_session_ ? &*battle_session_ : nullptr, unit.id);
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
