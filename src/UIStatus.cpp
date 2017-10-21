#include "UIStatus.h"
#include "Font.h"
#include "others/libconvert.h"
#include "Save.h"
#include "GameUtil.h"
#include "TeamMenu.h"
#include "ShowRoleDifference.h"
#include "Event.h"

UIStatus::UIStatus()
{
    button_medcine_ = new Button();
    button_medcine_->setText("醫療");
    addChild(button_medcine_, 350, 55);

    button_detoxification_ = new Button();
    button_detoxification_->setText("解毒");
    addChild(button_detoxification_, 400, 55);

    button_leave_ = new Button();
    button_leave_->setText("離隊");
    addChild(button_leave_, 450, 55);
}

UIStatus::~UIStatus()
{
}

void UIStatus::draw()
{
    if (role_ == nullptr || !show_button_)
    {
        button_medcine_->setVisible(false);
        button_detoxification_->setVisible(false);
        button_leave_->setVisible(false);
    }

    if (role_)
    {
        if (show_button_)
        {
            button_medcine_->setVisible(role_->Medcine > 0);
            button_detoxification_->setVisible(role_->Detoxification > 0);
            button_leave_->setVisible(role_->ID != 0);
        }
    }
    else
    {
        return;
    }
    TextureManager::getInstance()->renderTexture("head", role_->HeadID, x_ + 10, y_ + 20);

    auto font = Font::getInstance();
    BP_Color color = { 255, 255, 255, 255 };
	BP_Color color_name = { 249, 249, 199, 255 };
	BP_Color color_ability1 = { 249, 146, 16, 255 };
	BP_Color color_ability2 = { 236, 200, 40, 255 };
	BP_Color color_red = { 216, 20, 24, 255 };
	BP_Color color_magic = { 236, 200, 40, 255 };
	BP_Color color_magic_level1 = { 253, 101, 101, 255 };
	BP_Color color_magic_level2 = { 208, 152, 208, 255 };
	BP_Color color_magic_empty = { 236, 200, 40, 76 };
	BP_Color color_equip = { 165, 28, 218, 255 };
	
    int font_size = 22;

    int x, y;

    x = x_ + 200;
    y = y_ + 50;
    font->draw(convert::formatString("%s", role_->Name), 30, x - 10, y, color_name);
	font->draw("等級", font_size, x, y + 50, color_ability1);
    font->draw(convert::formatString("%5d", role_->Level), font_size, x + 66, y + 50, color_ability2);
	font->draw("經驗", font_size, x, y + 75, color_ability1);
	font->draw(convert::formatString("%5d", role_->Exp), font_size, x + 66, y + 75, color_ability2);

    std::string str = "";
	font->draw("升級", font_size, x, y + 100, color_ability1);

    int exp_up = GameUtil::getLevelUpExp(role_->Level);
	if (exp_up != INT_MAX)
	{
		str = convert::formatString("%7d", exp_up);
	}
	else
	{
		str = "------";
	}
    font->draw(str, font_size, x+55, y + 100, color_ability2);
	font->draw("生命", font_size, x + 175, y + 50, color_ability1);
	font->draw(convert::formatString("%5d/", role_->HP), font_size, x + 219, y + 50, color_ability2);
	font->draw(convert::formatString("%5d", role_->MaxHP), font_size, x + 285, y + 50, color_ability2);
	font->draw("內力", font_size, x + 175, y + 75, color_ability1);
	font->draw(convert::formatString("%5d/", role_->MP), font_size, x + 219, y + 75, color_ability2);
	font->draw(convert::formatString("%5d", role_->MaxMP), font_size, x + 285, y + 75, color_ability2);
	font->draw("體力", font_size, x + 175, y + 100, color_ability1);
	font->draw(convert::formatString("%5d/", role_->PhysicalPower), font_size, x + 219, y + 100, color_ability2);
	font->draw(convert::formatString("%5d", 100), font_size, x + 285, y + 100, color_ability2);


    x = x_ + 20;
    y = y_ + 200;

	font->draw("攻擊", font_size, x, y, color);
    font->draw(convert::formatString("%5d", role_->Attack), font_size, x + 44, y, color_ability2);
	font->draw("防禦", font_size, x + 200, y, color);
    font->draw(convert::formatString("%5d", role_->Defence), font_size, x + 244, y, color_ability2);
	font->draw("輕功", font_size, x + 400, y, color);
    font->draw(convert::formatString("%5d", role_->Speed), font_size, x + 444, y, color_ability2);

	font->draw("醫療", font_size, x, y + 25, color);
	font->draw(convert::formatString("%5d", role_->Medcine), font_size, x + 44, y + 25, color_ability2);
	font->draw("解毒", font_size, x + 200, y + 25, color);
	font->draw(convert::formatString("%5d", role_->Detoxification), font_size, x + 244, y + 25, color_ability2);
	font->draw("用毒", font_size, x + 400, y + 25, color);
	font->draw(convert::formatString("%5d", role_->UsePoison), font_size, x + 444, y + 25, color_ability2);


    x = x_ + 20;
    y = y_ + 270;
    font->draw("技能", 25, x - 10, y, color_red);

	font->draw("拳掌", font_size, x, y + 30, color);
	font->draw(convert::formatString("%5d", role_->Fist), font_size, x + 44, y + 30, color_ability2);
	font->draw("御劍", font_size, x, y + 55, color);
	font->draw(convert::formatString("%5d", role_->Sword), font_size, x + 44, y + 55, color_ability2);
	font->draw("耍刀", font_size, x, y + 80, color);
	font->draw(convert::formatString("%5d", role_->Knife), font_size, x + 44, y + 80, color_ability2);
	font->draw("特殊", font_size, x, y + 105, color);
	font->draw(convert::formatString("%5d", role_->Unusual), font_size, x + 44, y + 105, color_ability2);
	font->draw("暗器", font_size, x, y + 130, color);
	font->draw(convert::formatString("%5d", role_->HiddenWeapon), font_size, x + 44, y + 130, color_ability2);

    x = x_ + 220;
    y = y_ + 270;
    font->draw("武學", 25, x - 10, y, color_red);
    for (int i = 0; i < 10; i++)
    {
        auto magic = Save::getInstance()->getRoleLearnedMagic(role_, i);
        std::string str = "__________";
        if (magic)
        {
			int x1 = x + i % 2 * 200;
			int y1 = y + 30 + i / 2 * 25;
			
			str = convert::formatString("%s", magic->Name);
			font->draw(str, font_size, x1, y1, color_magic);
            str = convert::formatString("%3d", role_->getRoleShowLearnedMagicLevel(i));
			font->draw(str, font_size, x1+100, y1, role_->getRoleShowLearnedMagicLevel(i)>9 ? color_magic_level1 : color_magic_level2);
        }
		else
		{
			int x1 = x + i % 2 * 200;
			int y1 = y + 30 + i / 2 * 25;
			font->draw("__________", font_size, x1, y1, color_magic_empty);
		}

    }

    x = x_ + 420;
    y = y_ + 445;
    font->draw("修煉", 25, x - 10, y, color_red);
    auto book = Save::getInstance()->getItem(role_->PracticeItem);
    if (book)
    {
        TextureManager::getInstance()->renderTexture("item", book->ID, x, y + 30);
        font->draw(convert::formatString("%s", book->Name), font_size, x + 90, y + 30, color_magic);
        font->draw(convert::formatString("經驗%5d", role_->ExpForItem), 18, x + 90, y + 55, color);
        std::string str = "升級 ----";
        int exp_up = GameUtil::getFinishedExpForItem(role_, book);
        if (exp_up != INT_MAX)
        {
            str = convert::formatString("升級%5d", exp_up);
        }
        font->draw(str, 18, x + 90, y + 75, color);
    }

    x = x_ + 20;
    y = y_ + 445;
    font->draw("武器", 25, x - 10, y, color_red);
    auto equip0 = Save::getInstance()->getItem(role_->Equip0);
    if (equip0)
    {
        TextureManager::getInstance()->renderTexture("item", equip0->ID, x, y + 30);
        font->draw(convert::formatString("%s", equip0->Name), font_size, x + 90, y + 30, color_magic);
		font->draw("攻擊", 18, x + 90, y + 55, color);
		font->draw(convert::formatString("%+d", equip0->AddAttack), 18, x + 126, y + 75, color_magic_level2);
		font->draw("防禦", 18, x + 90, y + 75, color);
		font->draw(convert::formatString("%+d", equip0->AddDefence), 18, x + 126, y + 95, color_magic_level2);
		font->draw("輕功", 18, x + 90, y + 95, color);
		font->draw(convert::formatString("%+d", equip0->AddSpeed), 18, x + 126, y + 55, color_magic_level2);

    }

    x = x_ + 220;
    y = y_ + 445;
    font->draw("防具", 25, x - 10, y, color_red);
    auto equip1 = Save::getInstance()->getItem(role_->Equip1);
    if (equip1)
    {
        TextureManager::getInstance()->renderTexture("item", equip1->ID, x, y + 30);
        font->draw(convert::formatString("%s", equip1->Name), font_size, x + 90, y + 30, color_magic);
		font->draw("攻擊", 18, x + 90, y + 55, color);
		font->draw(convert::formatString("%+d", equip1->AddAttack), 18, x + 126, y + 75, color_magic_level2);
		font->draw("防禦", 18, x + 90, y + 75, color);
		font->draw(convert::formatString("%+d", equip1->AddDefence), 18, x + 126, y + 95, color_magic_level2);
		font->draw("輕功", 18, x + 90, y + 95, color);
		font->draw(convert::formatString("%+d", equip1->AddSpeed), 18, x + 126, y + 55, color_magic_level2);
    }
}

