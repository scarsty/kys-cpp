#include "UISkill.h"
#include "Font.h"
#include "Save.h"
#include "TextureManager.h"
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

    auto font = Font::getInstance();
    BP_Color color = { 255, 255, 255, 255 };
    const int font_size = 20;
    font->draw("技能", font_size, x_ + 50, y_ + 60, color);
    font->draw(convert::formatString("%s%7d", "醫療", role_->Medcine), font_size, x_ + 50, y_ + 100, color);
    font->draw(convert::formatString("%s%7d", "解毒", role_->Detoxification), font_size, x_ + 300, y_ + 100, color);

    font->draw("武學", font_size, x_ + 50, y_ + 150, color);
    for (int i = 0; i < 10; i++)
    {
        auto magic = role_->getLearnedMagic(i);
        if (magic)
        {

            auto str = convert::formatString("%s%7d", magic->Name, role_->getLearnedMagicLevel(i));
            if (i < 5)
            {
                font->draw(str, font_size, x_ + 50, y_ + 190 + 40 * i, color);
            }
            else
            {
                font->draw(str, font_size, x_ + 200, y_ + 190 + 40 * (i - 5), color);
            }
        }
    }

    font->draw("修煉物品", font_size, x_ + 50, y_ + 420, color);
    auto* book = Save::getInstance()->getItem(role_->PracticeBook);
    if (book != nullptr)
    {
        TextureManager::getInstance()->renderTexture("item", role_->PracticeBook, x_ + 60, y_ + 60);
        font->draw(convert::formatString("%s", book->Name), font_size, x_ + 50, y_ + 460, color);
        font->draw(convert::formatString("%d/%d", role_->ExpForBook, book->NeedExp), font_size, x_ + 50, y_ + 500, color);
    }
}
