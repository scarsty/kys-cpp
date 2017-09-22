#pragma once
#include "Application.h"
#include "TitleScene.h"

Application::Application()
{
}

Application::~Application()
{
}

/*
* 简要的函数说明文字 
*  @param [in] param1 参数1说明
*  @param [out] param2 参数2说明
*  @return 返回值说明
*/


/** 
*  主程序首先进入场景的入口
*  @param [in] viod
*  @return True
*/
int Application::launch()
{
	auto engine = Engine::getInstance();
    engine->init();
	BP_Event e;  ///< 事件结构对象
    auto s = new TitleScene(); //开始界面
    s->push(s);
    mainLoop(e);
    s->pop();
    return 0;
}

/**
*  主程序的绘制逻辑
*  @param [in] 事件结构
*  @return void
*/
void Application::mainLoop(BP_Event & e)
{
    auto engine = Engine::getInstance();
    auto loop = true;
    while (loop && engine->pollEvent(e) >= 0)
    {
        int t0 = engine->getTicks();
        //从最后一个独占屏幕的场景开始画
        int begin_base = 0;
        for (int i = Base::base_vector_.size() - 1; i >= 0; i--)	//从大到小寻找全屏层
        {
            if (Base::base_vector_[i]->full_window_)
            {
                begin_base = i;
                break;
            }
        }
        for (int i = begin_base; i < Base::base_vector_.size(); i++)  //从最高一个
        {
            auto &b = Base::base_vector_[i];
            if (b->visible_)
                b->draw();
        }
        //处理最上层的消息
		int test = Base::base_vector_.size();
        if (Base::base_vector_.size() > 0)
            Base::base_vector_.back()->dealEvent(e);
        switch (e.type)
        {
        case BP_QUIT:
            if (engine->showMessage("Quit"))
                loop = false;
            break;
        default:
            break;
        }
        engine->renderPresent();
        int t1 = engine->getTicks();		
        int t = max(0, 25 - (t1 - t0));
        engine->delay(t);
    }
}
