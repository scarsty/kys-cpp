#pragma once
#include "Base.h"
#include "Types.h"

class Head : public Base
{
private:
    Role* role_;
public:
    Head(Role* r = nullptr);
    virtual ~Head();

    virtual void draw();

};

