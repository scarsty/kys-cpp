#include "UISystem.h"
#include "UISave.h"
#include "Event.h"
#include "Script.h"
#include "Engine.h"

UISystem::UISystem()
{
    title_ = new MenuText();
    title_->setStrings({ "�xȡ�M��", "�����M��", "�ҵĴ��a", "�x�_�[��" });
    title_->setFontSize(24);
    title_->arrange(100, 50, 120, 0);
    addChild(title_);
}

UISystem::~UISystem()
{
}

void UISystem::onPressedOK()
{
    result_ = -1;
    if (title_->getResult() == 0)
    {
        //����
        UISave ui_save;
        ui_save.setMode(0);
        ui_save.setFontSize(22);
        result_ = ui_save.runAtPosition(400, 100);
    }
    else if (title_->getResult() == 1)
    {
        UISave ui_save;
        ui_save.setMode(1);
        ui_save.setFontSize(22);
        result_ = ui_save.runAtPosition(520, 100);
    }
    else if (title_->getResult() == 2)
    {
        Script::getInstance()->runScript("../game/script/1.lua");
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
        MenuText menu;
        menu.setStrings({ "�x�_�[��", "�����_�^", "���c�e��" });
        menu.setFontSize(24);
        menu.arrange(0, 0, 0, 40);
        int x = 760, y = 100;
        if (mode == 1)
        {
            x = Engine::getInstance()->getWindowsWidth() - 150;
            y = 20;
        }
        int r = menu.runAtPosition(x, y);
        if (r == 0)
        {
            exitAll();
            Event::getInstance()->forceExit();
            ret = 0;
        }
        else if (r == 1)
        {
            exitAll(1);
            Event::getInstance()->forceExit();
            ret = 0;
        }
        asking = false;
    }
    return ret;
}
