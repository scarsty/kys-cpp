#pragma once
#include "AppDelegate.h"
#include "HelloWorldScene.h"

AppDelegate::AppDelegate()
{

}

AppDelegate::~AppDelegate()
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
bool AppDelegate::applicationDidFinishLaunching()
{
	auto engine = Engine::getInstance();
    engine->init();
	BP_Event e;  ///< 事件结构对象
    HelloWorldScene h; //开始界面
    h.push(&h);
    mainLoop(e);
    h.pop();
    return true;
}

/**
*  主程序的绘制逻辑

*  @param [in] 事件结构

*  @return void
*/
void AppDelegate::mainLoop(BP_Event & e)
{
    auto engine = Engine::getInstance();
    auto loop = true;
    while (loop && engine->pollEvent(e) >= 0)
    {
        int t0 = engine->getTicks();
        //从最后一个独占的开始画
        int begin_base = 0;
        for (int i = Base::m_vcBase.size() - 1; i >= 0; i--)	//从大到小寻找全屏层
        {
            if (Base::m_vcBase[i]->m_nfull)				
            {
                begin_base = i;
                break;
            }
        }
        for (int i = begin_base; i < Base::m_vcBase.size(); i++)  //从最高一个
        {
            auto &b = Base::m_vcBase[i];
            if (b->m_bvisible)
                b->draw();
        }
        //处理最上层的消息
		int test = Base::m_vcBase.size();
        if (Base::m_vcBase.size() > 0)
            Base::m_vcBase.back()->dealEvent(e);
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
        int t = std::max(0, 25 - (t1 - t0));
        engine->delay(t);
    }
}
