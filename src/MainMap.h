#pragma once
#include <stack>
#include <queue>
#include "Scene.h"
#include "Save.h"

class Cloud : public Base
{
public:
    Cloud() {}
    virtual ~Cloud() {}

    enum CloudTowards
    {
        Left = 0,
        Right = 1,
        Up = 2,
        Down = 3,
    };

    Point position_;
    float speed_;

    const int max_X = 17280;
    const int max_Y = 8640;
    enum { num_style_ = 10 };
    int num_;

    BP_Color color_;
    uint8_t alpha_;

    void initRand();
    void setPositionOnScreen(int x, int y, int Center_X, int Center_Y);
    virtual void draw() override;
};

class MainMap : public Scene
{
public:
    MainMap();
    ~MainMap();

    static const int max_coord = 479;
    static const int coord_count_ = max_coord + 1;

    static MapArray *Earth, *Surface, *Building, *BuildX, *BuildY, *Entrance;
    static bool _readed;

    int16_t& man_x_, &man_y_ ;
    int step_ = 0;
    int man_pic_;
    int rest_time_ = 0;                    //停止操作的时间
    int const man_pic0_ = 2501;        //初始主角图偏移量
    int const num_manPic = 7;              //单向主角图张数
    int const offset_restPic = 2529;       //主角休息图偏移量
    int const num_restPic = 6;             //单向休息图张数
    int const offset_shipPic = 3715;       //初始主角图偏移量
    int const num_shipPic = 4;             //单向主角图张数
    int const begin_restTime = 200;        //开始休息的时间
    int const each_pictrueTime = 15;       //休息图切换间隔

    //todo: 休息未完成

    Cloud::CloudTowards cloud_towards = Cloud::Left;
    std::vector<Cloud*> cloud_vector_;

    void draw() override;
    virtual void dealEvent(BP_Event& e) override;
    virtual void enter() override;
    virtual void exit() override;

    void walk(int x, int y, Towards t);
    //void cloudMove();
    void getEntrance();
    virtual bool isBuilding(int x, int y);
    bool isWater(int x, int y);
    virtual bool isOutLine(int x, int y);
    bool isOutScreen(int x, int y);
    void getMousePosition(int _x, int _y);
    bool canWalk(int x, int y);
    bool checkEntrance(int x, int y);
    virtual void FindWay(int Mx, int My, int Fx, int Fy);
    void stopFindWay();
    std::stack<Point> wayQue;  //栈(路径栈)
};
