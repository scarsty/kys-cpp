#pragma once
#include "Point.h"
#include <vector>

class Camera
{public:
    Pointf pos;

    std::vector<Pointf> getProj(Pointf center, const std::vector<Pointf>& v);
};
