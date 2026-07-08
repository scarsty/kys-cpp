#pragma once
#include "Point.h"
#include <vector>

class Camera
{public:
    Pointf pos, center;

    void setViewport(float w, float h);
    void setFov(float fov) { fov_ = fov; }
    void setNearPlane(float near_plane) { near_plane_ = near_plane; }

    float getDepth(const Pointf& p);
    float getNearPlane() const { return near_plane_; }
    std::vector<Pointf> getProj(const std::vector<Pointf>& v);

private:
    float viewport_w_ = 800;
    float viewport_h_ = 600;
    float fov_ = 60.0f;
    float near_plane_ = 1.0f;
};
