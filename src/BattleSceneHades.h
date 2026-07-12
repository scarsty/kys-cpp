#pragma once
#include "battle/BattleCore.h"
#include "battle/BattleMovement.h"
#include "battle/BattlePresentation.h"
#include "battle/BattleRuntimeSession.h"
#include "BattlePostBattleSummary.h"
#include "BattlePresentationEffects.h"
#include "BattleReportCollector.h"
#include "BattleScenePauseControl.h"
#include "BattleSceneRenderMath.h"
#include "BattleScene.h"
#include "BattleSceneCamera.h"
#include "BattleSceneControls.h"
#include "BattleScenePaperCameraTouch.h"
#include "BattleSceneFrameApplier.h"
#include "BattleSceneMapState.h"
#include "BattleSceneUnitStore.h"
#include "ChessSessionTypes.h"
#include "PaperSky.h"
#include <array>
#include <cstddef>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace KysChess
{
class RoleComboState;
class ChessGameSession;
enum class Trigger;
}

class PositionSwapNode;

class BattleSceneHades : public BattleScene
{
    friend class PositionSwapNode;
public:
    explicit BattleSceneHades(KysChess::ChessGameSession& session);
    virtual ~BattleSceneHades();
    void setID(int id);

    //继承自基类的函数
    virtual void draw() override;
    virtual void dealEvent(EngineEvent& e) override;     //战场主循环
    virtual void dealEvent2(EngineEvent& e) override;    //用于停止自动
    virtual PointerResult onPointerEvent(const PointerEvent& event) override;
    virtual void onTouchSample(const TouchSample& sample) override;
    virtual void onPointerInputReset() override;
    virtual TouchPolicy touchPolicy() const override;
    virtual uint64_t controlLayoutRevision() const override { return battle_control_layout_revision_; }
    virtual void onEntrance() override;
    virtual void onExit() override;
    virtual void calExpGot() override;

    virtual void backRun() override {}

    virtual void backRun1();
    virtual void onPressedCancel() override;

protected:
    struct MovementRuntime;
    void renderExtraRoleInfo(
        const KysChess::Battle::BattleRuntimeUnit& unit,
        double x,
        double statusBarY);
    virtual int checkResult() override;
    void updateFrameApplierContext();
    bool shouldAdvanceBattleSimulation();
    void ageHurtFlashTimers();
    void advanceScenePresentationFrame();
    void finishBattleIfReady();
    void activateBattleControl(BattleControlId control);
    void drawBattleControls();
    bool canToggleBattlePause() const;
    void toggleBattlePause();
    void setBattlePaused(bool paused);
    void showInBattleLog();
    void cycleBattleSpeed();
    void togglePaperBattleView();
    void togglePaperCameraMode();

    void initializeSceneRuntime(const KysChess::Battle::BattleRuntimeSession& runtime);
    void initializeScenePresentationUnits(const std::vector<int>& unitIds);
    void initializeFormationPreviewRuntime();
    void initializeSessionBattleRuntime();
    void runPreBattlePositionSwapIfEnabled();
    void runPositionSwapLoop();
    void runListBasedSwap();
    const KysChess::Battle::BattleRuntimeSession* activeRuntimeSession() const;
    bool isManualCameraEnabled() const;
    std::optional<float> defaultPaperCameraAngleFromRuntimeUnits() const;
    std::optional<Pointf> defaultPaperCameraCenterFromRuntimeUnits() const;
    void updatePaperCameraAutoCenter(bool snap);
    void switchBattleViewMode(bool paperView);
    int getBattleStepsThisRender();
    void advanceBattleFrame();
    BattleSceneCameraBounds makeCameraBounds() const;
    Pointf clampPaperCameraCenter(Pointf center) const;
    Pointf clampCameraCenterForActiveView(Pointf center) const;
    Color calculateHurtFlashColor(int unitId, const Color& baseColor) const;
    void drawClassicView();
    void drawPaperView();
public:
    const BattleReport& getBattleReport() const { return battle_report_.report(); }
    bool completedSceneRun() const { return completed_scene_run_; }
    const std::optional<KysChess::ChessActionResult>& completedActionResult() const
    {
        return completed_action_result_;
    }
    BattlePostBattleSummary makePostBattleSummary() const;
    static int getOperationType(int attackAreaType);
    static const char* getOperationTypeName(int operationType);

protected:
    bool use_whole_scene_ = false;
    BattleReportCollector battle_report_;
    KysChess::ChessGameSession* session_transition_source_ = nullptr;
    std::optional<KysChess::ChessActionResult> completed_action_result_;
    int swapSelectedUnitId_ = -1;
    bool positionSwapActive_ = false;
    std::optional<KysChess::Battle::BattleRuntimeSession> formation_preview_runtime_;
    std::unordered_map<int, int> hurt_flash_timers_;
    BattleSceneUnitStore scene_units_;
    std::deque<BattleAttackEffect> attack_effects_;
    std::deque<BattleTextEffect> text_effects_;
    BattleSceneMapState battle_map_;
    Pointf pos_;
    float gravity_ = -4.0f;
    float friction_ = 0.1f;
    int frozen_ = 0;
    int slow_ = 0;
    int shake_ = 0;
    double previous_refresh_interval_ = 0.0;
    int battle_frame_ = 0;
    bool completed_scene_run_{};
    bool half_speed_step_on_next_render_ = true;
    bool battle_paused_ = false;
    bool active_paper_battle_view_ = false;
    bool paper_camera_auto_center_ = true;
    BattleSceneCamera camera_;
    BattleSceneCameraBounds frame_applier_camera_bounds_{};
    bool manual_camera_enabled_{};
    BattleSceneFrameApplier frame_applier_;
    Camera paper_camera_;
    PaperSky paper_sky_;
    float paper_camera_angle_ = -2.53f;
    float paper_camera_distance_ = battleScenePaperCameraDefaults().distance;
    float paper_camera_height_ = battleScenePaperCameraDefaults().height;
    BattleControlCapture battle_control_capture_;
    BattleScenePaperCameraTouch paper_camera_touch_;
    uint64_t battle_control_layout_revision_{};
};
