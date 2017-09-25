#include "Base.h"
#include <stdio.h>
#include <stdarg.h>

std::vector<Base*> Base::root_;

Base::~Base()
{
    for (auto c : childs_)
    {
        delete c;
    }
}

void Base::LOG(const char* format, ...)
{
    char s[1000];
    va_list arg_ptr;
    va_start(arg_ptr, format);
    vsprintf(s, format, arg_ptr);
    va_end(arg_ptr);
    fprintf(stderr, s);
}

void Base::drawAll()
{
    //从最后一个独占屏幕的场景开始画
    int begin_base = 0;
    for (int i = 0; i < Base::root_.size(); i++)    //记录最后一个全屏的层
    {
        root_[i]->backRun();
        if (Base::root_[i]->full_window_)
        {
            begin_base = i;
        }
    }
    for (int i = begin_base; i < Base::root_.size(); i++)  //从最后一个全屏层开始画
    {
        auto b = Base::root_[i];
        if (b->visible_)
        { b->drawSelfAndChilds(); }
    }
}

void Base::setPosition(int x, int y)
{
    for (auto c : childs_)
    {
        c->x_ += x - x_;
        c->y_ += y - y_;
    }
    x_ = x; y_ = y;
}

int Base::run()
{
    BP_Event e;
    auto engine = Engine::getInstance();
    push(this);
    entrance();
    loop_ = true;
    result_ = -1;
    while (loop_ && engine->pollEvent(e) >= 0)
    {
        int t0 = engine->getTicks();
        if (Base::root_.size() == 0) { break; }
        Base::drawAll();
        checkStateAndEvent(e);
        switch (e.type)
        {
        case BP_QUIT:
            //if (engine->showMessage("Quit"))
            //loop_ = false;
            break;
        default:
            break;
        }
        engine->renderPresent();
        int t1 = engine->getTicks();
        int t = 25 - (t1 - t0);
        if (t > 0)
        { engine->delay(t); }
    }
    exit();
    pop();
    return result_;
}

Base* Base::pop()
{
    Base* b = nullptr;
    if (root_.size() > 0)
    {
        b = root_.back();
        root_.pop_back();
    }
    //某些特殊的节点不可清理
    //if (b != UI::getInstance())
    //{
    //    delete b;
    //}
    return b;
}

void Base::drawSelfAndChilds()
{
    if (visible_)
    {
        draw();
        for (auto c : childs_)
        {
            c->draw();
        }
    }
}

void Base::addChild(Base* b, int x, int y)
{
    childs_.push_back(b);
    b->setPosition(x_ + x, y_ + y);
}

//只处理当前的节点和当前节点的子节点
void Base::checkStateAndEvent(BP_Event &e)
{
    for (auto c : childs_)
    {
        c->checkStateAndEvent(e);
    }
    //setState(Normal);
    if (e.type == BP_MOUSEMOTION)
    {
        if (inSide(e.motion.x, e.motion.y))
        {
            state_=Pass;
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
    dealEvent(e);
}
