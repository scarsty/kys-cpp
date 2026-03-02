#pragma once
#include <SDL3/SDL.h>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace QiEffect
{

// ============================================================================
// 形状模板接口
// ============================================================================

class IShapeTemplate
{
public:
    virtual ~IShapeTemplate() = default;
    virtual std::string_view name() const = 0;
    virtual std::span<const SDL_Point> points(int direction) const = 0;
};

// ============================================================================
// 形状注册表（单例）
// ============================================================================

class ShapeRegistry
{
public:
    static ShapeRegistry& instance();

    void registerShape(std::unique_ptr<IShapeTemplate> shape);
    const IShapeTemplate* get(std::string_view name) const;

private:
    ShapeRegistry();
    std::unordered_map<std::string, std::unique_ptr<IShapeTemplate>> shapes_;
};

}  // namespace QiEffect
