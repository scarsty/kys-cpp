#pragma once
#include "BattleScene.h"
#include "BattleMap.h"
#include "Head.h"
#include "Save.h"

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
    double man_x_ = 32, man_y_ = 32;

    Point getPositionOnWholeEarth(double x, double y)
    {
        auto p = getPositionOnRender(x, y, 0, 0);
        p.x += COORD_COUNT * TILE_W - render_center_x_;
        p.y += 2 * TILE_H - render_center_y_;
        return p;
    }

    Point getPositionOnRender(double x, double y, double view_x, double view_y)
    {
        Point p;
        x = x - view_x;
        y = y - view_y;
        p.x = -y * TILE_W + x * TILE_W + render_center_x_;
        p.y = y * TILE_H + x * TILE_H + render_center_y_;
        return p;
    }

};

