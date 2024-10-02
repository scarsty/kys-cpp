#pragma once
#include "BattleSceneAct.h"
#include "Head.h"
#include "UIKeyConfig.h"
#include <deque>
#include <unordered_map>

//j轻攻击，i重攻击，m闪身
//每场战斗可以选择从4种武学中选择轻重
//硬直，判定范围，威力，消耗体力，在不攻击的时候可以回复体力

class BattleSceneHades : public BattleSceneAct
{
public:
    BattleSceneHades();
    BattleSceneHades(int id);
    virtual ~BattleSceneHades();

    //继承自基类的函数
    virtual void draw() override;
    virtual void dealEvent(BP_Event& e) override;     //战场主循环
    virtual void dealEvent2(BP_Event& e) override;    //用于停止自动
    virtual void onEntrance() override;
    virtual void onExit() override;

    virtual void backRun() override {}

    virtual void backRun1();
    void Action(Role* r);
    void AI(Role* r);
    virtual void onPressedCancel() override;

protected:
    std::shared_ptr<Menu> menu_;
    std::vector<std::shared_ptr<Button>> button_magics_;
    std::shared_ptr<Button> button_item_;
    std::shared_ptr<TextBox> show_auto_;

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

    virtual int calMagicHurt(Role* r1, Role* r2, Magic* magic, int dis = -1)
    {
        //计算武学对单人的伤害
        //注意原公式中距离为1是无衰减的
        //即时战斗，降低每一次攻击的威力，否则打得太快了
        return BattleScene::calMagicHurt(r1, r2, magic, 1) / 20.0;
    }

    void makeSpecialMagicEffect();
};
