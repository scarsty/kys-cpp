#pragma once
#include "BattleScene.h"
#include "BattleMap.h"
#include "Head.h"
#include "Save.h"




//j轻攻击，i重攻击，k闪身
//每场战斗可以选择从4种武学中选择轻重
//硬直，判定范围，威力，消耗体力，在不攻击的时候可以回复体力

class BattleSceneHades : public BattleScene
{
public:
    BattleSceneHades();
    BattleSceneHades(int id);
    virtual ~BattleSceneHades();
    void setID(int id);

    //继承自基类的函数
    virtual void draw() override;
    virtual void dealEvent(BP_Event& e) override;     //战场主循环
    virtual void dealEvent2(BP_Event& e) override;    //用于停止自动
    virtual void onEntrance() override;
    virtual void onExit() override;
    virtual void backRun() override;

protected:
    int man_x1_ = 64 * TILE_W, man_y1_ = 64 * TILE_H;

    Point getPositionOnWholeEarth(int x, int y)
    {
        Point p;
        p.x = -y * TILE_W + x * TILE_W + COORD_COUNT * TILE_W;
        p.y = y * TILE_H + x * TILE_H;
        return p;
    }

    Point getPositionOnRender(int x, int y, int view_x, int view_y)
    {
        Point p;
        x = x - view_x;
        y = y - view_y;
        p.x = -y * TILE_W + x * TILE_W + render_center_x_;
        p.y = y * TILE_H + x * TILE_H + render_center_y_;
        return p;
    }

    Point posWholeEarthTo45(int mouse_x1, int mouse_y1)
    {
        mouse_x1 -= COORD_COUNT * TILE_W;
        Point p;
        p.x = ((mouse_x1) / TILE_W + (mouse_y1) / TILE_H) / 2;
        p.y = ((-mouse_x1) / TILE_W + (mouse_y1) / TILE_H) / 2;
        return p;
    }

    bool is_running_ = false;   //主角是否在跑动
    Role* role_;    //主角

};

