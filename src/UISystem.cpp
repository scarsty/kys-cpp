#include "UISystem.h"
#include "UISave.h"
#include "Event.h"

UISystem::UISystem()
{
    title_ = new MenuText();
    title_->setStrings({ "×x™n", "´æ™n", "ëxé_" });
    title_->setFontSize(24);
    title_->arrange(100, 50, 64, 0);
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
        //¶Áµµ
        auto ui_save = new UISave();
        ui_save->setMode(0);
        ui_save->setFontSize(22);
        result_ = ui_save->runAtPosition(400, 100);
        delete ui_save;
    }
    else if (title_->getResult() == 1)
    {
        auto ui_save = new UISave();
        ui_save->setMode(1);
        ui_save->setFontSize(22);
        result_ = ui_save->runAtPosition(464, 100);
        delete ui_save;
    }
    else if (title_->getResult() == 2)
    {
        result_ = askExit();
    }
    title_->setResult(-1);
}

int UISystem::askExit()
{
    static bool asking = false;
    if (!asking)
    {
        asking = true;
        auto menu = new MenuText();
        menu->setStrings({ "ëxé_ß[‘ò", "·µ»Øé_î^", "ÎÒücåeÁË" });
        menu->setFontSize(50);
        menu->arrange(0, 0, 0, 100);
        int r = menu->runAtPosition(528, 100);
        if (r == 0)
        {
            exitAll();
            Event::getInstance()->forceExit();
            return 0;
        }
        else if (r == 1)
        {
            exitAll(1);
            Event::getInstance()->forceExit();
            return 0;
        }
        asking = false;
    }
    return -1;
}
