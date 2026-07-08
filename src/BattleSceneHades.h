#pragma once
#include "battle/BattleCore.h"
#include "battle/BattleMovement.h"
#include "battle/BattlePresentation.h"
#include "battle/BattleRuntimeSession.h"
#include "BattlePostBattleSummary.h"
#include "BattlePresentationEffects.h"
#include "BattleSceneRenderMath.h"
#include "BattleScene.h"
#include "BattleSceneCamera.h"
#include "BattleSceneFrameApplier.h"
#include "BattleSceneMapState.h"
#include "BattleSceneSetupBuilder.h"
#include "BattleStatsView.h"
#include "BattleSceneUnitStore.h"
#include "ChessManager.h"
#include "ChessProgress.h"
#include "ChessRoleSave.h"
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
enum class Trigger;
}

class PositionSwapNode;

class BattleSceneHades : public BattleScene
{
    friend class PositionSwapNode;
public:
    BattleSceneHades(KysChess::ChessRoleSave& roleSave, KysChess::ChessProgress& progress, KysChess::ChessManager& chessManager);
    BattleSceneHades(int id, KysChess::ChessRoleSave& roleSave, KysChess::ChessProgress& progress, KysChess::ChessManager& chessManager);
    virtual ~BattleSceneHades();
    void setID(int id);

    //继承自基类的函数
    virtual void draw() override;
    virtual void dealEvent(EngineEvent& e) override;     //战场主循环
    virtual void dealEvent2(EngineEvent& e) override;    //用于停止自动
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
        double y);
    virtual int checkResult() override;
    void updateFrameApplierContext();
    bool shouldAdvanceBattleSimulation();
    void ageHurtFlashTimers();
    void advanceScenePresentationFrame();
    void finishBattleIfReady();
    bool handleBattleControlEvent(EngineEvent& e);
    void drawBattleControls();
    bool canToggleBattlePause() const;
    void toggleBattlePause();
    void setBattlePaused(bool paused);
    void showInBattleLog();
    void cycleBattleSpeed();

    void initializeBattleRuntime(KysChess::BattleSceneSetupBuilder::BattleSceneSetupBuildResult setupBuild);
    KysChess::Battle::BattleRuntimeSessionCreationInput makeBattleRuntimeSessionCreationInput(
        KysChess::BattleSceneSetupBuilder::BattleSceneSetupBuildResult setupBuild);
    void runPreBattlePositionSwapIfEnabled();

    void runPositionSwapLoop();
    void runListBasedSwap();
    bool isManualCameraEnabled() const;
    std::optional<float> defaultPaperCameraAngleFromRuntimeUnits() const;
    std::optional<Pointf> defaultPaperCameraCenterFromRuntimeUnits() const;
    void updatePaperCameraAutoCenter(bool snap);
    void switchBattleViewMode(bool paperView);
    int getBattleStepsThisRender();
    void advanceBattleFrame();
    BattleSceneCameraBounds makeCameraBounds() const;
    Color calculateHurtFlashColor(int unitId, const Color& baseColor) const;
    void drawClassicView();
    void drawPaperView();
public:
    const BattleReport& getBattleReport() const { return battle_report_.report(); }
    BattlePostBattleSummary makePostBattleSummary() const;
    void setBattleRuntimeRandomSeed(unsigned int seed);
    void setEnemyStars(const std::vector<int>& stars) { enemy_stars_ = stars; }
    void setTeammateWeapons(const std::vector<int>& weapons) { teammate_weapons_ = weapons; }
    void setTeammateArmors(const std::vector<int>& armors) { teammate_armors_ = armors; }
    void setEnemyWeapons(const std::vector<int>& weapons) { enemy_weapons_ = weapons; }
    void setEnemyArmors(const std::vector<int>& armors) { enemy_armors_ = armors; }
    void setCloneSpawnPositions(const std::vector<std::pair<int, int>>& positions) { clone_spawn_positions_ = positions; }
    void setEnemyCloneSpawnPositions(const std::vector<std::pair<int, int>>& positions) { enemy_clone_spawn_positions_ = positions; }
    void setCountFightsWon(bool countFightsWon) { count_fights_won_ = countFightsWon; }

    static int getOperationType(int attackAreaType);
    static const char* getOperationTypeName(int operationType);

protected:
    bool use_whole_scene_ = false;
    KysChess::ChessRoleSave& roleSave_;
    KysChess::ChessProgress& progress_;
    KysChess::ChessManager& chessManager_;
    BattleReportBuilder battle_report_;
    int swapSelectedUnitId_ = -1;
    bool positionSwapActive_ = false;
    std::optional<KysChess::Battle::BattleRuntimeSession> battle_session_;
    std::vector<int> enemy_stars_;
    std::vector<int> teammate_weapons_;
    std::vector<int> teammate_armors_;
    std::vector<int> enemy_weapons_;
    std::vector<int> enemy_armors_;
    std::vector<std::pair<int, int>> clone_spawn_positions_;
    std::vector<std::pair<int, int>> enemy_clone_spawn_positions_;
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
    bool half_speed_step_on_next_render_ = true;
    bool battle_paused_ = false;
    bool active_paper_battle_view_ = false;
    bool paper_camera_auto_center_ = true;
    BattleSceneCamera camera_;
    bool count_fights_won_ = true;
    unsigned int battle_random_seed_ = 1;
    BattleSceneCameraBounds frame_applier_camera_bounds_{};
    bool manual_camera_enabled_{};
    BattleSceneFrameApplier frame_applier_;
    Camera paper_camera_;
    PaperSky paper_sky_;
    float paper_camera_angle_ = -2.53f;
    float paper_camera_distance_ = 650.0f;
    float paper_camera_height_ = 300.0f;
};
