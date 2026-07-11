#include "RunNode.h"
#include "Audio.h"
#include "Font.h"
#include "GameUtil.h"
#include "UISystem.h"
#include <cassert>

std::vector<std::shared_ptr<RunNode>> RunNode::root_;
double RunNode::global_prev_present_ticks_ = 0;
double RunNode::refresh_interval_ = 16.666666;
int RunNode::render_message_ = 0;
std::vector<RunNode*> RunNode::run_owner_stack_;
uint64_t RunNode::ownership_epoch_ = 0;
std::weak_ptr<RunNode> RunNode::pointer_capture_;
PointerIdentity RunNode::pointer_capture_identity_{};
std::weak_ptr<RunNode> RunNode::pointer_activation_owner_;
PointerActivationSequence RunNode::pointer_activation_sequence_;

RunNode::~RunNode()
{
}

void RunNode::drawAll()
{
    //从最后一个独占屏幕的场景开始画
    int begin_base = 0;
    for (int i = 0; i < root_.size(); i++)    //记录最后一个全屏的层
    {
        root_[i]->backRunSelfChilds();
        if (root_[i]->full_window_)
        {
            begin_base = i;
        }
    }
    for (int i = begin_base; i < root_.size(); i++)    //从最后一个全屏层开始画
    {
        root_[i]->drawSelfChilds();
    }
}

//从绘制的根节点移除
std::shared_ptr<RunNode> RunNode::removeFromDraw(std::shared_ptr<RunNode> element)
{
    if (element == nullptr)
    {
        if (!root_.empty())
        {
            element = root_.back();
            cancelPointerCaptureForSubtree(element.get());
            root_.pop_back();
            return element;
        }
    }
    else
    {
        for (int i = 0; i < root_.size(); i++)
        {
            if (root_[i] == element)
            {
                cancelPointerCaptureForSubtree(element.get());
                root_.erase(root_.begin() + i);
                return element;
            }
        }
    }
    return nullptr;
}

//添加子节点
void RunNode::addChild(std::shared_ptr<RunNode> element)
{
    element->setTag(childs_.size());
    childs_.push_back(element);
}

//添加节点并同时设置子节点的位置
void RunNode::addChild(std::shared_ptr<RunNode> element, int x, int y)
{
    addChild(element);
    element->setPosition(x_ + x, y_ + y);
}

//移除某个节点
void RunNode::removeChild(std::shared_ptr<RunNode> element)
{
    for (int i = 0; i < childs_.size(); i++)
    {
        if (childs_[i] == element)
        {
            cancelPointerCaptureForSubtree(element.get());
            childs_.erase(childs_.begin() + i);
            break;
        }
    }
}

//清除子节点
void RunNode::clearChilds()
{
    for (const auto& child : childs_)
    {
        cancelPointerCaptureForSubtree(child.get());
    }
    childs_.clear();
}

//设置位置，会改变子节点的位置
void RunNode::setPosition(int x, int y)
{
    for (auto c : childs_)
    {
        c->setPosition(c->x_ + x - x_, c->y_ + y - y_);
    }
    x_ = x;
    y_ = y;
}

void RunNode::setAllChildState(int s)
{
    for (auto c : childs_)
    {
        c->state_ = s;
    }
}

void RunNode::setAllChildVisible(bool v)
{
    for (auto c : childs_)
    {
        c->setVisible(v);
    }
}

int RunNode::findNextVisibleChild(int i0, Direct direct)
{
    if (direct == DIrectNone || childs_.size() == 0)
    {
        return i0;
    }
    auto current = getChild(i0);

    double min1 = 9999, min2 = 9999 * 10;
    int i1 = i0, i2 = i0;
    //1表示平行于按键方向上的距离，2表示垂直于按键方向上的距离
    for (int i = 0; i < childs_.size(); i++)
    {
        if (i == i0 || childs_[i]->visible_ == false)
        {
            continue;
        }
        auto c = childs_[i];
        int dis1, dis2;
        double deg;
        switch (direct)
        {
        case DirectLeft:
            dis1 = current->x_ - c->x_;
            dis2 = abs(c->y_ - current->y_);
            break;
        case DirectUp:
            dis1 = current->y_ - c->y_;
            dis2 = abs(c->x_ - current->x_);
            break;
        case DirectRight:
            dis1 = c->x_ - current->x_;
            dis2 = abs(c->y_ - current->y_);
            break;
        case DirectDown:
            dis1 = c->y_ - current->y_;
            dis2 = abs(c->x_ - current->x_);
            break;
        default:
            break;
        }
        if (dis1 <= 0)
        {
            continue;
        }
        deg = atan2(dis2, dis1) * 180 / 3.14159265;
        //LOG("{} {} {} \n", dis1, dis2, deg);
        if (dis1 < min1 && deg < 15)
        {
            min1 = dis1;
            i1 = i;
        }
        if (dis1 + deg * 20 < min2)
        {
            min2 = dis1 + deg * 20;
            i1 = i;
        }
    }
    if (i1 != i0) { return i1; }
    return i2;
}

