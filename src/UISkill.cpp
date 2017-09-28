#include "UISkill.h"
#include "Font.h"
#include "others/libconvert.h"

UISkill::UISkill()
{
}

UISkill::~UISkill()
{
}

void UISkill::draw()
{
    if (role_ == nullptr) { return; }
    for (int i = 0; i < 10; i++)
    {
        auto magic = role_->getLearnedMagic(i);
        if (magic)
        {
            auto str = convert::formatString("%s%7d", magic->Name, role_->getLearnedMagicLevel(i));
            Font::getInstance()->draw(str, 30, 300, 100+40 * i, { 255, 255, 255, 255 });
        }
    }
}
