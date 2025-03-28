﻿#include "UIShop.h"
#include "Event.h"
#include "Font.h"
#include "PotConv.h"
#include "Save.h"
#include "strfunc.h"

UIShop::UIShop()
{
    plan_buy_.resize(SHOP_ITEM_COUNT, 0);
    for (int i = 0; i < SHOP_ITEM_COUNT; i++)
    {
        auto text = std::make_shared<Button>();
        text->setFontSize(24);
        addChild(text, 0, 30 + 25 * i);

        auto button_left = std::make_shared<Button>();
        text->addChild(button_left, 36 * 12 + 36, 5);
        button_left->setTexture("title", 104);
        buttons_.push_back(button_left);

        auto button_right = std::make_shared<Button>();
        text->addChild(button_right, 36 * 12 + 108, 5);
        button_right->setTexture("title", 105);
        buttons_.push_back(button_right);
    }
    button_ok_ = std::make_shared<Button>();
    button_ok_->setText("確認");
    addChild(button_ok_, 0, 190);
    button_cancel_ = std::make_shared<Button>();
    button_cancel_->setText("取消");
    addChild(button_cancel_, 100, 190);
    button_clear_ = std::make_shared<Button>();
    button_clear_->setText("清除");
    addChild(button_clear_, 200, 190);

    setPosition(200, 230);

    //的前面几个是物品项，后面3个是按钮
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
    Engine::getInstance()->fillColor({ 0, 0, 0, 128 }, 0, 0, -1, -1);
    int x = x_, y = y_;

    std::string str;
    auto font = Font::getInstance();

    str = "品名             價格    存貨    持有    計劃";
    font->draw(str, 24, x, y, { 200, 150, 50, 255 });

    for (int i = 0; i < 5; i++)
    {
        auto item = Save::getInstance()->getItem(shop_->ItemID[i]);
        int count = Save::getInstance()->getItemCountInBag(item->ID);
        str = std::string(item->Name) + std::string(abs(12 - Font::getTextDrawSize(item->Name)), ' ');
        str += std::format("{:8}{:8}{:8}{:8}", shop_->Price[i], shop_->Total[i], count, plan_buy_[i]);
        //str = std::format("{:12}{:8}{:8}{:8}{:8}", item->Name, shop_->Price[i], shop_->Total[i], count, plan_buy_[i]);
        //std::string m = PotConv::utf8tocp936((char*)str1.c_str());
        //std::cout << m << "\n";
        ((Button*)(getChild(i).get()))->setText(str);
    }

    int need_money = calNeedMoney();
    str = std::format("總計銀兩{:8}", need_money);
    font->draw(str, 24, 300 + x, y + 25 + 6 * 25, { 255, 255, 255, 255 });

    Color c = { 255, 255, 255, 255 };
    int money = Save::getInstance()->getMoneyCountInBag();
    str = std::format("持有銀兩{:8}", money);
    if (money < need_money)
    {
        c = { 250, 50, 50, 255 };
    }
    font->draw(str, 24, 300 + x, y + 25 + 7 * 25, c);
}

void UIShop::dealEvent(EngineEvent& e)
{
    static int first_press = 0;
    if (e.type == EVENT_KEY_DOWN && (e.key.key == K_LEFT || e.key.key == K_RIGHT) && active_child_ < SHOP_ITEM_COUNT)
    {
        if (first_press == 0 || first_press > 5)    //按一下的时候只走一格
        {
            int index = active_child_;
            if (e.key.key == K_LEFT)
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
        first_press++;
    }
    else
    {
        Menu::dealEvent(e);
    }
    if (e.type == EVENT_KEY_UP)
    {
        first_press = 0;
    }
    //LOG("%d ", first_press);
}

void UIShop::onPressedOK()
{
    for (int i = 0; i < SHOP_ITEM_COUNT * 2; i++)
    {
        if (buttons_[i]->getState() == NodePress)
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
    if (button_ok_->getState() == NodePress)
    {
        if (calNeedMoney() <= Save::getInstance()->getMoneyCountInBag())
        {
            for (int i = 0; i < SHOP_ITEM_COUNT; i++)
            {
                Event::getInstance()->addItemWithoutHint(shop_->ItemID[i], plan_buy_[i]);
                shop_->Total[i] -= plan_buy_[i];
            }
            Event::getInstance()->addItemWithoutHint(Item::MoneyItemID, -calNeedMoney());
            exitWithResult(0);
        }
    }
    if (button_cancel_->getState() == NodePress)
    {
        exitWithResult(-1);
    }
    if (button_clear_->getState() == NodePress)
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
