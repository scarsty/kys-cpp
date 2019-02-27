#pragma once
#include "Element.h"
#include "Menu.h"

class UISystem : public Element
{
public:
    UISystem();
    ~UISystem();

    MenuText title_;

    virtual void onPressedOK() override;
    virtual void onPressedCancel() override { exitWithResult(-1); }

    static int askExit(int mode = 0);
};

