#include "VirtualStick.h"
#include "Button.h"
#include "GameUtil.h"
#include "Menu.h"
#include "ShowExp.h"
#include "TextureManager.h"

#include "BattleSceneHades.h"
#include "BattleSceneSekiro.h"
#include "MainScenePaper.h"

VirtualStick::VirtualStick()
{
    Engine::getInstance()->getUISize(w_, h_);

    auto addButton = [this](std::shared_ptr<Button>& b, int id, int x, int y, int buttonid)
    {
        b = std::make_shared<Button>("title", id);
        b->setAlpha(128);
        b->button_id_ = buttonid;
        addChild(b, x, y);
    };

    {
        int x = w_ * 0.1;
        int y = h_ * 0.65;
        int r = h_ * 0.1;
        addButton(button_up_, 312, x, y - r, SDL_GAMEPAD_BUTTON_DPAD_UP);
        addButton(button_down_, 312, x, y + r, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
        addButton(button_left_, 312, x - r, y, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
        addButton(button_right_, 312, x + r, y, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
    }

    {
        int x = w_ * 0.8;
        int y = h_ * 0.65;
        int r = h_ * 0.15;
        addButton(button_a_, 300, x, y + r, GAMEPAD_BUTTON_SOUTH);
        addButton(button_b_, 302, x + r, y, GAMEPAD_BUTTON_EAST);
        addButton(button_x_, 304, x - r, y, GAMEPAD_BUTTON_WEST);
        addButton(button_y_, 306, x, y - r, GAMEPAD_BUTTON_NORTH);
    }
    //addButton(button_lb_, 300, w_ * 0.1, h_ * 0.5, GAMEPAD_BUTTON_LEFT_SHOULDER);
    //addButton(button_rb_, 302, w_ * 0.1, h_ * 0.8, GAMEPAD_BUTTON_RIGHT_SHOULDER);
    addButton(button_left_axis_, 320, w_ * 0.04, h_ * 0.5, GAMEPAD_BUTTON_LEFT_STICK);
    addButton(button_right_axis_, 320, w_ * 0.82, h_ * 0.08, GAMEPAD_BUTTON_RIGHT_STICK);
    button_left_axis_->setVisible(false);
    button_right_axis_->setVisible(false);

    addButton(button_view_, 310, w_ * 0.4 - 25, h_ * 0.9, GAMEPAD_BUTTON_BACK);
    addButton(button_menu_, 308, w_ * 0.6 - 25, h_ * 0.9, GAMEPAD_BUTTON_START);
    updateLayout();
}

void VirtualStick::onWindowResized()
{
    updateLayout();
    RunNode::onWindowResized();
}

void VirtualStick::updateLayout()
{
    Engine::getInstance()->getUISize(w_, h_);
    int x = static_cast<int>(w_ * 0.1);
    int y = static_cast<int>(h_ * 0.65);
    int r = static_cast<int>(h_ * 0.1);
    button_up_->setPosition(x, y - r);
    button_down_->setPosition(x, y + r);
    button_left_->setPosition(x - r, y);
    button_right_->setPosition(x + r, y);

    x = static_cast<int>(w_ * 0.8);
    y = static_cast<int>(h_ * 0.65);
    r = static_cast<int>(h_ * 0.15);
    button_a_->setPosition(x, y + r);
    button_b_->setPosition(x + r, y);
    button_x_->setPosition(x - r, y);
    button_y_->setPosition(x, y - r);

    button_left_axis_->setPosition(static_cast<int>(w_ * 0.04), static_cast<int>(h_ * 0.5));
    button_right_axis_->setPosition(static_cast<int>(w_ * 0.82), static_cast<int>(h_ * 0.08));
    button_view_->setPosition(static_cast<int>(w_ * 0.4 - 25), static_cast<int>(h_ * 0.9));
    button_menu_->setPosition(static_cast<int>(w_ * 0.6 - 25), static_cast<int>(h_ * 0.9));

    auto t = TextureManager::getInstance()->getTexture("title", 320);
    left_axis_center_x_ = static_cast<int>(t->w / 2 + w_ * 0.04);
    left_axis_center_y_ = static_cast<int>(t->h / 2 + h_ * 0.5);
    right_axis_center_x_ = static_cast<int>(t->w / 2 + w_ * 0.82);
    right_axis_center_y_ = static_cast<int>(t->h / 2 + h_ * 0.08);
    axis_radius_ = t->w / 2;
}

void VirtualStick::dealEvent(EngineEvent& e)
{
    auto battle_act = RunNode::getPointerFromRoot<BattleSceneAct>();
    auto battle = RunNode::getPointerFromRoot<BattleScene>();
    auto main_paper = RunNode::getPointerFromRoot<MainScenePaper>();
    auto show_exp = RunNode::getPointerFromRoot<ShowExp>();
    bool is_action = battle_act != nullptr;
    bool is_paper = is_action && battle_act->usePaperPresentation();
    bool is_turn_based_paper = battle && !is_action
        && GameUtil::getInstance()->getInt("game", "battle_presentation", 0) != 0;
    bool is_any_paper = is_paper || is_turn_based_paper || main_paper != nullptr;
    bool is_menu_active = RunNode::getPointerFromRoot<MenuText>() != nullptr;
    setStyle(is_action ? 1 : 0);
    int num = 0;
    auto engine = Engine::getInstance();
    engine->setTouchPinchScale(1.0f);
    engine->setTouchCameraPan(0, 0);
    engine->clearGameControllerButton();
    engine->clearGameControllerAxis();
    left_axis_x_ = 0;
    left_axis_y_ = 0;
    right_axis_x_ = 0;
    right_axis_y_ = 0;
    if (axis_radius_ == 0)
    {
        auto t = TextureManager::getInstance()->getTexture("title", 320);
        left_axis_center_x_ = t->w / 2 + w_ * 0.04;
        left_axis_center_y_ = t->h / 2 + h_ * 0.5;
        right_axis_center_x_ = t->w / 2 + w_ * 0.82;
        right_axis_center_y_ = t->h / 2 + h_ * 0.08;
        axis_radius_ = t->w / 2;
    }
    //LOG("{}", "clear button");
    for (auto c : childs_)
    {
        auto b = std::dynamic_pointer_cast<Button>(c);
        b->state_ = NodeNormal;
    }
    auto touch = SDL_GetTouchDevices(&num);
    bool pinch_active = false;
    bool is_press = false;
    bool is_press2 = false;    //将按键范围扩大一点，避免按到键中间被当成鼠标操作
    std::vector<std::pair<int, int>> pinch_points;
    for (int i_device = 0; i_device < num; i_device++)
    {
        if (!touch)
        {
            break;
        }
        int fingers = 0;
        auto finger_pp = SDL_GetTouchFingers(touch[i_device], &fingers);
        if (!finger_pp)
        {
            continue;
        }
        for (int i = 0; i < fingers; i++)
        {
            auto s = finger_pp[i];
            //LOG("{}: {} {} ", i, s->x * w_, s->y * h_);
            int windowW = 0;
            int windowH = 0;
            engine->getWindowSize(windowW, windowH);
            int x = 0;
            int y = 0;
            if (!engine->windowToUISpace(
                static_cast<int>(s->x * windowW),
                static_cast<int>(s->y * windowH),
                x,
                y,
                false))
            {
                continue;
            }
            bool finger_in_control = false;
            for (auto c : childs_)
            {
                auto b = std::dynamic_pointer_cast<Button>(c);
                int x2, y2, w2, h2;
                b->getPosition(x2, y2);
                b->getSize(w2, h2);
                bool in_control = x >= x2 - w2 / 2 && x < x2 + w2 * 3 / 2
                    && y >= y2 - h2 / 2 && y < y2 + h2 * 3 / 2;
                bool in_button = x >= x2 && x < x2 + w2
                    && y >= y2 && y < y2 + h2;
                is_press2 = is_press2 || in_control;
                finger_in_control = finger_in_control || in_control;
                if (b != button_left_axis_ && b != button_right_axis_)
                {
                    if (in_button)
                    {
                        b->state_ = NodePress;
                        engine->setGameControllerButton(b->button_id_, 1);
                        is_press = true;
                    }
                }
                else if (b == button_left_axis_ || b == button_right_axis_)
                {
                    if (b->getVisible())
                    {
                        bool is_left_axis = b == button_left_axis_;
                        int center_x = is_left_axis ? left_axis_center_x_ : right_axis_center_x_;
                        int center_y = is_left_axis ? left_axis_center_y_ : right_axis_center_y_;
                        double r = sqrt((x - center_x) * (x - center_x) + (y - center_y) * (y - center_y));
                        //LOG("{}", r);
                        auto& intval = button_interval_[b];
                        if (r < axis_radius_ * 1.5)
                        {
                            int axis_x = GameUtil::clamp((x - center_x) * 30000 / axis_radius_, -30000, 30000);
                            int axis_y = GameUtil::clamp((y - center_y) * 30000 / axis_radius_, -30000, 30000);
                            if (is_left_axis)
                            {
                                left_axis_x_ = x;
                                left_axis_y_ = y;
                                engine->setGameControllerAxis(SDL_GAMEPAD_AXIS_LEFTX, axis_x);
                                engine->setGameControllerAxis(SDL_GAMEPAD_AXIS_LEFTY, axis_y);
                            }
                            else
                            {
                                right_axis_x_ = x;
                                right_axis_y_ = y;
                                engine->setGameControllerAxis(SDL_GAMEPAD_AXIS_RIGHTX, axis_x);
                                engine->setGameControllerAxis(SDL_GAMEPAD_AXIS_RIGHTY, axis_y);
                            }
                            button_interval_[b].prev_press = engine->getTicks();
                            is_press = true;
                            b->state_ = NodePress;
                            intval.interval = 0;
                            if (is_action || is_paper || is_turn_based_paper)
                            {
                                intval.interval = 0;
                            }
                        }
                    }
                }
            }
            if (!finger_in_control)
            {
                pinch_points.emplace_back(x, y);
            }
        }
        SDL_free(finger_pp);
    }
    SDL_free(touch);
    if (is_any_paper && pinch_points.size() >= 2)
    {
        auto [x0, y0] = pinch_points[0];
        auto [x1, y1] = pinch_points[1];
        double distance = std::hypot(double(x1 - x0), double(y1 - y0));
        FPoint center = { (x0 + x1) * 0.5f, (y0 + y1) * 0.5f };
        if (previous_pinch_distance_ > 0 && distance > 0)
        {
            engine->setTouchPinchScale(std::clamp(float(distance / previous_pinch_distance_), 0.94f, 1.06f));
            engine->setTouchCameraPan(center.x - previous_pinch_center_.x, center.y - previous_pinch_center_.y);
        }
        previous_pinch_distance_ = distance;
        previous_pinch_center_ = center;
        pinch_active = true;
    }
    bool a_pressed = button_a_->state_ == NodePress;
    bool b_pressed = button_b_->state_ == NodePress;
    bool menu_pressed = button_menu_->state_ == NodePress;
    bool view_pressed = button_view_->state_ == NodePress;
    bool a_just_released = !a_pressed && a_touch_pressed_;
    bool b_just_released = !b_pressed && b_touch_pressed_;
    bool menu_just_released = !menu_pressed && menu_touch_pressed_;
    bool view_just_released = !view_pressed && view_touch_pressed_;
    a_touch_pressed_ = a_pressed;
    b_touch_pressed_ = b_pressed;
    menu_touch_pressed_ = menu_pressed;
    view_touch_pressed_ = view_pressed;

    auto sendGamepadButtonUp = [&](int button)
    {
        e.type = EVENT_GAMEPAD_BUTTON_UP;
        e.gbutton.button = button;
    };
    if (show_exp && a_just_released)
    {
        sendGamepadButtonUp(GAMEPAD_BUTTON_SOUTH);
    }
    else if (is_menu_active && (a_just_released || b_just_released))
    {
        sendGamepadButtonUp(a_just_released ? GAMEPAD_BUTTON_SOUTH : GAMEPAD_BUTTON_EAST);
    }
    else if (menu_just_released)
    {
        sendGamepadButtonUp(GAMEPAD_BUTTON_START);
    }
    else if (view_just_released)
    {
        sendGamepadButtonUp(GAMEPAD_BUTTON_BACK);
    }
    else if (!is_action && !is_paper)
    {
        if (a_just_released)
        {
            sendGamepadButtonUp(GAMEPAD_BUTTON_SOUTH);
            button_interval_[button_a_].interval = 100;
        }
        else if (b_just_released)
        {
            sendGamepadButtonUp(GAMEPAD_BUTTON_EAST);
            button_interval_[button_b_].interval = 100;
        }
    }
    if (is_press2)
    {
        prev_press_ = engine->getTicks();
    }
    //一定时间内忽略鼠标事件
    if ((e.type == EVENT_MOUSE_BUTTON_UP || e.type == EVENT_MOUSE_BUTTON_DOWN || e.type == EVENT_MOUSE_MOTION)
        && engine->getTicks() - prev_press_ < 500)
    {
        e.type = EVENT_FIRST;
    }
    if (!pinch_active)
    {
        previous_pinch_distance_ = 0;
        previous_pinch_center_ = {};
    }
}

void VirtualStick::draw()
{
    auto draw_axis = [](int x, int y)
    {
        if (x > 0 && y > 0)
        {
            auto t = TextureManager::getInstance()->getTexture("title", 312);
            TextureManager::getInstance()->renderTexture("title", 312, x - t->w / 2, y - t->h / 2, { { 255, 255, 255, 255 }, 128 });
        }
    };
    draw_axis(left_axis_x_, left_axis_y_);
    draw_axis(right_axis_x_, right_axis_y_);
}

void VirtualStick::setStyle(int style)
{
    if (style == 0)
    {
        button_up_->setVisible(true);
        button_down_->setVisible(true);
        button_left_->setVisible(true);
        button_right_->setVisible(true);
        button_left_axis_->setVisible(false);
        button_right_axis_->setVisible(false);
    }
    else
    {
        button_up_->setVisible(false);
        button_down_->setVisible(false);
        button_left_->setVisible(false);
        button_right_->setVisible(false);
        button_left_axis_->setVisible(true);
        button_right_axis_->setVisible(false);
    }
}
