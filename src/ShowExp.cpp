#include "ShowExp.h"
#include "Font.h"
#include "TextureManager.h"
ShowExp::ShowExp()
{
    x_ = 100;
    y_ = 100;
}

ShowExp::~ShowExp()
{
}

void ShowExp::draw()
{
    Engine::getInstance()->fillColor({ 0, 0, 0, 128 }, 0, 0, -1, -1);
    Font::getInstance()->draw(text_, 30, x_, y_, { 255, 255, 255, 255 });
    for (int i = 0; i < roles_.size(); i++)
    {
        auto r = roles_[i];
        int x = x_ + i % 5 * 180, y = y_ + 50 + i / 5 * 100;
        TextureManager::getInstance()->renderTexture("head", r->HeadID, x, y, { 255,255,255,255 }, 255, 0.5, 0.5);
        Font::getInstance()->draw(fmt1::format("{}", r->Name), 20, x + 90, y + 30, { 255, 255, 255, 255 });
        Font::getInstance()->draw(fmt1::format("{}", r->ExpGot), 20, x + 90, y + 55, { 255, 255, 255, 255 });
    }
}
