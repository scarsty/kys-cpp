#include "UISystem.h"
#include "UISave.h"

UISystem::UISystem()
{
    title_ = new MenuText();
    title_->setStrings({ "×x™n", "´æ™n" });
    title_->setFontSize(25);
    title_->arrange(0, 50, 65, 0);
    addChild(title_);
}


UISystem::~UISystem()
{
}

void UISystem::onPressedOK()
{
    if (title_->getResult()==0)
    {
        //¶Áµµ
        auto ui_save = new UISave();
        ui_save->setMode(0);
        ui_save->runAtPosition(400, 200);
        delete ui_save;
    }
    if (title_->getResult() == 1)
    {
        auto ui_save = new UISave();
        ui_save->setMode(1);
        ui_save->runAtPosition(400, 200);
        delete ui_save;
    }
}
