#pragma once
#include "Element.h"
#include "Types.h"
#include "Button.h"

class UIShop : public Element
{
public:
    UIShop();
    ~UIShop();

    Shop* shop_;

    std::vector<int> plan_buy_;
    std::vector<Button*> buttons_;

    Button* button_ok_, *button_cancel_, *button_clear_;

    virtual void draw() override;
    virtual void pressedOK() override;
    virtual void pressedCancel() override { }

    int calNeedMoney();
};

