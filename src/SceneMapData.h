#include "config.h"

#pragma once
class SceneMapData
{
public:
    SceneMapData();
    ~SceneMapData();

    short Data[Config::SLayerCount][Config::SceneMaxX][Config::SceneMaxY];
};