int RunNode::findFristVisibleChild()
{
    for (int i = 0; i < childs_.size(); i++)
    {
        if (childs_[i]->visible_)
        {
            return i;
        }
    }
    return -1;
}

void RunNode::forceActiveChild()
{
    for (int i = 0; i < childs_.size(); i++)
    {
        childs_[i]->setState(NodeNormal);
        if (i == active_child_)
        {
            childs_[i]->setState(NodePass);
        }
    }
}

//检测
void RunNode::checkActiveToResult()
{
    result_ = -1;
    int r = checkChildState();
    if (r == active_child_)
    {
        result_ = active_child_;
    }
}

void RunNode::checkFrame()
{
    if (stay_frame_ > 0 && current_frame_ >= stay_frame_)
    {
        setExit(true);
    }
}

bool RunNode::isPressOK(EngineEvent& e)
{
    bool ret = (e.type == EVENT_KEY_UP && (e.key.key == K_RETURN || e.key.key == K_SPACE))
        || (e.type == EVENT_GAMEPAD_BUTTON_UP && e.gbutton.button == GAMEPAD_BUTTON_SOUTH);
    return ret;
}

bool RunNode::isPressCancel(EngineEvent& e)
{
    bool ret = (e.type == EVENT_KEY_UP && e.key.key == K_ESCAPE)
        || (e.type == EVENT_GAMEPAD_BUTTON_UP && e.gbutton.button == GAMEPAD_BUTTON_EAST)
        || PointerInput::instance().isApplicationCancelEvent(e);
    return ret;
}

void RunNode::setExit(bool e)
{
    if (e && !exit_)
    {
        cancelPointerCaptureForSubtree(this);
    }
    if (e && !exit_ && running_)
    {
        ++ownership_epoch_;
    }
    exit_ = e;
}

void RunNode::setVisible(bool visible)
{
    if (!visible && visible_)
    {
        cancelPointerCaptureForSubtree(this);
    }
    visible_ = visible;
}

void RunNode::invalidatePointerOwnership()
{
    Engine::getInstance()->cancelImGuiPrimaryTouch();
    PointerInput::instance().rejectActiveTouchContacts();
    cancelPointerCapture();
    if (!run_owner_stack_.empty())
    {
        run_owner_stack_.back()->onPointerInputReset();
    }
    ++ownership_epoch_;
}

void RunNode::cancelPointerCapture()
{
    cancelPointerActivationCapture();
    auto captured = pointer_capture_.lock();
    if (!captured)
    {
        pointer_capture_.reset();
        return;
    }
    pointer_capture_.reset();
    PointerEvent cancel;
    cancel.phase = PointerPhase::Cancel;
    cancel.source = pointer_capture_identity_.source;
    cancel.pointerId = pointer_capture_identity_.pointerId;
    cancel.button = pointer_capture_identity_.button;
    cancel.uiPosition = PointerInput::instance().logicalPointerUiPosition();
    captured->onPointerEvent(cancel);
    if (!run_owner_stack_.empty())
    {
        run_owner_stack_.back()->bubblePointerEventPath(captured, cancel);
    }
}

void RunNode::cancelPointerActivationCapture()
{
    pointer_activation_owner_.reset();
    pointer_activation_sequence_.reset();
}

