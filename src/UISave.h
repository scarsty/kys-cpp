#pragma once
#include "Menu.h"

class UISave : public MenuText
{
public:
    UISave();
    ~UISave();

    int mode_ = 0;  //0Îª¶Áµµ£¬1Îª´æµµ

    void onEntrance() override;

};

