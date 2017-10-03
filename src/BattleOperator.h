#pragma once
#include "Element.h"
#include "Types.h"

//因为战斗场景的操作分为多种情况，写在原处比较麻烦，故单独列出一类用以响应事件
class BattleOperator : public Element
{
public:
    BattleOperator();
    ~BattleOperator();

    Element* battle_scene_;  //这个是指向BattleScene的指针，看一下是不是只操作前两层就够了，要直接用要dynamic_cast

    void setBattleScene(Element* b) { battle_scene_ = b; }

    Role* role = nullptr;

    void setRole(Role* r) { role = r; }

};

