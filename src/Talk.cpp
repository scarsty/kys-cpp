#include "Talk.h"
#include <string>
#include <iostream>
#include "Engine.h"
#include "PotConv.h"
#include "Font.h"

void Talk::draw()
{
    Font::getInstance()->draw(content_, 20, 0, 0, { 255, 255, 255, 255 });
}

void Talk::dealEvent(BP_Event& e)
{
    if (e.type == BP_KEYUP)
    {
        loop_ = false;
        e.type = BP_FIRSTEVENT;
    }
}

