#pragma once
#include "RunNode.h"
#include "Point.h"

class Cloud : public RunNode
{
public:
    Cloud() { x_ = -1000; y_ = -1000; }
    virtual ~Cloud() {}

    Point position_;
    int speed_x_, speed_y_;

    const int max_X_ = 17280;
    const int max_Y_ = 8640;
    const int num_style_ = 10;
    int num_;

    BP_Color color_;
    uint8_t alpha_;

    void initRand();
    void setPositionOnScreen(int x, int y, int Center_X, int Center_Y);
    virtual void draw() override;

    void flow();
};

