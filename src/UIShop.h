#pragma once
#include "Element.h"
#include "Types.h"

class UIShop : public Element
{
public:
    UIShop();
    ~UIShop();

    Shop* shop_;

    virtual void pressedOK() override { exitWithResult(0); }
    virtual void pressedCancel() override { exitWithResult(-1); }
    virtual void draw() override;
};

