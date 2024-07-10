#pragma once
#include "Button.h"
#include "RunNode.h"

class VirtualStick : public RunNode
{
private:
    int w_, h_;
    std::shared_ptr<Button> button_up_, button_down_, button_left_, button_right_,
        button_a_, button_b_, button_x_, button_y_,
        button_lb_, button_rb_,
        button_menu_;
    int prev_press_ = 0;

public:
    VirtualStick();
    virtual void dealEvent(BP_Event& e) override;
    virtual void draw() override;
};
