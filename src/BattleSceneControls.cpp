#include "BattleSceneControls.h"

#include <cassert>
#include <algorithm>

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

BattleInspectorLayout makeBattleInspectorLayout(std::span<const int> cardHeights, int page)
{
    BattleInspectorLayout layout;
    layout.pages.emplace_back();
    for (int row = 0; row < int(cardHeights.size()); ++row)
    {
        const int height = cardHeights[row];
        assert(height > 0 && height <= BattleInspectorLayout::effectCapacity);
        auto* current = &layout.pages.back();
        const int gap = current->rowCount > 0 ? BattleInspectorLayout::effectGap : 0;
        if (current->height + gap + height > BattleInspectorLayout::effectCapacity)
        {
            layout.pages.push_back({.firstRow = row});
            current = &layout.pages.back();
        }
        current->height += (current->rowCount > 0 ? BattleInspectorLayout::effectGap : 0) + height;
        ++current->rowCount;
    }
    layout.page = std::clamp(page, 0, int(layout.pages.size()) - 1);
    // 多頁時保持欄高，換頁不會讓游標下的控制位置跳動。
    const int height = layout.pages.size() > 1 ? 688
        : BattleInspectorLayout::effectHeader + std::max(28, layout.pages.front().height) + 12;
    layout.effects = {16, 704 - height, 276, height};
    return layout;
}

void addBattleInspectorControls(BattleControlLayout& controls, const BattleInspectorLayout& inspector)
{
    controls.set(BattleControlId::InspectorBody, BattleInspectorLayout::summary);
    controls.set(BattleControlId::InspectorEffects, inspector.effects);
    if (inspector.pages.size() > 1)
    {
        controls.set(BattleControlId::InspectorPrevious, BattleInspectorLayout::previous);
        controls.set(BattleControlId::InspectorNext, BattleInspectorLayout::next);
    }
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
