#pragma once
#include "Element.h"
#include "Types.h"
#include "Button.h"

class UIShop : public Element
{
public:
    UIShop();
    ~UIShop();

    void setShopID(int id);

    Shop* shop_ = nullptr;

    std::vector<int> plan_buy_;
    std::vector<Button*> buttons_;

    Button* button_ok_, *button_cancel_, *button_clear_;

    virtual void draw() override;
    virtual void onPressedOK() override;
    int calNeedMoney();

    DEFAULT_CANCEL_EXIT;
};

