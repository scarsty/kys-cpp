#pragma once
#include "Head.h"
#include "UIStatus.h"

class RandomRole : public UIStatus
{
public:
    RandomRole();
    virtual ~RandomRole();

    virtual void onEntrance() override;
    virtual void onPressedOK() override;
    virtual void onPressedCancel() override { exitWithResult(-1); }
    virtual void draw() override;

private:
    void randomizeRole();

    std::shared_ptr<Menu> action_menu_;
    std::shared_ptr<Button> button_random_;
    std::shared_ptr<Button> button_ok_;
    std::shared_ptr<Button> button_cancel_;
    std::shared_ptr<Head> head_;
};
