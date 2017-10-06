#include "Element.h"
#include "UI.h"

std::vector<Element*> Element::root_;
int Element::prev_present_ticks_ = 0;

Element::~Element()
{
    for (auto c : childs_)
    {
        delete c;
    }
}

void Element::drawAll()
{
    //从最后一个独占屏幕的场景开始画
    int begin_base = 0;
    for (int i = 0; i < root_.size(); i++)    //记录最后一个全屏的层
    {
        root_[i]->backRun();
        if (root_[i]->full_window_)
        {
            begin_base = i;
        }
    }
    for (int i = begin_base; i < root_.size(); i++)  //从最后一个全屏层开始画
    {
        auto b = root_[i];
        if (b->visible_)
        {
            b->drawSelfAndChilds();
        }
    }
}

//设置位置，会改变子节点的位置
void Element::setPosition(int x, int y)
{
    for (auto c : childs_)
    {
        c->setPosition(c->x_ + x - x_, c->y_ + y - y_);
    }
    x_ = x; y_ = y;
}

//运行
//参数为是否在root中运行，为真则参与绘制，为假则不会被画出
int Element::run(bool in_root /*= true*/)
{
    exit_ = false;
    if (in_root) { addOnRootTop(this); }
    onEntrance();
    while (!exit_)
    {
        if (Element::root_.size() == 0) { break; }
        drawAll();
        checkEventAndPresent(25, true);
    }
    onExit();
    if (in_root) { removeFromRoot(this); }
    return result_;
}

//从绘制的根节点移除
Element* Element::removeFromRoot(Element* element)
{
    if (element == nullptr)
    {
        if (root_.size() > 0)
        {
            element = root_.back();
            root_.pop_back();
        }
    }
    else
    {
        for (int i = 0; i < root_.size(); i++)
        {
            if (root_[i] == element)
            {
                root_.erase(root_.begin() + i);
                break;
            }
        }
    }
    return element;
}

//添加子节点
void Element::addChild(Element* element)
{
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
void Element::drawSelfAndChilds()
{
    if (visible_)
    {
        draw();
        for (auto c : childs_)
        {
            if (c->visible_) { c->drawSelfAndChilds(); }
        }
    }
}

//处理自身的事件响应
//只处理当前的节点和当前节点的子节点，检测鼠标是否在范围内
void Element::checkStateAndEvent(BP_Event& e)
{
    if (visible_)
    {
        //注意这里是反向
        for (int i = childs_.size() - 1; i >= 0; i--)
        {
            childs_[i]->checkStateAndEvent(e);
        }
        //setState(Normal);
        if (e.type == BP_MOUSEMOTION)
        {
            if (inSide(e.motion.x, e.motion.y))
            {
                state_ = Pass;
            }
            else
            {
                state_ = Normal;
            }
        }
        if (e.type == BP_MOUSEBUTTONDOWN || e.type == BP_MOUSEBUTTONUP)
        {
            if (inSide(e.motion.x, e.motion.y))
            {
                state_ = Press;
            }
            else
            {
                state_ = Normal;
            }
        }
        if ((e.type == BP_KEYDOWN && (e.key.keysym.sym == BPK_RETURN || e.key.keysym.sym == BPK_SPACE)))
        {
            if (state_ == Pass)
            {
                state_ = Press;
            }
        }
        //注意下个时序才会画，所以可以在dealEvent中改变原有状态
        dealEvent(e);
    }
}

//检测事件并将绘制的图显示出来
//drawAll与checkEventAndPresent(false)配合可以用来制作动画
void Element::checkEventAndPresent(int max_delay, bool check_event)
{
    BP_Event e;
    auto engine = Engine::getInstance();
    while (engine->pollEvent(e) > 0);  //实际是只要最后一个事件
    //engine->pollEvent(e);
    if (check_event)
    {
        checkStateAndEvent(e);
    }

    if ((e.type == BP_KEYUP && (e.key.keysym.sym == BPK_RETURN || e.key.keysym.sym == BPK_SPACE))
        || (e.type == BP_MOUSEBUTTONUP && e.button.button == BP_BUTTON_LEFT))
    {
        pressedOK();
    }
    if ((e.type == BP_KEYUP && e.key.keysym.sym == BPK_ESCAPE)
        || (e.type == BP_MOUSEBUTTONUP && e.button.button == BP_BUTTON_RIGHT))
    {
        pressedCancel();
    }

    switch (e.type)
    {
    case BP_QUIT:
        //if (engine->showMessage("Quit"))
        //loop_ = false;
        break;
    default:
        break;
    }
    int t1 = engine->getTicks();
    int t = max_delay - (t1 - prev_present_ticks_);
    if (t > max_delay) { t = max_delay; }
    if (t <= 0) { t = 1; }
    engine->delay(t);
    engine->renderPresent();
    prev_present_ticks_ = t1;
}

void Element::setAllChildState(State s)
{
    for (auto c : childs_)
    {
        c->state_ = s;;
    }
}

void Element::setChildState(int i, State s)
{
    if (i >= 0 && i < childs_.size())
    {
        childs_[i]->state_ = s;
    }
}

int Element::findNextVisibleChild(int i0, int direct)
{
    if (direct == 0 || childs_.size() == 0) { return i0; }
    direct = direct/abs(direct);

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

