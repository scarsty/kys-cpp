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

    //BP_Texture* earth_texture_ = nullptr;

    //int man_x_, man_y_;        //注意在子类中采用引用是因为存档设计和某些指令的问题，不表示推荐这种方案
    static Towards towards_;     //朝向，共用一个即可

    int screen_center_x_ = 0;
    int screen_center_y_ = 0;
    const int TILE_W = 18;  //小图块大小X
    const int TILE_H = 9;   //小图块大小Y

    //确定视野使用
    int view_width_region_ = 0;
    int view_sum_region_ = 0;

    int total_step_ = 0;        //键盘走路的计数
    BP_Keycode pre_pressed_;    //键盘走路的上次按键

    int step_ = 0;
    int man_pic_;
    void setManPic(int pic) { man_pic_ = pic; }

    void checkWalk(int x, int y, BP_Event& e);   //一些公共部分，未完成

    Point getPositionOnScreen(int x, int y, int CenterX, int CenterY);

    Towards CallFace(int x1, int y1, int x2, int y2);

    int calBlockTurn(int x, int y, int layer) { return 4 * (128 * (x + y) + x) + layer; }

    void getTowardsFromKey(BP_Keycode key);
    //获取面向一格的坐标
    void getTowardsPosition(int x0, int y0, Towards tw, int* x1, int* y1);

    virtual bool canWalk(int x, int y) { return false; }
    virtual bool isOutScreen(int x, int y) { return false; }

    std::vector<PointEx> way_que_;  //栈(路径栈)
    int minStep;                                                        //起点(Mx,My),终点(Fx,Fy),最少移动次数minStep
    int mouse_x_, mouse_y_;
    void stopFindWay() { way_que_.clear();/*while (!way_que_.empty()) { way_que_.pop(); }*/ }
    void FindWay(int Mx, int My, int Fx, int Fy);

};


