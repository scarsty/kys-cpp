#include "Head.h"
#include "Font.h"
#include "others/libconvert.h"

Texture Head::square_;

Head::Head(Role* r)
{
    role_ = r;
    if (square_.loaded == false)
    {
        square_.setTex(Engine::getInstance()->createSquareTexture(100));
    }
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

    BP_Rect r1 = { 5, 5, 200, 50 };
    //Engine::getInstance()->renderCopy(square_.tex[0], r0, r1);

    TextureManager::getInstance()->renderTexture("head", role_->HeadNum, x_, y_, color, 255, 0.5);
    Font::getInstance()->draw(role_->Name, 15, x_ + 140, y_ + 5, { 255, 255, 255, 255 });

    Font::getInstance()->draw(convert::formatString("%d", role_->Level), 15, x_ + 100, y_ + 5, { 255, 255, 255, 255 });
    Font::getInstance()->draw(convert::formatString("%3d/%3d", role_->HP, role_->MaxHP), 15, x_ + 140, y_ + 25, { 255, 255, 255, 255 });
    Font::getInstance()->draw(convert::formatString("%3d/%3d", role_->MP, role_->MaxMP), 15, x_ + 140, y_ + 45, { 255, 255, 255, 255 });
    Font::getInstance()->draw(convert::formatString("%3d", role_->PhysicalPower), 15, x_ + 140, y_ + 65, { 255, 255, 255, 255 });
}


