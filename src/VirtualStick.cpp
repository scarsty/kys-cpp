#include "VirtualStick.h"

#include "Button.h"

VirtualStick::VirtualStick()
{
    Engine::getInstance()->getStartWindowSize(w_, h_);

    auto addButton = [this](std::shared_ptr<Button>& b, int id, int x, int y, int buttonid)
    {
        b = std::make_shared<Button>("title", id);
        b->setAlpha(64);
        b->button_id_ = buttonid;
        addChild(b, x, y);
    };

    addButton(button_up_, 312, w_ * 0.1, h_ * 0.5, SDL_CONTROLLER_BUTTON_DPAD_UP);
    addButton(button_down_, 312, w_ * 0.1, h_ * 0.8, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    addButton(button_left_, 312, w_ * 0.1 - h_ * 0.15, h_ * 0.65, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    addButton(button_right_, 312, w_ * 0.1 + h_ * 0.15, h_ * 0.65, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);

    addButton(button_a_, 300, w_ * 0.8, h_ * 0.8, BP_CONTROLLER_BUTTON_A);
    addButton(button_b_, 302, w_ * 0.8 + h_ * 0.15, h_ * 0.65, BP_CONTROLLER_BUTTON_B);
    addButton(button_x_, 304, w_ * 0.8 - h_ * 0.15, h_ * 0.65, BP_CONTROLLER_BUTTON_X);
    addButton(button_y_, 306, w_ * 0.8, h_ * 0.5, BP_CONTROLLER_BUTTON_Y);

    //addButton(button_lb_, 300, w_ * 0.1, h_ * 0.5, BP_CONTROLLER_BUTTON_LEFTSHOULDER);
    //addButton(button_rb_, 302, w_ * 0.1, h_ * 0.8, BP_CONTROLLER_BUTTON_RIGHTSHOULDER);

    addButton(button_menu_, 308, w_ * 0.5 - 20, h_ * 0.9, BP_CONTROLLER_BUTTON_START);
}

void VirtualStick::dealEvent(SDL_Event& e)
{
    auto touch = SDL_GetTouchDevice(0);
    if (!touch)
    {
        return;
    }
    int fingers = SDL_GetNumTouchFingers(SDL_GetTouchDevice(0));
    auto engine = Engine::getInstance();
    engine->clearGameControllerButton();
    //fmt1::print("{} {}\n", a, b);
    for (int i = 0; i < fingers; i++)
    {
        auto s = SDL_GetTouchFinger(touch, i);
        //fmt1::print("{}: {} {} ", i, s->x, s->y);

        for (auto c : childs_)
        {
            auto b = std::dynamic_pointer_cast<Button>(c);
            if (b && b->inSide(s->x * w_, s->y * h_))
            {
                b->state_ = NodePress;
                engine->setGameControllerButton(b->button_id_, 1);
            }
        }
    }
}

void VirtualStick::draw()
{
    RunNode::draw();
}
