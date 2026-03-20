#pragma once
#include "RunNode.h"

class Fade : public RunNode
{
    int frame_ = 0;
    int total_ = 1;
    int from_ = 0;
    int to_ = 255;

    void draw() override
    {
        int alpha = from_ + (to_ - from_) * (frame_ + 1) / total_;
        alpha = std::clamp(alpha, 0, 255);
        Engine::getInstance()->fillColor({ 0, 0, 0, (uint8_t)alpha }, 0, 0, -1, -1);
        frame_++;
        if (frame_ >= total_)
        {
            setExit(true);
        }
    }

public:
    void set(int total, int from, int to)
    {
        total_ = total;
        from_ = from;
        to_ = to;
    }

    void reset()
    {
        frame_ = 0;
        setExit(false);
    }

    static void fadeIn(int total)
    {
        auto f = std::make_shared<Fade>();
        f->set(total, 255, 0);
        f->run();
    }

    static void fadeOut(int total)
    {
        auto f = std::make_shared<Fade>();
        f->set(total, 0, 255);
        f->run();
    }
};
