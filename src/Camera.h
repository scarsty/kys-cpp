#pragma once
#include "Point.h"
#include <vector>

class Camera
{public:
    Pointf pos, center;

    std::vector<Pointf> getProj(const std::vector<Pointf>& v);
};
