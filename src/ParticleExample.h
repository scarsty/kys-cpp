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

    void setStyle(PatticleStyle style);

private:
    PatticleStyle style_ = NONE;

    Texture* getDefaultTexture()
    {
        static Texture* t = TextureManager::getInstance()->loadTexture("title", 201);
        return t;
    }
};
