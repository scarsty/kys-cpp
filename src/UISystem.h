#pragma once
#include "RunNode.h"
#include "Menu.h"

class UISystem : public RunNode
{
public:
    UISystem();
    ~UISystem();

    std::shared_ptr<MenuText> title_;

    virtual void onPressedOK() override;
    virtual void onPressedCancel() override { exitWithResult(-1); }

    static int askExit(int mode = 0);
};

