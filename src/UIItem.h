#pragma once
#include "Base.h"
#include "Button.h"

class UIItem : public Base
{
public:
    UIItem();
    ~UIItem();

    std::vector<Button*> item_buttons_;

};

