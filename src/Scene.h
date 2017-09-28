#pragma once
#include "Base.h"
#include "Point.h"

//主地图，子场景，战斗场景均继承此类
class Scene : public Base
{
public:
    Scene();
    virtual ~Scene();
    virtual void draw() override {}
    virtual void dealEvent(BP_Event& e) override {}

    BP_Texture* earth_texture_ = nullptr;

    static Towards towards_;

    int screen_center_x_ = 0;
    int screen_center_y_ = 0;
    const int SUBMAP_TILE_W = 18;                               //小图块大小X
    const int SUBMAP_TILE_H = 9;                                //小图块大小Y
    const int MAINMAP_TILE_W = 18;                              //地面小图块大小X
    const int MAINMAP_TILE_H = 9;                               //地面小图块大小Y

    //确定视野使用
    int view_width_region_ = 0;
    int view_sum_region_ = 0;

    Point getPositionOnScreen(int x, int y, int CenterX, int CenterY);
    //Point getMapPoint(int x, int y, int CenterX, int CenterY);
    Towards CallFace(int x1, int y1, int x2, int y2);

    int calBlockTurn(int x, int y, int layer)
    {
        return 4 * (128 * (x + y) + x) + layer;
    }

    void getTowardsFromKey(BP_Keycode key)
    {
        switch (key)
        {
        case BPK_LEFT: towards_ = LeftDown; break;
        case BPK_RIGHT: towards_ = RightUp; break;
        case BPK_UP: towards_ = LeftUp; break;
        case BPK_DOWN: towards_ = RightDown; break;
        }
    }
    //获取面向一格的坐标
    void getTowardsPosition(int x0, int y0, Towards tw, int* x1, int* y1)
    {
        if (towards_ == None) { return; }
        *x1 = x0;
        *y1 = y0;
        switch (towards_)
        {
        case LeftDown: (*x1)--; break;
        case RightUp: (*x1)++; break;
        case LeftUp: (*y1)--; break;
        case RightDown: (*y1)++; break;
        }
    }

    virtual bool canWalk(int x, int y) { return false; }
    virtual bool isOutScreen(int x, int y) { return false; }

    std::vector<PointEx> way_que_;  //栈(路径栈)
    int minStep;                                                        //起点(Mx,My),终点(Fx,Fy),最少移动次数minStep
    int mouse_x_, mouse_y_;
    void stopFindWay() { way_que_.clear();/*while (!way_que_.empty()) { way_que_.pop(); }*/ }
    void FindWay(int Mx, int My, int Fx, int Fy);



};


