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
    result_ = -1;
    if (in_root) { addOnRootTop(this); }
    onEntrance();
    while (!exit_)
    {
        if (Element::root_.size() == 0) { break; }
        oneFrame(true);
    }
    onExit();
    if (in_root) { removeFromRoot(this); }
    return result_;
}

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

void Element::addChild(Element* element)
{
    childs_.push_back(element);
}

void Element::addChild(Element* element, int x, int y)
{
    addChild(element);
    element->setPosition(x_ + x, y_ + y);
}

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

void Element::clearChilds()
{
    for (auto c : childs_)
    {
        delete c;
    }
    childs_.clear();
}

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
        if (e.type == BP_MOUSEBUTTONDOWN)
        {
            if (inSide(e.motion.x, e.motion.y))
            {
                state_ = Press;
            }
        }
        //注意下个时序才会画，所以可以在dealEvent中改变原有状态
        dealEvent(e);
    }
}

//一次循环中的事务，注意也可以单独调用，用于场景动画
void Element::oneFrame(bool check_event)
{
    BP_Event e;
    auto engine = Engine::getInstance();
    
    drawAll();
    engine->pollEvent(e);
    if (check_event)
    {
        checkStateAndEvent(e);
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
    int t = 25 - (t1 - prev_present_ticks_);
    if (t > 25) { t = 25; }
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