void RunNode::cancelPointerCaptureForSubtree(const RunNode* subtree)
{
    auto captured = pointer_capture_.lock();
    const bool targetCaptureInvalidated = pointerCaptureInvalidationNeeded(
        static_cast<bool>(captured),
        captured && subtree->containsPointerTarget(captured));
    auto activationOwner = pointer_activation_owner_.lock();
    const bool activationCaptureInvalidated = activationOwner
        && subtree->containsPointerTarget(activationOwner);
    if (targetCaptureInvalidated || activationCaptureInvalidated)
    {
        const auto activationIdentity = pointer_activation_sequence_.identity();
        const bool rejectContacts = (targetCaptureInvalidated
                && pointerCaptureContactRejectionNeeded(pointer_capture_identity_.source))
            || (activationCaptureInvalidated && activationIdentity
                && pointerCaptureContactRejectionNeeded(activationIdentity->source));
        cancelPointerCapture();
        if (rejectContacts)
        {
            Engine::getInstance()->cancelImGuiPrimaryTouch();
            PointerInput::instance().rejectActiveTouchContacts();
        }
    }
}

RunNode::PointerResult RunNode::onPointerEvent(const PointerEvent& event)
{
    if (event.phase == PointerPhase::Motion)
    {
        state_ = inSideUi(event.uiPosition.x, event.uiPosition.y) ? NodePass : NodeNormal;
        return state_ == NodePass ? PointerResult::Handled : PointerResult::Ignored;
    }
    if (event.button == SDL_BUTTON_LEFT && event.phase == PointerPhase::ButtonDown
        && inSideUi(event.uiPosition.x, event.uiPosition.y))
    {
        state_ = NodePress;
        return PointerResult::Captured;
    }
    if (event.button == SDL_BUTTON_LEFT
        && (event.phase == PointerPhase::ButtonUp || event.phase == PointerPhase::Cancel))
    {
        state_ = event.phase == PointerPhase::ButtonUp
            && inSideUi(event.uiPosition.x, event.uiPosition.y)
            ? NodePress
            : NodeNormal;
        return PointerResult::Released;
    }
    return PointerResult::Ignored;
}

std::shared_ptr<RunNode> RunNode::findPointerTarget(SDL_FPoint uiPoint)
{
    if (!visible_ && !full_window_)
    {
        return nullptr;
    }
    for (int i = static_cast<int>(childs_.size()) - 1; i >= 0; --i)
    {
        if (auto target = childs_[i]->findPointerTarget(uiPoint))
        {
            return target;
        }
    }
    if (inSideUi(uiPoint.x, uiPoint.y))
    {
        return shared_from_this();
    }
    return nullptr;
}

bool RunNode::collectPointerTargetPath(
    const std::shared_ptr<RunNode>& target,
    std::vector<std::shared_ptr<RunNode>>& path)
{
    if (shared_from_this() == target)
    {
        path.push_back(shared_from_this());
        return true;
    }
    for (auto& child : childs_)
    {
        if (child->collectPointerTargetPath(target, path))
        {
            path.push_back(shared_from_this());
            return true;
        }
    }
    return false;
}

std::optional<RunNode::RoutedPointerEvent> RunNode::routePointerEventToAncestors(
    const std::shared_ptr<RunNode>& target,
    const PointerEvent& event)
{
    std::vector<std::shared_ptr<RunNode>> path;
    const bool found = collectPointerTargetPath(target, path);
    assert(found);
    const auto routed = routePointerEventPath(path.size(), [&](std::size_t index)
    {
        return path[index]->onPointerEvent(event);
    });
    if (!routed)
    {
        return std::nullopt;
    }
    return RoutedPointerEvent{path[routed->first], routed->second};
}

bool RunNode::notifyPointerActivationPath(const std::shared_ptr<RunNode>& target)
{
    if (shared_from_this() == target)
    {
        onPressedOK();
        return true;
    }
    for (auto& child : childs_)
    {
        if (child->notifyPointerActivationPath(target))
        {
            onPressedOK();
            return true;
        }
    }
    return false;
}

bool RunNode::bubblePointerEventPath(const std::shared_ptr<RunNode>& target, const PointerEvent& event)
{
    if (shared_from_this() == target)
    {
        return true;
    }
    for (auto& child : childs_)
    {
        if (child->bubblePointerEventPath(target, event))
        {
            onPointerEvent(event);
            return true;
        }
    }
    return false;
}

