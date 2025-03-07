#pragma once
#include "ParticleExample.h"
#include "RunNode.h"

class Weather : public RunNode
{
private:
    ParticleExample particle_;

    int slash_ = 0;

public:
    void draw() override;

    void backRun() override;

    static std::shared_ptr<Weather>& getInstance()
    {
        static auto w = std::make_shared<Weather>();
        return w;
    }

    void setWeather(bool north, int rain_count);

    void setSlash(int s) { slash_ = s; }
};
