#include "UIItem.h"
#include "Save.h"

UIItem::UIItem()
{
    item_buttons_.resize(15);
    for (int i = 0; i < item_buttons_.size(); i++)
    {
        auto b = new Button();
        item_buttons_[i] = b;
        b->setPosition(i % 5 * 85, 100 + i / 5 * 85);
        b->setTexture("item", Save::getInstance()->getItemByBagIndex(i)->ID);
        addChild(b);
    }
}

UIItem::~UIItem()
{
}
