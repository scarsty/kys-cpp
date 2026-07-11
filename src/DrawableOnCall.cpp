#include "DrawableOnCall.h"

DrawableOnCall::DrawableOnCall(std::function<void(DrawableOnCall*)> draw) : draw_(draw), id_(-1)
{
}

DrawableOnCall::DrawableOnCall(std::function<void(DrawableOnCall*)> draw, std::function<void(DrawableOnCall*, EngineEvent&)> deal_event) :
    draw_(draw), deal_event_(deal_event), id_(-1)
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

void DrawableOnCall::dealEvent(EngineEvent& e)
{
    if (deal_event_)
    {
        deal_event_(this, e);
    }
}
