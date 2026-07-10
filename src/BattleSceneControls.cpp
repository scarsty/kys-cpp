#include "BattleSceneControls.h"

#include <cassert>

namespace
{
constexpr int ButtonSize = 64;
constexpr int ButtonGap = 8;
constexpr int ButtonMargin = 12;

bool pointInRect(SDL_FPoint point, const SDL_Rect& rect)
{
    return point.x >= rect.x && point.x < rect.x + rect.w
        && point.y >= rect.y && point.y < rect.y + rect.h;
}
}

std::optional<SDL_Rect> BattleControlLayout::rect(BattleControlId id) const
{
    const auto it = rects_.find(id);
    return it == rects_.end() ? std::nullopt : std::optional<SDL_Rect>(it->second);
}

BattleControlId BattleControlLayout::hitTest(SDL_FPoint point) const
{
    for (const auto& [id, rect] : rects_)
    {
        if (pointInRect(point, rect)) return id;
    }
    return BattleControlId::None;
}

BattleControlLayout makeBattleControlLayout(int uiWidth, bool logVisible, bool cameraModeVisible)
{
    assert(uiWidth > 0);
    BattleControlLayout layout;
    int x = uiWidth - ButtonMargin - ButtonSize;
    auto takeButton = [&]()
    {
        const SDL_Rect rect{x, ButtonMargin, ButtonSize, ButtonSize};
        x -= ButtonSize + ButtonGap;
        return rect;
    };
    layout.set(BattleControlId::Pause, takeButton());
    if (logVisible) layout.set(BattleControlId::Log, takeButton());
    if (cameraModeVisible) layout.set(BattleControlId::CameraMode, takeButton());
    layout.set(BattleControlId::PaperView, takeButton());
    layout.set(BattleControlId::Speed, takeButton());
    return layout;
}

BattleControlId BattleControlCapture::onDown(PointerIdentity pointer, SDL_FPoint point, const BattleControlLayout& layout)
{
    if (pointer_) return BattleControlId::None;
    const auto hit = layout.hitTest(point);
    if (hit != BattleControlId::None)
    {
        pointer_ = pointer;
        armed_ = hit;
    }
    return hit;
}

BattleControlId BattleControlCapture::onUp(PointerIdentity pointer, SDL_FPoint point, const BattleControlLayout& layout)
{
    if (!pointer_ || *pointer_ != pointer) return BattleControlId::None;
    const auto armed = armed_;
    reset();
    return layout.hitTest(point) == armed ? armed : BattleControlId::None;
}

void BattleControlCapture::cancel(PointerIdentity pointer)
{
    if (pointer_ && *pointer_ == pointer) reset();
}

void BattleControlCapture::reset()
{
    pointer_.reset();
    armed_ = BattleControlId::None;
}
