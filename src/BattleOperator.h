#pragma once
#include "Element.h"
#include "Types.h"

//因为战斗场景的操作分为多种情况，写在原处比较麻烦，故单独列出一类用以操作选择行动目标
class BattleOperator : public Element
{
public:
    BattleOperator();
    ~BattleOperator();

    int* select_x_ = nullptr, *select_y_ = nullptr;
    MapSquare* select_layer_ = nullptr, *effect_layer_ = nullptr;

    Role* role_ = nullptr;
    Magic* magic_ = nullptr;
    int level_index_ = 0;
    void setRoleAndMagic(Role* r, Magic* m = nullptr, int l = 0) { role_ = r; magic_ = m; level_index_ = l; }
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


    void dealMoveEvent(BP_Event& e);
    void dealActionEvent(BP_Event& e);

    void onEntrance() override;

};

