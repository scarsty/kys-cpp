#pragma once
#include "Scene.h"
#include "Save.h"

class SubScene : public Scene
{
private:
    SubScene();
public:
    SubScene(int id);
    virtual ~SubScene();

    int view_x_ = 0, view_y_ = 0;

    const int COORD_COUNT = SUBMAP_COORD_COUNT;

    int const MAN_PIC_0 = 2501;            //初始场景主角图偏移量
    int const MAN_PIC_COUNT = 7;           //单向主角图张数
    int submap_id_;   //场景号

    SubMapInfo* submap_info_;

    int exit_music_;

public:
    SubMapInfo* getMapInfo() { return submap_info_; }

    void changeExitMusic(int m) { exit_music_ = m; }

    void setID(int id) { submap_id_ = id; }

    //注意视角和主角的位置可能不一样
    void setViewPosition(int x, int y) { view_x_ = x; view_y_ = y; }
    void setPosition(int x, int y) { setManPosition(x, y); setViewPosition(x, y); }

    virtual void draw() override;
    virtual void dealEvent(BP_Event& e) override;
    virtual void backRun() override;
    virtual void onEntrance() override;
    virtual void onExit() override;

    void tryWalk(int x, int y);

    //第三个参数为朝向
    bool checkEvent(int x, int y, int tw = Towards_None, int item_id = -1);

    //第一类事件，主动触发
    bool checkEvent1(int x, int y, int tw) { return checkEvent(x, y, tw, -1); }
    //第二类事件，物品触发
    bool checkEvent2(int x, int y, int tw, int item_id) { return checkEvent(x, y, tw, item_id); }
    //第三类事件，经过触发
    bool checkEvent3(int x, int y) { return checkEvent(x, y, Towards_None, -1); }

    virtual bool isBuilding(int x, int y);
    virtual bool isOutLine(int x, int y);
    bool isWater(int x, int y);
    bool isCanPassEvent(int x, int y);
    bool isCannotPassEvent(int x, int y);
    bool isFall(int x, int y);
    bool isExit(int x, int y);

    virtual bool isOutScreen(int x, int y) override;
    virtual bool canWalk(int x, int y) override;

    void getMousePosition(int _x, int _y);

    Point getPositionOnWholeEarth(int x, int y);

    int calManPic() { return MAN_PIC_0 + Scene::towards_ * MAN_PIC_COUNT + step_; }  //每个方向的第一张是静止图
    void forceManPic(int pic) { man_pic_ = -pic; }
};

