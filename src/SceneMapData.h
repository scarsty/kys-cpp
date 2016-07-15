#include "config.h"

#pragma once
class SceneMapData
{
public:
    SceneMapData();
    ~SceneMapData();

    short Data[config::SLayerCount][config::SceneMaxX][config::SceneMaxY];
};

