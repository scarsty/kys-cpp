#pragma once
#include "BattleSceneAct.h"
#include "BattleStatsView.h"
#include "Head.h"
#include "QiEffectRenderer.h"
#include <deque>
#include <set>
#include <unordered_map>

class PositionSwapNode;

class BattleSceneHades : public BattleSceneAct
{
    friend class PositionSwapNode;
public:
    BattleSceneHades();
    BattleSceneHades(int id);
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
    struct DashSnapshot {
        Pointf pos;
        int pic;
    };
    std::unordered_map<int, std::deque<DashSnapshot>> dash_trails_;
    std::unordered_map<int, int> hurt_flash_timers_;
    QiEffect::Renderer qi_renderer_;  // 真气特效渲染器（合并版）

    bool isDashing(const Role* r) const;
    void updateDashTrail(Role* r);
    void clearDashTrail(const Role* r);
    Color calculateHurtFlashColor(const Role* r, const Color& base_color) const;

    // 武功范围地板特效绘制辅助函数
    void drawCircleOutline(SDL_Renderer* renderer, int cx, int cy, float radius,
                          Uint8 r, Uint8 g, Uint8 b, Uint8 alpha);
    void drawCircleFill(SDL_Renderer* renderer, int cx, int cy, float radius,
                       Uint8 r, Uint8 g, Uint8 b, Uint8 alpha);
    void renderAttackAuras();  // 渲染武功范围地板特效（混合方案）

    void renderExtraRoleInfo(Role* r, double x, double y);
    //int calHurt(Role* r0, Role* r1);
    virtual int checkResult() override;
    virtual void setRoleInitState(Role* r) override;
    Role* findNearestEnemy(int team, Pointf p);
    Role* findFarthestEnemy(int team, Pointf p);
    int calCast(int act_type, int operation_type, Role* r);
    int calCoolDown(int act_type, int operation_type, Role* r);

    void defaultMagicEffect(AttackEffect& ae, Role* r);
    virtual int calRolePic(Role* r, int style, int frame) override;

    virtual int calMagicHurt(Role* r1, Role* r2, Magic* magic, int dis = -1) override;


    void makeSpecialMagicEffect();
    void runPositionSwapLoop();

public:
    BattleTracker& getTracker() { return tracker_; }
    const std::deque<Role>& getFriendsObj() const { return friends_obj_; }
    const std::deque<Role>& getEnemiesObj() const { return enemies_obj_; }
    void setEnemyStars(const std::vector<int>& stars) { enemy_stars_ = stars; }

protected:
    BattleTracker tracker_;
    Role* swapSelected_ = nullptr;
    std::set<Role*> ultHitRoles_;    // roles hit by ultimate this frame
    std::set<Role*> ultCasters_;     // roles that chose ultimate skill
    std::vector<int> enemy_stars_;
    bool dash_text_shown_ = false;   // 轻功文字是否已显示

    // DEBUG: 用于空格暂停
    bool debug_paused_ = false;
    std::map<Role*, int> debug_prev_cooldown_;  // 记录每个角色上一帧的 CoolDown

    // DEBUG: 用于绘制攻击范围（辅助线，可简化为简洁显示）
    struct AttackRangeDebug {
        Role* attacker = nullptr;
        Pointf target_pos;
        int select_distance = 0;  // AI决策距离
        Magic* magic = nullptr;   // 使用的武功
        int operation_type = -1;  // 操作类型
        int frame_count = 0;      // 显示帧数
    };
    std::vector<AttackRangeDebug> debug_attack_ranges_;

    // 按攻击方式设计的通用范围特效（点/线/十字/面），与武功动画叠加显示
    struct RangeIndicatorEffect {
        Pointf center;           // 中心（施法者或目标）
        Pointf direction;        // 朝向（线/十字时使用）
        int attack_area_type = 0; // 0点 1线 2十字 3面
        int frame = 0;
        int total_frame = 12;    // 短时显示，避免画面杂乱
        float radius = 0;         // 点/面：半径；线/十字：射程
    };
    std::deque<RangeIndicatorEffect> range_indicator_effects_;

    void spawnRangeIndicatorEffect(Pointf center, Pointf direction, int attack_area_type, float radius_or_reach);
    void drawRangeIndicatorEffects(void* renderer);  // SDL_Renderer*
    void drawAttackRangeSimple(void* renderer, const AttackRangeDebug& ard);
};
