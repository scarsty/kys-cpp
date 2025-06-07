#include "RunNode.h"
#include "Audio.h"
#include "Font.h"
#include "GameUtil.h"
#include "UISystem.h"
#include "VirtualStick.h"

std::vector<std::shared_ptr<RunNode>> RunNode::root_;
double RunNode::global_prev_present_ticks_ = 0;
double RunNode::refresh_interval_ = 16.666666;
int RunNode::render_message_ = 1;
int RunNode::use_virtual_stick_ = 0;

static std::shared_ptr<VirtualStick>& virtual_stick()
{
    static std::shared_ptr<VirtualStick> v = std::make_shared<VirtualStick>();
    return v;
}

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
    if (use_virtual_stick_)
    {
        virtual_stick()->drawSelfChilds();
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
        exit_ = true;
    }
}

bool RunNode::isPressOK(EngineEvent& e)
{
    bool ret = (e.type == EVENT_KEY_UP && (e.key.key == K_RETURN || e.key.key == K_SPACE))
        || (e.type == EVENT_MOUSE_BUTTON_UP && e.button.button == BUTTON_LEFT)
        || (e.type == EVENT_GAMEPAD_BUTTON_UP && e.gbutton.button == GAMEPAD_BUTTON_SOUTH);
    return ret;
}

bool RunNode::isPressCancel(EngineEvent& e)
{
    bool ret = (e.type == EVENT_KEY_UP && e.key.key == K_ESCAPE)
        || (e.type == EVENT_MOUSE_BUTTON_UP && e.button.button == BUTTON_RIGHT)
        || (e.type == EVENT_GAMEPAD_BUTTON_UP && e.gbutton.button == GAMEPAD_BUTTON_EAST);
    return ret;
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
    }
    backRun();
}

//检测事件
void RunNode::dealEventSelfChilds(bool check_event)
{
    if (check_event)
    {
        EngineEvent e;
        e.type = EVENT_FIRST;
        //此处这样设计的原因是某些系统下会连续生成一大串事件，如果每个循环仅处理一个会造成响应慢
        while (Engine::getInstance()->pollEvent(e))
        {
            if (isSpecialEvent(e))
            {
                break;
            }
        }
        if (e.type == EVENT_GAMEPAD_AXIS_MOTION)
        {
            //摇杆移动会产生大量事件，暂时不处理
            EngineEvent e1;
            while (Engine::getInstance()->pollEvent(e1)) {};
            //右摇杆控制鼠标，手感有问题，待处理
            //if (e.caxis.axis == SDL_GAMEPAD_AXIS_RIGHTX || e.caxis.axis == SDL_GAMEPAD_AXIS_RIGHTY)
            //{
            //    auto axis_x = Engine::getInstance()->gameControllerGetAxis(GAMEPAD_AXIS_RIGHTX);
            //    auto axis_y = Engine::getInstance()->gameControllerGetAxis(GAMEPAD_AXIS_RIGHTY);
            //
            //    //LOG("{} {}  ", axis_x, axis_y);
            //    if (abs(axis_x) < 10000) { axis_x = 0; }
            //    if (abs(axis_y) < 10000) { axis_y = 0; }
            //    if (axis_x != 0 || axis_y != 0)
            //    {
            //        axis_x = GameUtil::limit(axis_x, -30000, 30000);
            //        axis_y = GameUtil::limit(axis_y, -30000, 30000);
            //        int x, y;
            //        Engine::getInstance()->getMouseStateInStartWindow(x, y);
            //        x += axis_x / 3000;
            //        y += axis_y / 3000;
            //        int w, h;
            //        Engine::getInstance()->getStartWindowSize(w, h);
            //        if (x >= w) { x = w - 1; }
            //        if (x < 0) { x = 0; }
            //        if (y >= h) { y = h - 1; }
            //        if (y < 0) { y = 0; }
            //        Engine::getInstance()->setMouseStateInStartWindow(x, y);
            //        e.type = EVENT_MOUSE_MOTION;
            //        e.motion.x = x;
            //        e.motion.y = y;
            //    }
            //}
        }
        if (e.type == EVENT_DID_ENTER_BACKGROUND)
        {
            //暂停音频
            Audio::getInstance()->pauseMusic();
        }
        if (e.type == EVENT_WILL_ENTER_FOREGROUND)
        {
            //恢复音频
            Audio::getInstance()->continueMusic();
        }
        if (e.type == EVENT_GAMEPAD_ADDED || e.type == EVENT_GAMEPAD_REMOVED)
        {
            LOG("Controllers changed\n");
            Engine::getInstance()->checkGameControllers();
        }
        if (use_virtual_stick_)
        {
            virtual_stick()->dealEvent(e);
        }
        checkStateSelfChilds(e, check_event);
        if (e.type == EVENT_QUIT
            || (e.type == EVENT_WINDOW_CLOSE_REQUESTED))
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
bool RunNode::isSpecialEvent(EngineEvent& e)
{
    //LOG("type = {}\n", e.type);
    return e.type == EVENT_QUIT
        || e.type == EVENT_WINDOW_CLOSE_REQUESTED
        || e.type == EVENT_KEY_DOWN
        || e.type == EVENT_KEY_UP
        || e.type == EVENT_TEXT_EDITING
        || e.type == EVENT_TEXT_INPUT
        || e.type == EVENT_MOUSE_MOTION
        || e.type == EVENT_MOUSE_BUTTON_DOWN
        || e.type == EVENT_MOUSE_BUTTON_UP
        || e.type == EVENT_MOUSE_WHEEL
        || e.type == EVENT_GAMEPAD_AXIS_MOTION
        || e.type == EVENT_GAMEPAD_BUTTON_DOWN
        || e.type == EVENT_GAMEPAD_BUTTON_UP
        || e.type == EVENT_DID_ENTER_BACKGROUND
        || e.type == EVENT_WILL_ENTER_FOREGROUND
        || e.type == EVENT_GAMEPAD_ADDED
        || e.type == EVENT_GAMEPAD_REMOVED;
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
    //检测鼠标经过，按下等状态
    //BP_MOUSEMOTION似乎有些问题，待查
    //LOG("{} ", e.type);
    if (e.type == EVENT_MOUSE_MOTION)
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
    if ((e.type == EVENT_MOUSE_BUTTON_DOWN || e.type == EVENT_MOUSE_BUTTON_UP) && e.button.button == BUTTON_LEFT)
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
        int w = e->getPresentWidth();
        int h = e->getPresentHeight();
        Font::getInstance()->draw(std::format("Render one frame in {:.3f} ms", t), 20, w - 300, h - 60);
        Font::getInstance()->draw(std::format("Render texture time is {}", Engine::getInstance()->getRenderTimes()), 20, w - 300, h - 35);
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
    //LOG("{}/{}/{} ", t, t_delay, global_prev_present_ticks_);
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
