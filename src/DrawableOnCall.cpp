#include "DrawableOnCall.h"

DrawableOnCall::DrawableOnCall(std::function<void(DrawableOnCall*)> draw) : draw_(draw), id_(-1)
{
}

void DrawableOnCall::updateScreenWithID(int id)
{
    id_ = id;
}

int DrawableOnCall::getID()
{
    return id_;
}

void DrawableOnCall::draw()
{
    draw_(this);
}
