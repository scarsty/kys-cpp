#pragma once
#include "BattleSceneAct.h"
#include "BattleStatsView.h"
#include "ChessManager.h"
#include "ChessProgress.h"
#include "ChessRoleSave.h"
#include "Head.h"
#include <deque>
#include <set>
#include <unordered_map>

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

    virtual void backRun() override {}

    virtual void backRun1();
    void Action(Role* r);
    void AI(Role* r);
    virtual void onPressedCancel() override;

protected:
    void renderExtraRoleInfo(Role* r, double x, double y);
    //int calHurt(Role* r0, Role* r1);
    virtual int checkResult() override;
    virtual void setRoleInitState(Role* r) override;
    Role* findNearestEnemy(int team, Pointf p);
    Role* findFarthestEnemy(int team, Pointf p);
    Role* findRandomEnemy(int team);
    Role* findWeakestVulnerableEnemy(int team);
    int calCast(int act_type, int operation_type, Role* r);
    int calCoolDown(int act_type, int operation_type, Role* r);

    void defaultMagicEffect(AttackEffect& ae, Role* r);
    void applyScriptedAttackEffect(AttackEffect& ae, Role* r);
    template<typename Cmp> Magic* selectMagic(Role* r, Cmp cmp);
    void createSkillAttackEffect(Role* r, Magic* magic, bool isUltimate, int operationType = -1);
    Magic* triggerAutoUltimate(Role* r, bool consumeMP);
    int getUltimateExtraProjectileCount(Role* r);
    void spawnAreaImpactProjectiles(Role* attacker,
                                    Role* origin,
                                    int areaSize,
                                    int eftId,
                                    int damage,
                                    int stunFrames = 0,
                                    Role* trackedTarget = nullptr);
    void spawnTrackingProjectileSpread(const AttackEffect& prototype,
                                       int projectileCount,
                                       int initialFrame = 0,
                                       int frameStep = 0,
                                       int randomFrameRange = 0);
    void spawnUltimateExtraProjectiles(const AttackEffect& prototype, int extraCount);
    void spawnHitExtraProjectiles(const AttackEffect& prototype, int extraCount, Role* target);
    void spawnExtraProjectiles(const AttackEffect& prototype, int extraCount, const char* logLabel, Role* target);
    void refreshEnemyTopDebuffs();
    void applyTeamHeal(Role* source, int flatHeal, int pctHeal, const char* reason);
    void applyTempAttackBuff(Role* role,
                             KysChess::RoleComboState& state,
                             int attackBonus,
                             int durationFrames,
                             const std::string& reason);
    void tickTempAttackBuffs(Role* role, KysChess::RoleComboState& state);
    void triggerShieldBreakEffects(Role* role, KysChess::RoleComboState& state);
    void collectTriggeredTeamHeal(KysChess::RoleComboState& state,
                                  KysChess::Trigger trigger,
                                  int& flatHeal,
                                  int& pctHeal);
    int getProjectileBounceCount(Role* r) const;
    void primeProjectileBounce(AttackEffect& ae);
    void spawnProjectileBounce(AttackEffect& source, Role* hitTarget);
    virtual int calRolePic(Role* r, int style, int frame) override;

    virtual int calMagicHurt(Role* r1, Role* r2, Magic* magic, int dis = -1) override;


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
    Role* assignFlankTarget(Role* r);
    Color calculateHurtFlashColor(const Role* r, const Color& base_color) const;
    void addFloatingText(Role* role, const std::string& text, Color color, int size = 12, int type = 0);
    void addRoleEffect(Role* role, int eftId, int totalFrames = 0);
    void logBattleDamage(Role* source, Role* target, int amount, const std::string& skillName = "", const std::string& detailText = "");
    void logBattleHeal(Role* source, Role* target, int amount, const std::string& reason = "");
    void logBattleStatus(Role* source, Role* target, const std::string& text);
    int getHitExtraProjectileCount(Role* r);

    std::vector<Point> findPath(Point start45, Point goal45);
    std::vector<Pointf> smoothPath(const std::vector<Point>& path45);

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
    std::set<Role*> ultCasters_;     // roles that chose ultimate skill
    std::vector<int> enemy_stars_;
    std::vector<int> teammate_weapons_;
    std::vector<int> teammate_armors_;
    std::vector<int> enemy_weapons_;
    std::vector<int> enemy_armors_;
    std::vector<std::pair<int, int>> clone_spawn_positions_;
    std::unordered_map<int, int> hurt_flash_timers_;
    std::set<int> execution_popup_roles_;
    bool manual_camera_dragging_ = false;
    double previous_refresh_interval_ = 0.0;
    int battle_frame_ = 0;
    bool half_speed_step_on_next_render_ = true;
    Pointf camera_target_;
    int close_up_total_ = 0;

    struct PathInfo {
        std::vector<Pointf> waypoints;
        int current_waypoint = 0;
        int frames_since_update = 0;
        Role* target = nullptr;
        Point target_cell;
        int frames_following = 0;
        int frames_sliding = 0;
        int frames_stuck = 0;
    };
    std::unordered_map<Role*, PathInfo> paths_;
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
