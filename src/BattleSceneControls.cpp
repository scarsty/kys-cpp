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

BattleControlLayout makeBattleControlLayout(int uiWidth)
{
    assert(uiWidth > 0);
    BattleControlLayout layout;
    const int right = uiWidth - ButtonMargin - ButtonSize;
    const int step = ButtonSize + ButtonGap;
    auto button = [&](int column, int row)
    {
        return SDL_Rect{right - column * step, ButtonMargin + row * step, ButtonSize, ButtonSize};
    };
    layout.set(BattleControlId::Pause, button(0, 0));
    layout.set(BattleControlId::Log, button(1, 0));
    layout.set(BattleControlId::Speed, button(2, 0));
    layout.set(BattleControlId::Surrender, button(0, 1));
    layout.set(BattleControlId::CameraMode, button(1, 1));
    layout.set(BattleControlId::PaperView, button(2, 1));
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
