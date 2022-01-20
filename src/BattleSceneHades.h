#pragma once
#include "Scene.h"
#include "BattleMap.h"
#include "Save.h"

class BattleSceneHades : public Scene
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
    int battle_id_ = 0;
    BattleInfo* info_;

    Save* save_;

    std::vector<Role*> battle_roles_;    //所有参战角色

    //地面层，建筑层，选择层（负值为不可选，0和正值为可选），效果层
    MapSquareInt earth_layer_, building_layer_, select_layer_, effect_layer_;
};

