#include "Element.h"
#include "UISystem.h"

std::vector<Element*> Element::root_;
int Element::prev_present_ticks_ = 0;
int Element::refresh_interval_ = 25;

Element::~Element()
{
    for (auto c : childs_)
    {
        if (c && c->parent_ == this)
        {
            delete c;
        }
    }
}

void Element::drawAll()
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

//设置位置，会改变子节点的位置
void Element::setPosition(int x, int y)
{
    for (auto c : childs_)
    {
        c->setPosition(c->x_ + x - x_, c->y_ + y - y_);
    }
    x_ = x;
    y_ = y;
}

//从绘制的根节点移除
Element* Element::removeFromRoot(Element* element)
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
void Element::addChild(Element* element)
{
    element->setTag(childs_.size());
    if (element->parent_ == nullptr)
    {
        element->parent_ = this;
    }
    childs_.push_back(element);
}

//添加节点并同时设置子节点的位置
void Element::addChild(Element* element, int x, int y)
{
    addChild(element);
    element->setPosition(x_ + x, y_ + y);
}

//移除某个节点
void Element::removeChild(Element* element)
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
void Element::clearChilds()
{
    for (auto c : childs_)
    {
        delete c;
    }
    childs_.clear();
}

//画出自身和子节点
void Element::drawSelfChilds()
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

void Element::setAllChildState(int s)
{
    for (auto c : childs_)
    {
        c->state_ = s;
    }
}

void Element::setAllChildVisible(bool v)
{
    for (auto c : childs_)
    {
        c->visible_ = v;
    }
}

int Element::findNextVisibleChild(int i0, int direct)
{
    if (direct == 0 || childs_.size() == 0)
    {
        return i0;
    }
    direct = direct > 0 ? 1 : -1;

    int i1 = i0;
    for (int i = 1; i < childs_.size(); i++)
    {
        i1 += direct;
        i1 = (i1 + childs_.size()) % childs_.size();
        if (childs_[i1]->visible_)
        {
            return i1;
        }
    }
    return i0;
}

int Element::findFristVisibleChild()
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

void Element::checkFrame()
{
    if (stay_frame_ > 0 && current_frame_ >= stay_frame_)
    {
        exit_ = true;
    }
}

//处理自身的事件响应
//只处理当前的节点和当前节点的子节点，检测鼠标是否在范围内
//注意全屏类的节点要一直接受事件
void Element::checkStateSelfChilds(BP_Event& e, bool check_event)
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
            checkChildState();
            //可以在dealEvent中改变原有状态，强制设置某些情况
            dealEvent(e);
            //为简化代码，将按下回车和ESC的操作写在此处
            if (isPressOK(e))
            {
                onPressedOK();
                setAllChildState(Normal);
            }
            if (isPressCancel(e))
            {
                onPressedCancel();
                setAllChildState(Normal);
            }
        }
        dealEvent2(e);
    }
    else
    {
        state_ = Normal;
    }
}

void Element::backRunSelfChilds()
{
    for (auto c : childs_)
    {
        c->backRunSelfChilds();
    }
    backRun();
}

//检测事件
void Element::dealEventSelfChilds(bool check_event)
{
    if (check_event)
    {
        BP_Event e;
        e.type = BP_FIRSTEVENT;
        while (Engine::pollEvent(e))
        {
            //一些按键类提前停止避免丢键
            if (e.type == BP_MOUSEBUTTONDOWN || e.type == BP_MOUSEBUTTONUP || e.type == BP_KEYDOWN || e.type == BP_KEYUP)
            {
                break;
            }
        }
        //Engine::pollEvent(e);
        //if (e.type == BP_MOUSEBUTTONUP)
        //{
        //    printf("BP_MOUSEBUTTONUP\n");
        //}
        checkStateSelfChilds(e, check_event);
        switch (e.type)
        {
        case BP_QUIT:
            UISystem::askExit(1);
            break;
        default:
            break;
        }
    }
    else
    {
        Engine::pollEvent();
    }
}

void Element::forceActiveChild()
{
    for (int i = 0; i < childs_.size(); i++)
    {
        childs_[i]->setState(Normal);
        if (i == active_child_)
        {
            childs_[i]->setState(Pass);
        }
    }
}

void Element::checkChildState()
{
    //注意active是不改的，维持上一次的状态
    //获取子节点的状态
    for (int i = 0; i < getChildCount(); i++)
    {
        if (getChild(i)->getState() != Normal)
        {
            active_child_ = i;
        }
    }
}

void Element::checkSelfState(BP_Event& e)
{
    //检测鼠标经过，按下等状态
    //注意BP_MOUSEMOTION在mac下面有些问题，待查
    if (e.type == BP_MOUSEMOTION)
    {
        if (inSide(e.motion.x, e.motion.y))
        {
            state_ = Pass;
            if (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT))
            {
                state_ = Press;
            }
        }
        else
        {
            state_ = Normal;
        }
    }
    if ((e.type == BP_MOUSEBUTTONDOWN || e.type == BP_MOUSEBUTTONUP) && e.button.button == BP_BUTTON_LEFT)
    {
        if (inSide(e.button.x, e.button.y))
        {
            state_ = Press;
        }
        else
        {
            state_ = Normal;
        }
    }
    if ((e.type == BP_KEYDOWN || e.type == BP_KEYUP) && (e.key.keysym.sym == BPK_RETURN || e.key.keysym.sym == BPK_SPACE))
    {
        //按下键盘的空格或者回车时，将pass的按键改为press
        if (state_ == Pass)
        {
            state_ = Press;
        }
    }
}

void Element::present()
{
    int t1 = Engine::getTicks();
    int t = refresh_interval_ - (t1 - prev_present_ticks_);
    if (t > refresh_interval_)
    {
        t = refresh_interval_;
    }
    if (t > 0)
    {
        Engine::delay(t);
    }
    prev_present_ticks_ = t1;
    Engine::getInstance()->renderPresent();
}

//运行本节点，参数为是否在root中运行，为真则参与绘制，为假则不会被画出
int Element::run(bool in_root /*= true*/)
{
    exit_ = false;
    visible_ = true;
    if (in_root)
    {
        addOnRootTop(this);
    }
    onEntrance();
    running_ = true;
    while (!exit_)
    {
        if (root_.empty())
        {
            break;
        }
        dealEventSelfChilds(true);
        drawAll();
        checkFrame();
        present();
    }
    running_ = false;
    onExit();
    if (in_root)
    {
        removeFromRoot(this);
    }
    return result_;
}

void Element::exitAll(int begin)
{
    for (int i = begin; i < root_.size(); i++)
    {
        root_[i]->exit_ = true;
        root_[i]->visible_ = false;
    }
}

//专门用来某些情况下动画的显示和延时
//中间可以插入一个函数补充些什么，想不到更好的方法了
int Element::drawAndPresent(int times, std::function<void(void*)> func, void* data)
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
