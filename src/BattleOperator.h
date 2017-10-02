#pragma once
#include "Element.h"

class BattleOperator :
    public Element
{
public:
    BattleOperator();
    ~BattleOperator();

    Element* battle_scene_;  //这个是指向BattleScene的指针，看一下是不是只操作前两层就够了，要直接用要dynamic_cast

    void setBattleScene(Element* b) { battle_scene_ = b; }

};

