#pragma once
#include "Types.h"
#include "UIStatus.h"
#include "Head.h"

class RandomRole : public UIStatus
{
public:
    RandomRole();
    virtual ~RandomRole();

    virtual void onPressedOK() override;
    virtual void onPressedCancel() override { exitWithResult(-1); }
    virtual void draw() override;

    Button* button_ok_;
    Head* head_;
};

