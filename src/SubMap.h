#pragma once
#include "Scene.h"
#include "Save.h"

class SubMap : public Scene
{
private:
    SubMap();
public:
    SubMap(int num);
    virtual ~SubMap();

    int view_x_ = 0, view_y_ = 0;
    int16_t& man_x_, &man_y_;
    int man_pic_;
    int step_ = 0;

    const int MAX_COORD = MAX_SUBMAP_COORD;

    int const MAN_PIC_0 = 2501;            //初始场景主角图偏移量
    int const MAN_PIC_COUNT = 7;           //单向主角图张数
    int scene_id_;   //场景号

    SubMapRecord* record_;

public:
    SubMapRecord* getRecord() { return record_; }

    void setSceneNum(int num) { scene_id_ = num; }

    void setPosition(int x, int y) { setManPosition(x, y); setViewPosition(x, y); }
    //注意视角和主角的位置可能不一样
    void setManPosition(int x, int y) { man_x_ = x; man_y_ = y; }
    void setViewPosition(int x, int y) { view_x_ = x; view_y_ = y; }

    virtual void draw() override;
    virtual void dealEvent(BP_Event& e) override;
    virtual void backRun() override;
    virtual void entrance() override;
    virtual void exit() override;

    void tryWalk(int x, int y, Towards t);

    bool checkEvent1(int x, int y, Towards t);
    bool checkEvent2(int x, int y, Towards t, Item* item);
    bool checkEvent3(int x, int y);

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

    //以下事件用
    static SubMapRecord* current_submap_record_;
    static int current_submap_id_;
    //static int current_item_id_;
    static int event_x_, event_y_;

    static SubMapRecord* getCurrentSubMapRecord() { return current_submap_record_; }
    static int getCurrentSubMapID() { return current_submap_id_; }
    //static int getCurrentItemID() { return current_item_id_; }

    Point getPositionOnWholeEarth(int x,int y);

};

