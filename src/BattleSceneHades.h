#pragma once
#include "battle/BattleMovement.h"
#include "battle/BattlePresentation.h"
#include "battle/BattleRuntimeSession.h"
#include "BattleSceneAct.h"
#include "BattleSceneBattleAdapter.h"
#include "BattleStatsView.h"
#include "BattleScenePresentationPlayer.h"
#include "ChessManager.h"
#include "ChessProgress.h"
#include "ChessRoleSave.h"
#include "Head.h"
#include <deque>
#include <map>
#include <optional>
#include <set>
#include <unordered_map>
#include <vector>

namespace KysChess
{
struct RoleComboState;
enum class Trigger;
}

class PositionSwapNode;

class BattleSceneHades : public BattleSceneAct
{
    friend class PositionSwapNode;
public:
    BattleSceneHades(KysChess::ChessRoleSave& roleSave, KysChess::ChessProgress& progress, KysChess::ChessManager& chessManager);
    BattleSceneHades(int id, KysChess::ChessRoleSave& roleSave, KysChess::ChessProgress& progress, KysChess::ChessManager& chessManager);
    virtual ~BattleSceneHades();

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
    struct SceneBattleFrameInput
    {
        bool shouldAdvance = true;
    };
    struct SceneBattleFrameResult
    {
        bool advanced = false;
    };
    void renderExtraRoleInfo(Role* r, double x, double y);
    //int calHurt(Role* r0, Role* r1);
    virtual int checkResult() override;
    virtual void setRoleInitState(Role* r) override;
    SceneBattleFrameInput buildBattleFrameInput();
    KysChess::BattleSceneBattleAdapter::BattleFrameApplyContext buildCoreRuntimeContext(
        KysChess::BattleSceneBattleAdapter::BattleActionFrameAdapterContext& actionContext,
        KysChess::BattleSceneBattleAdapter::BattleMovementPhysicsFrameAdapterContext& movementPhysicsContext,
        KysChess::Battle::BattleFrameScratch& scratch);
    void applyCoreFrameResult(
        KysChess::BattleSceneBattleAdapter::BattleFrameApplyContext& bundle,
        const KysChess::Battle::BattleFrameResult& frameResult,
        const KysChess::BattleSceneBattleAdapter::BattleActionFrameAdapterContext& actionContext,
        const KysChess::BattleSceneBattleAdapter::BattleMovementPhysicsFrameAdapterContext& movementPhysicsContext);
    void applyLegacyBattleFrameResult(const SceneBattleFrameResult& result);
    void playCorePresentationFrame();

    void updateEnemyTopDebuffState();
    int getSharedBleedMaxStacks(Role* source) const;
    bool attackCanHitInvincible(Role* role) const;
    Magic* commitAutoUltimate(Role* role, bool consumeMP);
    void playAutoUltimateReady(Role* role);
    void queueCoreAttackSpawn(KysChess::Battle::BattleAttackSpawnRequest request);
    void initializeBattleRuntimeSession();
    KysChess::Battle::BattleRuntimeState& battleRuntime();
    const KysChess::Battle::BattleRuntimeState& battleRuntime() const;
    virtual int calRolePic(Role* r, int style, int frame) override;

    int calculateHitMagicBaseDamage(Role* attacker, Role* defender, Magic* magic);
    void initializeBattleRuntimeStaticState();
    void populateCoreHitState(
        KysChess::BattleSceneBattleAdapter::BattleFrameApplyContext& bundle,
        KysChess::Battle::BattleFrameScratch& scratch);
    void appendCoreHitInputsForAttack(KysChess::BattleSceneBattleAdapter::BattleFrameApplyContext& bundle,
                                      KysChess::Battle::BattleFrameScratch& scratch,
                                      int attackId,
                                      const KysChess::Battle::BattleAttackState& attackState);


