#include "RunNode.h"
#include "Font.h"
#include "UISystem.h"

std::vector<std::shared_ptr<RunNode>> RunNode::root_;
double RunNode::global_prev_present_ticks_ = 0;
double RunNode::refresh_interval_ = 16.666666;
int RunNode::render_message_ = 0;

RunNode::~RunNode()
{
}

void RunNode::drawAll()
{
    //从最后一个独占屏幕的场景开始画
    int begin_base = 0;
    for (int i = 0; i < root_.size(); i++)    //记录最后一个全屏的层
    {
        root_[i]->backRun();
        root_[i]->current_frame_++;
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
                root_.erase(root_.begin() + i);
                return element;
                break;
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
            childs_.erase(childs_.begin() + i);
            break;
        }
    }
}

//清除子节点
void RunNode::clearChilds()
{
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
        c->visible_ = v;
    }
}

int RunNode::findNextVisibleChild(int i0, Direct direct)
{
    if (direct == DIrectNone || childs_.size() == 0)
    {
        return i0;
    }
    auto current = getChild(i0);

    int min1 = 9999, min2 = 9999 * 2;
    int i1 = i0;
    //1表示按键方向上的距离，2表示垂直于按键方向上的距离
    for (int i = 0; i < childs_.size(); i++)
    {
        if (i == i0 || childs_[i]->visible_ == false)
        {
            continue;
        }
        auto c = childs_[i];
        int dis1, dis2;
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
            dis1 += 10000;
        }
        if (dis1 <= min1 && dis1 + dis2 < min2)
        {
            min1 = (std::min)(min1, dis1);
            min2 = (std::min)(min2, dis1 + dis2);
            i1 = i;
        }
        //以上数字的取法：如有坐标一致的点，不考虑第二距离（好像不太明显，以后再改吧）
    }
    return i1;
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
        exit_ = true;
    }
}

//画出自身和子节点
void RunNode::drawSelfChilds()
{
    if (visible_)
    {
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
void RunNode::checkStateSelfChilds(BP_Event& e, bool check_event)
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
    }
    backRun();
}

//检测事件
void RunNode::dealEventSelfChilds(bool check_event)
{
    if (check_event)
    {
        BP_Event e;
        e.type = BP_FIRSTEVENT;
        //此处这样设计的原因是某些系统下会连续生成一大串事件，如果每个循环仅处理一个会造成响应慢
        while (Engine::getInstance()->pollEvent(e))
        {
            if (isSpecialEvent(e))
            {
                break;
            }
        }
        if (e.type == BP_CONTROLLERAXISMOTION)
        {
            BP_Event e1;
            while (Engine::getInstance()->pollEvent(e1));
            if (e.caxis.axis == SDL_CONTROLLER_AXIS_RIGHTX || e.caxis.axis == SDL_CONTROLLER_AXIS_RIGHTY)
            {
                auto axis_x = Engine::getInstance()->gameControllerGetAxis(BP_CONTROLLER_AXIS_RIGHTX);
                auto axis_y = Engine::getInstance()->gameControllerGetAxis(BP_CONTROLLER_AXIS_RIGHTY);

                int x, y;
                Engine::getInstance()->getMouseStateInStartWindow(x, y);
                //fmt1::print("{} {}  ", axis_x, axis_y);
                if (abs(axis_x) < 5000) { axis_x = 0; }
                if (abs(axis_y) < 5000) { axis_y = 0; }
                if (axis_x != 0 || axis_y != 0)
                {
                    x += axis_x / 500;
                    y += axis_y / 500;
                    int w, h;
                    Engine::getInstance()->getWindowSize(w, h);
                    if (x >= w) { x = w - 1; }
                    if (x < 0) { x = 0; }
                    if (y >= h) { y = h - 1; }
                    if (y < 0) { y = 0; }
                    Engine::getInstance()->setMouseState(x, y);
                    e.type = BP_MOUSEMOTION;
                    e.motion.x = x;
                    e.motion.y = y;
                }
            }
        }
        checkStateSelfChilds(e, check_event);
        if (e.type == BP_QUIT
            || (e.type == BP_WINDOWEVENT && e.window.event == BP_WINDOWEVENT_CLOSE))
        {
            UISystem::askExit(1);
        }
    }
    else
    {
        Engine::pollEvent();
    }
}

