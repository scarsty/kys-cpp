#pragma once
#include "Menu.h"
#include "Head.h"

class BattleMenu : public MenuText
{
public:
    BattleMenu();
    virtual ~BattleMenu();

    virtual void onEntrance() override;

    Role* role = nullptr;
    void setRole(Role* r) { role = r; }
    void runAsRole(Role* r);
};



