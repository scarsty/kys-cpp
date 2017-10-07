#include "Head.h"
#include "Font.h"
#include "others/libconvert.h"
#include "GameUtil.h"

Head::Head(Role* r)
{
    role_ = r;
    setTextPosition(30, 65);
    setFontSize(15);
}

Head::~Head()
{
}

void Head::draw()
{
    if (w_ * h_ <= 0)
    {
        auto tex = TextureManager::getInstance()->loadTexture("title", 102);
        if (tex)
        {
            w_ = tex->w;
            h_ = tex->h;
        }
        else
        {
            w_ = 200;
            h_ = 80;
        }
    }
    if (role_ == nullptr) { return; }
    BP_Color color = { 255, 255, 255, 255 }, white = { 255, 255, 255, 255 };
    auto font = Font::getInstance();
    TextureManager::getInstance()->renderTexture("title", 102, x_, y_);
    if (state_ == Normal)
    {
        color = { 128, 128, 128, 255 };
    }

    TextureManager::getInstance()->renderTexture("head", role_->HeadID, x_ + 10, y_, color, 255, 0.5, 0.5);
    font->draw(role_->Name, 16, x_ + 117, y_ + 9, white);

    BP_Rect r1;

    //注意这里计算某个数字长度的方法
    font->draw(convert::formatString("%d", role_->Level), 16, x_ + 99 - 4 * GameUtil::digit(role_->Level), y_ + 5, { 250, 200, 50, 255 });

    BP_Color c;
    r1 = { x_ + 97, y_ + 32, 137 * role_->HP / role_->MaxHP, 9 };
    c = { 196, 25, 16, 255 };
    Engine::getInstance()->renderSquareTexture(&r1, c, 192);
    font->draw(convert::formatString("%3d/%3d", role_->HP, role_->MaxHP), 16, x_ + 138, y_ + 28, { 250, 200, 50, 255 });

    r1 = { x_ + 97, y_ + 48, 137 * role_->MP / role_->MaxMP, 9 };
    c = { 200, 200, 200, 255 };
    if (role_->MPType == 0)
    {
        c = { 112, 12, 112, 255 };
    }
    else if (role_->MPType == 1)
    {
        c = { 224, 180, 32, 255 };
    }
    Engine::getInstance()->renderSquareTexture(&r1, c, 192);
    font->draw(convert::formatString("%3d/%3d", role_->MP, role_->MaxMP), 16, x_ + 138, y_ + 44, white);

    r1 = { x_ + 116, y_ + 65, 82 * role_->PhysicalPower / 100, 9 };
    c = { 128, 128, 255, 255 };
    Engine::getInstance()->renderSquareTexture(&r1, c, 192);
    font->draw(convert::formatString("%d", role_->PhysicalPower), 16, x_ + 154 - 4 * GameUtil::digit(role_->PhysicalPower), y_ + 61, { 250, 200, 50, 255 });

    TextBox::draw();
}


