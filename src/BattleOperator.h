#pragma once
#include "Element.h"
#include "Types.h"

//因为战斗场景的操作分为多种情况，写在原处比较麻烦，故单独列出一类用以响应事件
class BattleOperator : public Element
{
public:
    BattleOperator();
    ~BattleOperator();

    int* select_x_ = nullptr, *select_y_ = nullptr;
    MapSquare* select_layer_ = nullptr, *effect_layer_ = nullptr;

    Role* role_ = nullptr;
    Magic* magic_ = nullptr;

    void setRoleAndMagic(Role* r, Magic* m = nullptr) { role_ = r; magic_ = m; }
    void dealEvent(BP_Event& e) override;

    int mode_ = Move;
    enum
    {
        Other = -1,
        Move,
        Action,
    };
    void setMode(int m) { mode_ = m; }
    int getMode() { return mode_; }

    Element* battle_scene_ = nullptr;
    void setBattleScene(Element* element) { battle_scene_ = element; }

};

