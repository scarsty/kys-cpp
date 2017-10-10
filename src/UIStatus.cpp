#include "UIStatus.h"
#include "Font.h"
#include "others/libconvert.h"
#include "Save.h"
#include "GameUtil.h"
#include "TeamMenu.h"
#include "ShowRoleDifference.h"

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
        button_medcine_->setVisible(false);
        button_detoxification_->setVisible(false);
        button_leave_->setVisible(false);
        return;
    }
    TextureManager::getInstance()->renderTexture("head", role_->HeadID, x_ + 10, y_ + 20);

    auto font = Font::getInstance();
    BP_Color color = { 255, 255, 255, 255 };
    int font_size = 22;

    int x, y;

    x = x_ + 200;
    y = y_ + 50;
    font->draw(convert::formatString("%s", role_->Name), 30, x - 10, y, color);
    font->draw(convert::formatString("等級%5d", role_->Level), font_size, x, y + 50, color);
    font->draw(convert::formatString("經驗%5d", role_->Exp), font_size, x, y + 75, color);

    std::string str = "升級 ----";
    int exp_up = GameUtil::getLevelUpExp(role_->Level);
    if (exp_up != INT_MAX)
    {
        str = convert::formatString("升級%5d", exp_up);
    }
    font->draw(str, font_size, x, y + 100, color);
    font->draw(convert::formatString("生命%5d/%5d", role_->HP, role_->MaxHP), font_size, x + 155, y + 50, color);
    font->draw(convert::formatString("內力%5d/%5d", role_->MP, role_->MaxMP), font_size, x + 155, y + 75, color);
    font->draw(convert::formatString("體力%5d/%5d", role_->PhysicalPower, 100), font_size, x + 155, y + 100, color);

    x = x_ + 20;
    y = y_ + 200;

    font->draw(convert::formatString("攻擊%5d", role_->Attack), font_size, x, y, color);
    font->draw(convert::formatString("防禦%5d", role_->Defence), font_size, x + 200, y, color);
    font->draw(convert::formatString("輕功%5d", role_->Speed), font_size, x + 400, y, color);

    font->draw(convert::formatString("醫療%5d", role_->Medcine), font_size, x, y + 25, color);
    font->draw(convert::formatString("解毒%5d", role_->Detoxification), font_size, x + 200, y + 25, color);
    font->draw(convert::formatString("用毒%5d", role_->UsePoison), font_size, x + 400, y + 25, color);

    x = x_ + 20;
    y = y_ + 270;
    font->draw("技能", 25, x - 10, y, color);
    font->draw(convert::formatString("拳掌%5d", role_->Fist), font_size, x, y + 30, color);
    font->draw(convert::formatString("御劍%5d", role_->Sword), font_size, x, y + 55, color);
    font->draw(convert::formatString("耍刀%5d", role_->Knife), font_size, x, y + 80, color);
    font->draw(convert::formatString("特殊%5d", role_->Unusual), font_size, x, y + 105, color);
    font->draw(convert::formatString("暗器%5d", role_->HiddenWeapon), font_size, x, y + 130, color);

    x = x_ + 220;
    y = y_ + 270;
    font->draw("武學", 25, x - 10, y, color);
    for (int i = 0; i < 10; i++)
    {
        auto magic = Save::getInstance()->getRoleLearnedMagic(role_, i);
        std::string str = "__________";
        if (magic)
        {
            str = convert::formatString("%-12s%3d", magic->Name, role_->getRoleShowLearnedMagicLevel(i));
        }
        int x1 = x + i % 2 * 200;
        int y1 = y + 30 + i / 2 * 25;
        font->draw(str, font_size, x1, y1, color);
    }

    x = x_ + 420;
    y = y_ + 445;
    font->draw("修煉", 25, x - 10, y, color);
    auto book = Save::getInstance()->getItem(role_->PracticeItem);
    if (book)
    {
        TextureManager::getInstance()->renderTexture("item", book->ID, x, y + 30);
        font->draw(convert::formatString("%s", book->Name), font_size, x + 90, y + 30, color);
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
    font->draw("武器", 25, x - 10, y, color);
    auto equip1 = Save::getInstance()->getItem(role_->Equip1);
    if (equip1)
    {
        TextureManager::getInstance()->renderTexture("item", equip1->ID, x, y + 30);
        font->draw(convert::formatString("%s", equip1->Name), font_size, x + 90, y + 30, color);
        font->draw(convert::formatString("攻擊%+d", equip1->AddAttack), 18, x + 90, y + 55, color);
        font->draw(convert::formatString("防禦%+d", equip1->AddDefence), 18, x + 90, y + 75, color);
        font->draw(convert::formatString("輕功%+d", equip1->AddSpeed), 18, x + 90, y + 95, color);
    }

    x = x_ + 220;
    y = y_ + 445;
    font->draw("防具", 25, x - 10, y, color);
    auto equip2 = Save::getInstance()->getItem(role_->Equip2);
    if (equip2)
    {
        TextureManager::getInstance()->renderTexture("item", equip2->ID, x, y + 30);
        font->draw(convert::formatString("%s", equip2->Name), font_size, x + 90, y + 30, color);
        font->draw(convert::formatString("攻擊%+d", equip2->AddAttack), 18, x + 90, y + 55, color);
        font->draw(convert::formatString("防禦%+d", equip2->AddDefence), 18, x + 90, y + 75, color);
        font->draw(convert::formatString("輕功%+d", equip2->AddSpeed), 18, x + 90, y + 95, color);
    }
}

void UIStatus::dealEvent(BP_Event& e)
{
}

void UIStatus::pressedOK()
{
    if (role_ == nullptr) { return; }

    if (button_leave_->getState() == Press)
    {

    }
    else if (button_medcine_->getState() == Press)
    {
        auto team_menu = new TeamMenu();
        team_menu->run();
        auto role = team_menu->getRole();
        delete team_menu;
        if (role)
        {
            GameUtil::medcine(role_, role);
            Role r = *role;
            auto df = new ShowRoleDifference(&r, role);
            df->run();
            delete df;
        }
    }
    else if (button_detoxification_->getState() == Press)
    {
        auto team_menu = new TeamMenu();
        team_menu->run();
        auto role = team_menu->getRole();
        delete team_menu;
        if (role)
        {
            GameUtil::detoxification(role_, role);
            Role r = *role;
            auto df = new ShowRoleDifference(&r, role);
            df->run();
            delete df;
        }
    }
}
