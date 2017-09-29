#include "Head.h"
#include "Font.h"
#include "others/libconvert.h"

Head::Head(Role* r)
{
    role_ = r;
}

Head::~Head()
{
}

void Head::draw()
{
    if (role_ == nullptr) { return; }
    BP_Color color = { 255, 255, 255, 255 };
    TextureManager::getInstance()->renderTexture("mmap", 2002, x_, y_);
    if (state_ == Normal)
    {
        color = { 128, 128, 128, 255 };
    }

    TextureManager::getInstance()->renderTexture("head", role_->HeadNum, x_ + 10, y_, color, 255, 0.5);
    Font::getInstance()->draw(role_->Name, 15, x_ + 115, y_ + 8, { 255, 255, 255, 255 });

    int level_x = 99;
    if (role_->Level >= 10) { level_x -= 3; }
    BP_Rect r1;
    Font::getInstance()->draw(convert::formatString("%d", role_->Level), 15, x_ + level_x, y_ + 5, { 255, 255, 255, 255 });

    BP_Color c;
    r1 = { x_ + 97, y_ + 32, 137 * role_->HP / role_->MaxHP, 9 };
    c = { 196, 25, 16, 255 };
    Engine::getInstance()->renderSquareTexture(&r1, c, 192);
    Font::getInstance()->draw(convert::formatString("%3d/%3d", role_->HP, role_->MaxHP), 15, x_ + 138, y_ + 28, { 255, 255, 255, 255 });

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
    Font::getInstance()->draw(convert::formatString("%3d/%3d", role_->MP, role_->MaxMP), 15, x_ + 138, y_ + 44, { 255, 255, 255, 255 });

    r1 = { x_ + 116, y_ + 65, 83 * role_->MP / role_->MaxMP, 9 };
    c = { 128, 128, 255, 255 };
    Engine::getInstance()->renderSquareTexture(&r1, c, 192);
    Font::getInstance()->draw(convert::formatString("%3d", role_->PhysicalPower), 15, x_ + 148, y_ + 61, { 255, 255, 255, 255 });
}


