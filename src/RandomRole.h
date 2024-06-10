#pragma once
#include "Head.h"
#include "UIStatus.h"

class RandomRole : public UIStatus
{
public:
    RandomRole();
    virtual ~RandomRole();

    virtual void onPressedOK() override;
    virtual void onPressedCancel() override { exitWithResult(-1); }
    virtual void draw() override;

    std::shared_ptr<Button> button_ok_;
    std::shared_ptr<Head> head_;
};
