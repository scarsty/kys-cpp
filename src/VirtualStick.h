#pragma once
#include "RunNode.h"

class VirtualStick : public RunNode
{
public:
    virtual void dealEvent(SDL_Event& e) override;
    virtual void draw() override;
};
