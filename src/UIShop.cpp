#include "UIShop.h"
#include "Font.h"
#include "others/libconvert.h"
#include "Save.h"
#include "Event.h"

UIShop::UIShop()
{
    x_ = 200;
    y_ = 200;
    plan_buy_.resize(SHOP_ITEM_COUNT, 0);
    for (int i = 0; i < SHOP_ITEM_COUNT; i++)
    {
        auto button_left_ = new Button();
        button_left_->setTexture("title", 104);
        addChild(button_left_, 36 * 12 + 36, 30 + 25 * i);
        buttons_.push_back(button_left_);

        auto button_right_ = new Button();
        button_right_->setTexture("title", 105);
        addChild(button_right_, 36 * 12 + 108, 30 + 25 * i);
        buttons_.push_back(button_right_);
    }
    button_ok_ = new Button();
    button_ok_->setText("確認");
    addChild(button_ok_, 360, 180);
    button_cancel_ = new Button();
    button_cancel_->setText("取消");
    addChild(button_cancel_, 450, 180);
    button_clear_ = new Button();
    button_clear_->setText("清除");
    addChild(button_clear_, 540, 180);
}

UIShop::~UIShop()
{
}

void UIShop::setShopID(int id)
{
    shop_ = Save::getInstance()->getShop(id);
}

void UIShop::draw()
{
    Engine::getInstance()->fillColor({ 0, 0, 0, 192 }, 0, 0, -1, -1);
    int x = x_, y = y_;

    std::string str;
    auto font = Font::getInstance();

    str = convert::formatString("%-12s%8s%8s%8s%8s", "品名", "價格", "存貨", "持有", "計劃");
    font->draw(str, 24, x, y, { 255, 255, 255, 255 });


    for (int i = 0; i < 5; i++)
    {
        auto item = Save::getInstance()->getItem(shop_->ItemID[i]);
        int count = Save::getInstance()->getItemCountInBag(item->ID);
        str = convert::formatString("%-12s%8d%8d%8d%8d", item->Name, shop_->Price[i], shop_->Total[i], count, plan_buy_[i]);
        font->draw(str, 24, x, y + 25 + i * 25, { 255, 255, 255, 255 });
    }

    int need_money = calNeedMoney();
    str = convert::formatString("總計%6d", need_money);
    font->draw(str, 24, x, y + 25 + 6 * 25, { 255, 255, 255, 255 });

    BP_Color c = { 255, 255, 255, 255 };
    int money = Save::getInstance()->getMoneyCountInBag();
    str = convert::formatString("持有銀兩%6d", money);
    if (money < need_money) { c = { 250, 50, 50, 255 }; };
    font->draw(str, 24, x + 144, y + 25 + 6 * 25, c);
}

void UIShop::pressedOK()
{
    for (int i = 0; i < SHOP_ITEM_COUNT * 2; i++)
    {
        if (buttons_[i]->getState() == Press)
        {
            int index = i / 2;
            int lr = i % 2;
            if (lr == 0)
            {
                if (plan_buy_[index] > 0)
                {
                    plan_buy_[index]--;
                }
            }
            else
            {
                if (plan_buy_[index] < shop_->Total[index])
                {
                    plan_buy_[index]++;
                }
            }
        }
    }
    if (button_ok_->getState() == Press)
    {
        if (calNeedMoney() <= Save::getInstance()->getMoneyCountInBag())
        {
            for (int i = 0; i < SHOP_ITEM_COUNT; i++)
            {
                Event::getInstance()->addItemWithoutHint(shop_->ItemID[i], plan_buy_[i]);
                shop_->Total[i] -= plan_buy_[i];
            }
            Event::getInstance()->addItemWithoutHint(MONEY_ITEM_ID, -calNeedMoney());
            exitWithResult(0);
        }
    }
    if (button_cancel_->getState() == Press)
    {
        exitWithResult(-1);
    }
    if (button_clear_->getState() == Press)
    {
        for (int i = 0; i < SHOP_ITEM_COUNT; i++)
        {
            plan_buy_[i] = 0;
        }
    }
}

int UIShop::calNeedMoney()
{
    int need_money = 0;
    for (int i = 0; i < SHOP_ITEM_COUNT; i++)
    {
        need_money += plan_buy_[i] * shop_->Price[i];
    }
    return need_money;
}

