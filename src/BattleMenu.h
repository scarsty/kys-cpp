#pragma once
#include "Menu.h"
#include "Head.h"

class BattleScene;

//注意，AI选择行动的行为也在这里面
class BattleMenu : public MenuText
{
public:
    BattleMenu();
    virtual ~BattleMenu();

    virtual void onEntrance() override;

    Role* role_ = nullptr;
    void setRole(Role* r) { role_ = r; }
    int runAsRole(Role* r);

    BattleScene* battle_scene_ = nullptr;
    void setBattleScene(BattleScene* b) { battle_scene_ = b; }

    void dealEvent(BP_Event& e) override;

    int autoSelect(Role* r);

    int ai_action_ = 0;
    int ai_move_x_, ai_move_y_;
    int ai_action_x_, ai_action_y_;
    int ai_towards_;
    Magic* ai_magic_ = nullptr;

};



