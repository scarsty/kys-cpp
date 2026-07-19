#pragma once

#include "RunNode.h"
#include <string>

struct DayNightLighting
{
    Color ambient_color{ 255, 255, 255, 255 };
    Color sky_zenith{ 54, 107, 169, 255 };
    Color sky_mid{ 104, 157, 204, 255 };
    Color sky_horizon{ 176, 208, 228, 255 };
    Color sun_inner{ 255, 246, 180, 205 };
    Color sun_outer{ 255, 255, 220, 16 };
    float sun_azimuth = 0.25f;
    float sun_elevation = 0.42f;
    float star_alpha = 0.0f;
};

class DayNightSystem : public RunNode
{
public:
    static std::shared_ptr<DayNightSystem> getInstance();

    void backRun() override;

    float getTimeOfDay() const { return time_of_day_; }
    bool isDynamic() const;
    DayNightLighting getLighting() const;
    DayNightLighting getLighting(float time_of_day) const;
    std::string getTimeLabel() const;

private:
    float time_of_day_ = -1.0f;
    uint64_t previous_ticks_ = 0;
};