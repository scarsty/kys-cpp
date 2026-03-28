#pragma once
#include "Point.h"
#include "RunNode.h"

class Cloud : public RunNode
{
public:
    Cloud()
    {
        x_ = -1000;
        y_ = -1000;
    }
    virtual ~Cloud() {}

    const int num_style_ = 10;

    Point position_;
    int speed_x_, speed_y_;

    int max_X_ = 17280;
    int max_Y_ = 8640;

    int num_;

    Color color_;
    uint8_t alpha_;

    void initRand();
    void setPositionOnScreen(int x, int y, int center_x, int center_y);
    void draw() override;

    void flow();
};

class CloudGroup : public RunNode
{
    std::vector<Cloud> cloud_vector_;

public:
    void init(int cloud_num, int max_x, int max_y)
    {
        cloud_vector_.resize(cloud_num);
        for (auto& c : cloud_vector_)
        {
            c.max_X_ = max_x;
            c.max_Y_ = max_y;
            c.initRand();
        }
    }

    void backRun() override
    {
    }

    int setPositionOnScreen(int x, int y, int center_x, int center_y)
    {
        int view_cloud = 0;
        for (auto& c : cloud_vector_)
        {
            c.flow();
            c.setPositionOnScreen(x, y, center_x, center_y);
            int x, y;
            c.getPosition(x, y);
            if (x > -center_x * 1 && x < center_x * 3 && y > -0 && y < center_y * 2)
            {
                view_cloud++;
            }
        }
        return view_cloud;
    }

    void draw() override
    {
        for (auto& c : cloud_vector_)
        {
            c.draw();
            c.flow();
        }
    }
};
