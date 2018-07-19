#pragma once
#include "Scene.h"
#include "Cloud.h"
#include "Types.h"
#include "ParticleExample.h"

class ParticleWeather : public Element, public ParticleExample
{
public:
    //ע������̳з����Ƚϳ���������ʱ������Ҫ������
    virtual void draw() override
    {
        ParticleSystem::draw();
    }
    void setPosition(int x, int y)
    {
        Element::setPosition(x, y);
        ParticleSystem::setPosition(x, y);
    }
};

class MainScene : public Scene
{
private:
    MainScene();
    ~MainScene();

public:
    static MainScene* getInstance()
    {
        static MainScene ms;
        return &ms;
    }

    MapSquareInt earth_layer_, surface_layer_, building_layer_, build_x_layer_, build_y_layer_, entrance_layer_;

    void divide2(MapSquareInt* m);

    int MAN_PIC_0 = 2501;                   //��ʼ����ͼƫ����
    int MAN_PIC_COUNT = 7;                  //��������ͼ����
    int REST_PIC_0 = 2529;                  //������Ϣͼƫ����
    int REST_PIC_COUNT = 6;                 //������Ϣͼ����
    int SHIP_PIC_0 = 3715;                  //��ʼ����ͼƫ����
    int SHIP_PIC_COUNT = 4;                 //��������ͼ����
    int BEGIN_REST_TIME = 200;              //��ʼ��Ϣ��ʱ��
    int REST_INTERVAL = 15;                 //��Ϣͼ�л����

    int force_submap_ = -1;
    int force_submap_x_ = -1;
    int force_submap_y_ = -1;
    int force_event_ = -1;

    //todo: ��Ϣδ���

    Cloud::CloudTowards cloud_towards = Cloud::Left;
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

    bool checkEntrance(int x, int y, bool only_check = false);    //����ͼ��Ҫ�Ǽ�����

    void forceEnterSubScene(int submap_id, int x, int y, int event = -1);    //����һ���¼�ѭ����ǿ�ƽ���ĳ���������ڿ�ʼ�Ͷ�ȡ�浵

    bool inNorth() { return man_x_ + man_y_ <= 220; }
    int view_cloud_ = 0;
    int getViewCloud() { return view_cloud_; }

    void setWeather();
    ParticleWeather* getWeather() { return weather_; }
    ParticleWeather* weather_ = nullptr;
};
