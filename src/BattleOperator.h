#pragma once
#include "Base.h"

class BattleOperator :
    public Base
{
public:
    BattleOperator();
    ~BattleOperator();

    Base* battle_scene_;  //这个是指向BattleScene的指针，看一下是不是只操作前两层就够了，要直接用要dynamic_cast

    void setBattleScene(Base* b) { battle_scene_ = b; }

};