//是否为游戏需要处理的类型，避免丢失一些操作
bool RunNode::isSpecialEvent(BP_Event& e)
{
    //fmt1::print("type = {}\n", e.type);
    return  e.type == BP_QUIT
        || e.type == BP_WINDOWEVENT
        || e.type == BP_KEYDOWN
        || e.type == BP_KEYUP
        || e.type == BP_TEXTEDITING
        || e.type == BP_TEXTINPUT
        || e.type == BP_MOUSEMOTION
        || e.type == BP_MOUSEBUTTONDOWN
        || e.type == BP_MOUSEBUTTONUP
        || e.type == BP_MOUSEWHEEL
        || e.type == BP_CONTROLLERAXISMOTION
        || e.type == BP_CONTROLLERBUTTONDOWN
        || e.type == BP_CONTROLLERBUTTONUP;
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

void RunNode::checkSelfState(BP_Event& e)
{
    //检测鼠标经过，按下等状态
    //BP_MOUSEMOTION似乎有些问题，待查
    //fmt1::print("{} ", e.type);
    if (e.type == BP_MOUSEMOTION)
    {
        if (inSide(e.motion.x, e.motion.y))
        {
            state_ = NodePass;
        }
        else
        {
            state_ = NodeNormal;
        }
    }
    if ((e.type == BP_MOUSEBUTTONDOWN || e.type == BP_MOUSEBUTTONUP) && e.button.button == BP_BUTTON_LEFT)
    {
        if (inSide(e.button.x, e.button.y))
        {
            state_ = NodePress;
        }
        else
        {
            state_ = NodeNormal;
        }
    }
    if ((e.type == BP_KEYDOWN || e.type == BP_KEYUP) && (e.key.keysym.sym == BPK_RETURN || e.key.keysym.sym == BPK_SPACE))
    {
        //按下键盘的空格或者回车时，将pass的按键改为press
        if (state_ == NodePass)
        {
            state_ = NodePress;
        }
    }
    if ((e.type == BP_CONTROLLERBUTTONDOWN || e.type == BP_CONTROLLERBUTTONUP) && e.cbutton.button == BP_CONTROLLER_BUTTON_A)
    {
        //int x, y;
        //Engine::getInstance()->getMouseState(x, y);
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
        Font::getInstance()->draw(fmt1::format("Render one frame in {:.3f} ms", t), 20, e->getWindowWidth() - 300, e->getWindowHeight() - 60);
        Font::getInstance()->draw(fmt1::format("RenderCopy time is {}", Engine::getInstance()->getRenderTimes()), 20, e->getWindowWidth() - 300, e->getWindowHeight() - 35);
        e->resetRenderTimes();
    }
    e->renderMainTextureToWindow();
    e->renderPresent();
    e->setRenderMainTexture();
    auto t_delay = refresh_interval_ - t;
    if (t_delay > refresh_interval_)
    {
        t_delay = refresh_interval_;
    }
    //fmt1::print("{}/{}/{} ", t, t_delay, global_prev_present_ticks_);
    if (t_delay > 0)
    {
        Engine::delay(t_delay);
    }
    global_prev_present_ticks_ = Engine::getTicks();
}

//运行本节点，参数为是否在root中运行，为真则参与绘制，为假则不会被画出
int RunNode::run(bool in_root /*= true*/)
{
    exit_ = false;
    visible_ = true;
    if (in_root)
    {
        addIntoDrawTop(shared_from_this());
    }
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
    onExit();
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
        root_[i]->exit_ = true;
        root_[i]->visible_ = false;
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