bool RunNode::containsPointerTarget(const std::shared_ptr<RunNode>& target) const
{
    if (target.get() == this) return true;
    for (const auto& child : childs_)
    {
        if (child->containsPointerTarget(target)) return true;
    }
    return false;
}

void RunNode::clearPointerHoverSelfChilds()
{
    for (auto& child : childs_)
    {
        child->clearPointerHoverSelfChilds();
    }
    if (state_ == NodePass)
    {
        state_ = NodeNormal;
    }
}

void RunNode::dispatchPointerEvent(const PointerEvent& event)
{
    const PointerIdentity identity{event.source, event.pointerId, event.button};
    if (event.source == PointerSource::Mouse && event.button == SDL_BUTTON_RIGHT
        && event.phase == PointerPhase::ButtonUp)
    {
        onPressedCancel();
        return;
    }

    if (event.phase == PointerPhase::Motion
        && pointer_capture_.expired()
        && !pointer_activation_sequence_.active())
    {
        clearPointerHoverSelfChilds();
    }

    if (pointer_activation_sequence_.active())
    {
        auto activationOwner = pointer_activation_owner_.lock();
        if (!activationOwner
            || activationOwner.get() != this
            || !activationOwner->visible_
            || activationOwner->exit_
            || !containsPointerTarget(activationOwner))
        {
            cancelPointerActivationCapture();
        }
        else
        {
            const auto action = pointer_activation_sequence_.process(event);
            if (action == PointerActivationAction::Hold)
            {
                return;
            }
            if (action == PointerActivationAction::Activate)
            {
                pointer_activation_owner_.reset();
                activationOwner->onPressedOK();
                return;
            }
            if (action == PointerActivationAction::Cancel)
            {
                pointer_activation_owner_.reset();
                return;
            }
        }
    }

    if (event.phase == PointerPhase::ButtonDown)
    {
        auto target = findPointerTarget(event.uiPosition);
        if (!target)
        {
            target = shared_from_this();
        }
        const auto routed = routePointerEventToAncestors(target, event);
        if (routed && routed->result == PointerResult::Captured)
        {
            pointer_capture_ = routed->target;
            pointer_capture_identity_ = identity;
            bubblePointerEventPath(routed->target, event);
            if (!routed->target->visible_ || routed->target->exit_
                || !containsPointerTarget(routed->target))
            {
                cancelPointerCapture();
            }
        }
        else if (pointerActivationSequenceShouldStart(
            pointerActivationScope(),
            event,
            routed.has_value()))
        {
            const bool started = pointer_activation_sequence_.start(event);
            assert(started);
            pointer_activation_owner_ = shared_from_this();
        }
        return;
    }

    if (auto captured = pointer_capture_.lock())
    {
        if (!captured->visible_ || captured->exit_ || !containsPointerTarget(captured))
        {
            cancelPointerCapture();
            return;
        }
        if (pointer_capture_identity_.source == identity.source
            && pointer_capture_identity_.pointerId == identity.pointerId
            && (event.phase == PointerPhase::Motion || pointer_capture_identity_.button == identity.button))
        {
            const auto result = captured->onPointerEvent(event);
            bubblePointerEventPath(captured, event);
            if (event.phase == PointerPhase::ButtonUp || event.phase == PointerPhase::Cancel)
            {
                pointer_capture_.reset();
                if (result == PointerResult::Released
                    && event.phase == PointerPhase::ButtonUp
                    && captured->inSideUi(event.uiPosition.x, event.uiPosition.y))
                {
                    notifyPointerActivationPath(captured);
                }
            }
            return;
        }
    }

    if (event.phase == PointerPhase::Wheel)
    {
        auto target = findPointerTarget(event.uiPosition);
        if (!target)
        {
            target = shared_from_this();
        }
        routePointerEventToAncestors(target, event);
        return;
    }

    auto target = findPointerTarget(event.uiPosition);
    if (!target)
    {
        target = shared_from_this();
    }
    target->onPointerEvent(event);
}

