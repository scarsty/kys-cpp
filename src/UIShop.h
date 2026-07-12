#pragma once
#include "Types.h"
#include "Menu.h"
#include "Button.h"

class UIShop : public Menu
{
public:
    UIShop();
    ~UIShop();

    void setShopID(int id);

    Shop* shop_ = nullptr;

    std::vector<int> plan_buy_;
    std::vector<std::shared_ptr<Button>> buttons_;

    std::shared_ptr<Button> button_ok_, button_cancel_, button_clear_;

    virtual void draw() override;
    virtual void dealEvent(EngineEvent& e) override;
    virtual void onEntrance() override;
    virtual void onPressedOK() override;
    int calNeedMoney();

    DEFAULT_CANCEL_EXIT;

private:
    static constexpr int SHOP_BOX_X = -10;
    static constexpr int SHOP_BOX_Y = -3;
    static constexpr int SHOP_BOX_WIDTH = 620;
    static constexpr int SHOP_BOX_HEIGHT = 270;
    static constexpr int SHOP_MONEY_X = 30;
    static constexpr int SHOP_BUTTON_X = 330;
    static constexpr int SHOP_BUTTON_GAP = 90;
    static constexpr int SHOP_HINT_X = 220;
    static constexpr int SHOP_HINT_Y = 285;
    static constexpr int SHOP_ROW_HEIGHT = 32;
    static constexpr int SHOP_ROW_WIDTH = 600;
    static constexpr int SHOP_PANEL_HEIGHT = 270;
    static constexpr Color SHOP_TEXT_COLOR = { 48, 32, 16, 255 };
    static constexpr Color SHOP_TEXT_ACTIVE_COLOR = { 96, 24, 16, 255 };
    static constexpr Color SHOP_MONEY_WARNING_COLOR = { 180, 32, 24, 255 };
};

