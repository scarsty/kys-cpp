#pragma once
#include "Scene.h"
#include "Cloud.h"
#include "Types.h"

class MainScene : public Scene
{
private:
    static MainScene main_map_;
    MainScene();
    ~MainScene();

public:
    static MainScene* getIntance() { return &main_map_; }

    static const int COORD_COUNT = MAINMAP_COORD_COUNT;
    MapSquare earth_layer_ , surface_layer_, building_layer_, build_x_layer_, build_y_layer_, entrance_layer_;
    bool data_readed_ = false;

    void divide2(MapSquare& m);

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
    virtual void onEntrance() override;
    virtual void onExit() override;

    void tryWalk(int x, int y, Towards t);
    //void cloudMove();
    void getEntrance();

    virtual bool isBuilding(int x, int y);
    bool isWater(int x, int y);
    virtual bool isOutLine(int x, int y);

    virtual bool isOutScreen(int x, int y) override;
    virtual bool canWalk(int x, int y) override;

    bool checkEntrance(int x, int y);    //主地图主要是检测入口
};