void RunNode::handleLegacyGlobalEvent(const EngineEvent& event)
{
    if (event.type == EVENT_WINDOW_RESIZED)
    {
        Engine::getInstance()->commitPresentPosition(
            Engine::getInstance()->getMainTexture(),
            event);
        invalidatePointerOwnership();
    }
    if (event.type == EVENT_WINDOW_PIXEL_SIZE_CHANGED
        || event.type == EVENT_WINDOW_MAXIMIZED
        || event.type == EVENT_WINDOW_RESTORED)
    {
        invalidatePointerOwnership();
    }
    if (event.type == EVENT_WINDOW_HIDDEN || event.type == EVENT_WINDOW_FOCUS_LOST)
    {
        Engine::getInstance()->cancelImGuiPrimaryTouch();
#ifdef __EMSCRIPTEN__
        PointerInput::instance().resetTouchState();
#else
        PointerInput::instance().rejectActiveTouchContacts();
#endif
        cancelPointerCapture();
        if (!run_owner_stack_.empty()) run_owner_stack_.back()->onPointerInputReset();
        ++ownership_epoch_;
    }
    if (event.type == EVENT_DID_ENTER_BACKGROUND)
    {
        Audio::getInstance()->pauseMusic();
        Engine::getInstance()->cancelImGuiPrimaryTouch();
#ifdef __EMSCRIPTEN__
        PointerInput::instance().resetTouchState();
#else
        PointerInput::instance().rejectActiveTouchContacts();
#endif
        cancelPointerCapture();
        if (!run_owner_stack_.empty()) run_owner_stack_.back()->onPointerInputReset();
        ++ownership_epoch_;
    }
    if (event.type == EVENT_DID_ENTER_FOREGROUND)
    {
        Audio::getInstance()->resumeMusic();
    }
    if (event.type == EVENT_GAMEPAD_ADDED || event.type == EVENT_GAMEPAD_REMOVED)
    {
        LOG("Controllers changed\n");
        Engine::getInstance()->checkGameControllers();
    }
    if (event.type == EVENT_QUIT || event.type == EVENT_WINDOW_CLOSE_REQUESTED)
    {
        UISystem::askExit(1);
    }
}

//画出自身和子节点
void RunNode::drawSelfChilds()
{
    if (visible_)
    {
        if (dark_)
        {
            //画上一层半透明黑，用于某些UI
            Engine::getInstance()->fillColor({ 0, 0, 0, 128 }, 0, 0, -1, -1);
        }
        draw();
        for (auto c : childs_)
        {
            if (c->visible_)
            {
                c->drawSelfChilds();
            }
        }
    }
}

//处理自身的事件响应
//只处理当前的节点和当前节点的子节点，检测鼠标是否在范围内
//注意全屏类的节点要一直接受事件
void RunNode::checkStateSelfChilds(EngineEvent& e, bool check_event)
{
    //if (exit_) { return; }
    if (visible_ || full_window_)
    {
        //注意这里是反向
        for (int i = childs_.size() - 1; i >= 0; i--)
        {
            childs_[i]->checkStateSelfChilds(e, check_event);
        }
        if (check_event)
        {
            checkSelfState(e);
            int r = checkChildState();
            if (r >= 0)
            {
                active_child_ = r;
            }
            //可以在dealEvent中改变原有状态，强制设置某些情况
            if (deal_event_)
            {
                dealEvent(e);
                //为简化代码，将按下回车和ESC的操作写在此处
                if (isPressOK(e))
                {
                    onPressedOK();
                    //setAllChildState(Normal);
                }
                if (isPressCancel(e))
                {
                    onPressedCancel();
                    //setAllChildState(Normal);
                }
            }
        }
        dealEvent2(e);
    }
    else
    {
        state_ = NodeNormal;
    }
}

void RunNode::backRunSelfChilds()
{
    for (auto c : childs_)
    {
        c->backRunSelfChilds();
        if (c->stay_frame_ > 0 && c->current_frame_ > c->stay_frame_)
        {
            removeChild(c);
        }
    }
    backRun();
    current_frame_++;    //此处统计帧数
}

