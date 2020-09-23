#include "Head.h"
#include "Font.h"
#include "GameUtil.h"
#include "convert.h"

Head::Head(Role* r)
{
    role_ = r;
    setTextPosition(20, 65);
    setFontSize(20);
    setTextColor({ 255, 255, 255, 255 });
    setHaveBox(false);
}

Head::~Head()
{
}

void Head::draw()
{
    w_ = 250;
    h_ = 90;
    if (role_ == nullptr) { return; }
    BP_Color color = { 255, 255, 255, 255 }, white = { 255, 255, 255, 255 };
    auto font = Font::getInstance();

    if (!only_head_)
    {
        TextureManager::getInstance()->renderTexture("title", 102, x_, y_);
    }

    if (state_ == Normal)
    {
        color = { 160, 160, 160, 255 };
    }
    //中毒时突出绿色
    color.r -= 2 * role_->Poison;
    color.b -= 2 * role_->Poison;
    TextureManager::getInstance()->renderTexture("head", role_->HeadID, x_ + 10, y_, color, 255, 0.5, 0.5);

    TextBox::draw();

    if (only_head_) { return; }

    //下面都是画血条等

    font->draw(role_->Name, 16, x_ + 117, y_ + 9, white);
    BP_Rect r1 = { 0, 0, 0, 0 };
    font->draw(fmt::format("{}", role_->Level), 16, x_ + 99 - 4 * GameUtil::digit(role_->Level), y_ + 5, { 250, 200, 50, 255 });

    BP_Color c, c_text;
    if (role_->MaxHP > 0)
    {
        r1 = { x_ + 96, y_ + 32, 138 * role_->HP / role_->MaxHP, 9 };
    }
    else
    {
        r1 = { 0, 0, 0, 0 };
    }
    c = { 196, 25, 16, 255 };
    Engine::getInstance()->renderSquareTexture(&r1, c, 192);
    font->draw(fmt::format("{:3}/{:3}", role_->HP, role_->MaxHP), 16, x_ + 138, y_ + 28, { 250, 200, 50, 255 });

    if (role_->MaxMP > 0)
    {
        r1 = { x_ + 96, y_ + 48, 138 * role_->MP / role_->MaxMP, 9 };
    }
    else
    {
        r1 = { 0, 0, 0, 0 };
    }
    c = { 200, 200, 200, 255 };
    c_text = white;
    if (role_->MPType == 0)
    {
        c = { 112, 12, 112, 255 };
        c_text = { 240, 150, 240, 255 };
    }
    else if (role_->MPType == 1)
    {
        c = { 224, 180, 32, 255 };
        c_text = { 250, 200, 50, 255 };
    }
    Engine::getInstance()->renderSquareTexture(&r1, c, 192);
    font->draw(fmt::format("{:3}/{:3}", role_->MP, role_->MaxMP), 16, x_ + 138, y_ + 44, c_text);

    r1 = { x_ + 115, y_ + 65, 83 * role_->PhysicalPower / 100, 9 };
    c = { 128, 128, 255, 255 };
    Engine::getInstance()->renderSquareTexture(&r1, c, 192);
    font->draw(fmt::format("{}", role_->PhysicalPower), 16, x_ + 154 - 4 * GameUtil::digit(role_->PhysicalPower), y_ + 61, { 250, 200, 50, 255 });

}
