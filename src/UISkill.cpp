#include "UISkill.h"
#include "Font.h"

UISkill::UISkill()
{
}

UISkill::~UISkill()
{
}

void UISkill::draw()
{
    for (int i = 0; i < 10; i++)
    {
        auto magic = role_->getLearnedMagic(i);
        if (magic)
        {
            Font::getInstance()->draw(magic->Name, 20, 300, 30 * i, { 255, 255, 255, 255 });
        }
    }
}
