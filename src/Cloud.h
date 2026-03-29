#pragma once
#include "RunNode.h"
#include "Point.h"

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
        for (auto& cloud : cloud_vector_)
        {
            cloud.max_X_ = max_x;
            cloud.max_Y_ = max_y;
            cloud.initRand();
        }
    }

    void backRun() override
    {
    }

    int setPositionOnScreen(int x, int y, int center_x, int center_y)
    {
        int view_cloud = 0;
        for (auto& cloud : cloud_vector_)
        {
            cloud.flow();
            cloud.setPositionOnScreen(x, y, center_x, center_y);
            int screen_x, screen_y;
            cloud.getPosition(screen_x, screen_y);
            if (screen_x > -center_x && screen_x < center_x * 3 && screen_y > 0 && screen_y < center_y * 2)
            {
                view_cloud++;
            }
        }
        return view_cloud;
    }

    void draw() override
    {
        for (auto& cloud : cloud_vector_)
        {
            cloud.draw();
            cloud.flow();
        }
    }
};

