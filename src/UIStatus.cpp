#include "UIStatus.h"
#include "Event.h"
#include "Font.h"
#include "GameUtil.h"
#include "Save.h"
#include "ShowRoleDifference.h"
#include "TeamMenu.h"
#include "convert.h"

UIStatus::UIStatus()
{
    menu_ = std::make_shared<Menu>();
    button_medicine_ = std::make_shared<Button>();
    button_medicine_->setText("醫療");
    menu_->addChild(button_medicine_, 350, 55);
    button_detoxification_ = std::make_shared<Button>();
    button_detoxification_->setText("解毒");
    menu_->addChild(button_detoxification_, 400, 55);
    button_leave_ = std::make_shared<Button>();
    button_leave_->setText("離隊");
    menu_->addChild(button_leave_, 450, 55);

    addChild(menu_);
}

UIStatus::~UIStatus()
{
}

void UIStatus::draw()
{
    if (role_ == nullptr || !show_button_)
    {
        button_medicine_->setVisible(false);
        button_detoxification_->setVisible(false);
        button_leave_->setVisible(false);
    }

    if (role_)
    {
        if (show_button_)
        {
            button_medicine_->setVisible(role_->Medicine > 0);
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
    BP_Color color_white = { 255, 255, 255, 255 };
    BP_Color color_name = { 255, 215, 0, 255 };
    BP_Color color_ability1 = { 255, 250, 205, 255 };
    BP_Color color_ability2 = { 236, 200, 40, 255 };
    BP_Color color_red = { 255, 90, 60, 255 };
    BP_Color color_magic = { 236, 200, 40, 255 };
    BP_Color color_magic_level1 = { 253, 101, 101, 255 };
    BP_Color color_purple = { 208, 152, 208, 255 };
    BP_Color color_magic_empty = { 236, 200, 40, 255 };
    BP_Color color_equip = { 165, 28, 218, 255 };

    auto select_color1 = [&](int v, int max_v) -> BP_Color
    {
        if (v >= max_v * 0.9)
        {
            return color_red;
        }
        else if (v >= max_v * 0.8)
        {
            return { 255, 165, 79, 255 };
        }
        else if (v >= max_v * 0.7)
        {
            return { 255, 255, 50, 255 };
        }
        else if (v < 0)
        {
            return color_purple;
        }
        return color_white;
    };

    auto select_color2 = [&](int v) -> BP_Color
    {
        if (v > 0)
        {
            return color_red;
        }
        if (v < 0)
        {
            return color_purple;
        }
        return color_white;
    };

    int font_size = 22;

    int x, y;

    x = x_ + 200;
    y = y_ + 50;
    font->draw(fmt::format("{}", role_->Name), 30, x - 10, y, color_name);
    font->draw("等級", font_size, x, y + 50, color_ability1);
    font->draw(fmt::format("{:5}", role_->Level), font_size, x + 66, y + 50, color_white);
    font->draw("經驗", font_size, x, y + 75, color_ability1);
    font->draw(fmt::format("{:5}", role_->Exp), font_size, x + 66, y + 75, color_white);

    std::string str = "";
    font->draw("升級", font_size, x, y + 100, color_ability1);

    int exp_up = GameUtil::getLevelUpExp(role_->Level);
    if (exp_up != INT_MAX)
    {
        str = fmt::format("{:6}", exp_up);
    }
    else
    {
        str = "------";
    }
    font->draw(str, font_size, x + 55, y + 100, color_white);
    font->draw("生命", font_size, x + 175, y + 50, color_ability1);
    font->draw(fmt::format("{:5}/", role_->HP), font_size, x + 219, y + 50, color_white);
    font->draw(fmt::format("{:5}", role_->MaxHP), font_size, x + 285, y + 50, color_white);
    font->draw("內力", font_size, x + 175, y + 75, color_ability1);

    BP_Color c = color_white;
    if (role_->MPType == 0)
    {
        c = color_purple;
    }
    else if (role_->MPType == 1)
    {
        c = color_magic;
    }

    font->draw(fmt::format("{:5}/", role_->MP), font_size, x + 219, y + 75, c);
    font->draw(fmt::format("{:5}", role_->MaxMP), font_size, x + 285, y + 75, c);
    font->draw("體力", font_size, x + 175, y + 100, color_ability1);
    font->draw(fmt::format("{:5}/", role_->PhysicalPower), font_size, x + 219, y + 100, color_white);
    font->draw(fmt::format("{:5}", 100), font_size, x + 285, y + 100, color_white);

    x = x_ + 20;
    y = y_ + 200;

    font->draw("攻擊", font_size, x, y, color_ability1);
    font->draw(fmt::format("{:5}", role_->Attack), font_size, x + 44, y, select_color1(role_->Attack, Role::getMaxValue()->Attack));
    font->draw("防禦", font_size, x + 200, y, color_ability1);
    font->draw(fmt::format("{:5}", role_->Defence), font_size, x + 244, y, select_color1(role_->Defence, Role::getMaxValue()->Defence));
    font->draw("輕功", font_size, x + 400, y, color_ability1);
    font->draw(fmt::format("{:5}", role_->Speed), font_size, x + 444, y, select_color1(role_->Speed, Role::getMaxValue()->Speed));

    font->draw("醫療", font_size, x, y + 25, color_ability1);
    font->draw(fmt::format("{:5}", role_->Medicine), font_size, x + 44, y + 25, select_color1(role_->Medicine, Role::getMaxValue()->Medicine));
    font->draw("解毒", font_size, x + 200, y + 25, color_ability1);
    font->draw(fmt::format("{:5}", role_->Detoxification), font_size, x + 244, y + 25, select_color1(role_->Detoxification, Role::getMaxValue()->Detoxification));
    font->draw("用毒", font_size, x + 400, y + 25, color_ability1);
    font->draw(fmt::format("{:5}", role_->UsePoison), font_size, x + 444, y + 25, select_color1(role_->UsePoison, Role::getMaxValue()->UsePoison));

    x = x_ + 20;
    y = y_ + 270;
    font->draw("技能", 25, x - 10, y, color_name);

    font->draw("拳掌", font_size, x, y + 30, color_ability1);
    font->draw(fmt::format("{:5}", role_->Fist), font_size, x + 44, y + 30, select_color1(role_->Fist, Role::getMaxValue()->Fist));
    font->draw("御劍", font_size, x, y + 55, color_ability1);
    font->draw(fmt::format("{:5}", role_->Sword), font_size, x + 44, y + 55, select_color1(role_->Sword, Role::getMaxValue()->Sword));
    font->draw("耍刀", font_size, x, y + 80, color_ability1);
    font->draw(fmt::format("{:5}", role_->Knife), font_size, x + 44, y + 80, select_color1(role_->Knife, Role::getMaxValue()->Knife));
    font->draw("特殊", font_size, x, y + 105, color_ability1);
    font->draw(fmt::format("{:5}", role_->Unusual), font_size, x + 44, y + 105, select_color1(role_->Unusual, Role::getMaxValue()->Unusual));
    font->draw("暗器", font_size, x, y + 130, color_ability1);
    font->draw(fmt::format("{:5}", role_->HiddenWeapon), font_size, x + 44, y + 130, select_color1(role_->HiddenWeapon, Role::getMaxValue()->HiddenWeapon));

    x = x_ + 220;
    y = y_ + 270;
    font->draw("武學", 25, x - 10, y, color_name);
    for (int i = 0; i < 10; i++)
    {
        auto magic = Save::getInstance()->getRoleLearnedMagic(role_, i);
        std::string str = "__________";
        if (magic)
        {
            int x1 = x + i % 2 * 200;
            int y1 = y + 30 + i / 2 * 25;
            str = fmt::format("{}", magic->Name);
            font->draw(str, font_size, x1, y1, color_ability1);
            str = fmt::format("{:3}", role_->getRoleShowLearnedMagicLevel(i));
            font->draw(str, font_size, x1 + 100, y1, role_->getRoleShowLearnedMagicLevel(i) > 9 ? color_red : color_purple);
        }
        else
        {
            int x1 = x + i % 2 * 200;
            int y1 = y + 30 + i / 2 * 25;
            font->draw("__________", font_size, x1, y1, color_ability1);
        }
    }

    x = x_ + 420;
    y = y_ + 445;
    font->draw("修煉", 25, x - 10, y, color_name);
    auto book = Save::getInstance()->getItem(role_->PracticeItem);
    if (book)
    {
        TextureManager::getInstance()->renderTexture("item", book->ID, x, y + 30);
        font->draw(fmt::format("{}", book->Name), font_size, x + 90, y + 30, color_name);
        font->draw(fmt::format("經驗{:5}", role_->ExpForItem), 18, x + 90, y + 55, color_ability1);
        std::string str = "升級 ----";
        int exp_up = GameUtil::getFinishedExpForItem(role_, book);
        if (exp_up != INT_MAX)
        {
            str = fmt::format("升級{:5}", exp_up);
        }
        font->draw(str, 18, x + 90, y + 75, color_ability1);
    }

    x = x_ + 20;
    y = y_ + 445;
    font->draw("武器", 25, x - 10, y, color_name);
    auto equip = Save::getInstance()->getItem(role_->Equip0);
    if (equip)
    {
        TextureManager::getInstance()->renderTexture("item", equip->ID, x, y + 30);
        font->draw(fmt::format("{}", equip->Name), font_size, x + 90, y + 30, color_name);
        font->draw("攻擊", 18, x + 90, y + 55, color_ability1);
        font->draw(fmt::format("{:+}", equip->AddAttack), 18, x + 126, y + 55, select_color2(equip->AddAttack));
        font->draw("防禦", 18, x + 90, y + 75, color_ability1);
        font->draw(fmt::format("{:+}", equip->AddDefence), 18, x + 126, y + 75, select_color2(equip->AddDefence));
        font->draw("輕功", 18, x + 90, y + 95, color_ability1);
        font->draw(fmt::format("{:+}", equip->AddSpeed), 18, x + 126, y + 95, select_color2(equip->AddSpeed));
    }

    x = x_ + 220;
    y = y_ + 445;
    font->draw("防具", 25, x - 10, y, color_name);
    equip = Save::getInstance()->getItem(role_->Equip1);
    if (equip)
    {
        TextureManager::getInstance()->renderTexture("item", equip->ID, x, y + 30);
        font->draw(fmt::format("{}", equip->Name), font_size, x + 90, y + 30, color_name);
        font->draw("攻擊", 18, x + 90, y + 55, color_ability1);
        font->draw(fmt::format("{:+}", equip->AddAttack), 18, x + 126, y + 55, select_color2(equip->AddAttack));
        font->draw("防禦", 18, x + 90, y + 75, color_ability1);
        font->draw(fmt::format("{:+}", equip->AddDefence), 18, x + 126, y + 75, select_color2(equip->AddDefence));
        font->draw("輕功", 18, x + 90, y + 95, color_ability1);
        font->draw(fmt::format("{:+}", equip->AddSpeed), 18, x + 126, y + 95, select_color2(equip->AddSpeed));
    }
}

void UIStatus::dealEvent(BP_Event& e)
{
}

void UIStatus::onPressedOK()
{
    if (role_ == nullptr)
    {
        return;
    }

    if (menu_->getResult() == 0)
    {
        auto team_menu = std::make_shared<TeamMenu>();
        team_menu->setText(fmt::format("{}要為誰醫療", role_->Name));
        team_menu->run();
        auto role = team_menu->getRole();
        if (role)
        {
            Role r = *role;
            GameUtil::medicine(role_, role);
            auto df = std::make_shared<ShowRoleDifference>(&r, role);
            df->setText(fmt::format("{}接受{}醫療", role->Name, role_->Name));
            df->run();
        }
    }
    else if (menu_->getResult() == 1)
    {
        auto team_menu = std::make_shared<TeamMenu>();
        team_menu->setText(fmt::format("{}要為誰解毒", role_->Name));
        team_menu->run();
        auto role = team_menu->getRole();
        if (role)
        {
            Role r = *role;
            GameUtil::detoxification(role_, role);
            auto df = std::make_shared<ShowRoleDifference>(&r, role);
            df->setText(fmt::format("{}接受{}解毒", role->Name, role_->Name));
            df->run();
        }
    }
    else if (menu_->getResult() == 2)
    {
        Event::getInstance()->callLeaveEvent(role_);
        role_ = nullptr;
    }
}

void UIStatus::setRoleName(std::string name)
{
    memset(role_->Name, '\0', sizeof(role_->Name));
    memcpy(role_->Name, name.c_str(), std::min(name.size(), sizeof(role_->Name)));
}
