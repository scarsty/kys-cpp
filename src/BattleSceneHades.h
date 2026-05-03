#pragma once
#include "battle/BattleCastSystem.h"
#include "battle/BattleCore.h"
#include "battle/BattleComboTriggerSystem.h"
#include "battle/BattleDamageSystem.h"
#include "battle/BattleMovement.h"
#include "battle/BattlePresentation.h"
#include "BattleSceneAct.h"
#include "BattleStatsView.h"
#include "BattleScenePresentationPlayer.h"
#include "ChessManager.h"
#include "ChessProgress.h"
#include "ChessRoleSave.h"
#include "Head.h"
#include <deque>
#include <map>
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
    void Action(Role* r);
    void AI(Role* r);
    virtual void onPressedCancel() override;

protected:
    struct MovementRuntime;
    void renderExtraRoleInfo(Role* r, double x, double y);
    //int calHurt(Role* r0, Role* r1);
    virtual int checkResult() override;
    virtual void setRoleInitState(Role* r) override;
    bool advanceBattleFrameBeforeDamage();
    void applyPendingPreResolvedDamageFrame();
    void advanceBattleFrameAfterDamage();
    Role* findNearestEnemy(int team, Pointf p);
    Role* findFarthestEnemy(int team, Pointf p);
    int calCast(int act_type, int operation_type, Role* r);
    int calCoolDown(int act_type, int operation_type, Role* r);

    void applyLegacyMagicHitTransaction(const KysChess::Battle::BattleAttackEvent& event, Role* r);
    void queuePreResolvedHpDamage(Role* source,
                                  Role* target,
                                  int damage,
                                  bool critical,
                                  bool ultimate,
                                  bool executed,
                                  const std::string& skillName = "",
                                  const std::string& detailText = "",
                                  Color damageColor = {},
                                  int damageTextSize = 0);
    int pendingPreResolvedHpDamage(Role* target) const;
    KysChess::Battle::BattleDamageTransactionResult applyAcceptedHitSideEffectTransaction(
        Role* source,
        Role* target,
        KysChess::Battle::BattleDamageRequest request);
    template<typename Cmp> Magic* selectMagic(Role* r, Cmp cmp);
    void queueCoreSkillAttackSpawn(Role* r, Magic* magic, bool isUltimate, int operationType = -1);
    Magic* triggerAutoUltimate(Role* r, bool consumeMP);
    int getUltimateExtraProjectileCount(Role* r);
    void spawnAreaImpactProjectiles(Role* attacker,
                                    Role* origin,
                                    int areaSize,
                                    int eftId,
                                    int damage,
                                    int stunFrames = 0,
                                    Role* trackedTarget = nullptr,
                                    int maxTargets = 0);
    void spawnTrackingProjectileSpread(const KysChess::Battle::BattleAttackEvent& prototype,
                                       int projectileCount,
                                       int initialFrame = 0,
                                       int frameStep = 0,
                                       int randomFrameRange = 0);
    void spawnUltimateExtraProjectiles(const KysChess::Battle::BattleAttackEvent& prototype, int extraCount);
    void spawnHitExtraProjectiles(const KysChess::Battle::BattleAttackEvent& prototype, int extraCount, Role* target);
    void spawnExtraProjectiles(
        const KysChess::Battle::BattleAttackEvent& prototype,
        int extraCount,
        const char* logLabel,
        Role* target);
    void spawnSpiralBleedProjectiles(Role* attacker, int bleedStacks, int projectileCount = 6);
    void spawnNearbyTrackingProjectiles(const KysChess::Battle::BattleAttackEvent& prototype,
                                        Role* centerTarget,
                                        int rangePixels,
                                        int damagePct = 40);
    void refreshEnemyTopDebuffs();
    void applyTeamHeal(Role* source, int flatHeal, int pctHeal, const char* reason);
    void applyTeamMP(Role* source, int amount, const char* reason);
    void applyTeamShield(Role* source, int amount, const char* reason, bool refreshOnly);
    int getSharedBleedMaxStacks(Role* source) const;
    void applyTempAttackBuff(Role* role,
                             KysChess::RoleComboState& state,
                             int attackBonus,
                             int durationFrames,
                             const std::string& reason);
    void triggerShieldBreakEffects(Role* role, KysChess::RoleComboState& state);
    bool isLastAliveInTeam(Role* role) const;
    bool attackCanHitInvincible(Role* role) const;
    void applyComboFrameRuntimeEvent(
        Role* role,
        KysChess::RoleComboState& state,
        const KysChess::Battle::BattleComboFrameRuntimeEvent& event,
        std::map<int, KysChess::RoleComboState>& comboStates);
    void attachProjectileBouncePrime(KysChess::Battle::BattleAttackSpawnRequest& request);
    void queueCoreAttackSpawn(KysChess::Battle::BattleAttackSpawnRequest request);
    virtual int calRolePic(Role* r, int style, int frame) override;

    int resolveLegacyMagicBaseDamage(Role* attacker, Role* defender, Magic* magic);


    void makeSpecialMagicEffect();
    void runPositionSwapLoop();
    void runListBasedSwap();
    bool isManualCameraEnabled() const;
    int getBattleStepsThisRender();
    void advanceBattleFrame();
    void handleManualCameraInput(const EngineEvent& e);
    void focusCameraOn(const Pointf& focusPoint, int zoomFrames);
    void updateAutoCamera();
    void clampCameraCenter();
    void prepareCoreMovementDecisions();
    KysChess::Battle::BattleFrameState makeCoreFrameState(
        KysChess::Battle::BattleWorldState world);
    void populateCoreStatusState(KysChess::Battle::BattleFrameState& frameState);
    void applyCoreStatusState(const KysChess::Battle::BattleFrameState& frameState);
    void handleCoreStatusEvents(const std::vector<KysChess::Battle::BattleStatusEvent>& events);
    KysChess::Battle::BattleWorldState makeNoOpCoreWorld() const;
    KysChess::Battle::BattleFrameState makeCoreResultFrameState();
    int resolveCoreBattleResult();
    KysChess::Battle::BattleMovementConfig makeCoreMovementConfig() const;
    KysChess::Battle::BattleUnitState makeCoreMovementUnit(Role* role, const MovementRuntime* movementRuntime);
    void applyCoreMovementSnapshot(const KysChess::Battle::BattleTickResult& result, const std::map<int, Role*>& rolesByBattleId);
    KysChess::Battle::BattlePresentationSnapshot makePresentationSnapshot() const;
    void beginPresentationFrame();
    void publishPresentationFrame();
    Color calculateHurtFlashColor(const Role* r, const Color& base_color) const;
    void addFloatingText(Role* role, const std::string& text, Color color, int size = 12, int type = 0);
    void addRoleEffect(Role* role, int eftId, int totalFrames = 0);
    void addDamageNumber(Role* role, int damage, Color color, int baseSize = 15);
    void logBattleDamage(Role* source, Role* target, int amount, const std::string& skillName = "", const std::string& detailText = "");
    void logBattleHeal(Role* source, Role* target, int amount, const std::string& reason = "");
    void logBattleStatus(Role* source, Role* target, const std::string& text);
    int getHitExtraProjectileCount(Role* r);
    bool roleForcesRangedMagic(Role* role) const;
    int getForcedRangedMinSelectDistance(Role* role) const;
    int getProjectileSpeedMultiplierPct(Role* role) const;

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
    std::set<Role*> ultCasters_;     // roles that chose ultimate skill
    std::vector<int> enemy_stars_;
    std::vector<int> teammate_weapons_;
    std::vector<int> teammate_armors_;
    std::vector<int> enemy_weapons_;
    std::vector<int> enemy_armors_;
    std::vector<std::pair<int, int>> clone_spawn_positions_;
    std::unordered_map<int, int> hurt_flash_timers_;
    std::unordered_map<int, std::set<int>> shared_hit_group_targets_;
    std::unordered_map<Role*, KysChess::Battle::BattleCastResult> pending_cast_results_;
    KysChess::Battle::BattleAttackWorld active_attack_world_;
    std::vector<KysChess::Battle::BattleAttackSpawnRequest> pending_core_attack_spawns_;
    int next_shared_hit_group_id_ = 1;
    bool manual_camera_dragging_ = false;
    double previous_refresh_interval_ = 0.0;
    int battle_frame_ = 0;
    bool half_speed_step_on_next_render_ = true;
    Pointf camera_target_;
    int close_up_total_ = 0;
    bool count_fights_won_ = true;
    int core_movement_frame_ = -1;
    std::unordered_map<Role*, KysChess::Battle::MovementDecision> core_movement_decisions_;
    KysChess::Battle::BattlePresentationRecorder presentation_recorder_;
    BattleScenePresentationPlayer presentation_player_;
    KysChess::Battle::BattlePresentationFrame last_presentation_frame_;

    struct PendingPreResolvedHpDamage {
        Role* source = nullptr;
        Role* target = nullptr;
        int damage = 0;
        bool critical = false;
        bool ultimate = false;
        bool executed = false;
        std::string skillName;
        std::string detailText;
        Color damageColor;
        int damageTextSize = 0;
    };
    std::vector<PendingPreResolvedHpDamage> pending_pre_resolved_hp_damage_;

    struct MovementRuntime {
        int core_assigned_slot = 0;
        int core_slot_switch_cooldown = 0;
        int movement_dash_frames = 0;
        int movement_dash_cooldown = 0;
        int movement_dash_spread_frames = 0;
    };
    std::unordered_map<Role*, MovementRuntime> movement_runtime_;
};

template<typename Cmp>
Magic* BattleSceneHades::selectMagic(Role* r, Cmp cmp)
{
    auto v = r->getLearnedMagics();
    if (v.empty()) return nullptr;
    Magic* chosen = v[0];
    double power = r->getMagicPower(v[0]);
    for (size_t i = 1; i < v.size(); ++i)
    {
        double p = r->getMagicPower(v[i]);
        if (cmp(p, power))
        {
            power = p;
            chosen = v[i];
        }
    }
    return chosen;
}
