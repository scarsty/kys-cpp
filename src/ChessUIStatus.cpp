#include "ChessUIStatus.h"
#include "Engine.h"

void ChessUIStatus::draw()
{
    // Draw translucent black background
    Engine::getInstance()->fillColor({0, 0, 0, 128}, x_, y_, 600, 500);

    if (!role_) return;

    TextureManager::getInstance()->renderTexture("head", role_->HeadID, x_ + 10, y_ + 20);

    auto font = Font::getInstance();
    Color color_white = { 255, 255, 255, 255 };
    Color color_name = { 255, 215, 0, 255 };
    Color color_ability1 = { 255, 250, 205, 255 };
    Color color_ability2 = { 236, 200, 40, 255 };
    Color color_red = { 255, 90, 60, 255 };
    Color color_magic = { 236, 200, 40, 255 };
    Color color_magic_level1 = { 253, 101, 101, 255 };
    Color color_purple = { 208, 152, 208, 255 };
    Color color_magic_empty = { 236, 200, 40, 255 };
    Color color_equip = { 165, 28, 218, 255 };

    auto select_color1 = [&](int v, int max_v) -> Color
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

    auto select_color2 = [&](int v) -> Color
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

    int x = x_ + 200;
    int y = y_ + 50;

    font->draw("生命", font_size, x + 175, y + 50, color_ability1);
    font->draw(std::format("{:5}/", role_->HP), font_size, x + 219, y + 50, color_white);
    font->draw(std::format("{:5}", role_->MaxHP), font_size, x + 285, y + 50, color_white);
    font->draw("內力", font_size, x + 175, y + 75, color_ability1);

    Color c = color_white;
    if (role_->MPType == 0)
    {
        c = color_purple;
    }
    else if (role_->MPType == 1)
    {
        c = color_magic;
    }

    font->draw(std::format("{:5}/", role_->MP), font_size, x + 219, y + 75, c);
    font->draw(std::format("{:5}", role_->MaxMP), font_size, x + 285, y + 75, c);

    x = x_ + 20;
    y = y_ + 200;

    font->draw("攻擊", font_size, x, y, color_ability1);
    font->draw(std::format("{:5}", role_->Attack), font_size, x + 44, y, select_color1(role_->Attack, Role::getMaxValue()->Attack));
    font->draw("防禦", font_size, x + 200, y, color_ability1);
    font->draw(std::format("{:5}", role_->Defence), font_size, x + 244, y, select_color1(role_->Defence, Role::getMaxValue()->Defence));
    font->draw("輕功", font_size, x + 400, y, color_ability1);
    font->draw(std::format("{:5}", role_->Speed), font_size, x + 444, y, select_color1(role_->Speed, Role::getMaxValue()->Speed));

    x = x_ + 20;
    y = y_ + 270;
    font->draw("技能", 25, x - 10, y, color_name);

    font->draw("拳掌", font_size, x, y + 30, color_ability1);
    font->draw(std::format("{:5}", role_->Fist), font_size, x + 44, y + 30, select_color1(role_->Fist, Role::getMaxValue()->Fist));
    font->draw("御劍", font_size, x, y + 55, color_ability1);
    font->draw(std::format("{:5}", role_->Sword), font_size, x + 44, y + 55, select_color1(role_->Sword, Role::getMaxValue()->Sword));
    font->draw("耍刀", font_size, x, y + 80, color_ability1);
    font->draw(std::format("{:5}", role_->Knife), font_size, x + 44, y + 80, select_color1(role_->Knife, Role::getMaxValue()->Knife));
    font->draw("特殊", font_size, x, y + 105, color_ability1);
    font->draw(std::format("{:5}", role_->Unusual), font_size, x + 44, y + 105, select_color1(role_->Unusual, Role::getMaxValue()->Unusual));
    font->draw("暗器", font_size, x, y + 130, color_ability1);
    font->draw(std::format("{:5}", role_->HiddenWeapon), font_size, x + 44, y + 130, select_color1(role_->HiddenWeapon, Role::getMaxValue()->HiddenWeapon));

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
            str = std::format("{}", magic->Name);
            font->draw(str, font_size, x1, y1, color_ability1);
            str = std::format("{:3}", role_->getRoleShowLearnedMagicLevel(i));
            font->draw(str, font_size, x1 + 100, y1, role_->getRoleShowLearnedMagicLevel(i) > 9 ? color_red : color_purple);
        }
        else
        {
            int x1 = x + i % 2 * 200;
            int y1 = y + 30 + i / 2 * 25;
            font->draw("", font_size, x1, y1, color_ability1);
        }
    }
}   