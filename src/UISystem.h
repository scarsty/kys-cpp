#pragma once
#include "RunElement.h"
#include "Menu.h"

class UISystem : public RunElement
{
public:
    UISystem();
    ~UISystem();

    MenuText title_;

    virtual void onPressedOK() override;
    virtual void onPressedCancel() override { exitWithResult(-1); }

    static int askExit(int mode = 0);
};

