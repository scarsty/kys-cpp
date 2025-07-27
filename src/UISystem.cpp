#include "UISystem.h"
#include "Engine.h"
#include "Event.h"
#include "GameUtil.h"
#include "Script.h"
#include "UIKeyConfig.h"
#include "UISave.h"

UISystem::UISystem()
{
    title_ = std::make_shared<MenuText>();
    title_->setStrings({ "讀取進度", "保存進度", "我的代碼", "戰鬥模式", "離開遊戲" });
    title_->setFontSize(24);
    title_->arrange(100, 50, 120, 0);
    title_->setLRStyle(1);
    addChild(title_);
}

UISystem::~UISystem()
{
}

void UISystem::onPressedOK()
{
    result_ = -1;
    int x = 400 + 120 * title_->getResult();
    if (title_->getResult() == 0)
    {
        //读档
        auto ui_load = std::make_shared<UISave>();
        ui_load->setMode(0);
        ui_load->setFontSize(22);
        result_ = ui_load->runAtPosition(x, 100);
    }
    else if (title_->getResult() == 1)
    {
        //存档
        auto ui_save = std::make_shared<UISave>();
        ui_save->setMode(1);
        ui_save->setFontSize(22);
        result_ = ui_save->runAtPosition(x, 100);
    }
    else if (title_->getResult() == 2)
    {
        Script::getInstance()->runScript(GameUtil::PATH() + "script/1.lua");
    }
    else if (title_->getResult() == 3)
    {
        std::vector<std::string> modes = { "回合制", "半即時回合制", "黑帝斯", "隻狼" };
        int mode = GameUtil::getInstance()->getInt("game", "battle_mode", 0);
        if (mode >= 0 && mode < modes.size())
        {
            modes[mode] += "（當前）";
        }
        auto menu = std::make_shared<MenuText>();
        menu->setStrings(modes);
        menu->setFontSize(24);
        menu->arrange(0, 0, 0, 40);
        result_ = menu->runAtPosition(x, 100);
        if (result_ >= 0)
        {
            GameUtil::getInstance()->setKey("game", "battle_mode", std::to_string(result_));
            auto text = std::make_shared<TextBox>();
            mode = GameUtil::getInstance()->getInt("game", "battle_mode", 0);
            text->setText("戰鬥模式已設置為：" + modes[mode]);
            text->setFontSize(24);
            text->runAtPosition(x, 100);
        }
    }
    else if (title_->getResult() == title_->getChildCount() - 1)
    {
        result_ = askExit();
    }
    title_->setResult(-1);
}

int UISystem::askExit(int mode)
{
    static bool asking = false;
    int ret = -1;
    if (!asking)
    {
        asking = true;
        auto menu = std::make_shared<MenuText>();
        menu->setStrings({ "我點錯了", "離開遊戲", "返回開頭" });
        menu->setFontSize(24);
        int x = 880, y = 100;
        if (mode == 1)
        {
            //menu->getChild(1)->setVisible(false);
            x = Engine::getInstance()->getStartWindowWidth() - 150;
            y = 20;
            menu->setIsDark(1);
        }
        menu->arrange(0, 0, 0, 40);
        int r = menu->runAtPosition(x, y);
        if (r == 1)
        {
            exitAll();
            Event::getInstance()->forceExit();
            ret = 0;
            Engine::getInstance()->destroy();
            exit(0);    //爱咋咋地
        }
        else if (r == 2)
        {
            exitAll(1);
            Event::getInstance()->forceExit();
            ret = 0;
        }
        asking = false;
    }
    return ret;
}
