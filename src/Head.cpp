#include "Head.h"
#include "Font.h"

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
    TextureManager::getInstance()->renderTexture("head", role_->HeadNum, x_, y_, color, 255, 0.5);
    Font::getInstance()->draw(std::string(role_->Name), 15, x_ + 140, y_ + 5, { 255, 255, 255, 255 });
}


