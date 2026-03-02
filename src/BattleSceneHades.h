#pragma once
#include "BattleSceneAct.h"
#include "BattleStatsView.h"
#include "Head.h"
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
    void runListBasedSwap();

public:
    BattleTracker& getTracker() { return tracker_; }
    const std::deque<Role>& getFriendsObj() const { return friends_obj_; }
    const std::deque<Role>& getEnemiesObj() const { return enemies_obj_; }
    void setEnemyStars(const std::vector<int>& stars) { enemy_stars_ = stars; }

protected:
    BattleTracker tracker_;
    Role* swapSelected_ = nullptr;
    bool positionSwapActive_ = false;
    std::set<Role*> ultHitRoles_;    // roles hit by ultimate this frame
    std::set<Role*> ultCasters_;     // roles that chose ultimate skill
    std::vector<int> enemy_stars_;
};
