#pragma once
#include "Menu.h"
#include "Head.h"
#include "UIItem.h"

class BattleScene;

//注意，AI选择行动的行为也在这里面
class BattleActionMenu : public MenuText
{
public:
    BattleActionMenu();
    virtual ~BattleActionMenu();

    virtual void onEntrance() override;

    Role* role_ = nullptr;
    void setRole(Role* r) { role_ = r; }
    int runAsRole(Role* r) { setRole(r); return run(); }

    BattleScene* battle_scene_ = nullptr;
    void setBattleScene(BattleScene* b) { battle_scene_ = b; }

    void dealEvent(BP_Event& e) override;

    int autoSelect(Role* role);

    MapSquare* distance_layer_;

    void calDistanceLayer(int x, int y, int max_step = 64);

};

class BattleMagicMenu : public MenuText
{
public:
    BattleMagicMenu() {}
    virtual ~BattleMagicMenu() {}

    virtual void onEntrance() override;

    void dealEvent(BP_Event& e) override;

    Role* role_ = nullptr;
    Magic* magic_ = nullptr;
    void setRole(Role* r) { role_ = r; }
    int runAsRole(Role* r) { setRole(r); return run(); }

    Magic* getMagic() { return magic_; }

    virtual void pressedOK() override { exitWithResult(0); }
    virtual void pressedCancel() override { magic_ = nullptr; exitWithResult(-1); }

};


class BattleItemMenu : public UIItem
{
public:
    BattleItemMenu();
    virtual ~BattleItemMenu() {}
private:
    Role* role_ = nullptr;
public:
    void setRole(Role* r) { role_ = r; }
    Role* getRole() { return role_; }
};