    void runPositionSwapLoop();
    void runListBasedSwap();
    bool isManualCameraEnabled() const;
    int getBattleStepsThisRender();
    void advanceBattleFrame();
    void handleManualCameraInput(const EngineEvent& e);
    void focusCameraOn(const Pointf& focusPoint, int zoomFrames);
    void updateAutoCamera();
    void clampCameraCenter();
    void applyCoreStatusState(const KysChess::BattleSceneBattleAdapter::BattleFrameApplyContext& bundle);
    void initializeCoreDamageState();
    void applyCoreDamageTransactions(
        const KysChess::BattleSceneBattleAdapter::BattleFrameApplyContext& bundle,
        const std::vector<KysChess::Battle::BattleHitResolutionResult>& hitResults);
    void applyCoreTeamEffectState(const KysChess::BattleSceneBattleAdapter::BattleFrameApplyContext& bundle);
    void applyCoreFrameApplications(
        const KysChess::BattleSceneBattleAdapter::BattleFrameApplyContext& bundle,
        const KysChess::Battle::BattleFrameApplications& applications);
    KysChess::BattleSceneBattleAdapter::BattleActionFrameAdapterContext makeBattleActionFrameAdapterContext();
    void populateCoreMovementWorld();
    KysChess::Battle::BattleMovementConfig makeCoreMovementConfig() const;
    KysChess::Battle::BattleUnitState makeCoreMovementUnit(
        Role* role);
    KysChess::Battle::BattlePresentationSnapshot makePresentationSnapshot() const;
    void beginPresentationFrame();
    void publishPresentationFrame();
    Color calculateHurtFlashColor(const Role* r, const Color& base_color) const;
    void recordFloatingTextPresentation(Role* role, const std::string& text, Color color, int size = 12, int type = 0);
    void recordRoleEffectPresentation(Role* role, int eftId, int totalFrames = 0);
    void recordDamageNumberPresentation(Role* role, int damage, Color color, int baseSize = 15);
    void recordBattleStatusLog(Role* source, Role* target, const std::string& text);
public:
    BattleTracker& getTracker() { return tracker_; }
    const std::deque<Role>& getFriendsObj() const { return friends_obj_; }
    const std::deque<Role>& getEnemiesObj() const { return enemies_obj_; }
    void setEnemyStars(const std::vector<int>& stars) { enemy_stars_ = stars; }
    void setTeammateWeapons(const std::vector<int>& weapons) { teammate_weapons_ = weapons; }
    void setTeammateArmors(const std::vector<int>& armors) { teammate_armors_ = armors; }
    void setEnemyWeapons(const std::vector<int>& weapons) { enemy_weapons_ = weapons; }
    void setEnemyArmors(const std::vector<int>& armors) { enemy_armors_ = armors; }
    void setCloneSpawnPositions(const std::vector<std::pair<int, int>>& positions) { clone_spawn_positions_ = positions; }
    void setCountFightsWon(bool countFightsWon) { count_fights_won_ = countFightsWon; }

    static int getOperationType(int attackAreaType);
    static const char* getOperationTypeName(int operationType);

protected:
    bool use_whole_scene_ = false;
    KysChess::ChessRoleSave& roleSave_;
    KysChess::ChessProgress& progress_;
    KysChess::ChessManager& chessManager_;
    BattleTracker tracker_;
    Role* swapSelected_ = nullptr;
    bool positionSwapActive_ = false;
    std::set<Role*> ultHitRoles_;    // roles hit by ultimate this frame
    std::set<Role*> criticalHitRoles_;
    std::optional<KysChess::Battle::BattleRuntimeSession> battle_session_;
    std::vector<int> enemy_stars_;
    std::vector<int> teammate_weapons_;
    std::vector<int> teammate_armors_;
    std::vector<int> enemy_weapons_;
    std::vector<int> enemy_armors_;
    std::vector<std::pair<int, int>> clone_spawn_positions_;
    std::unordered_map<int, int> hurt_flash_timers_;
    int next_shared_hit_group_id_ = 1;
    bool manual_camera_dragging_ = false;
    double previous_refresh_interval_ = 0.0;
    int battle_frame_ = 0;
    bool half_speed_step_on_next_render_ = true;
    Pointf camera_target_;
    int close_up_total_ = 0;
    bool count_fights_won_ = true;
    int core_movement_frame_ = -1;
    KysChess::Battle::BattlePresentationRecorder presentation_recorder_;
    BattleScenePresentationPlayer presentation_player_;
    KysChess::Battle::BattlePresentationFrame last_presentation_frame_;
};
