#include "Base.h"
#include <stdio.h>
#include <stdarg.h>

std::vector<Base*> Base::base_need_draw_;

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
    for (int i = 0; i < Base::base_need_draw_.size(); i--)    //记录最后一个全屏的层
    {
        base_need_draw_[i]->backRun();
        if (Base::base_need_draw_[i]->full_window_)
        {
            begin_base = i;
        }
    }
    for (int i = begin_base; i < Base::base_need_draw_.size(); i++)  //从最后一个全屏层开始画
    {
        auto b = Base::base_need_draw_[i];
        if (b->visible_)
        { b->draw(); }
    }
}

bool Base::inSide(int x, int y)
{
    return x > x_ && x < x_ + w_ && y > y_ && y < y_ + h_;
}

void Base::run()
{
    BP_Event e;
    auto engine = Engine::getInstance();
    init();
    push(this);
    entrance();
    loop_ = true;
    while (loop_ && engine->pollEvent(e) >= 0)
    {
        int t0 = engine->getTicks();
        if (Base::base_need_draw_.size() == 0) { break; }
        Base::drawAll();
        //仅处理本层的消息
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
}

Base* Base::pop()
{
    Base* b = nullptr;
    if (base_need_draw_.size() > 0)
    {
        b = base_need_draw_.back();
        base_need_draw_.pop_back();
    }
    return b;
}
