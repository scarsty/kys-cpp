#pragma once
#include "BattleSceneAct.h"
#include "Head.h"
#include "UIKeyConfig.h"
#include <deque>
#include <unordered_map>

class BattleSceneSekiro : public BattleSceneAct
{
public:
    BattleSceneSekiro();
    virtual void draw() override;
    virtual void dealEvent(BP_Event& e) override;     //战场主循环
    virtual void dealEvent2(BP_Event& e) override;    //用于停止自动
    virtual void onEntrance() override;
    virtual void onExit() override;

    virtual void backRun() override {}

    virtual void backRun1();
    void Action(Role* r);
    void AI(Role* r);

    virtual void onPressedCancel() override {}

    virtual int checkResult() override;
    virtual void setRoleInitState(Role* r) override;

    void renderExtraRoleInfo(Role* r, double x, double y);
    Role* findNearestEnemy(int team, Pointf p);
    Role* findFarthestEnemy(int team, Pointf p);
    int calCast(int act_type, int operation_type, Role* r);
    int calCoolDown(int act_type, int operation_type, Role* r);

    void defaultMagicEffect(AttackEffect& ae, Role* r);
    virtual int calRolePic(Role* r, int style, int frame) override;

protected:
    const double MAX_POSTURE = 100;

    int sword_light_ = 0;
    BP_Color sword_light_color_ = { 255, 255, 255, 255 };
    int switch_magic_ = 0;

    int easy_block_ = 0;
};

//暂时设计：
//身上可以携带4个物品（或者干脆不要）
//战斗中可以按键切换当前武功（例如十字键上下）
//AI初步设计为兜圈，寻机突进攻击，高手的攻击会快一些
//我方可以看破或者格挡，增加敌人的架势条（使用体力）
