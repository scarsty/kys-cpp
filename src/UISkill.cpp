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

	Font::getInstance()->draw("普通", 30, 350, 60, { 255, 255, 255, 255 });
	Font::getInstance()->draw(convert::formatString("%s%7d","医疗", role_->Medcine), 30, 350, 100, { 255, 255, 255, 255 });
	Font::getInstance()->draw(convert::formatString("%s%7d", "解毒", role_->Detoxification), 30, 600, 100, { 255, 255, 255, 255 });

	Font::getInstance()->draw("武学", 30, 350, 150, { 255, 255, 255, 255 });
    for (int i = 0; i < 10; i++)
    {
        auto magic = role_->getLearnedMagic(i);
        if (magic)
        {
			
            auto str = convert::formatString("%s%7d", magic->Name, role_->getLearnedMagicLevel(i));
			if(i < 5)
				Font::getInstance()->draw(str, 30, 350, 190+40 * i, { 255, 255, 255, 255 });
			else
				Font::getInstance()->draw(str, 30, 500, 190 + 40 * (i - 5), { 255, 255, 255, 255 });
        }
    }

	Font::getInstance()->draw("修炼物品", 30, 350, 420, { 255, 255, 255, 255 });
	auto *save = Save::getInstance();
	auto *book = save->getItem(role_->PracticeBook);
	if (save != nullptr && book != nullptr)
	{
		Font::getInstance()->draw(convert::formatString("%s", book->Name), 30, 350, 460, { 255, 255, 255, 255 });
		Font::getInstance()->draw(convert::formatString("%d", role_->ExpForBook), 30, 350, 500, { 255, 255, 255, 255 });
	}
	

	

}
