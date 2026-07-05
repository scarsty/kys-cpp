#include "Camera.h"
#include <cmath>

namespace
{
float dot(const Pointf& a, const Pointf& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Pointf cross(const Pointf& a, const Pointf& b)
{
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

Pointf normalized(Pointf p)
{
    auto n = p.norm();
    if (n > 0)
    {
        p *= 1.0 / n;
    }
    return p;
}
}

void Camera::setViewport(float w, float h)
{
    if (w > 0) { viewport_w_ = w; }
    if (h > 0) { viewport_h_ = h; }
}

std::vector<Pointf> Camera::getProj(const std::vector<Pointf>& v)
{
    std::vector<Pointf> out;
    out.reserve(v.size());

    auto forward = normalized(center - pos);
    if (forward.norm() == 0)
    {
        forward = { 0, 1, 0 };
    }

    Pointf world_up = { 0, 0, 1 };
    auto right = normalized(cross(forward, world_up));
    if (right.norm() == 0)
    {
        world_up = { 0, 1, 0 };
        right = normalized(cross(forward, world_up));
    }
    auto up = cross(right, forward);

    constexpr float pi = 3.14159265358979323846f;
    auto fov_rad = fov_ * pi / 180.0f;
    auto focal = viewport_h_ * 0.5f / std::tan(fov_rad * 0.5f);

    for (auto& p : v)
    {
        auto rel = p - pos;
        auto z = dot(rel, forward);
        if (z < near_plane_)
        {
            z = near_plane_;
        }
        auto x = dot(rel, right);
        auto y = dot(rel, up);
        out.push_back({ viewport_w_ * 0.5f + x * focal / z, viewport_h_ * 0.5f - y * focal / z, z });
    }

    return out;
}
