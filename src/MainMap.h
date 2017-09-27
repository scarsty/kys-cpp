#pragma once
#include "Scene.h"
#include "Cloud.h"
#include "Types.h"

class MainMap : public Scene
{
public:
    MainMap();
    ~MainMap();

    static const int MAX_COORD = MAX_MAINMAP_COORD;
    typedef int16_t MapArray[MAX_COORD * MAX_COORD];
    static MapArray Earth_, Surface_, Building_, BuildX_, BuildY_, Entrance_;

    static int16_t& Earth(int x, int y) { return Earth_[x + y * MAX_COORD]; }
    static int16_t& Surface(int x, int y) { return Surface_[x + y * MAX_COORD]; }
    static int16_t& Building(int x, int y) { return Building_[x + y * MAX_COORD]; }
    static int16_t& BuildX(int x, int y) { return BuildX_[x + y * MAX_COORD]; }
    static int16_t& BuildY(int x, int y) { return BuildY_[x + y * MAX_COORD]; }

    static void divide2(MapArray& m);

    static bool data_readed_;

    int16_t& man_x_, &man_y_;
    int step_ = 0;
    int man_pic_;
    int rest_time_ = 0;                     //停止操作的时间
    
    int MAN_PIC_0 = 2501;                   //初始主角图偏移量
    int MAN_PIC_COUNT = 7;                  //单向主角图张数
    int REST_PIC_0 = 2529;                  //主角休息图偏移量
    int REST_PIC_COUNT = 6;                 //单向休息图张数
    int SHIP_PIC_0 = 3715;                  //初始主角图偏移量
    int SHIP_PIC_COUNT = 4;                 //单向主角图张数
    int BEGIN_REST_TIME = 200;              //开始休息的时间
    int REST_INTERVAL = 15;                 //休息图切换间隔

    //todo: 休息未完成

    Cloud::CloudTowards cloud_towards = Cloud::Left;
    std::vector<Cloud*> cloud_vector_;

    void draw() override;
    virtual void dealEvent(BP_Event& e) override;
    virtual void entrance() override;
    virtual void exit() override;

    void tryWalk(int x, int y, Towards t);
    //void cloudMove();
    void getEntrance();
    
    virtual bool isBuilding(int x, int y);
    bool isWater(int x, int y);
    virtual bool isOutLine(int x, int y);
    
    virtual bool isOutScreen(int x, int y) override;
    virtual bool canWalk(int x, int y) override;
    
    bool checkEntrance(int x, int y);
    void getMousePosition(int _x, int _y);
};
