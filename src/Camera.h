#pragma once
#include "Point.h"
#include <vector>

class Camera
{public:
    Pointf pos, center;

    // 投影输出画布尺寸，单位为像素；结果坐标会落在这个 viewport 的屏幕坐标系中。
    void setViewport(float w, float h);
    // 垂直视角，单位为角度；值越大透视越广，近处物体变形越明显。
    void setFov(float fov) { fov_ = fov; }
    // 近裁剪面到相机的距离；比它更近的点会被钳到该距离，避免透视除零或异常放大。
    void setNearPlane(float near_plane) { near_plane_ = near_plane; }

    float getDepth(const Pointf& p);
    float getNearPlane() const { return near_plane_; }
    std::vector<Pointf> getProj(const std::vector<Pointf>& v);

private:
    // 默认值只作为调用 setViewport 前的兜底；Paper 模式会按 scene 渲染尺寸设置。
    float viewport_w_ = 800;
    float viewport_h_ = 600;
    float fov_ = 60.0f;
    float near_plane_ = 1.0f;
};
