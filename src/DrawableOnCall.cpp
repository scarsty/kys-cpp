#include "DrawableOnCall.h"

DrawableOnCall::DrawableOnCall(std::function<void(DrawableOnCall*)> draw) : draw_(draw)
{
}

void DrawableOnCall::updateScreenWithContext(const DrawableItemContext& context)
{
    context_ = context;
}

void DrawableOnCall::draw()
{
    draw_(this);
}
