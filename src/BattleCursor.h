#pragma once
#include "Element.h"
#include "Types.h"
#include "Head.h"

class BattleScene;

//因为战斗场景的操作分为多种情况，写在原处比较麻烦，故单独列出一类用以操作光标
//注意，AI选择目标的行为也在这里面
class BattleCursor : public Element
{
public:
    BattleCursor();
    ~BattleCursor();

    int* select_x_ = nullptr, *select_y_ = nullptr;
    MapSquare* select_layer_ = nullptr, *effect_layer_ = nullptr;

    Role* role_ = nullptr;
    Magic* magic_ = nullptr;
    int level_index_ = 0;
    void setRoleAndMagic(Role* r, Magic* m = nullptr, int l = 0);

    Head* head_selected_;
    void setHead(Head* h) { head_selected_ = h; }
    Head* getHead() { return head_selected_; }

    int mode_ = Move;
    enum
    {
        Other = -1,
        Move,
        Action,
        Check,
    };
    void setMode(int m) { mode_ = m; }
    int getMode() { return mode_; }

    BattleScene* battle_scene_ = nullptr;
    void setBattleScene(BattleScene* b) { battle_scene_ = b; }

    void dealEvent(BP_Event& e) override;

    void dealMoveEvent(BP_Event& e);
    void dealActionEvent(BP_Event& e);

    void onEntrance() override;

    virtual void pressedOK() override { exitWithResult(0); }
    virtual void pressedCancel() override { exitWithResult(-1); }

};