void UIStatus::dealEvent(BP_Event& e)
{
}

void UIStatus::onPressedOK()
{
    if (role_ == nullptr) { return; }

    if (button_leave_->getState() == Press)
    {
        Event::getInstance()->callLeaveEvent(role_);
        role_ = nullptr;
    }
    else if (button_medcine_->getState() == Press)
    {
        auto team_menu = new TeamMenu();
        team_menu->setText(convert::formatString("%s要為誰醫療", role_->Name));
        team_menu->run();
        auto role = team_menu->getRole();
        delete team_menu;
        if (role)
        {
            Role r = *role;
            GameUtil::medcine(role_, role);
            auto df = new ShowRoleDifference(&r, role);
            df->setText(convert::formatString("%s接受%s醫療", role->Name, role_->Name));
            df->run();
            delete df;
        }
    }
    else if (button_detoxification_->getState() == Press)
    {
        auto team_menu = new TeamMenu();
        team_menu->setText(convert::formatString("%s要為誰解毒", role_->Name));
        team_menu->run();
        auto role = team_menu->getRole();
        delete team_menu;
        if (role)
        {
            Role r = *role;
            GameUtil::detoxification(role_, role);
            auto df = new ShowRoleDifference(&r, role);
            df->setText(convert::formatString("%s接受%s解毒", role->Name, role_->Name));
            df->run();
            delete df;
        }
    }
}
