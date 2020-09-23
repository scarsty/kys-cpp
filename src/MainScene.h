#pragma once
#include "Cloud.h"
#include "Object.h"
#include "ParticleExample.h"
#include "Scene.h"
#include "Types.h"

class ParticleWeather : public RunNode, public ParticleExample
{
public:
    //注意这个继承方法比较扯淡，其他时候尽量不要这样用
    virtual void draw() override
    {
        int c = ParticleSystem::draw();
        Engine::getInstance()->resetRenderTimes(Engine::getInstance()->getRenderTimes() + c);
    }
    virtual void setPosition(int x, int y)
    {
        RunNode::setPosition(x, y);
        ParticleSystem::setPosition(x, y);
    }
};

class MainScene : public Scene
{
public:
    MainScene();
    ~MainScene();

public:
    static std::shared_ptr<MainScene> getInstance()
    {
        static std::shared_ptr<MainScene> ms = std::make_shared<MainScene>();
        return ms;
    }

    MapSquare<Object> earth_layer_, surface_layer_, building_layer_;
    MapSquareInt build_x_layer_, build_y_layer_, entrance_layer_;
    bool data_readed_ = false;

    void divide2(MapSquareInt& m1, MapSquare<Object>& m);

    int MAN_PIC_0 = 2501;         //初始主角图偏移量
    int MAN_PIC_COUNT = 7;        //单向主角图张数
    int REST_PIC_0 = 2529;        //主角休息图偏移量
    int REST_PIC_COUNT = 6;       //单向休息图张数
    int SHIP_PIC_0 = 3715;        //初始主角图偏移量
    int SHIP_PIC_COUNT = 4;       //单向主角图张数
    int BEGIN_REST_TIME = 200;    //开始休息的时间
    int REST_INTERVAL = 15;       //休息图切换间隔

    int force_submap_ = -1;
    int force_submap_x_ = -1;
    int force_submap_y_ = -1;
    int force_event_ = -1;

    //todo: 休息未完成

    std::vector<Cloud> cloud_vector_;

    virtual void draw() override;
    virtual void backRun() override;
    virtual void dealEvent(BP_Event& e) override;
    virtual void onEntrance() override;
    virtual void onExit() override;
    virtual void onPressedCancel() override;

    void tryWalk(int x, int y);
    void setEntrance();

    virtual bool isBuilding(int x, int y);
    int isWater(int x, int y);

    virtual bool isOutScreen(int x, int y) override;
    virtual bool canWalk(int x, int y) override;

    bool checkEntrance(int x, int y, bool only_check = false);    //主地图主要是检测入口

    void forceEnterSubScene(int submap_id, int x, int y, int event = -1);    //在下一个事件循环会强制进入某场景，用于开始和读取存档

    bool inNorth() { return man_x_ + man_y_ <= 220; }
    int view_cloud_ = 0;
    int getViewCloud() { return view_cloud_; }

    void setWeather();
    std::shared_ptr<ParticleWeather> getWeather() { return weather_; }
    std::shared_ptr<ParticleWeather> weather_;
};