//检测事件
void RunNode::dealEventSelfChilds(bool check_event)
{
    auto& input = PointerInput::instance();
    input.pumpSdlEvents();

    if (!check_event)
    {
        while (!input.empty() && input.frontIsPointer())
        {
            const auto event = input.popPending();
            if (event.type == EVENT_FINGER_DOWN
                || event.type == EVENT_FINGER_MOTION
                || event.type == EVENT_FINGER_UP
                || event.type == SDL_EVENT_FINGER_CANCELED)
            {
                input.processFingerEvent(event);
            }
        }
        input.rejectActiveTouchContacts();
        if (!input.empty())
        {
            const auto event = input.popPending();
            handleLegacyGlobalEvent(event);
            if (input.isApplicationCancelEvent(event))
            {
                Engine::getInstance()->processImGuiApplicationCancel();
            }
            else
            {
                Engine::getInstance()->processImGuiEvent(event);
            }
        }
        while (!input.empty() && input.frontIsPointer())
        {
            const auto event = input.popPending();
            if (event.type == EVENT_FINGER_DOWN
                || event.type == EVENT_FINGER_MOTION
                || event.type == EVENT_FINGER_UP
                || event.type == SDL_EVENT_FINGER_CANCELED)
            {
                input.processFingerEvent(event);
            }
        }
        input.rejectActiveTouchContacts();
        cancelPointerCapture();
        onPointerInputReset();
        Engine::getInstance()->cancelImGuiPrimaryTouch();
        ++ownership_epoch_;
        return;
    }

    RunNode* const frameOwner = run_owner_stack_.empty() ? nullptr : run_owner_stack_.back();
    const PointerDispatchRevision frameRevision{
        ownership_epoch_,
        frameOwner ? frameOwner->controlLayoutRevision() : 0,
    };
    auto ownerStillValid = [&]()
    {
        return !run_owner_stack_.empty()
            && run_owner_stack_.back() == frameOwner
            && pointerDispatchCanContinue(
                frameRevision,
                {ownership_epoch_, frameOwner->controlLayoutRevision()})
            && !exit_;
    };

    while (!input.empty() && input.frontIsPointer())
    {
        const auto event = input.popPending();
        if (isResidualSyntheticPointerEvent(event))
        {
            continue;
        }

        if (event.type == EVENT_MOUSE_MOTION
            || event.type == EVENT_MOUSE_BUTTON_DOWN
            || event.type == EVENT_MOUSE_BUTTON_UP
            || event.type == EVENT_MOUSE_WHEEL)
        {
            const bool consumedByImGui = Engine::getInstance()->processImGuiEvent(event);
            const auto pointer = input.makeMousePointerEvent(event);
            if (!consumedByImGui)
            {
                dispatchPointerEvent(pointer);
            }
        }
        else if (auto finger = input.processFingerEvent(event))
        {
            if (finger->primary)
            {
                const bool captured = Engine::getInstance()->processImGuiPrimaryTouch(finger->sample);
                if (finger->sample.phase == TouchPhase::Down)
                {
                    input.setImGuiOwnsTouchSequence(captured);
                }
            }

            if (!input.imguiOwnsTouchSequence())
            {
                if (touchPolicy() == TouchPolicy::DirectTouch)
                {
                    onTouchSample(finger->sample);
                }
                else if (finger->primaryGameEligible)
                {
                    dispatchPointerEvent(input.makeTouchPointerEvent(finger->sample));
                }
            }
            if (finger->physicalSequenceEnded)
            {
                input.setImGuiOwnsTouchSequence(false);
            }
        }

        if (!ownerStillValid())
        {
            return;
        }
    }

    EngineEvent updateEvent = {};
    updateEvent.type = EVENT_FIRST;
    if (!input.empty())
    {
        const auto event = input.popPending();
        handleLegacyGlobalEvent(event);
        const bool consumedByImGui = input.isApplicationCancelEvent(event)
            ? Engine::getInstance()->processImGuiApplicationCancel()
            : Engine::getInstance()->processImGuiEvent(event);
        if (consumedByImGui && input.isApplicationCancelEvent(event))
        {
            Engine::getInstance()->cancelImGuiPrimaryTouch();
            input.rejectActiveTouchContacts();
            cancelPointerCapture();
            onPointerInputReset();
            ++ownership_epoch_;
        }
        if (!consumedByImGui)
        {
            updateEvent = event;
        }
    }
    if (frameOwner == (run_owner_stack_.empty() ? nullptr : run_owner_stack_.back()) && !exit_)
    {
        checkStateSelfChilds(updateEvent, true);
    }
}

//获取子节点的状态
int RunNode::checkChildState()
{
    int r = -1;
    for (int i = 0; i < getChildCount(); i++)
    {
        if (getChild(i)->getState() != NodeNormal)
        {
            r = i;
        }
    }
    return r;
}

