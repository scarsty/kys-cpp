#include "VirtualStick.h"

void VirtualStick::dealEvent(SDL_Event& e)
{
    auto a = SDL_GetTouchDevice(0);
    int b = SDL_GetNumTouchFingers(a);
    //fmt1::print("{} {}\n", a, b);
    for (int i = 0; i < b; i++)
    {
        auto s = SDL_GetTouchFinger(a, i);
        fmt1::print("{}: {} {} ", i, s->x, s->y);
    }
}

void VirtualStick::draw()
{
}
