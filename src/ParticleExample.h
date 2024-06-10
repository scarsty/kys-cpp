#pragma once
#include "ParticleSystem.h"

class ParticleExample : public ParticleSystem
{
public:
    ParticleExample() {}
    virtual ~ParticleExample() {}

    enum PatticleStyle
    {
        NONE,
        FIRE,
        FIRE_WORK,
        SUN,
        GALAXY,
        FLOWER,
        METEOR,
        SPIRAL,
        EXPLOSION,
        SMOKE,
        SNOW,
        RAIN,
    };

    PatticleStyle style_ = NONE;
    void setStyle(PatticleStyle style);
    //SDL_Texture* getDefaultTexture()
    //{
    //    static SDL_Texture* t = IMG_LoadTexture(_renderer, "fire.png");
    //    //fmt1::print(SDL_GetError());
    //    return t;
    //}
};
