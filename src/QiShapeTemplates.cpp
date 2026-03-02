#include "QiShapeTemplates.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <numbers>

namespace QiEffect
{
namespace
{

// ============================================================================
// 辅助函数：几何构建
// ============================================================================

inline void addDisk(std::vector<SDL_Point>& pts, int cx, int cy, int r)
{
    for (int y = -r; y <= r; ++y)
    {
        for (int x = -r; x <= r; ++x)
        {
            if (x * x + y * y <= r * r)
            {
                pts.push_back({cx + x, cy + y});
            }
        }
    }
}

inline void addRect(std::vector<SDL_Point>& pts, int x1, int y1, int x2, int y2)
{
    for (int y = y1; y <= y2; ++y)
    {
        for (int x = x1; x <= x2; ++x)
        {
            pts.push_back({x, y});
        }
    }
}

inline void addWedge(std::vector<SDL_Point>& pts, float a_min, float a_max, int r_min, int r_max)
{
    for (int r = r_min; r <= r_max; ++r)
    {
        int n_steps = std::max(3, r / 2);
        for (int i = 0; i <= n_steps; ++i)
        {
            float t = (float)i / (float)n_steps;
            float a = a_min + t * (a_max - a_min);
            int x = (int)std::lround((float)r * std::cos(a));
            int y = (int)std::lround((float)r * std::sin(a));
            pts.push_back({x, y});
        }
    }
}

inline void dedupPoints(std::vector<SDL_Point>& pts)
{
    std::sort(pts.begin(), pts.end(), [](const SDL_Point& a, const SDL_Point& b) {
        return (a.y == b.y) ? (a.x < b.x) : (a.y < b.y);
    });
    pts.erase(std::unique(pts.begin(), pts.end(), [](const SDL_Point& a, const SDL_Point& b) {
        return a.x == b.x && a.y == b.y;
    }), pts.end());
}

inline void rotate90(std::vector<SDL_Point>& pts)
{
    for (auto& p : pts)
    {
        int tmp = p.x;
        p.x = -p.y;
        p.y = tmp;
    }
}

// ============================================================================
// 8 方向旋转模板实现
// ============================================================================

class RotatedPointTemplate : public IShapeTemplate
{
public:
    RotatedPointTemplate(std::string name, std::vector<SDL_Point> base)
        : name_(std::move(name))
    {
        auto pts = base;
        for (int dir = 0; dir < 8; ++dir)
        {
            if (dir == 4)
            {
                // 方向 4 = 镜像反转
                for (auto& p : pts) p.x = -p.x;
            }
            points_[dir] = pts;
            rotate90(pts);
            if (dir == 3)
            {
                // 重置到初始位置再镜像
                pts = base;
                for (auto& p : pts) p.x = -p.x;
            }
        }
    }

    std::string_view name() const override { return name_; }
    std::span<const SDL_Point> points(int direction) const override
    {
        return points_[direction & 7];
    }

private:
    std::string name_;
    std::array<std::vector<SDL_Point>, 8> points_;
};

// ============================================================================
// 形状工厂函数
// ============================================================================

static std::unique_ptr<IShapeTemplate> makeSword()
{
    std::vector<SDL_Point> pts;

    // 剑尖（锐利 V 形）
    for (int i = 0; i <= 4; ++i)
    {
        pts.push_back({10, -i});
        if (i > 0) pts.push_back({10, i});
    }

    // 剑身（笔直线段）
    addRect(pts, 0, -1, 9, 1);

    // 剑柄（十字护手）
    addRect(pts, -2, -2, -1, 2);
    addRect(pts, -4, -1, -3, 1);

    dedupPoints(pts);
    return std::make_unique<RotatedPointTemplate>("Sword", std::move(pts));
}

static std::unique_ptr<IShapeTemplate> makeBlade()
{
    std::vector<SDL_Point> pts;

    // 刀尖（弧形）
    for (int x = 8; x <= 12; ++x)
    {
        float t = (float)(x - 8) / 4.0f;
        int y = (int)std::lround(2.5f - 1.5f * std::sin(t * std::numbers::pi_v<float>));
        pts.push_back({x, -y});
        pts.push_back({x, -y - 1});
    }

    // 刀身（宽厚矩形）
    addRect(pts, 0, -3, 9, 3);

    // 刀柄
    addRect(pts, -2, -1, -1, 1);

    dedupPoints(pts);
    return std::make_unique<RotatedPointTemplate>("Blade", std::move(pts));
}

static std::unique_ptr<IShapeTemplate> makeFist()
{
    std::vector<SDL_Point> pts;

    // 指关节（5 个圆形节点）
    addDisk(pts, -3, -3, 2);
    addDisk(pts, 1, -3, 2);
    addDisk(pts, 5, -2, 2);
    addDisk(pts, -1, 0, 3);

    // 掌心
    addRect(pts, -3, 2, 1, 5);
    addDisk(pts, 0, 4, 3);

    dedupPoints(pts);
    return std::make_unique<RotatedPointTemplate>("Fist", std::move(pts));
}

static std::unique_ptr<IShapeTemplate> makePalm()
{
    std::vector<SDL_Point> pts;

    // 扇形密集点（60 度）
    const float span = std::numbers::pi_v<float> / 3.0f;
    addWedge(pts, -span * 0.5f, span * 0.5f, 4, 14);

    // 边缘强调
    for (int r = 6; r <= 14; ++r)
    {
        int y = (int)std::lround((float)r * std::sin(span * 0.5f));
        int x = (int)std::lround((float)r * std::cos(span * 0.5f));
        pts.push_back({x, y});
        pts.push_back({x, -y});
    }

    dedupPoints(pts);
    return std::make_unique<RotatedPointTemplate>("Palm", std::move(pts));
}

static std::unique_ptr<IShapeTemplate> makeRibbon()
{
    std::vector<SDL_Point> pts;

    // 中心球
    addDisk(pts, 0, 0, 4);

    // 两侧装饰
    addDisk(pts, -6, 2, 2);
    addDisk(pts, -6, -2, 2);

    // 延伸
    addRect(pts, -8, -1, -1, 1);

    dedupPoints(pts);
    return std::make_unique<RotatedPointTemplate>("Ribbon", std::move(pts));
}

}  // namespace

ShapeRegistry& ShapeRegistry::instance()
{
    static ShapeRegistry inst;
    return inst;
}

ShapeRegistry::ShapeRegistry()
{
    registerShape(makeSword());
    registerShape(makeBlade());
    registerShape(makeFist());
    registerShape(makePalm());
    registerShape(makeRibbon());

    // 调试输出
    printf("【真气特效】形状模板已初始化：%zu 个形状\n", shapes_.size());
    for (const auto& [name, shape] : shapes_)
    {
        printf("  - %s: %zu 个点（方向0）\n", name.c_str(), shape->points(0).size());
    }
}

void ShapeRegistry::registerShape(std::unique_ptr<IShapeTemplate> shape)
{
    if (!shape) return;
    shapes_.emplace(std::string(shape->name()), std::move(shape));
}

const IShapeTemplate* ShapeRegistry::get(std::string_view name) const
{
    auto it = shapes_.find(std::string(name));
    if (it == shapes_.end()) return nullptr;
    return it->second.get();
}

}  // namespace QiEffect
