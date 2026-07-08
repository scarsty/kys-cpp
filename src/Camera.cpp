#include "Camera.h"
//#include "opencv4/opencv2/opencv.hpp"

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
    const auto n = p.norm();
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

float Camera::getDepth(const Pointf& p)
{
    auto forward = normalized(center - pos);
    if (forward.norm() == 0)
    {
        forward = { 0, 1, 0 };
    }
    return dot(p - pos, forward);
}

std::vector<Pointf> Camera::getProj(const std::vector<Pointf>& v)
{
    //cv::Point3f vec = { pos.x - center.x, pos.y - center.y, pos.z - center.z };
    //cv::Point3f vecz = { 0, 0, 1 };

    //float cos_theta = vec.dot(vecz) / (cv::norm(vec) * cv::norm(vecz));
    //auto theta = acos(cos_theta);

    //auto asix = vecz.cross(vec);
    //if (cv::norm(asix) != 0)
    //{
    //    asix = asix / cv::norm(asix);
    //}
    //asix = asix * theta;
    //cv::Mat rvec = cv::Mat(3, 1, CV_32F);
    //rvec.at<float>(0, 0) = asix.x;
    //rvec.at<float>(1, 0) = asix.y;
    //rvec.at<float>(2, 0) = asix.z;
    //cv::Mat tvec = cv::Mat(3, 1, CV_32F);
    //tvec.at<float>(0, 0) = vec.x;
    //tvec.at<float>(1, 0) = vec.y;
    //tvec.at<float>(2, 0) = vec.z;

    //std::vector<cv::Point3f> vf;
    //for (auto& p : v)
    //{
    //    vf.push_back({ p.x - center.x, p.y - center.y, p.z - center.z });
    //}
    //vf.push_back({ 0, 0, 0 });
    //std::vector<cv::Point2f> v_out;

    //cv::Mat cameraMatrix = cv::Mat::eye(3, 3, CV_32F);
    //cameraMatrix.at<float>(0, 0) = 100;
    //cameraMatrix.at<float>(1, 1) = 100;
    //cameraMatrix.at<float>(0, 2) = 0;
    //cameraMatrix.at<float>(1, 2) = 0;

    //cv::projectPoints(vf, rvec, tvec, cameraMatrix, cv::Mat(), v_out);
    //std::vector<Pointf> v_out2;
    //auto angle = atan2(vec.x, vec.y);
    //auto sin_a = sin(angle);
    //auto cos_a = cos(angle);
    //auto center1 = v_out.back();
    //for (auto& p : v_out)
    //{
    //    auto p0 = p - center1;
    //    p.x = p0.x * cos_a - p0.y * sin_a;
    //    p.y = p0.x * sin_a + p0.y * cos_a;

    //    p.x *= 1;
    //    p.y *= 1;
    //    p.x += 400;
    //    p.y += 350;
    //    v_out2.push_back({ p.x, p.y });
    //}
    //auto center2 = v_out2.back();
    //v_out2.pop_back();
    //for (auto& p : v_out2)
    //{
    //    //p.x -= center2.x;
    //    //p.y -= center2.y;
    //}

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
    const auto fov_rad = fov_ * pi / 180.0f;
    const auto focal = viewport_h_ * 0.5f / std::tan(fov_rad * 0.5f);

    for (auto& p : v)
    {
        auto rel = p - pos;
        auto z = dot(rel, forward);
        if (z < near_plane_)
        {
            z = near_plane_;
        }
        const auto x = dot(rel, right);
        const auto y = dot(rel, up);
        out.push_back({ viewport_w_ * 0.5f + x * focal / z, viewport_h_ * 0.5f - y * focal / z, z });
    }

    return out;
}
