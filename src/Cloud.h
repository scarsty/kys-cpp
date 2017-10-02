#pragma once
#include "Element.h"
#include "Point.h"

class Cloud : public Element
{
public:
    Cloud() { x_ = -1000; y_ = -1000; }
    virtual ~Cloud() {}

    enum CloudTowards
    {
        Left = 0,
        Right = 1,
        Up = 2,
        Down = 3,
    };

    Point position_;
    float speed_;

    const int max_X = 17280;
    const int max_Y = 8640;
    const int num_style_ = 10;
    int num_;

    BP_Color color_;
    uint8_t alpha_;

    void initRand();
    void setPositionOnScreen(int x, int y, int Center_X, int Center_Y);
    virtual void draw() override;
};

