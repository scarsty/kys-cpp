#include "Talk.h"
#include <string>
#include <iostream>
#include "Engine.h"
#include "PotConv.h"
#include "Font.h"

void Talk::draw()
{
    if (content_.size() > 0)
    {
        TextureManager::getInstance()->renderTexture("head", head_id_, 0, 0);
        Font::getInstance()->draw(content_, 25, 0, 0, { 255, 255, 255, 255 });
    }
}

void Talk::dealEvent(BP_Event& e)
{
    if (e.type == BP_KEYUP)
    {
        setExit(true);
        e.type = BP_FIRSTEVENT;
    }
}

