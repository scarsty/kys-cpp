#pragma once
#include "Button.h"
#include "RunNode.h"
#include <unordered_map>

class VirtualStick : public RunNode
{
private:
    int w_, h_;
    std::shared_ptr<Button> button_up_, button_down_, button_left_, button_right_,
        button_a_, button_b_, button_x_, button_y_,
        button_lb_, button_rb_,
        button_view_, button_menu_, button_left_axis_;
    int prev_press_ = 0;

    struct Interval
    {
        int interval = 20;
        int prev_press = 0;
    };

    int axis_center_x_ = 0;
    int axis_center_y_ = 0;
    int axis_x_ = 0;
    int axis_y_ = 0;
    double axis_radius_ = 0;

    std::unordered_map<std::shared_ptr<Button>, Interval> button_interval_;

public:
    VirtualStick();
    virtual void dealEvent(EngineEvent& e) override;
    virtual void draw() override;
    void setStyle(int style);    //0-使用方向键 1-使用摇杆
};