#pragma once
#include "BattleScene.h"
#include "Font.h"
#include "Head.h"
#include "Save.h"
#include "UIKeyConfig.h"
#include <deque>
#include <unordered_map>

class BattleSceneSekiro : public BattleScene
{
public:
    BattleSceneSekiro();
    void setID(int id);
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

    bool canWalk45(int x, int y)
    {
        if (isOutLine(x, y) || isBuilding(x, y) || isWater(x, y))
        {
            return false;
        }
        else
        {
            return true;
        }
    }

    bool canWalk90(int x, int y)
    {
        auto p = pos90To45(x, y);
        return canWalk45(p.x, p.y);
    }
    bool canWalk90(Pointf p, Role* r, int dis = -1)
    {
        if (r->Pos.z > 1) { return true; }
        if (dis == -1) { dis = TILE_W; }
        for (auto r1 : battle_roles_)
        {
            if (r1 == r || r1->Dead) { continue; }
            double dis1 = EuclidDis(p, r1->Pos);
            if (dis1 < dis)
            {
                double dis0 = EuclidDis(r->Pos, r1->Pos);
                if (dis0 >= dis1) { return false; }
            }
        }
        auto p45 = pos90To45(p.x, p.y);
        return canWalk45(p45.x, p45.y);
    }
    virtual bool canWalk(int x, int y) override { return canWalk45(x, y); }

    virtual int checkResult() override;
    virtual void setRoleInitState(Role* r) override;

    void renderExtraRoleInfo(Role* r, double x, double y);
    Role* findNearestEnemy(int team, Pointf p);
    Role* findFarthestEnemy(int team, Pointf p);

    void decreaseToZero(int& i)
    {
        if (i > 0) { i--; }
    }
    //void defaultMagicEffect(AttackEffect& ae, Role* r);
    virtual int calRolePic(Role* r, int style, int frame) override;

protected:
    Pointf pos_;    //坐标为俯视，而非在画面的位置，其中y需除以2画在上面
    double gravity_ = -4;
    double friction_ = 0.1;

    std::deque<Role*> enemies_;
    std::deque<Role> enemies_obj_;

    std::vector<std::shared_ptr<Head>> heads_;
    std::vector<std::shared_ptr<Head>> head_boss_;

    bool is_running_ = false;    //主角是否在跑动
    Role* role_ = nullptr;

    UIKeyConfig::Keys keys_;
};


//暂时设计：
//身上可以携带4个物品（或者干脆不要）
//战斗中可以按键切换当前武功（例如十字键上下）
//AI初步设计为兜圈，寻机突进攻击，高手的攻击会快一些
//我方可以看破或者格挡，增加敌人的架势条（使用体力）
