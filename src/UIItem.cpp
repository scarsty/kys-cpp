#include "UIItem.h"
#include "Save.h"

UIItem::UIItem()
{
    item_buttons_.resize(21);
    for (int i = 0; i < item_buttons_.size(); i++)
    {
        auto b = new Button();
        item_buttons_[i] = b;
        b->setPosition(i % 7 * 85 + 40, i / 7 * 85 + 100);
        //b->setTexture("item", Save::getInstance()->getItemByBagIndex(i)->ID);
        addChild(b);
    }
    title_ = new MenuText();
    title_->setStrings({ "劇情", "藥品", "兵甲", "拳經", "劍譜", "刀錄", "奇門", "心法" });
    title_->setFontSize(25);
    title_->arrange(0, 50, 65, 0);

    addChild(title_);
}

UIItem::~UIItem()
{
}
