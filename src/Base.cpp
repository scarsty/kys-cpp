#include "Base.h"
#include <stdio.h>
#include <stdarg.h>

std::vector<Base*> Base::base_root_;

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
    for (int i = 0; i < Base::base_root_.size(); i++)    //记录最后一个全屏的层
    {
        base_root_[i]->backRun();
        if (Base::base_root_[i]->full_window_)
        {
            begin_base = i;
        }
    }
    for (int i = begin_base; i < Base::base_root_.size(); i++)  //从最后一个全屏层开始画
    {
        auto b = Base::base_root_[i];
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

bool Base::inSide(int x, int y)
{
    return x > x_ && x < x_ + w_ && y > y_ && y < y_ + h_;
}

int Base::run()
{
    BP_Event e;
    auto engine = Engine::getInstance();
    push(this);
    enter();
    loop_ = true;
    result_ = -1;
    while (loop_ && engine->pollEvent(e) >= 0)
    {
        int t0 = engine->getTicks();
        if (Base::base_root_.size() == 0) { break; }
        Base::drawAll();

        //处理子节点和本节点的消息        
        for (auto c : childs_)
        {
            c->dealEvent(e);
        }
        dealEvent(e);

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
        if (t>0)
        engine->delay(t);
    }
    exit();
    pop();
    return result_;
}

Base* Base::pop()
{
    Base* b = nullptr;
    if (base_root_.size() > 0)
    {
        b = base_root_.back();
        base_root_.pop_back();
    }
    return b;
}

void Base::drawSelfAndChilds()
{
    draw();
    for (auto c : childs_)
    {
        c->draw();
    }
}

void Base::addChild(Base* b, int x, int y)
{
    childs_.push_back(b);
    b->setPosition(x_ + x, y_ + y);
}