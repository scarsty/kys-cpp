#include "Head.h"

Head::Head(Role* r)
{
    role_ = r;
}

Head::~Head()
{
}

void Head::draw()
{
    TextureManager::getInstance()->renderTexture("head", 2002, x_, y_);
    TextureManager::getInstance()->renderTexture("head", role_->HeadNum, x_,y_);
}