void RunNode::checkSelfState(EngineEvent& e)
{
    if ((e.type == EVENT_KEY_DOWN || e.type == EVENT_KEY_UP) && (e.key.key == K_RETURN || e.key.key == K_SPACE))
    {
        //按下键盘的空格或者回车时，将pass的按键改为press
        if (state_ == NodePass)
        {
            state_ = NodePress;
        }
    }
    if ((e.type == EVENT_GAMEPAD_BUTTON_DOWN || e.type == EVENT_GAMEPAD_BUTTON_UP) && e.gbutton.button == GAMEPAD_BUTTON_SOUTH)
    {
        if (state_ == NodePass)
        {
            state_ = NodePress;
        }
    }
}

void RunNode::present()
{
    auto t = Engine::getTicks() - global_prev_present_ticks_;
    auto e = Engine::getInstance();
    if (render_message_)
    {
        int w = e->getUIWidth();
        int h = e->getUIHeight();
        Font::getInstance()->draw(std::format("Render one frame in {:.3f} ms", t), 20, w - 300, h - 60);
        Font::getInstance()->draw(std::format("Render texture time is {}", Engine::getInstance()->getRenderTimes()), 20, w - 300, h - 35);
        e->resetRenderTimes();
    }
    e->renderMainTextureToWindow();
    Font::getInstance()->executeDrawCalls();
    e->renderImGuiOverlay();
    e->renderPresent();
    e->setRenderMainTexture();

    static double time_debt = 0.0;
    auto t_delay = refresh_interval_ - t - time_debt;
    if (t_delay > 0)
    {
        Engine::delay(t_delay);
        time_debt = 0.0;
    }
    else
    {
        // time_debt = -t_delay;
        // if (time_debt > refresh_interval_) { time_debt = refresh_interval_; }
    }
    global_prev_present_ticks_ = Engine::getTicks();
}

//运行本节点，参数为是否在root中运行，为真则参与绘制，为假则不会被画出
int RunNode::run(bool in_root /*= true*/)
{
    if (!root_.empty() && root_.back()->exit_) { return 1; }    //若当前根节点已经退出，则不再运行
    exit_ = false;
    visible_ = true;
    if (in_root)
    {
        addIntoDrawTop(shared_from_this());
    }
    if (!run_owner_stack_.empty())
    {
        cancelPointerCapture();
        run_owner_stack_.back()->onPointerInputReset();
    }
    PointerInput::instance().rejectActiveTouchContacts();
    Engine::getInstance()->cancelImGuiPrimaryTouch();
    run_owner_stack_.push_back(this);
    ++ownership_epoch_;
    onEntrance();
    running_ = true;
    while (true)
    {
        if (root_.empty())
        {
            break;
        }
        if (exit_)
        {
            break;
        }
        dealEventSelfChilds(true);
        if (exit_)
        {
            break;
        }
        drawAll();
        checkFrame();
        present();
    }
    running_ = false;
    cancelPointerCaptureForSubtree(this);
    PointerInput::instance().rejectActiveTouchContacts();
    Engine::getInstance()->cancelImGuiPrimaryTouch();
    onPointerInputReset();
    onExit();
    assert(!run_owner_stack_.empty() && run_owner_stack_.back() == this);
    run_owner_stack_.pop_back();
    ++ownership_epoch_;
    GameUtil::lastDialogDismissTime() = Engine::getTicks();
    if (in_root)
    {
        removeFromDraw(shared_from_this());
    }
    return result_;
}

void RunNode::exitAll(int begin)
{
    for (int i = begin; i < root_.size(); i++)
    {
        root_[i]->setExit(true);
        root_[i]->setVisible(false);
    }
}

//专门用来某些情况下动画的显示和延时
//中间可以插入一个函数补充些什么，想不到更好的方法了
int RunNode::drawAndPresent(int times, std::function<void(void*)> func, void* data)
{
    if (times < 1)
    {
        return 0;
    }
    if (times > 100)
    {
        times = 100;
    }
    for (int i = 0; i < times; i++)
    {
        dealEventSelfChilds(false);
        drawAll();
        if (func)
        {
            func(data);
        }
        present();
        if (exit_)
        {
            break;
        }
    }
    return times;
}
